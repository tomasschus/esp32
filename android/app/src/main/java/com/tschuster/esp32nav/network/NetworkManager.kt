package com.tschuster.esp32nav.network

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.net.*
import android.net.wifi.WifiManager
import android.net.wifi.WifiNetworkSpecifier
import android.os.Handler
import android.os.Looper
import android.util.Log
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow

/** Contraseña fija para todos los ESP32. Debe coincidir con MAPS_AP_PASS en el firmware. */
const val ESP32_PASSWORD = "esp32nav12"
private const val TAG = "ESP32Nav/NetMgr"
private const val SSID_PREFIX = "ESP32-NAV"
private const val PREF_FILE = "esp32nav_prefs"
private const val PREF_LAST_SSID = "last_ssid"

class NetworkManager(private val context: Context) {

    enum class WifiState {
        IDLE,
        SCANNING,
        CONNECTING,
        CONNECTED,
        UNAVAILABLE
    }

    private val cm = context.getSystemService(ConnectivityManager::class.java)
    private val wm = context.getSystemService(WifiManager::class.java)
    private val prefs = context.getSharedPreferences(PREF_FILE, Context.MODE_PRIVATE)

    private val _wifiState = MutableStateFlow(WifiState.IDLE)
    private val _esp32Network = MutableStateFlow<Network?>(null)
    private val _cellNetwork = MutableStateFlow<Network?>(null)
    private val _scanResults = MutableStateFlow<List<String>>(emptyList())

    val wifiState = _wifiState.asStateFlow()
    val esp32Network = _esp32Network.asStateFlow()
    val cellNetwork = _cellNetwork.asStateFlow()
    val scanResults = _scanResults.asStateFlow()

    private var esp32Callback: ConnectivityManager.NetworkCallback? = null
    private var cellularCallback: ConnectivityManager.NetworkCallback? = null
    private var internetCallback: ConnectivityManager.NetworkCallback? = null
    private var scanReceiver: BroadcastReceiver? = null

    // Celular tiene prioridad; internet (fallback) se usa solo si celular no está disponible.
    private var activeCellularNet: Network? = null
    private var activeInternetNet: Network? = null

    private fun updateCellNetwork() {
        val net = activeCellularNet ?: activeInternetNet
        val prev = _cellNetwork.value
        Log.d(
                TAG,
                "updateCellNetwork → celular=$activeCellularNet fallback=$activeInternetNet → usando=$net (prev=$prev)"
        )
        _cellNetwork.value = net
        if (net != null) {
            Log.i(
                    TAG,
                    "cellNetwork emitido → MapFetcher/NavRouter usarán esta red para tiles e internet (net=$net)"
            )
        } else {
            Log.w(
                    TAG,
                    "cellNetwork=null → MapFetcher.fetch() devolverá null hasta que haya celular/fallback"
            )
        }
    }

    fun getLastSsid(): String? = prefs.getString(PREF_LAST_SSID, null)

    // ── Escanear redes WiFi con prefijo ESP32-NAV ─────────────────
    fun startScan() {
        if (_wifiState.value == WifiState.SCANNING) {
            Log.d(TAG, "startScan: ya escaneando, ignorado")
            return
        }
        Log.i(TAG, "startScan: iniciando escaneo WiFi")
        _wifiState.value = WifiState.SCANNING

        scanReceiver?.let { runCatching { context.unregisterReceiver(it) } }

        val receiver =
                object : BroadcastReceiver() {
                    @Suppress("DEPRECATION")
                    override fun onReceive(ctx: Context, intent: Intent) {
                        val success =
                                intent.getBooleanExtra(WifiManager.EXTRA_RESULTS_UPDATED, false)
                        Log.d(
                                TAG,
                                "onReceive SCAN_RESULTS: success=$success, total=${wm.scanResults.size} redes"
                        )
                        val ssids =
                                wm.scanResults
                                        .mapNotNull { r ->
                                            r.SSID.takeIf {
                                                it.startsWith(SSID_PREFIX, ignoreCase = true)
                                            }
                                        }
                                        .distinct()
                                        .sorted()
                        Log.i(
                                TAG,
                                "startScan resultado: encontradas ${ssids.size} redes ESP32 → $ssids"
                        )
                        _scanResults.value = ssids
                        _wifiState.value = WifiState.IDLE
                        runCatching { context.unregisterReceiver(this) }
                    }
                }
        scanReceiver = receiver

        if (android.os.Build.VERSION.SDK_INT >= 33) {
            context.registerReceiver(
                    receiver,
                    IntentFilter(WifiManager.SCAN_RESULTS_AVAILABLE_ACTION),
                    Context.RECEIVER_NOT_EXPORTED
            )
        } else {
            context.registerReceiver(
                    receiver,
                    IntentFilter(WifiManager.SCAN_RESULTS_AVAILABLE_ACTION)
            )
        }

        @Suppress("DEPRECATION") val started = wm.startScan()
        Log.d(TAG, "wm.startScan() retornó: $started")
    }

    // ── Conectar a un ESP32 por SSID ─────────────────────────────
    fun connectToEsp32(ssid: String, password: String = ESP32_PASSWORD) {
        Log.i(
                TAG,
                "connectToEsp32: ssid='$ssid' password=${if (password.isEmpty()) "(vacía/abierta)" else "***"}"
        )
        _wifiState.value = WifiState.CONNECTING

        val specBuilder = WifiNetworkSpecifier.Builder().setSsid(ssid)
        if (password.isNotBlank()) specBuilder.setWpa2Passphrase(password)

        val request =
                NetworkRequest.Builder()
                        .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
                        .removeCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
                        .setNetworkSpecifier(specBuilder.build())
                        .build()

        Log.d(TAG, "requestNetwork: TRANSPORT_WIFI, sin INTERNET, specifier SSID='$ssid'")

        val cb =
                object : ConnectivityManager.NetworkCallback() {
                    override fun onAvailable(network: Network) {
                        Log.i(
                                TAG,
                                "onAvailable(ESP32): network=$network networkHandle=${network.getNetworkHandle()}"
                        )
                        _esp32Network.value = network
                        _wifiState.value = WifiState.CONNECTED
                        prefs.edit().putString(PREF_LAST_SSID, ssid).apply()
                        Log.i(
                                TAG,
                                "esp32Network emitido → VM llamará esp32Client.connect(this) con esta red"
                        )
                    }
                    override fun onCapabilitiesChanged(
                            network: Network,
                            caps: NetworkCapabilities
                    ) {
                        Log.d(TAG, "onCapabilitiesChanged: $caps")
                    }
                    override fun onLost(network: Network) {
                        Log.w(TAG, "onLost: se perdió la red $network")
                        _esp32Network.value = null
                        _wifiState.value = WifiState.IDLE
                    }
                    override fun onUnavailable() {
                        Log.e(
                                TAG,
                                "onUnavailable: no se pudo conectar a '$ssid' (timeout o red no encontrada)"
                        )
                        _wifiState.value = WifiState.UNAVAILABLE
                    }
                }

        esp32Callback?.let {
            Log.d(TAG, "connectToEsp32: cancelando callback anterior")
            runCatching { cm.unregisterNetworkCallback(it) }
        }
        esp32Callback = cb

        val mainHandler = Handler(Looper.getMainLooper())
        try {
            // timeout = 30 s; handler = main thread para callbacks seguros
            cm.requestNetwork(request, cb, mainHandler, 30_000)
            Log.d(TAG, "requestNetwork enviado con timeout=30s")
        } catch (e: SecurityException) {
            Log.e(TAG, "requestNetwork SecurityException: ${e.message}", e)
            _wifiState.value = WifiState.UNAVAILABLE
        } catch (e: Exception) {
            Log.e(TAG, "requestNetwork excepción: ${e::class.simpleName}: ${e.message}", e)
            _wifiState.value = WifiState.UNAVAILABLE
        }
    }

    // ── Red con internet: datos móviles preferido, cualquier internet como fallback ──
    fun requestCellular() {
        cellularCallback?.let { runCatching { cm.unregisterNetworkCallback(it) } }
        internetCallback?.let { runCatching { cm.unregisterNetworkCallback(it) } }
        activeCellularNet = null
        activeInternetNet = null

        // 1. Preferir datos móviles explícitamente
        val cb1 =
                object : ConnectivityManager.NetworkCallback() {
                    override fun onAvailable(network: Network) {
                        Log.i(
                                TAG,
                                "Datos móviles disponibles: network=$network handle=${network.getNetworkHandle()}"
                        )
                        activeCellularNet = network
                        updateCellNetwork()
                    }
                    override fun onLost(network: Network) {
                        Log.w(TAG, "Datos móviles perdidos: $network")
                        if (activeCellularNet == network) {
                            activeCellularNet = null
                            updateCellNetwork()
                        }
                    }
                }
        cellularCallback = cb1
        runCatching {
            cm.requestNetwork(
                    NetworkRequest.Builder()
                            .addTransportType(NetworkCapabilities.TRANSPORT_CELLULAR)
                            .addCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
                            .build(),
                    cb1
            )
        }
                .onFailure { Log.w(TAG, "No se pudo solicitar red celular: ${it.message}") }

        // 2. Fallback: cualquier red con acceso a internet, excepto la del ESP32 (sin DNS real)
        val cb2 =
                object : ConnectivityManager.NetworkCallback() {
                    override fun onAvailable(network: Network) {
                        if (network == _esp32Network.value) {
                            Log.d(TAG, "Internet fallback ignorada: es la red del ESP32 ($network)")
                            return
                        }
                        Log.i(
                                TAG,
                                "Internet (fallback) disponible: network=$network handle=${network.getNetworkHandle()}"
                        )
                        activeInternetNet = network
                        updateCellNetwork()
                    }
                    override fun onLost(network: Network) {
                        Log.w(TAG, "Internet (fallback) perdida: $network")
                        if (activeInternetNet == network) {
                            activeInternetNet = null
                            updateCellNetwork()
                        }
                    }
                }
        internetCallback = cb2
        cm.requestNetwork(
                NetworkRequest.Builder()
                        .addCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
                        .build(),
                cb2
        )
        Log.i(TAG, "Solicitando datos móviles (primario) + cualquier internet (fallback)")
    }

    fun release() {
        Log.i(TAG, "release: liberando callbacks")
        esp32Callback?.let { runCatching { cm.unregisterNetworkCallback(it) } }
        cellularCallback?.let { runCatching { cm.unregisterNetworkCallback(it) } }
        internetCallback?.let { runCatching { cm.unregisterNetworkCallback(it) } }
        scanReceiver?.let { runCatching { context.unregisterReceiver(it) } }
    }
}
