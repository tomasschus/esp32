package com.tschuster.esp32nav

import android.app.Application
import android.location.Location
import android.util.Log
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.tschuster.esp32nav.location.LocationTracker
import com.tschuster.esp32nav.network.Esp32Client
import com.tschuster.esp32nav.network.MapFetcher
import com.tschuster.esp32nav.network.NavRoute
import com.tschuster.esp32nav.network.NavRouter
import com.tschuster.esp32nav.network.NetworkManager
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.collect
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
    private val mapFetcher = MapFetcher()
    private val navRouter = NavRouter()
    private val esp32Client = Esp32Client()

    private val _ui = MutableStateFlow(UiState(lastSsid = networkManager.getLastSsid()))
    val ui: StateFlow<UiState> = _ui.asStateFlow()

    private val _mapTile = MutableStateFlow<ByteArray?>(null)
    val mapTile: StateFlow<ByteArray?> = _mapTile.asStateFlow()

    private var lastLocation: Location? = null
    private var mapJob: Job? = null
    private var wsRetryJob: Job? = null
    private var stepIdx = 0

    init {
        observeNetworkManager()
        observeLocation()
        networkManager.requestCellular() // datos móviles desde el inicio (sin esperar WiFi)
    }

    // ── Scan ─────────────────────────────────────────────────────────
    fun startScan() {
        networkManager.startScan()
    }

    // ── Connect ───────────────────────────────────────────────────────
    fun connectToDevice(ssid: String, password: String = "") {
        _ui.value =
                _ui.value.copy(
                        isConnecting = true,
                        status = "Aprobá el diálogo WiFi del sistema…",
                )
        networkManager.connectToEsp32(ssid, password)
    }

    // ── Observe NetworkManager flows ──────────────────────────────────
    private fun observeNetworkManager() {
        // scan/wifi state
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
                        Log.e(TAG, "UNAVAILABLE: conexión fallida o timeout")
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

        // scan results
        networkManager
                .scanResults
                .onEach { results -> _ui.value = _ui.value.copy(scanResults = results) }
                .launchIn(viewModelScope)

        // connected to ESP32 network
        networkManager
                .esp32Network
                .filterNotNull()
                .onEach { network ->
                    Log.i(
                            TAG,
                            "VM: esp32Network recibido → network=$network llamando esp32Client.connect(network) + startMapLoop + wsRetry"
                    )
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

        // red con internet (datos móviles preferred) — para tiles, geocoding y routing
        networkManager
                .cellNetwork
                .filterNotNull()
                .onEach { network ->
                    Log.i(
                            TAG,
                            "VM: cellNetwork recibido → network=$network → setCellularNetwork + setInternetNetwork (tiles y rutas usarán esta red)"
                    )
                    mapFetcher.setCellularNetwork(network, getApplication())
                    navRouter.setInternetNetwork(network)
                }
                .launchIn(viewModelScope)

        // disconnected
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

    // ── Location ─────────────────────────────────────────────────────
    private fun observeLocation() {
        locationTracker
                .locationFlow()
                .onEach { loc ->
                    lastLocation = loc
                    _ui.value = _ui.value.copy(location = loc)
                    esp32Client.sendGps(loc.latitude, loc.longitude)
                    advanceNavStep(loc)
                }
                .launchIn(viewModelScope)
    }

    // ── WebSocket retry loop ──────────────────────────────────────────
    // El servidor WebSocket del ESP32 solo arranca cuando el usuario navega a la
    // pantalla de Mapas. Reintentamos cada 5 s hasta que conecte.
    private fun startWsRetryLoop(network: android.net.Network) {
        wsRetryJob?.cancel()
        Log.d(TAG, "startWsRetryLoop: network=$network (misma red que esp32Network)")
        wsRetryJob =
                viewModelScope.launch {
                    esp32Client.state.collect { state ->
                        if (state == Esp32Client.State.ERROR && _ui.value.isConnected) {
                            Log.w(
                                    TAG,
                                    "VM: WebSocket en ERROR, reintentando en 5 s con network=$network"
                            )
                            delay(5_000L)
                            if (_ui.value.isConnected) {
                                Log.d(TAG, "VM: wsRetry llamando esp32Client.connect(network)")
                                esp32Client.connect(network)
                            }
                        }
                    }
                }
    }

    // ── Map loop ─────────────────────────────────────────────────────
    private fun startMapLoop() {
        mapJob?.cancel()
        Log.i(
                TAG,
                "VM: startMapLoop iniciado (cada 5s mapFetcher.fetch → si cellClient set, usa red celular)"
        )
        mapJob =
                viewModelScope.launch {
                    while (true) {
                        val loc = lastLocation
                        if (loc != null) {
                            Log.d(
                                    TAG,
                                    "VM: mapLoop iteración loc=${loc.latitude},${loc.longitude} zoom=${_ui.value.zoom} → mapFetcher.fetch()"
                            )
                            val tile = mapFetcher.fetch(loc.latitude, loc.longitude, _ui.value.zoom)
                            if (tile != null) {
                                Log.i(
                                        TAG,
                                        "VM: tile descargado (${tile.size} bytes) → enviando al ESP32"
                                )
                                _mapTile.value = tile
                                esp32Client.sendMapTile(tile)
                            } else {
                                Log.w(
                                        TAG,
                                        "VM: tile==null (cellClient null o fetch falló; ver logs ESP32Nav/Map)"
                                )
                            }
                        } else {
                            Log.d(TAG, "VM: mapLoop sin ubicación aún, esperando 5s")
                        }
                        delay(5_000L)
                    }
                }
    }

    fun setZoom(zoom: Int) {
        _ui.value = _ui.value.copy(zoom = zoom.coerceIn(10, 19))
    }

    fun centerOnLocation() {
        val loc = lastLocation ?: return
        viewModelScope.launch {
            val tile = mapFetcher.fetch(loc.latitude, loc.longitude, _ui.value.zoom)
            if (tile != null) {
                _mapTile.value = tile
                esp32Client.sendMapTile(tile)
            }
        }
    }

    // ── Navigation ───────────────────────────────────────────────────
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
                        errorMsg = null
                )
        viewModelScope.launch {
            val destCoord = navRouter.geocode(destination)
            if (destCoord == null) {
                _ui.value =
                        _ui.value.copy(
                                isSearchingRoute = false,
                                errorMsg = "No se encontró \"$destination\""
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
                                errorMsg = "No se pudo calcular la ruta"
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
