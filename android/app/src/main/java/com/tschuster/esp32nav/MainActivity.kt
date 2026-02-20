package com.tschuster.esp32nav

import android.Manifest
import android.os.Bundle
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Clear
import androidx.compose.material.icons.filled.LocationOn
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Search
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Snackbar
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.view.WindowCompat
import com.tschuster.esp32nav.map.MapController
import com.tschuster.esp32nav.network.ESP32_PASSWORD
import com.tschuster.esp32nav.ui.theme.ESP32NavTheme

private const val TAG = "ESP32Nav/Main"

class MainActivity : ComponentActivity() {

    private val vm: MainViewModel by viewModels()
    private lateinit var mapController: MapController

    private var pendingAction = Action.SCAN
    private var pendingSsid = ""
    private var pendingPassword = ""
    private enum class Action { SCAN, CONNECT }

    private val permLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { grants ->
        val denied = grants.filter { !it.value }.keys
        if (denied.isEmpty()) {
            when (pendingAction) {
                Action.SCAN    -> vm.startScan()
                Action.CONNECT -> vm.connectToDevice(pendingSsid, pendingPassword)
            }
        } else {
            Log.e(TAG, "permisos DENEGADOS: $denied")
        }
    }

    private fun requestPerms(action: Action, ssid: String = "", password: String = "") {
        pendingAction   = action
        pendingSsid     = ssid
        pendingPassword = password
        permLauncher.launch(arrayOf(
            Manifest.permission.ACCESS_FINE_LOCATION,
            Manifest.permission.ACCESS_COARSE_LOCATION,
            Manifest.permission.NEARBY_WIFI_DEVICES,
            Manifest.permission.CHANGE_NETWORK_STATE,
        ))
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        WindowCompat.setDecorFitsSystemWindows(window, false)

        mapController = MapController(this)

        setContent {
            LaunchedEffect(Unit) {
                vm.registerMapCallbacks(
                        enableFollow = { mapController.enableFollow() },
                        setCenter    = { lat, lon -> mapController.animateTo(lat, lon) },
                        setZoom      = { z -> mapController.setZoom(z) },
                        setRoute     = { pts -> mapController.setRoute(pts) },
                )
            }

            ESP32NavTheme {
                val state by vm.ui.collectAsState()
                MainScreen(
                    state            = state,
                    mapController    = mapController,
                    onScan           = { requestPerms(Action.SCAN) },
                    onConnect        = { ssid, pw -> requestPerms(Action.CONNECT, ssid, pw) },
                    onSearch         = vm::searchRoute,
                    onClear          = vm::clearRoute,
                    onZoomIn         = { vm.setZoom(state.zoom + 1) },
                    onZoomOut        = { vm.setZoom(state.zoom - 1) },
                    onCenterLocation = vm::centerOnLocation,
                    onDismissError   = vm::dismissError,
                )
            }
        }
    }

    override fun onResume() { super.onResume(); mapController.onResume() }
    override fun onPause()  { super.onPause();  mapController.onPause()  }
}

// ─────────────────────────────────────────────────────────────────────────────
// MainScreen: mapa fullscreen atrás, TODO el resto flota encima.
// ─────────────────────────────────────────────────────────────────────────────

@Composable
fun MainScreen(
    state: UiState,
    mapController: MapController,
    onScan: () -> Unit,
    onConnect: (String, String) -> Unit,
    onSearch: (String) -> Unit,
    onClear: () -> Unit,
    onZoomIn: () -> Unit,
    onZoomOut: () -> Unit,
    onCenterLocation: () -> Unit,
    onDismissError: () -> Unit,
) {
    Box(modifier = Modifier.fillMaxSize()) {

        // ── CAPA 0: Mapa ocupa toda la pantalla ──────────────────────────
        AndroidView(
            factory  = { mapController.mapView },
            modifier = Modifier.fillMaxSize(),
        )

        // A partir de aquí todo flota con safeDrawingPadding
        Box(
            modifier = Modifier
                .fillMaxSize()
                .safeDrawingPadding(),
        ) {

            // ── CAPA 1: Controles superiores (header + buscador) ─────────
            Column(
                modifier = Modifier
                    .align(Alignment.TopStart)
                    .fillMaxWidth()
                    .padding(horizontal = 12.dp, vertical = 8.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                // Header
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .background(Color(0xEE1A1A2E), RoundedCornerShape(12.dp))
                        .padding(horizontal = 16.dp, vertical = 10.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text(
                        text       = "ESP32 NAV",
                        fontSize   = 16.sp,
                        fontWeight = FontWeight.Bold,
                        color      = MaterialTheme.colorScheme.primary,
                    )
                    Spacer(Modifier.width(10.dp))
                    Text(
                        text     = state.status,
                        fontSize = 12.sp,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                        color    = Color(0xCCEEEEEE),
                        modifier = Modifier.weight(1f),
                    )
                    if (state.isConnected) {
                        Text(
                            text  = "Z${state.zoom}",
                            fontSize = 12.sp,
                            color = MaterialTheme.colorScheme.secondary,
                        )
                    }
                }

                // Panel de conexión (solo cuando no conectado)
                if (!state.isConnected) {
                    ConnectCard(
                        isScanning   = state.isScanning,
                        isConnecting = state.isConnecting,
                        scanResults  = state.scanResults,
                        lastSsid     = state.lastSsid,
                        onScan       = onScan,
                        onConnect    = onConnect,
                    )
                }

                // Buscador de destino (solo cuando conectado)
                if (state.isConnected) {
                    NavSearchBar(
                        route       = state.route,
                        destination = state.navDestination,
                        isSearching = state.isSearchingRoute,
                        onSearch    = onSearch,
                        onClear     = onClear,
                    )
                }

                // Paso de navegación activo
                state.route?.let { route ->
                    if (state.currentStepIndex < route.steps.size) {
                        val step = route.steps[state.currentStepIndex]
                        NavStepCard(
                            instruction = step.instruction,
                            distanceM   = step.distanceM,
                            etaMin      = route.etaMin,
                            stepNum     = state.currentStepIndex + 1,
                            totalSteps  = route.steps.size,
                        )
                    }
                }
            }

            // ── CAPA 2: Badge de coordenadas GPS (inferior izquierdo) ────
            state.location?.let { loc ->
                LocationBadge(
                    lat      = loc.latitude,
                    lon      = loc.longitude,
                    acc      = loc.accuracy,
                    modifier = Modifier
                        .align(Alignment.BottomStart)
                        .padding(start = 12.dp, bottom = 12.dp),
                )
            }

            // ── CAPA 3: Botones de mapa (inferior derecho) ───────────────
            if (state.isConnected) {
                Column(
                    modifier = Modifier
                        .align(Alignment.BottomEnd)
                        .padding(end = 12.dp, bottom = 12.dp),
                    verticalArrangement   = Arrangement.spacedBy(6.dp),
                    horizontalAlignment   = Alignment.CenterHorizontally,
                ) {
                    if (state.location != null) {
                        MapFab(onClick = onCenterLocation) {
                            Icon(
                                imageVector        = Icons.Default.LocationOn,
                                contentDescription = "Mi ubicación",
                                tint               = Color.White,
                                modifier           = Modifier.size(20.dp),
                            )
                        }
                    }
                    MapFab(onClick = onZoomIn)  { Text("+", fontSize = 20.sp, fontWeight = FontWeight.Bold, color = Color.White) }
                    ZoomBadge(zoom = state.zoom)
                    MapFab(onClick = onZoomOut) { Text("−", fontSize = 20.sp, fontWeight = FontWeight.Bold, color = Color.White) }
                }
            }

            // ── CAPA 4: Snackbar de error (inferior centrado) ────────────
            state.errorMsg?.let { msg ->
                Snackbar(
                    action = {
                        TextButton(onClick = onDismissError) {
                            Text("OK", color = MaterialTheme.colorScheme.primary)
                        }
                    },
                    modifier       = Modifier.align(Alignment.BottomCenter).padding(16.dp),
                    containerColor = Color(0xFF2A1A2E),
                ) { Text(msg, color = Color(0xFFEEEEEE)) }
            }
        }
    }
}

// ── ConnectCard ───────────────────────────────────────────────────────────────

@Composable
fun ConnectCard(
    isScanning: Boolean,
    isConnecting: Boolean,
    scanResults: List<String>,
    lastSsid: String?,
    onScan: () -> Unit,
    onConnect: (String, String) -> Unit,
    modifier: Modifier = Modifier,
) {
    Card(
        modifier = modifier.fillMaxWidth(),
        colors   = CardDefaults.cardColors(containerColor = Color(0xEE1A1A2E)),
        shape    = RoundedCornerShape(12.dp),
    ) {
        Column(modifier = Modifier.padding(20.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(
                    text       = "Dispositivos ESP32",
                    fontSize   = 15.sp,
                    fontWeight = FontWeight.SemiBold,
                    color      = Color(0xFFEEEEEE),
                    modifier   = Modifier.weight(1f),
                )
                IconButton(onClick = onScan, enabled = !isScanning && !isConnecting) {
                    if (isScanning) {
                        CircularProgressIndicator(modifier = Modifier.size(20.dp),
                            color = MaterialTheme.colorScheme.secondary, strokeWidth = 2.dp)
                    } else {
                        Icon(Icons.Default.Refresh, contentDescription = "Buscar",
                            tint = MaterialTheme.colorScheme.secondary)
                    }
                }
            }

            if (isConnecting) {
                Spacer(Modifier.height(8.dp))
                Text(
                    text     = "Buscá el diálogo del sistema y tocá \"Conectar\"",
                    fontSize = 13.sp,
                    color    = MaterialTheme.colorScheme.primary,
                )
            }

            if (lastSsid != null) {
                Spacer(Modifier.height(8.dp))
                DeviceRow(ssid = lastSsid, label = "Último usado",
                    isConnecting = isConnecting, onConnect = onConnect)
            }

            val fresh = scanResults.filter { it != lastSsid }
            if (fresh.isNotEmpty()) {
                if (lastSsid != null) {
                    Spacer(Modifier.height(8.dp))
                    HorizontalDivider(color = Color(0x44FFFFFF))
                    Spacer(Modifier.height(8.dp))
                }
                fresh.forEach { ssid ->
                    DeviceRow(ssid = ssid, label = null, isConnecting = isConnecting, onConnect = onConnect)
                    Spacer(Modifier.height(4.dp))
                }
            } else if (!isScanning && lastSsid == null) {
                Spacer(Modifier.height(12.dp))
                Text(
                    text     = "Presioná el ícono para buscar dispositivos.",
                    fontSize = 13.sp,
                    color    = Color(0x88EEEEEE),
                )
            }
        }
    }
}

@Composable
fun DeviceRow(
    ssid: String,
    label: String?,
    isConnecting: Boolean,
    onConnect: (String, String) -> Unit,
) {
    Row(modifier = Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Column(modifier = Modifier.weight(1f)) {
            Text(text = ssid, fontSize = 14.sp, fontWeight = FontWeight.Medium, color = Color(0xFFEEEEEE))
            if (label != null) {
                Text(text = label, fontSize = 11.sp, color = Color(0x88EEEEEE))
            }
        }
        Spacer(Modifier.width(8.dp))
        OutlinedButton(
            onClick = { onConnect(ssid, ESP32_PASSWORD) },
            enabled = !isConnecting,
            colors  = ButtonDefaults.outlinedButtonColors(contentColor = MaterialTheme.colorScheme.primary),
        ) {
            if (isConnecting) {
                CircularProgressIndicator(modifier = Modifier.size(14.dp),
                    strokeWidth = 2.dp, color = MaterialTheme.colorScheme.primary)
            } else {
                Text("Conectar", fontSize = 13.sp)
            }
        }
    }
}

// ── NavSearchBar ──────────────────────────────────────────────────────────────

@Composable
fun NavSearchBar(
    route: Any?,
    destination: String,
    isSearching: Boolean,
    onSearch: (String) -> Unit,
    onClear: () -> Unit,
    modifier: Modifier = Modifier,
) {
    var query by remember { mutableStateOf(destination) }

    Row(modifier = modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        OutlinedTextField(
            value         = query,
            onValueChange = { query = it },
            modifier      = Modifier.weight(1f),
            placeholder   = { Text("Buscar destino…", fontSize = 14.sp) },
            leadingIcon   = { Icon(Icons.Default.LocationOn, contentDescription = null) },
            trailingIcon  = if (route != null) {
                { IconButton(onClick = { query = ""; onClear() }) {
                    Icon(Icons.Default.Clear, contentDescription = "Limpiar ruta")
                }}
            } else null,
            singleLine      = true,
            keyboardOptions = KeyboardOptions(imeAction = ImeAction.Search),
            keyboardActions = KeyboardActions(onSearch = { if (query.isNotBlank()) onSearch(query) }),
            colors = OutlinedTextFieldDefaults.colors(
                focusedBorderColor    = MaterialTheme.colorScheme.primary,
                unfocusedBorderColor  = Color(0x66FFFFFF),
                cursorColor           = MaterialTheme.colorScheme.primary,
                focusedTextColor      = Color(0xFFEEEEEE),
                unfocusedTextColor    = Color(0xFFEEEEEE),
                focusedContainerColor = Color(0xDD1A1A2E),
                unfocusedContainerColor = Color(0xCC1A1A2E),
            ),
            shape = RoundedCornerShape(10.dp),
        )
        Spacer(Modifier.width(8.dp))
        IconButton(
            onClick  = { if (query.isNotBlank()) onSearch(query) },
            enabled  = !isSearching,
            modifier = Modifier
                .background(MaterialTheme.colorScheme.primary, RoundedCornerShape(10.dp))
                .size(52.dp),
        ) {
            if (isSearching) {
                CircularProgressIndicator(modifier = Modifier.size(20.dp),
                    color = Color.White, strokeWidth = 2.dp)
            } else {
                Icon(Icons.Default.Search, contentDescription = "Buscar", tint = Color.White)
            }
        }
    }
}

// ── NavStepCard ───────────────────────────────────────────────────────────────

@Composable
fun NavStepCard(
    instruction: String,
    distanceM: Int,
    etaMin: Int,
    stepNum: Int,
    totalSteps: Int,
    modifier: Modifier = Modifier,
) {
    val distText = if (distanceM >= 1000) "${"%.1f".format(distanceM / 1000.0)} km" else "$distanceM m"

    Card(
        modifier = modifier.fillMaxWidth(),
        colors   = CardDefaults.cardColors(containerColor = Color(0xEE0F3020)),
        shape    = RoundedCornerShape(10.dp),
    ) {
        Row(modifier = Modifier.padding(14.dp), verticalAlignment = Alignment.CenterVertically) {
            Column(modifier = Modifier.weight(1f)) {
                Text(text = instruction, fontSize = 15.sp, fontWeight = FontWeight.SemiBold,
                    color = Color(0xFFEEEEEE))
                Spacer(Modifier.height(4.dp))
                Text(text = "En $distText  •  Paso $stepNum de $totalSteps",
                    fontSize = 12.sp, color = Color(0xFF778899))
            }
            Spacer(Modifier.width(12.dp))
            Column(horizontalAlignment = Alignment.End) {
                Text(text = "$etaMin min", fontSize = 18.sp, fontWeight = FontWeight.Bold,
                    color = Color(0xFF4DCC88))
                Text(text = "ETA", fontSize = 11.sp, color = Color(0xFF778899))
            }
        }
    }
}

// ── LocationBadge ─────────────────────────────────────────────────────────────

@Composable
fun LocationBadge(lat: Double, lon: Double, acc: Float, modifier: Modifier = Modifier) {
    Row(
        modifier = modifier
            .background(Color(0xCC1A1A2E), RoundedCornerShape(8.dp))
            .padding(horizontal = 10.dp, vertical = 5.dp),
        horizontalArrangement = Arrangement.spacedBy(8.dp),
        verticalAlignment     = Alignment.CenterVertically,
    ) {
        Text(text = "%.5f, %.5f".format(lat, lon), fontSize = 11.sp, color = Color(0xCCEEEEEE))
        Text(
            text  = "±${"%.0f".format(acc)}m",
            fontSize = 11.sp,
            color = if (acc < 10f) Color(0xFF4DCC88) else Color(0xFFFFAA44),
        )
    }
}

// ── MapFab ────────────────────────────────────────────────────────────────────

@Composable
fun MapFab(onClick: () -> Unit, content: @Composable () -> Unit) {
    Box(
        modifier = Modifier
            .size(44.dp)
            .background(Color(0xDD1A1A2E), RoundedCornerShape(12.dp))
            .clickable(onClick = onClick),
        contentAlignment = Alignment.Center,
    ) { content() }
}

// ── ZoomBadge ─────────────────────────────────────────────────────────────────

@Composable
fun ZoomBadge(zoom: Int) {
    Box(
        modifier = Modifier
            .background(Color(0x881A1A2E), RoundedCornerShape(6.dp))
            .padding(horizontal = 8.dp, vertical = 3.dp),
        contentAlignment = Alignment.Center,
    ) {
        Text(text = "Z$zoom", fontSize = 11.sp, fontWeight = FontWeight.SemiBold,
            color = Color(0xCCEEEEEE))
    }
}
