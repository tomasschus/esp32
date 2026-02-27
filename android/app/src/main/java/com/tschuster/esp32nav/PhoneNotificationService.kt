package com.tschuster.esp32nav

import android.app.Notification
import android.content.ComponentName
import android.media.AudioManager
import android.media.MediaMetadata
import android.media.session.MediaController
import android.media.session.MediaSessionManager
import android.media.session.PlaybackState
import android.provider.Settings
import android.service.notification.NotificationListenerService
import android.service.notification.StatusBarNotification
import android.util.Log
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.flow.asStateFlow

private const val TAG = "ESP32Nav/PhoneNotifSvc"

private val BLOCKED_PACKAGES = setOf(
    "com.tschuster.esp32nav",
    "android",
    "com.android.systemui",
    "com.android.phone",
    "com.google.android.gms",
    "com.google.android.gsf",
    "com.google.android.inputmethod.latin",
)

/**
 * Configuración por paquete.
 * - [displayName]: nombre a mostrar en la pantalla (null = detectar del sistema)
 * - [bypassFlags]: ignorar FLAG_FOREGROUND_SERVICE / FLAG_ONGOING_EVENT (p.ej. Google Maps)
 * - [type]: cómo interpretar la notificación
 *
 * Para agregar una app nueva: una línea en APP_REGISTRY.
 */
private enum class NotifType { MAPS, MESSAGING, DEFAULT }

private data class AppConfig(
    val displayName: String?  = null,
    val bypassFlags: Boolean  = false,
    val type: NotifType       = NotifType.DEFAULT,
)

private val APP_REGISTRY: Map<String, AppConfig> = mapOf(
    // ── Navegación ────────────────────────────────────────────────────
    "com.google.android.apps.maps"        to AppConfig(bypassFlags = true, type = NotifType.MAPS),

    // ── Mensajería ────────────────────────────────────────────────────
    "com.whatsapp"                         to AppConfig("WhatsApp",           type = NotifType.MESSAGING),
    "com.whatsapp.w4b"                     to AppConfig("WhatsApp Business",  type = NotifType.MESSAGING),
    "org.telegram.messenger"               to AppConfig("Telegram",           type = NotifType.MESSAGING),
    "org.telegram.messenger.beta"          to AppConfig("Telegram",           type = NotifType.MESSAGING),
    "com.instagram.android"                to AppConfig("Instagram",          type = NotifType.MESSAGING),
    "com.facebook.orca"                    to AppConfig("Messenger",          type = NotifType.MESSAGING),
    "com.discord"                          to AppConfig("Discord",            type = NotifType.MESSAGING),
    "org.thoughtcrime.securesms"           to AppConfig("Signal",             type = NotifType.MESSAGING),

    // ── Email ─────────────────────────────────────────────────────────
    "com.google.android.gm"               to AppConfig("Gmail"),
    "com.microsoft.office.outlook"         to AppConfig("Outlook"),

    // ── Otras (solo override de nombre) ───────────────────────────────
    "com.google.android.dialer"            to AppConfig("Teléfono"),
    "com.android.dialer"                   to AppConfig("Teléfono"),
    "com.spotify.music"                    to AppConfig("Spotify"),
)

data class PhoneNotif(val app: String, val title: String, val text: String)

data class GmapsStep(
    val step: String,
    val street: String,
    val dist: String,
    val eta: String,
    val maneuver: String,
)

data class MediaState(
    val app: String,
    val title: String,
    val artist: String,
    val playing: Boolean,
    val vol: Int,
)

class PhoneNotificationService : NotificationListenerService() {

    companion object {
        private val _notifFlow = MutableSharedFlow<PhoneNotif>(extraBufferCapacity = 8)
        val notifFlow = _notifFlow.asSharedFlow()

        private val _gmapsFlow = MutableStateFlow<GmapsStep?>(null)
        val gmapsFlow = _gmapsFlow.asStateFlow()

        private val _mediaFlow = MutableStateFlow<MediaState?>(null)
        val mediaFlow = _mediaFlow.asStateFlow()

        // Referencia estática para ejecutar comandos de media desde MainViewModel
        private var instance: PhoneNotificationService? = null

        fun executeCommand(cmd: String) {
            instance?.executeMediaCommandInternal(cmd) ?: Log.w(TAG, "executeCommand: servicio no activo")
        }

        fun isPermissionGranted(context: android.content.Context): Boolean {
            val cn = ComponentName(context, PhoneNotificationService::class.java)
            val flat = Settings.Secure.getString(
                context.contentResolver,
                "enabled_notification_listeners"
            )
            return flat?.contains(cn.flattenToString()) == true
        }
    }

    private var mediaSessionManager: MediaSessionManager? = null
    private var activeController: MediaController? = null
    private val audioManager by lazy { getSystemService(AUDIO_SERVICE) as AudioManager }

    private val serviceScope = CoroutineScope(Dispatchers.Default)
    private var gmapsPollerJob: Job? = null

    // ── Lifecycle ─────────────────────────────────────────────────────

    override fun onCreate() {
        super.onCreate()
        instance = this
        Log.i(TAG, "Servicio creado")
    }

    override fun onDestroy() {
        super.onDestroy()
        serviceScope.cancel()
        activeController?.unregisterCallback(mediaCallback)
        activeController = null
        instance = null
        Log.i(TAG, "Servicio destruido")
    }

    override fun onListenerConnected() {
        super.onListenerConnected()
        Log.i(TAG, "Listener conectado — iniciando MediaSession watcher + gmaps poller")
        setupMediaSessionWatcher()
        startGmapsPoller()
    }

    override fun onListenerDisconnected() {
        super.onListenerDisconnected()
        gmapsPollerJob?.cancel()
        activeController?.unregisterCallback(mediaCallback)
        activeController = null
        _mediaFlow.value = null
        Log.i(TAG, "Listener desconectado")
    }

    // ── Google Maps poller ────────────────────────────────────────────
    // Google Maps actualiza la notificación en curso sin disparar onNotificationPosted
    // en cada cambio de giro → leemos activeNotifications cada 2 s.

    private fun startGmapsPoller() {
        gmapsPollerJob?.cancel()
        gmapsPollerJob = serviceScope.launch {
            while (isActive) {
                delay(2_000L)
                pollGmapsNotification()
            }
        }
    }

    private fun pollGmapsNotification() {
        val all = try { activeNotifications } catch (_: Exception) { return }
        val mapsNotifs = all?.filter { it.packageName == "com.google.android.apps.maps" }
        if (mapsNotifs.isNullOrEmpty()) return

        // Buscar la notificación con instrucción de giro (preferencia sobre resumen de viaje).
        // Google Maps puede tener varias notificaciones activas simultáneamente.
        var bestTitle = ""
        var bestText  = ""
        var foundInstruction = false

        for (sbn in mapsNotifs) {
            val extras  = sbn.notification.extras
            val title   = extras.getString(Notification.EXTRA_TITLE)?.trim() ?: ""
            val text    = extras.getString(Notification.EXTRA_TEXT)?.trim() ?: ""
            val bigText = extras.getCharSequence(Notification.EXTRA_BIG_TEXT)?.toString()?.trim() ?: ""
            val subText = extras.getCharSequence(Notification.EXTRA_SUB_TEXT)?.toString()?.trim() ?: ""
            // EXTRA_TEXT_LINES — algunas versiones de Maps ponen la instrucción aquí
            val lines   = extras.getCharSequenceArray(Notification.EXTRA_TEXT_LINES)
            val lineText = lines?.firstOrNull { it.isNotBlank() }?.toString()?.trim() ?: ""

            val candidate = text.ifBlank { lineText }.ifBlank { bigText }.ifBlank { subText }

            Log.d(TAG, "gmaps poll id=${sbn.id}: title='$title' text='$candidate'")

            // Si el título tiene patrón de instrucción (mención de distancia sin ·) → prioridad máxima
            val (step, _) = parseGmapsTitle(title.ifBlank { candidate })
            val looksLikeInstruction = step != title.take(60)          // regex matched
                    || "girar" in title.lowercase() || "turn" in title.lowercase()
                    || "doblar" in title.lowercase() || "tomar" in title.lowercase()
                    || "continua" in title.lowercase() || "salida" in title.lowercase()

            if (looksLikeInstruction && !foundInstruction) {
                bestTitle = title
                bestText  = candidate
                foundInstruction = true
            } else if (!foundInstruction) {
                // Guardar como fallback (resumen de viaje)
                if (bestTitle.isBlank()) { bestTitle = title; bestText = candidate }
            }
        }

        if (bestTitle.isBlank() && bestText.isBlank()) return
        handleGoogleMaps(bestTitle, bestText)
    }

    // ── Notificaciones entrantes ──────────────────────────────────────

    // último (title, text) enviado por package — evita duplicados
    private val lastSent = HashMap<String, Pair<String, String>>()

    override fun onNotificationPosted(sbn: StatusBarNotification) {
        val pkg    = sbn.packageName
        if (pkg in BLOCKED_PACKAGES) return

        val config = APP_REGISTRY[pkg]
        val flags  = sbn.notification.flags

        // Filtrar servicios en primer plano y eventos persistentes, salvo bypass explícito
        if (config?.bypassFlags != true) {
            if (flags and Notification.FLAG_FOREGROUND_SERVICE != 0) return
            if (flags and Notification.FLAG_ONGOING_EVENT != 0) return
            if (flags and Notification.FLAG_GROUP_SUMMARY != 0) return
        }

        val extras = sbn.notification.extras
        val title  = extras.getString(Notification.EXTRA_TITLE)?.trim() ?: ""
        val text   = extras.getString(Notification.EXTRA_TEXT)?.trim()  ?: ""
        if (title.isBlank() && text.isBlank()) return

        when (config?.type) {
            NotifType.MAPS -> {
                // BigText y SubText como fallback — algunas versiones de Maps los usan
                val bigText = extras.getCharSequence(Notification.EXTRA_BIG_TEXT)?.toString()?.trim() ?: ""
                val subText = extras.getCharSequence(Notification.EXTRA_SUB_TEXT)?.toString()?.trim() ?: ""
                val bestText = text.ifBlank { bigText }.ifBlank { subText }
                Log.d(TAG, "gmaps raw: title='$title' text='$text' big='$bigText' sub='$subText'")
                handleGoogleMaps(title, bestText)
            }
            NotifType.MESSAGING -> {
                // Intentar usar EXTRA_TEXT_LINES para el último mensaje de conversaciones agrupadas
                val lines = extras.getCharSequenceArray(Notification.EXTRA_TEXT_LINES)
                val msgText = lines?.lastOrNull()?.toString()?.trim()?.takeIf { it.isNotBlank() } ?: text
                dispatchGenericNotif(pkg, title, msgText, config.displayName)
            }
            else -> {
                if (title.isBlank()) return
                dispatchGenericNotif(pkg, title, text, config?.displayName)
            }
        }
    }

    // ── Google Maps ───────────────────────────────────────────────────

    private fun handleGoogleMaps(title: String, text: String) {
        // Formato A (instrucción): "En 200 m, girar a la derecha" / "Turn right in 200 m"
        //   text = "Av. Corrientes · 15 min"
        // Formato B (resumen entre giros): "3 min · 1,1 km · Llegada: 03:02"
        //   text = "" (vacío)
        // Formato C (sin distancia): title="" text="instrucción completa"

        val instrSource = title.ifBlank { text }
        val (step, dist) = parseGmapsTitle(instrSource)

        // Si parseGmapsTitle no detectó patrón de maniobra (devuelve el título íntegro)
        // Y el título tiene separadores ·/•, es el resumen de viaje entre giros.
        val isSummary = step == instrSource.take(60) &&
                        (instrSource.contains('·') || instrSource.contains('•'))

        val finalStep: String
        val finalDist: String
        val finalStreet: String
        val finalEta: String

        if (isSummary) {
            // "3 min · 1,1 km · Llegada: 03:02"
            val (eta, remaining) = parseSummaryTitle(instrSource)
            finalStep   = ""
            finalDist   = remaining
            finalStreet = ""
            finalEta    = eta
        } else {
            val (street, eta) = parseGmapsText(if (title.isNotBlank()) text else "")
            finalStep   = step
            finalDist   = dist
            finalStreet = street
            finalEta    = eta
        }

        val maneuver = if (isSummary) "straight" else inferManeuver(instrSource)
        val gmaps = GmapsStep(step = finalStep, street = finalStreet,
                              dist = finalDist, eta = finalEta, maneuver = maneuver)
        _gmapsFlow.value = gmaps
        Log.d(TAG, "gmaps[${if(isSummary) "resumen" else "giro"}]: step='$finalStep' " +
                   "street='$finalStreet' dist='$finalDist' eta='$finalEta'")
    }

    /** "3 min · 1,1 km · Llegada: 03:02"  →  eta="3 min", dist="1,1 km" */
    private fun parseSummaryTitle(title: String): Pair<String, String> {
        val parts  = title.split("·", "•").map { it.trim() }
        val reEta  = Regex("""\d+\s*min""")
        val reDist = Regex("""\d+[,.]?\d*\s*(km|m)\b""")
        val eta    = parts.firstOrNull { reEta.containsMatchIn(it) && !reDist.containsMatchIn(it) } ?: ""
        val dist   = parts.firstOrNull { reDist.containsMatchIn(it) } ?: ""
        return eta to dist
    }

    private fun parseGmapsTitle(title: String): Pair<String, String> {
        // "En 200 m, girar a la derecha" → step="girar a la derecha", dist="200 m"
        val reEsEnDist = Regex("""^[Ee]n\s+(\d+[\.,]?\d*\s*(?:m|km)),?\s*(.+)$""")
        reEsEnDist.find(title.trim())?.let { m ->
            return m.groupValues[2].trimEnd('.') to m.groupValues[1]
        }
        // "Turn right in 200 m"
        val reEnIn = Regex("""^(.+)\s+in\s+(\d+[\.,]?\d*\s*(?:m|km|ft|mi))\.?$""")
        reEnIn.find(title.trim())?.let { m ->
            return m.groupValues[1] to m.groupValues[2]
        }
        return title.take(60) to ""
    }

    private fun parseGmapsText(text: String): Pair<String, String> {
        // "Av. Corrientes · 15 min"  →  street="Av. Corrientes", eta="15 min"
        val parts = text.split("·", "•", "|").map { it.trim() }
        return when {
            parts.size >= 2 -> parts[0] to parts[1]
            parts.size == 1 -> parts[0] to ""
            else             -> "" to ""
        }
    }

    private fun inferManeuver(title: String): String {
        val t = title.lowercase()
        return when {
            "media vuelta" in t || "u-turn" in t || "u turn" in t || "retorno" in t -> "uturn"
            "rotonda" in t || "roundabout" in t -> "roundabout"
            "izquierda" in t || "left" in t -> "turn-left"
            "derecha" in t || "right" in t -> "turn-right"
            "recto" in t || "straight" in t || "continúa" in t || "continue" in t -> "straight"
            "destino" in t || "arrived" in t || "llegaste" in t -> "arrive"
            else -> "straight"
        }
    }

    // ── Notificaciones genéricas ──────────────────────────────────────

    private fun dispatchGenericNotif(pkg: String, title: String, text: String, overrideLabel: String? = null) {
        if (lastSent[pkg] == Pair(title, text)) return   // duplicado exacto → ignorar
        lastSent[pkg] = Pair(title, text)

        val appName = overrideLabel ?: getAppLabel(pkg)
        _notifFlow.tryEmit(PhoneNotif(app = appName, title = title.take(32), text = text.take(60)))
        Log.d(TAG, "notif: app='$appName' title='$title'")
    }

    private fun getAppLabel(pkg: String): String {
        return try {
            val info = packageManager.getApplicationInfo(pkg, 0)
            packageManager.getApplicationLabel(info).toString()
        } catch (e: Exception) {
            pkg.substringAfterLast('.')
        }
    }

    // ── MediaSession ──────────────────────────────────────────────────

    private fun setupMediaSessionWatcher() {
        val msm = getSystemService(MEDIA_SESSION_SERVICE) as? MediaSessionManager ?: return
        mediaSessionManager = msm
        val cn = ComponentName(this, PhoneNotificationService::class.java)

        try {
            msm.addOnActiveSessionsChangedListener({ controllers ->
                onActiveSessionsChanged(controllers)
            }, cn)
            onActiveSessionsChanged(msm.getActiveSessions(cn))
        } catch (e: SecurityException) {
            Log.w(TAG, "Sin permiso para MediaSessionManager: ${e.message}")
        }
    }

    private fun onActiveSessionsChanged(controllers: List<MediaController>?) {
        activeController?.unregisterCallback(mediaCallback)
        activeController = controllers?.firstOrNull()
        activeController?.registerCallback(mediaCallback)
        updateMediaState()
        Log.d(TAG, "sesiones activas: ${controllers?.map { it.packageName }}")
    }

    private val mediaCallback = object : MediaController.Callback() {
        override fun onMetadataChanged(metadata: MediaMetadata?) = updateMediaState()
        override fun onPlaybackStateChanged(state: PlaybackState?) = updateMediaState()
    }

    private fun updateMediaState() {
        val mc = activeController ?: run {
            _mediaFlow.value = null
            return
        }
        val meta = mc.metadata ?: return
        val state = mc.playbackState
        val vol = audioManager.getStreamVolume(AudioManager.STREAM_MUSIC)
        val maxVol = audioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC)
        val volPct = if (maxVol > 0) (vol * 100) / maxVol else 0

        _mediaFlow.value = MediaState(
            app = getAppLabel(mc.packageName),
            title = (meta.getString(MediaMetadata.METADATA_KEY_TITLE) ?: "").take(48),
            artist = (meta.getString(MediaMetadata.METADATA_KEY_ARTIST) ?: "").take(32),
            playing = state?.state == PlaybackState.STATE_PLAYING,
            vol = volPct,
        )
    }

    // ── Comandos de media (llamados desde MainViewModel) ──────────────

    private fun executeMediaCommandInternal(cmd: String) {
        val mc = activeController
        if (mc == null) {
            Log.w(TAG, "executeCommand '$cmd': sin sesión activa")
            return
        }
        val controls = mc.transportControls
        when (cmd) {
            "play"     -> controls.play()
            "pause"    -> controls.pause()
            "next"     -> controls.skipToNext()
            "prev"     -> controls.skipToPrevious()
            "vol_up"   -> audioManager.adjustStreamVolume(
                AudioManager.STREAM_MUSIC, AudioManager.ADJUST_RAISE, 0)
            "vol_down" -> audioManager.adjustStreamVolume(
                AudioManager.STREAM_MUSIC, AudioManager.ADJUST_LOWER, 0)
            else -> Log.w(TAG, "executeCommand: cmd desconocido '$cmd'")
        }
        Log.d(TAG, "executeCommand '$cmd' OK → ${mc.packageName}")
    }
}
