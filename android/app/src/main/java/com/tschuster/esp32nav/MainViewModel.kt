package com.tschuster.esp32nav

import android.app.Application
import android.location.Location
import android.util.Log
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.tschuster.esp32nav.location.LocationTracker
import com.tschuster.esp32nav.network.Esp32Client
import com.tschuster.esp32nav.network.NavRoute
import com.tschuster.esp32nav.network.NavRouter
import com.tschuster.esp32nav.network.NetworkManager
import com.tschuster.esp32nav.network.VectorFetcher
import com.tschuster.esp32nav.util.VectorRenderer
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

private const val TAG = "ESP32Nav/VM"

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
        val errorMsg: String? = null,
)

class MainViewModel(app: Application) : AndroidViewModel(app) {

    private val networkManager = (app as ESP32NavApp).networkManager
    private val locationTracker = LocationTracker(app)
    private val navRouter = NavRouter()
    private val esp32Client = Esp32Client()
    private val vectorFetcher = VectorFetcher()

    private val _ui = MutableStateFlow(UiState(lastSsid = networkManager.getLastSsid()))
    val ui: StateFlow<UiState> = _ui.asStateFlow()

    private var lastLocation: Location? = null
    private var mapJob: Job? = null
    private var roadsJob: Job? = null
    private var wsRetryJob: Job? = null
    private var autoReconnectJob: Job? = null
    private var stepIdx = 0

    // Callbacks registrados por la UI para controlar el MapView (Android)
    private var enableFollowCallback: (() -> Unit)? = null
    private var setCenterCallback: ((Double, Double) -> Unit)? = null
    private var setZoomCallback: ((Int) -> Unit)? = null
    private var setRouteCallback: ((List<Pair<Double, Double>>) -> Unit)? = null
    private var initialCenterDone = false

    init {
        observeNetworkManager()
        observeLocation()
        networkManager.requestCellular()
        startAutoReconnect()
    }

    // ── Callbacks del mapa ────────────────────────────────────────────
    /**
     * Registrado desde la Activity una vez que el MapView está listo.
     * [enableFollow] → reactiva el seguimiento automático de ubicación.
     * [setCenter]    → centra el mapa en una coordenada.
     * [setZoom]      → cambia el zoom del mapa.
     * [setRoute]     → dibuja la ruta como polilínea en el mapa Android.
     */
    fun registerMapCallbacks(
            enableFollow: () -> Unit,
            setCenter: (Double, Double) -> Unit,
            setZoom: (Int) -> Unit,
            setRoute: (List<Pair<Double, Double>>) -> Unit,
    ) {
        enableFollowCallback = enableFollow
        setCenterCallback = setCenter
        setZoomCallback = setZoom
        setRouteCallback = setRoute
        // Si ya tenemos ubicación cuando se registran los callbacks (Activity recreada),
        // centrar inmediatamente sin esperar al próximo tick de GPS.
        if (!initialCenterDone) {
            lastLocation?.let { loc ->
                setCenter(loc.latitude, loc.longitude)
                initialCenterDone = true
            }
        }
        // Restaurar ruta si ya hay una activa
        _ui.value.route?.geometry?.let { setRoute(it) }
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
                                        status = if (autoReconnecting) "Buscando ESP32…" else "Desconectado",
                                        errorMsg = if (autoReconnecting) null
                                        else "No se pudo conectar. ¿Aprobaste el diálogo del sistema? Verificá que el ESP32 esté encendido.",
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
                        startAutoReconnect()
                    }
                }
                .launchIn(viewModelScope)
    }

    // ── Location ──────────────────────────────────────────────────────
    private fun observeLocation() {
        locationTracker
                .locationFlow()
                .onEach { loc ->
                    lastLocation = loc
                    _ui.value = _ui.value.copy(location = loc)
                    esp32Client.sendGps(loc.latitude, loc.longitude)
                    advanceNavStep(loc)
                    // Primera ubicación: centrar el mapa explícitamente una sola vez.
                    // Las siguientes actualizaciones las maneja enableFollowLocation().
                    if (!initialCenterDone && setCenterCallback != null) {
                        setCenterCallback?.invoke(loc.latitude, loc.longitude)
                        initialCenterDone = true
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
                                        ws == NetworkManager.WifiState.UNAVAILABLE) {
                            Log.i(TAG, "autoReconnect: intentando reconectar a '$ssid' (próximo delay=${delaySec}s)")
                            _ui.value = _ui.value.copy(status = "Buscando ESP32…", errorMsg = null)
                            networkManager.connectToEsp32(ssid)
                            if (delaySec < 15L) delaySec += 5L
                        }
                    }
                }
    }

    // ── Map loop ──────────────────────────────────────────────────────
    // Envía un frame vectorial (calles + ruta + posición) al ESP32 cada 1 segundo.
    // Las calles se obtienen de Overpass API con caché; la ruta viene de OSRM.
    private fun startMapLoop() {
        mapJob?.cancel()
        roadsJob?.cancel()
        Log.i(TAG, "startMapLoop: enviando frames vectoriales cada 1 s → ESP32")
        mapJob = viewModelScope.launch {
            while (true) {
                if (_ui.value.isConnected) {
                    val loc = lastLocation
                    if (loc != null) {
                        val zoom = _ui.value.zoom.toDouble()

                        // Disparar consulta Overpass en background si es necesario y no hay una en curso
                        if (vectorFetcher.needsRefresh(loc.latitude, loc.longitude) &&
                                roadsJob?.isActive != true) {
                            roadsJob = viewModelScope.launch(Dispatchers.IO) {
                                vectorFetcher.fetchAndCache(loc.latitude, loc.longitude)
                            }
                        }

                        // Calles del caché (ya reproyectadas a píxeles)
                        val roads = vectorFetcher.getCachedRoads(zoom, loc.latitude, loc.longitude)

                        // Ruta proyectada a píxeles y simplificada
                        val routePx = _ui.value.route?.geometry?.map { (lat, lon) ->
                            VectorRenderer.latLonToPixel(lat, lon, loc.latitude, loc.longitude, zoom)
                        }?.let { VectorRenderer.simplify(it, 1.5) } ?: emptyList()

                        // Posición = centrada horizontalmente, 3/4 hacia abajo
                        val bearing = if (loc.hasBearing()) loc.bearing else -1f
                        val heading = bearing.toInt()

                        // Heading-up: rotar calles y ruta alrededor del marcador
                        val cx = VectorRenderer.SCREEN_W / 2
                        val cy = VectorRenderer.POS_Y
                        val rotatedRoads = if (bearing >= 0f) roads.map { seg ->
                            VectorRenderer.RoadSegment(
                                VectorRenderer.rotatePoints(seg.pixels, bearing, cx, cy),
                                seg.width,
                                seg.name
                            )
                        } else roads
                        val rotatedRoute = if (bearing >= 0f)
                            VectorRenderer.rotatePoints(routePx, bearing, cx, cy)
                        else routePx

                        // Nombres de calles: uno por nombre único, prioridad a calles mayores
                        val labels = buildList<VectorRenderer.StreetLabel> {
                            val seen = mutableSetOf<String>()
                            rotatedRoads
                                .filter { it.name.isNotBlank() }
                                .sortedByDescending { it.width }
                                .forEach { seg ->
                                    if (seg.name !in seen && size < 20) {
                                        val mid = seg.pixels.getOrNull(seg.pixels.size / 2) ?: return@forEach
                                        if (mid.first in -10..330 && mid.second in -10..490) {
                                            seen.add(seg.name)
                                            add(VectorRenderer.StreetLabel(mid.first, mid.second, seg.name))
                                        }
                                    }
                                }
                        }

                        val json = VectorRenderer.buildFrame(
                            rotatedRoads, rotatedRoute, labels,
                            cx, cy,
                            heading
                        )
                        esp32Client.sendVectorFrame(json)
                        Log.d(TAG, "vec frame: ${json.length} chars, roads=${rotatedRoads.size}, route=${rotatedRoute.size} pts")
                    }
                }
                delay(1_000L)
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
        enableFollowCallback?.invoke()
    }

    // ── Navegación ────────────────────────────────────────────────────
    fun searchRoute(destination: String) {
        if (destination.isBlank()) return
        val origin =
                lastLocation
                        ?: run {
                            _ui.value = _ui.value.copy(errorMsg = "Sin ubicación aún")
                            return
                        }
        _ui.value =
                _ui.value.copy(
                        isSearchingRoute = true,
                        navDestination = destination,
                        errorMsg = null,
                )
        viewModelScope.launch {
            val destCoord = navRouter.geocode(destination)
            if (destCoord == null) {
                _ui.value =
                        _ui.value.copy(
                                isSearchingRoute = false,
                                errorMsg = "No se encontró \"$destination\"",
                        )
                return@launch
            }
            val route =
                    navRouter.route(
                            origin.latitude,
                            origin.longitude,
                            destCoord.first,
                            destCoord.second
                    )
            if (route == null) {
                _ui.value =
                        _ui.value.copy(
                                isSearchingRoute = false,
                                errorMsg = "No se pudo calcular la ruta",
                        )
                return@launch
            }
            stepIdx = 0
            _ui.value =
                    _ui.value.copy(isSearchingRoute = false, route = route, currentStepIndex = 0)
            setRouteCallback?.invoke(route.geometry)
            sendCurrentStep()
        }
    }

    fun clearRoute() {
        _ui.value = _ui.value.copy(route = null, currentStepIndex = 0, navDestination = "")
        setRouteCallback?.invoke(emptyList())
        esp32Client.sendNavStep("Sin navegación", 0, 0)
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

    private fun sendCurrentStep() {
        val route = _ui.value.route ?: return
        if (stepIdx < route.steps.size) {
            val step = route.steps[stepIdx]
            esp32Client.sendNavStep(step.instruction, step.distanceM, route.etaMin)
        } else {
            esp32Client.sendNavStep("Llegaste al destino", 0, 0)
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
        esp32Client.disconnect()
    }
}
