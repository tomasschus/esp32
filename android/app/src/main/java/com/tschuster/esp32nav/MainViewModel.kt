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
        val zoom: Int = 15,
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

    private val _ui = MutableStateFlow(UiState(lastSsid = networkManager.getLastSsid()))
    val ui: StateFlow<UiState> = _ui.asStateFlow()

    private var lastLocation: Location? = null
    private var mapJob: Job? = null
    private var wsRetryJob: Job? = null
    private var stepIdx = 0

    // Callbacks registrados por la UI para controlar el MapView
    private var captureCallback: (() -> ByteArray?)? = null
    private var enableFollowCallback: (() -> Unit)? = null
    private var setCenterCallback: ((Double, Double) -> Unit)? = null
    private var setZoomCallback: ((Int) -> Unit)? = null
    private var initialCenterDone = false

    init {
        observeNetworkManager()
        observeLocation()
        networkManager.requestCellular()
    }

    // ── Callbacks del mapa ────────────────────────────────────────────
    /**
     * Registrado desde la Activity una vez que el MapView está listo.
     * [capture]      → dibuja el MapView y devuelve JPEG bytes (hilo principal).
     * [enableFollow] → reactiva el seguimiento automático de ubicación.
     * [setZoom]      → cambia el zoom del mapa.
     */
    fun registerMapCallbacks(
            capture: () -> ByteArray?,
            enableFollow: () -> Unit,
            setCenter: (Double, Double) -> Unit,
            setZoom: (Int) -> Unit,
    ) {
        captureCallback = capture
        enableFollowCallback = enableFollow
        setCenterCallback = setCenter
        setZoomCallback = setZoom
        // Si ya tenemos ubicación cuando se registran los callbacks (Activity recreada),
        // centrar inmediatamente sin esperar al próximo tick de GPS.
        if (!initialCenterDone) {
            lastLocation?.let { loc ->
                setCenter(loc.latitude, loc.longitude)
                initialCenterDone = true
            }
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
                        _ui.value =
                                _ui.value.copy(
                                        isConnecting = false,
                                        status = "Desconectado",
                                        errorMsg =
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
                    esp32Client.connect(network)
                    startMapLoop()
                    startWsRetryLoop(network)
                }
                .launchIn(viewModelScope)

        // La red celular se usa para geocoding y routing (NavRouter)
        networkManager
                .cellNetwork
                .filterNotNull()
                .onEach { network ->
                    Log.i(TAG, "cellNetwork recibido → NavRouter usará esta red")
                    navRouter.setInternetNetwork(network)
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

    // ── Map loop ──────────────────────────────────────────────────────
    // Captura el MapView (renderizado en pantalla con tiles OSM reales)
    // y envía el JPEG al ESP32 por WebSocket cada 5 segundos.
    private fun startMapLoop() {
        mapJob?.cancel()
        Log.i(TAG, "startMapLoop: capturando mapa OSM cada 5 s → ESP32")
        mapJob =
                viewModelScope.launch {
                    while (true) {
                        if (_ui.value.isConnected) {
                            val tile = captureCallback?.invoke()
                            if (tile != null) {
                                Log.d(TAG, "tile capturado: ${tile.size} bytes → enviando al ESP32")
                                esp32Client.sendMapTile(tile)
                            } else {
                                Log.d(TAG, "tile null: MapView no listo aún")
                            }
                        }
                        delay(5_000L)
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
        // Reactiva el seguimiento automático (OSMDroid centra en la última ubicación conocida)
        enableFollowCallback?.invoke()
        // Captura inmediata tras que el mapa se re-centra
        viewModelScope.launch {
            delay(600L)
            val tile = captureCallback?.invoke() ?: return@launch
            Log.d(TAG, "centerOnLocation: tile ${tile.size} bytes → ESP32")
            esp32Client.sendMapTile(tile)
        }
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
            sendCurrentStep()
        }
    }

    fun clearRoute() {
        _ui.value = _ui.value.copy(route = null, currentStepIndex = 0, navDestination = "")
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
        wsRetryJob?.cancel()
        esp32Client.disconnect()
    }
}
