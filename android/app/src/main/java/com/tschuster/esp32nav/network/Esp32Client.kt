package com.tschuster.esp32nav.network

import android.net.Network
import android.util.Log
import java.util.concurrent.TimeUnit
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import okhttp3.*
import okio.ByteString.Companion.toByteString

private const val TAG = "ESP32Nav/WS"

/**
 * WebSocket client hacia el ESP32.
 *
 * Protocolo (envío): Texto → JSON: {"t":"gps","lat":0.0,"lon":0.0}
 *                              {"t":"nav","step":"...","dist":"200m","eta":"12 min"}
 *                              {"t":"notif","app":"...","title":"...","text":"..."}
 *                              {"t":"gmaps","step":"...","street":"...","dist":"...","eta":"...","maneuver":"..."}
 *                              {"t":"media","app":"...","title":"...","artist":"...","playing":true,"vol":75}
 * Protocolo (recepción): {"t":"media_cmd","cmd":"play"|"pause"|"next"|"prev"|"vol_up"|"vol_down"}
 * Binario → JPEG bytes directos (legacy)
 */
class Esp32Client {

    enum class State {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        ERROR
    }

    private val _state = MutableStateFlow(State.DISCONNECTED)
    val state = _state.asStateFlow()

    private var ws: WebSocket? = null
    private var httpClient: OkHttpClient? = null

    /** Llamado cuando el ESP32 envía un mensaje de texto (p.ej. media_cmd). */
    var onMessageReceived: ((String) -> Unit)? = null

    companion object {
        const val ESP32_IP = "192.168.4.1"
        const val ESP32_PORT = 8080
    }

    // ── Conectar ─────────────────────────────────────────────────
    fun connect(network: Network) {
        disconnect()
        _state.value = State.CONNECTING
        val url = "ws://$ESP32_IP:$ESP32_PORT/ws"
        Log.i(
                TAG,
                "connect: url=$url network=$network networkHandle=${network.getNetworkHandle()} socketFactory=${network.socketFactory}"
        )

        httpClient =
                OkHttpClient.Builder()
                        .socketFactory(network.socketFactory)
                        .connectTimeout(10, TimeUnit.SECONDS)
                        .readTimeout(0, TimeUnit.SECONDS) // WebSocket: sin timeout de lectura
                        .build()

        val request = Request.Builder().url(url).build()

        Log.d(TAG, "newWebSocket() llamado (OkHttp usará el socketFactory de la red ESP32)")
        httpClient!!.newWebSocket(
                request,
                object : WebSocketListener() {
                    override fun onOpen(webSocket: WebSocket, response: Response) {
                        Log.i(TAG, "WebSocket conectado al ESP32 response.code=${response.code}")
                        ws = webSocket
                        _state.value = State.CONNECTED
                    }
                    override fun onFailure(
                            webSocket: WebSocket,
                            t: Throwable,
                            response: Response?
                    ) {
                        Log.e(
                                TAG,
                                "WebSocket onFailure: message=${t.message} type=${t.javaClass.simpleName} cause=${t.cause}"
                        )
                        Log.e(
                                TAG,
                                "WebSocket onFailure: response code=${response?.code} message=${response?.message}"
                        )
                        t.printStackTrace()
                        ws = null
                        _state.value = State.ERROR
                    }
                    override fun onMessage(webSocket: WebSocket, text: String) {
                        onMessageReceived?.invoke(text)
                    }
                    override fun onClosed(webSocket: WebSocket, code: Int, reason: String) {
                        Log.w(TAG, "WebSocket cerrado: code=$code reason=$reason")
                        ws = null
                        _state.value = State.DISCONNECTED
                    }
                }
        )
    }

    // ── Enviar GPS ───────────────────────────────────────────────
    fun sendGps(lat: Double, lon: Double, speedKmh: Int = 0) {
        ws?.send(
                """{"t":"gps","lat":${"%.6f".format(lat)},"lon":${"%.6f".format(lon)},"spd":$speedKmh}"""
        )
    }

    // ── Enviar tile de mapa (JPEG) ───────────────────────────────
    fun sendMapTile(jpeg: ByteArray) {
        val socket = ws
        if (socket == null) {
            Log.w(
                    TAG,
                    "sendMapTile: WebSocket es null (no conectado o ya cerrado), no se envía (${jpeg.size} bytes)"
            )
            return
        }
        val ok = socket.send(jpeg.toByteString())
        if (ok) {
            Log.d(TAG, "sendMapTile: enviados ${jpeg.size} bytes por WebSocket")
        } else {
            Log.e(
                    TAG,
                    "sendMapTile: send() devolvió false (cola llena o cerrado), ${jpeg.size} bytes no enviados"
            )
        }
    }

    // ── Enviar frame vectorial ───────────────────────────────────
    fun sendVectorFrame(json: String) {
        ws?.send(json)
    }

    // ── Enviar paso de navegación ────────────────────────────────
    /** [etaFormatted] ya debe ser el texto final, p. ej. "1 h 25 min" o "45 min". */
    fun sendNavStep(step: String, distanceM: Int, etaFormatted: String) {
        val safeStep = step.replace("\"", "'")
        val distStr =
                if (distanceM >= 1000) {
                    "%.1f km".format(distanceM / 1000.0)
                } else {
                    "$distanceM m"
                }
        ws?.send("""{"t":"nav","step":"$safeStep","dist":"$distStr","eta":"$etaFormatted"}""")
    }

    fun sendNavArrived() {
        ws?.send("""{"t":"nav","step":"Llegaste al destino","dist":"0m","eta":"0 min"}""")
    }

    // ── Enviar notificación genérica ─────────────────────────────
    fun sendNotif(app: String, title: String, text: String) {
        val safeApp   = app.replace("\"", "'").take(24)
        val safeTitle = title.replace("\"", "'").take(32)
        val safeText  = text.replace("\"", "'").take(60)
        ws?.send("""{"t":"notif","app":"$safeApp","title":"$safeTitle","text":"$safeText"}""")
    }

    // ── Enviar paso de Google Maps ───────────────────────────────
    fun sendGmaps(step: String, street: String, dist: String, eta: String, maneuver: String) {
        val s   = step.replace("\"", "'").take(60)
        val st  = street.replace("\"", "'").take(48)
        val d   = dist.replace("\"", "'").take(16)
        val e   = eta.replace("\"", "'").take(16)
        val m   = maneuver.replace("\"", "'").take(32)
        ws?.send("""{"t":"gmaps","step":"$s","street":"$st","dist":"$d","eta":"$e","maneuver":"$m"}""")
    }

    // ── Enviar estado de media ───────────────────────────────────
    fun sendMedia(app: String, title: String, artist: String, playing: Boolean, vol: Int) {
        val safeApp    = app.replace("\"", "'").take(24)
        val safeTitle  = title.replace("\"", "'").take(48)
        val safeArtist = artist.replace("\"", "'").take(32)
        ws?.send("""{"t":"media","app":"$safeApp","title":"$safeTitle","artist":"$safeArtist","playing":$playing,"vol":$vol}""")
    }

    // ── Desconectar ──────────────────────────────────────────────
    fun disconnect() {
        ws?.close(1000, "disconnect")
        ws = null
        // No llamar shutdown() en el executor: reconstruirlo en cada reconexión es costoso.
        // El cliente es GC'd naturalmente al nullear la referencia.
        httpClient = null
        _state.value = State.DISCONNECTED
    }
}
