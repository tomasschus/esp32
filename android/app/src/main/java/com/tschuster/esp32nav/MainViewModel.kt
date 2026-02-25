package com.tschuster.esp32nav

import android.app.Application
import android.content.Intent
import android.content.SharedPreferences
import android.location.Location
import android.util.Log
import androidx.core.content.ContextCompat
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.tschuster.esp32nav.location.LocationTracker
import com.tschuster.esp32nav.network.Esp32Client
import com.tschuster.esp32nav.network.GeocodeSuggestion
import com.tschuster.esp32nav.network.NavRoute
import com.tschuster.esp32nav.network.NavRouter
import com.tschuster.esp32nav.network.NetworkManager
import com.tschuster.esp32nav.network.VectorFetcher
import com.tschuster.esp32nav.util.VectorRenderer
import com.tschuster.esp32nav.util.formatEtaMinutes
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.filter
import kotlinx.coroutines.flow.filterNotNull
import kotlinx.coroutines.flow.launchIn
import kotlinx.coroutines.flow.onEach
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

private const val TAG = "ESP32Nav/VM"
private const val PREF_RECENT = "recent_searches"
private const val PREF_RECENT_PREFIX = "recent_"
private const val MAX_RECENT = 3

data class UiState(
        val status: String = "Iniciando…",
        val isConnecting: Boolean = false,
        val isConnected: Boolean = false,
        val isScanning: Boolean = false,
        val scanResults: List<String> = emptyList(),
        val lastSsid: String? = null,
        val location: Location? = null,
        val zoom: Int = 17,
        val route: NavRoute? = null,
        val currentStepIndex: Int = 0,
        val navDestination: String = "",
        val isSearchingRoute: Boolean = false,
        val suggestions: List<GeocodeSuggestion> = emptyList(),
        val isSearchingSuggestions: Boolean = false,
        val recentSearches: List<GeocodeSuggestion> = emptyList(),
        val errorMsg: String? = null,
)

class MainViewModel(app: Application) : AndroidViewModel(app) {

    private val networkManager = (app as ESP32NavApp).networkManager
    private val locationTracker = LocationTracker(app)
    private val navRouter = NavRouter()
    private val esp32Client = Esp32Client()
    private val vectorFetcher = VectorFetcher()
    private val recentPrefs: SharedPreferences =
            getApplication<Application>()
                    .getSharedPreferences(PREF_RECENT, Application.MODE_PRIVATE)

    private val _ui =
            MutableStateFlow(
                    UiState(
                            lastSsid = networkManager.getLastSsid(),
                            recentSearches = loadRecentSearches(),
                    )
            )
    val ui: StateFlow<UiState> = _ui.asStateFlow()

    private var lastLocation: Location? = null
    private var mapJob: Job? = null
    private var roadsJob: Job? = null
    private var wsRetryJob: Job? = null
    private var autoReconnectJob: Job? = null
    private var suggestionsJob: Job? = null
    private var stepIdx = 0
    private var arrivedClearJob: Job? = null
    private var recalcJob: Job? = null
    private var lastRecalcTimeMs: Long = 0L

    private val OFF_ROUTE_TOLERANCE_M = 100f
    private val RECALC_COOLDOWN_MS = 60_000L

    // Callbacks registrados por la UI para controlar el MapView (Android)
    private var enableFollowCallback: (() -> Unit)? = null
    private var setCenterCallback: ((Double, Double) -> Unit)? = null
    private var setZoomCallback: ((Int) -> Unit)? = null
    private var setRouteCallback: ((List<Pair<Double, Double>>) -> Unit)? = null
    private var setNavModeCallback: ((Boolean) -> Unit)? = null
    private var updateBearingCallback: ((Float) -> Unit)? = null
    private var initialCenterDone = false

    init {
        observeNetworkManager()
        observeLocation()
        networkManager.requestCellular()
        startAutoReconnect()
    }

    private fun loadRecentSearches(): List<GeocodeSuggestion> {
        val list = mutableListOf<GeocodeSuggestion>()
        for (i in 0 until MAX_RECENT) {
            val label = recentPrefs.getString("${PREF_RECENT_PREFIX}${i}_label", null) ?: break
            val lat =
                    recentPrefs.getString("${PREF_RECENT_PREFIX}${i}_lat", null)?.toDoubleOrNull()
                            ?: break
            val lon =
                    recentPrefs.getString("${PREF_RECENT_PREFIX}${i}_lon", null)?.toDoubleOrNull()
                            ?: break
            list.add(GeocodeSuggestion(label = label, lat = lat, lon = lon))
        }
        return list
    }

    private fun saveRecentSearches(list: List<GeocodeSuggestion>) {
        recentPrefs.edit().apply {
            for (i in 0 until MAX_RECENT) {
                if (i < list.size) {
                    putString("${PREF_RECENT_PREFIX}${i}_label", list[i].label)
                    putString("${PREF_RECENT_PREFIX}${i}_lat", list[i].lat.toString())
                    putString("${PREF_RECENT_PREFIX}${i}_lon", list[i].lon.toString())
                } else {
                    remove("${PREF_RECENT_PREFIX}${i}_label")
                    remove("${PREF_RECENT_PREFIX}${i}_lat")
                    remove("${PREF_RECENT_PREFIX}${i}_lon")
                }
            }
            apply()
        }
    }

    private fun addToRecentSearches(suggestion: GeocodeSuggestion) {
        val current = _ui.value.recentSearches
        val updated =
                listOf(suggestion) +
                        current.filter { it.label != suggestion.label }.take(MAX_RECENT - 1)
        val newList = updated.take(MAX_RECENT)
        saveRecentSearches(newList)
        _ui.value = _ui.value.copy(recentSearches = newList)
    }

    // ── Callbacks del mapa ────────────────────────────────────────────
    /**
     * Registrado desde la Activity una vez que el MapView está listo. [enableFollow] → reactiva el
     * seguimiento automático de ubicación. [setCenter] → centra el mapa en una coordenada.
     * [setZoom] → cambia el zoom del mapa. [setRoute] → dibuja la ruta como polilínea en el mapa
     * Android. [setNavMode] → activa/desactiva modo navegación (seguimiento + rotación).
     * [updateBearing] → rota el mapa según el heading del usuario.
     */
    fun registerMapCallbacks(
            enableFollow: () -> Unit,
            setCenter: (Double, Double) -> Unit,
            setZoom: (Int) -> Unit,
            setRoute: (List<Pair<Double, Double>>) -> Unit,
            setNavMode: (Boolean) -> Unit,
            updateBearing: (Float) -> Unit,
    ) {
        enableFollowCallback = enableFollow
        setCenterCallback = setCenter
        setZoomCallback = setZoom
        setRouteCallback = setRoute
        setNavModeCallback = setNavMode
        updateBearingCallback = updateBearing
        // Si ya tenemos ubicación cuando se registran los callbacks (Activity recreada),
        // centrar inmediatamente sin esperar al próximo tick de GPS.
        if (!initialCenterDone) {
            lastLocation?.let { loc ->
                setCenter(loc.latitude, loc.longitude)
                initialCenterDone = true
            }
        }
        // Restaurar ruta si ya hay una activa (p.ej. tras rotación de pantalla)
        _ui.value.route?.let { route ->
            setRoute(route.geometry)
            setNavMode(true)
        }
    }

    // ── Scan / Connect ────────────────────────────────────────────────
    fun startScan() = networkManager.startScan()

    fun connectToDevice(ssid: String, password: String = "") {
        _ui.value =
                _ui.value.copy(
                        isConnecting = true,
                        status = "Aprobá el diálogo WiFi del sistema…",
                )
        networkManager.connectToEsp32(ssid, password)
    }

    // ── Observe NetworkManager ────────────────────────────────────────
    private fun observeNetworkManager() {
        networkManager
                .wifiState
                .onEach { state ->
                    Log.d(TAG, "wifiState → $state")
                    _ui.value =
                            _ui.value.copy(
                                    isScanning = state == NetworkManager.WifiState.SCANNING,
                                    isConnecting = state == NetworkManager.WifiState.CONNECTING,
                            )
                    if (state == NetworkManager.WifiState.UNAVAILABLE) {
                        val autoReconnecting = autoReconnectJob?.isActive == true
                        _ui.value =
                                _ui.value.copy(
                                        isConnecting = false,
                                        status =
                                                if (autoReconnecting) "Buscando ESP32…"
                                                else "Desconectado",
                                        errorMsg =
                                                if (autoReconnecting) null
                                                else
                                                        "No se pudo conectar. ¿Aprobaste el diálogo del sistema? Verificá que el ESP32 esté encendido.",
                                )
                    }
                }
                .launchIn(viewModelScope)

        networkManager
                .scanResults
                .onEach { results -> _ui.value = _ui.value.copy(scanResults = results) }
                .launchIn(viewModelScope)

        networkManager
                .esp32Network
                .filterNotNull()
                .onEach { network ->
                    Log.i(TAG, "esp32Network recibido → conectando WS + iniciando map loop")
                    _ui.value =
                            _ui.value.copy(
                                    isConnecting = false,
                                    isConnected = true,
                                    lastSsid = networkManager.getLastSsid(),
                                    status = "Conectado",
                            )
                    autoReconnectJob?.cancel()
                    esp32Client.connect(network)
                    startMapLoop()
                    startWsRetryLoop(network)
                    startNavService()
                }
                .launchIn(viewModelScope)

        // La red celular se usa para geocoding, routing y Overpass API
        networkManager
                .cellNetwork
                .filterNotNull()
                .onEach { network ->
                    Log.i(TAG, "cellNetwork recibido → NavRouter + VectorFetcher usarán esta red")
                    navRouter.setInternetNetwork(network)
                    vectorFetcher.setInternetNetwork(network)
                }
                .launchIn(viewModelScope)

        networkManager
                .esp32Network
                .filter { it == null }
                .onEach {
                    if (_ui.value.isConnected) {
                        _ui.value = _ui.value.copy(isConnected = false, status = "Desconectado")
                        mapJob?.cancel()
                        wsRetryJob?.cancel()
                        stopNavService()
                        startAutoReconnect()
                    }
                }
                .launchIn(viewModelScope)
    }

    private fun startNavService() {
        ContextCompat.startForegroundService(
                getApplication(),
                Intent(getApplication(), NavigationService::class.java)
        )
    }

    private fun stopNavService() {
        getApplication<Application>()
                .stopService(Intent(getApplication(), NavigationService::class.java))
    }

    // ── Location ──────────────────────────────────────────────────────
    private fun observeLocation() {
        locationTracker
                .locationFlow()
                .onEach { loc ->
                    lastLocation = loc
                    _ui.value = _ui.value.copy(location = loc)
                    val speedKmh = if (loc.hasSpeed()) (loc.speed * 3.6f).toInt() else 0
                    esp32Client.sendGps(loc.latitude, loc.longitude, speedKmh)
                    advanceNavStep(loc)
                    checkOffRouteAndRecalc(loc)
                    // Primera ubicación: centrar el mapa explícitamente una sola vez.
                    // Las siguientes actualizaciones las maneja enableFollowLocation().
                    if (!initialCenterDone && setCenterCallback != null) {
                        setCenterCallback?.invoke(loc.latitude, loc.longitude)
                        initialCenterDone = true
                    }
                    // Nav mode: rotar el mapa con el heading del usuario
                    if (_ui.value.route != null && loc.hasBearing()) {
                        updateBearingCallback?.invoke(loc.bearing)
                    }
                }
                .launchIn(viewModelScope)
    }

    // ── WebSocket retry loop ──────────────────────────────────────────
    private fun startWsRetryLoop(network: android.net.Network) {
        wsRetryJob?.cancel()
        wsRetryJob =
                viewModelScope.launch {
                    esp32Client.state.collect { state ->
                        if (state == Esp32Client.State.ERROR && _ui.value.isConnected) {
                            Log.w(TAG, "WebSocket en ERROR, reintentando en 5 s")
                            delay(5_000L)
                            if (_ui.value.isConnected) esp32Client.connect(network)
                        }
                    }
                }
    }

    // ── Auto-reconexión ───────────────────────────────────────────────
    // Si hay un SSID guardado y no estamos conectados, intenta reconectar
    // cada 15 s de forma silenciosa (sin mostrar error al usuario).
    private fun startAutoReconnect() {
        val ssid = networkManager.getLastSsid() ?: return
        if (autoReconnectJob?.isActive == true) return
        Log.i(TAG, "startAutoReconnect: iniciando loop para '$ssid'")
        autoReconnectJob =
                viewModelScope.launch {
                    var delaySec = 5L
                    while (true) {
                        delay(delaySec * 1_000L)
                        if (_ui.value.isConnected) break
                        val ws = networkManager.wifiState.value
                        if (ws == NetworkManager.WifiState.IDLE ||
                                        ws == NetworkManager.WifiState.UNAVAILABLE
                        ) {
                            Log.i(
                                    TAG,
                                    "autoReconnect: intentando reconectar a '$ssid' (próximo delay=${delaySec}s)"
                            )
                            _ui.value = _ui.value.copy(status = "Buscando ESP32…", errorMsg = null)
                            networkManager.connectToEsp32(ssid)
                            if (delaySec < 15L) delaySec += 5L
                        }
                    }
                }
    }

    // ── Map loop ──────────────────────────────────────────────────────
    // Envía un frame vectorial (calles + ruta + posición) al ESP32 cada 500 ms (2 Hz).
    // El ESP32 solo renderiza el último frame recibido; más frecuencia = más fluido sin cola.
    private fun startMapLoop() {
        mapJob?.cancel()
        roadsJob?.cancel()
        Log.i(TAG, "startMapLoop: enviando frames vectoriales cada 500 ms → ESP32")
        mapJob =
                viewModelScope.launch {
                    while (true) {
                        if (_ui.value.isConnected) {
                            val loc = lastLocation
                            if (loc != null) {
                                val zoom = _ui.value.zoom.toDouble()

                                // Disparar consulta Overpass en background si es necesario y no hay
                                // una en curso
                                if (vectorFetcher.needsRefresh(loc.latitude, loc.longitude) &&
                                                roadsJob?.isActive != true
                                ) {
                                    roadsJob =
                                            viewModelScope.launch(Dispatchers.IO) {
                                                vectorFetcher.fetchAndCache(
                                                        loc.latitude,
                                                        loc.longitude
                                                )
                                            }
                                }

                                // Toda la geometría en Default para no bloquear el hilo principal
                                val routeGeometry = _ui.value.route?.geometry
                                val json =
                                        withContext(Dispatchers.Default) {
                                            val roads =
                                                    vectorFetcher.getCachedRoads(
                                                            zoom,
                                                            loc.latitude,
                                                            loc.longitude
                                                    )

                                            val routePx =
                                                    routeGeometry
                                                            ?.map { (lat, lon) ->
                                                                VectorRenderer.latLonToPixel(
                                                                        lat,
                                                                        lon,
                                                                        loc.latitude,
                                                                        loc.longitude,
                                                                        zoom
                                                                )
                                                            }
                                                            ?.let {
                                                                VectorRenderer.simplify(it, 1.5)
                                                            }
                                                            ?: emptyList()

                                            val bearing = if (loc.hasBearing()) loc.bearing else -1f
                                            val heading = bearing.toInt()
                                            val cx = VectorRenderer.SCREEN_W / 2
                                            val cy = VectorRenderer.POS_Y

                                            val rotatedRoads =
                                                    if (bearing >= 0f)
                                                            roads.map { seg ->
                                                                VectorRenderer.RoadSegment(
                                                                        VectorRenderer.rotatePoints(
                                                                                seg.pixels,
                                                                                bearing,
                                                                                cx,
                                                                                cy
                                                                        ),
                                                                        seg.width,
                                                                        seg.name
                                                                )
                                                            }
                                                    else roads
                                            val rotatedRoute =
                                                    if (bearing >= 0f)
                                                            VectorRenderer.rotatePoints(
                                                                    routePx,
                                                                    bearing,
                                                                    cx,
                                                                    cy
                                                            )
                                                    else routePx

                                            val labels =
                                                    buildList<VectorRenderer.StreetLabel> {
                                                        val seen = mutableSetOf<String>()
                                                        rotatedRoads
                                                                .filter { it.name.isNotBlank() }
                                                                .sortedByDescending { it.width }
                                                                .forEach { seg ->
                                                                    if (seg.name !in seen &&
                                                                                    size < 20
                                                                    ) {
                                                                        val mid =
                                                                                seg.pixels
                                                                                        .getOrNull(
                                                                                                seg.pixels
                                                                                                        .size /
                                                                                                        2
                                                                                        )
                                                                                        ?: return@forEach
                                                                        if (mid.first in -10..330 &&
                                                                                        mid.second in
                                                                                                -10..490
                                                                        ) {
                                                                            seen.add(seg.name)
                                                                            add(
                                                                                    VectorRenderer
                                                                                            .StreetLabel(
                                                                                                    mid.first,
                                                                                                    mid.second,
                                                                                                    seg.name
                                                                                            )
                                                                            )
                                                                        }
                                                                    }
                                                                }
                                                    }

                                            VectorRenderer.buildFrame(
                                                    rotatedRoads,
                                                    rotatedRoute,
                                                    labels,
                                                    cx,
                                                    cy,
                                                    heading
                                            )
                                        }
                                esp32Client.sendVectorFrame(json)
                                Log.d(TAG, "vec frame: ${json.length} chars")
                            }
                        }
                        delay(500L)
                    }
                }
    }

    // ── Zoom / centrar ────────────────────────────────────────────────
    fun setZoom(zoom: Int) {
        val z = zoom.coerceIn(10, 19)
        _ui.value = _ui.value.copy(zoom = z)
        setZoomCallback?.invoke(z)
    }

    fun centerOnLocation() {
        lastLocation?.let { loc -> setCenterCallback?.invoke(loc.latitude, loc.longitude) }
        enableFollowCallback?.invoke()
    }

    // ── Navegación ────────────────────────────────────────────────────

    /** Paso 1: busca sugerencias en la API y las muestra al usuario. */
    fun searchSuggestions(query: String) {
        if (query.isBlank()) {
            _ui.value = _ui.value.copy(suggestions = emptyList(), isSearchingSuggestions = false)
            return
        }
        suggestionsJob?.cancel()
        _ui.value =
                _ui.value.copy(
                        isSearchingSuggestions = true,
                        suggestions = emptyList(),
                        errorMsg = null
                )
        val origin = lastLocation
        suggestionsJob =
                viewModelScope.launch {
                    val hits = navRouter.suggest(query, origin?.latitude, origin?.longitude)
                    _ui.value = _ui.value.copy(isSearchingSuggestions = false, suggestions = hits)
                }
    }

    /** Navegar a un punto del mapa (tap en el mapa). */
    fun routeToMapPoint(lat: Double, lon: Double) {
        routeToSuggestion(GeocodeSuggestion(label = "Destino en el mapa", lat = lat, lon = lon))
    }

    /** Paso 2: el usuario eligió una sugerencia → calcular ruta. */
    fun routeToSuggestion(suggestion: GeocodeSuggestion) {
        addToRecentSearches(suggestion)
        val origin =
                lastLocation
                        ?: run {
                            _ui.value =
                                    _ui.value.copy(
                                            errorMsg = "Sin ubicación aún",
                                            suggestions = emptyList()
                                    )
                            return
                        }
        _ui.value =
                _ui.value.copy(
                        isSearchingRoute = true,
                        navDestination = suggestion.label,
                        suggestions = emptyList(),
                        errorMsg = null,
                )
        viewModelScope.launch {
            val route =
                    navRouter.route(
                            origin.latitude,
                            origin.longitude,
                            suggestion.lat,
                            suggestion.lon
                    )
            if (route == null) {
                _ui.value =
                        _ui.value.copy(
                                isSearchingRoute = false,
                                errorMsg = "No se pudo calcular la ruta"
                        )
                return@launch
            }
            stepIdx = 0
            _ui.value =
                    _ui.value.copy(isSearchingRoute = false, route = route, currentStepIndex = 0)
            setRouteCallback?.invoke(route.geometry)
            setNavModeCallback?.invoke(true)
            sendCurrentStep()
        }
    }

    fun clearSuggestions() {
        _ui.value = _ui.value.copy(suggestions = emptyList())
    }

    fun clearRoute() {
        arrivedClearJob?.cancel()
        arrivedClearJob = null
        recalcJob?.cancel()
        lastRecalcTimeMs = 0L
        _ui.value =
                _ui.value.copy(
                        route = null,
                        currentStepIndex = 0,
                        navDestination = "",
                        suggestions = emptyList()
                )
        setRouteCallback?.invoke(emptyList())
        setNavModeCallback?.invoke(false)
        esp32Client.sendNavStep("Sin navegación", 0, "0 min")
    }

    private fun advanceNavStep(loc: Location) {
        val route = _ui.value.route ?: return
        if (stepIdx >= route.steps.size) return
        val step = route.steps[stepIdx]
        val dist = FloatArray(1)
        Location.distanceBetween(loc.latitude, loc.longitude, step.lat, step.lon, dist)
        if (dist[0] < 30f) {
            stepIdx++
            _ui.value = _ui.value.copy(currentStepIndex = stepIdx)
            sendCurrentStep()
        }
    }

    /** Distancia mínima en metros desde el punto (lat, lon) a la polilínea de la ruta. */
    private fun distanceToRoute(
            lat: Double,
            lon: Double,
            geometry: List<Pair<Double, Double>>
    ): Float {
        if (geometry.size < 2) return Float.MAX_VALUE
        var minDist = Float.MAX_VALUE
        val dist = FloatArray(1)
        for (i in 0 until geometry.size - 1) {
            val a = geometry[i]
            val b = geometry[i + 1]
            val (closestLat, closestLon) =
                    closestPointOnSegment(lat, lon, a.first, a.second, b.first, b.second)
            Location.distanceBetween(lat, lon, closestLat, closestLon, dist)
            if (dist[0] < minDist) minDist = dist[0]
        }
        return minDist
    }

    /**
     * Proyección del punto (px,py) sobre el segmento (ax,ay)-(bx,by); devuelve el punto más cercano
     * en el segmento.
     */
    private fun closestPointOnSegment(
            px: Double,
            py: Double,
            ax: Double,
            ay: Double,
            bx: Double,
            by: Double
    ): Pair<Double, Double> {
        val dx = bx - ax
        val dy = by - ay
        val lenSq = dx * dx + dy * dy
        if (lenSq <= 0.0) return ax to ay
        var t = ((px - ax) * dx + (py - ay) * dy) / lenSq
        t = t.coerceIn(0.0, 1.0)
        return (ax + t * dx) to (ay + t * dy)
    }

    private fun checkOffRouteAndRecalc(loc: Location) {
        val route = _ui.value.route ?: return
        if (route.geometry.size < 2) return
        if (recalcJob?.isActive == true) return
        val now = System.currentTimeMillis()
        if (lastRecalcTimeMs > 0L && (now - lastRecalcTimeMs) < RECALC_COOLDOWN_MS) return
        val distM = distanceToRoute(loc.latitude, loc.longitude, route.geometry)
        if (distM <= OFF_ROUTE_TOLERANCE_M) return
        Log.i(TAG, "Fuera de ruta (${distM.toInt()} m > $OFF_ROUTE_TOLERANCE_M), recalculando…")
        recalcJob = viewModelScope.launch { recalculateRoute() }
    }

    private suspend fun recalculateRoute() {
        val route = _ui.value.route ?: return
        val dest = route.geometry.lastOrNull() ?: return
        val origin = lastLocation ?: return
        _ui.value = _ui.value.copy(isSearchingRoute = true)
        val newRoute = navRouter.route(origin.latitude, origin.longitude, dest.first, dest.second)
        _ui.value = _ui.value.copy(isSearchingRoute = false)
        if (newRoute == null) {
            Log.w(TAG, "Recálculo de ruta falló")
            return
        }
        lastRecalcTimeMs = System.currentTimeMillis()
        stepIdx = 0
        _ui.value = _ui.value.copy(route = newRoute, currentStepIndex = 0)
        setRouteCallback?.invoke(newRoute.geometry)
        setNavModeCallback?.invoke(true)
        sendCurrentStep()
        Log.i(TAG, "Ruta recalculada (${newRoute.geometry.size} puntos)")
    }

    private fun sendCurrentStep() {
        val route = _ui.value.route ?: return
        if (stepIdx < route.steps.size) {
            val step = route.steps[stepIdx]
            esp32Client.sendNavStep(
                    step.instruction,
                    step.distanceM,
                    formatEtaMinutes(route.etaMin)
            )
        } else {
            esp32Client.sendNavStep("Llegaste al destino", 0, "0 min")
            // Estilo Google Maps: cerrar navegación automáticamente tras unos segundos
            arrivedClearJob?.cancel()
            arrivedClearJob =
                    viewModelScope.launch {
                        delay(6_000L)
                        clearRoute()
                    }
        }
    }

    fun dismissError() {
        _ui.value = _ui.value.copy(errorMsg = null)
    }

    override fun onCleared() {
        super.onCleared()
        mapJob?.cancel()
        roadsJob?.cancel()
        wsRetryJob?.cancel()
        autoReconnectJob?.cancel()
        suggestionsJob?.cancel()
        arrivedClearJob?.cancel()
        recalcJob?.cancel()
        esp32Client.disconnect()
    }
}
