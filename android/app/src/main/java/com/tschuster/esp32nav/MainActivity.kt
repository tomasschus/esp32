package com.tschuster.esp32nav

import android.Manifest
import android.os.Bundle
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.compose.foundation.Image
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
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.view.WindowCompat
import com.tschuster.esp32nav.network.ESP32_PASSWORD
import com.tschuster.esp32nav.ui.theme.ESP32NavTheme

private const val TAG = "ESP32Nav/Main"

class MainActivity : ComponentActivity() {

    private val vm: MainViewModel by viewModels()

    private var pendingAction = Action.SCAN
    private var pendingSsid = ""
    private var pendingPassword = ""
    private enum class Action {
        SCAN,
        CONNECT
    }

    private val permLauncher =
            registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { grants
                ->
                Log.d(TAG, "permisos resultado: $grants")
                val denied = grants.filter { !it.value }.keys
                if (denied.isEmpty()) {
                    Log.i(
                            TAG,
                            "todos los permisos otorgados → acción=$pendingAction ssid='$pendingSsid'"
                    )
                    when (pendingAction) {
                        Action.SCAN -> vm.startScan()
                        Action.CONNECT -> vm.connectToDevice(pendingSsid, pendingPassword)
                    }
                } else {
                    Log.e(TAG, "permisos DENEGADOS: $denied")
                }
            }

    private fun requestPerms(action: Action, ssid: String = "", password: String = "") {
        Log.d(TAG, "requestPerms: action=$action ssid='$ssid'")
        pendingAction = action
        pendingSsid = ssid
        pendingPassword = password
        permLauncher.launch(
                arrayOf(
                        Manifest.permission.ACCESS_FINE_LOCATION,
                        Manifest.permission.ACCESS_COARSE_LOCATION,
                        Manifest.permission.NEARBY_WIFI_DEVICES,
                        Manifest.permission.CHANGE_NETWORK_STATE,
                )
        )
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        WindowCompat.setDecorFitsSystemWindows(window, false)

        setContent {
            ESP32NavTheme {
                val state by vm.ui.collectAsState()
                val mapTile by vm.mapTile.collectAsState()
                MainScreen(
                        state = state,
                        mapTile = mapTile,
                        onScan = { requestPerms(Action.SCAN) },
                        onConnect = { ssid, pw -> requestPerms(Action.CONNECT, ssid, pw) },
                        onSearch = vm::searchRoute,
                        onClear = vm::clearRoute,
                        onZoomIn = { vm.setZoom(state.zoom + 1) },
                        onZoomOut = { vm.setZoom(state.zoom - 1) },
                        onCenterLocation = vm::centerOnLocation,
                        onDismissError = vm::dismissError,
                )
            }
        }
    }
}

// ── Composables ───────────────────────────────────────────────────────────────

@Composable
fun MainScreen(
        state: UiState,
        mapTile: ByteArray?,
        onScan: () -> Unit,
        onConnect: (ssid: String, password: String) -> Unit,
        onSearch: (String) -> Unit,
        onClear: () -> Unit,
        onZoomIn: () -> Unit,
        onZoomOut: () -> Unit,
        onCenterLocation: () -> Unit,
        onDismissError: () -> Unit,
) {
    Box(modifier = Modifier.fillMaxSize().background(MaterialTheme.colorScheme.background)) {
        Column(modifier = Modifier.fillMaxSize()) {
            HeaderBar(status = state.status, zoom = state.zoom)

            if (!state.isConnected) {
                ConnectCard(
                        isScanning = state.isScanning,
                        isConnecting = state.isConnecting,
                        scanResults = state.scanResults,
                        lastSsid = state.lastSsid,
                        onScan = onScan,
                        onConnect = onConnect,
                        modifier = Modifier.padding(16.dp),
                )
            }

            if (state.isConnected) {
                NavSearchBar(
                        route = state.route,
                        destination = state.navDestination,
                        isSearching = state.isSearchingRoute,
                        onSearch = onSearch,
                        onClear = onClear,
                        modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp),
                )
            }

            state.route?.let { route ->
                if (state.currentStepIndex < route.steps.size) {
                    val step = route.steps[state.currentStepIndex]
                    NavStepCard(
                            instruction = step.instruction,
                            distanceM = step.distanceM,
                            etaMin = route.etaMin,
                            stepNum = state.currentStepIndex + 1,
                            totalSteps = route.steps.size,
                            modifier = Modifier.padding(horizontal = 16.dp),
                    )
                }
            }

            Box(modifier = Modifier.weight(1f)) {
                MapTileView(mapTile = mapTile, modifier = Modifier.fillMaxSize())
                if (state.isConnected && state.location != null) {
                    IconButton(
                            onClick = onCenterLocation,
                            modifier =
                                    Modifier.align(Alignment.BottomEnd)
                                            .padding(12.dp)
                                            .size(44.dp)
                                            .background(
                                                    MaterialTheme.colorScheme.primary,
                                                    RoundedCornerShape(12.dp)
                                            ),
                    ) {
                        Icon(
                                imageVector = Icons.Default.LocationOn,
                                contentDescription = "Ir a mi ubicación",
                                tint = Color.White,
                                modifier = Modifier.size(22.dp),
                        )
                    }
                }
            }

            state.location?.let { loc ->
                LocationBar(lat = loc.latitude, lon = loc.longitude, acc = loc.accuracy)
            }

            if (state.isConnected) {
                ZoomControls(
                        zoom = state.zoom,
                        onZoomIn = onZoomIn,
                        onZoomOut = onZoomOut,
                        modifier = Modifier.padding(16.dp)
                )
            }
        }

        state.errorMsg?.let { msg ->
            Snackbar(
                    action = {
                        TextButton(onClick = onDismissError) {
                            Text("OK", color = MaterialTheme.colorScheme.primary)
                        }
                    },
                    modifier = Modifier.align(Alignment.BottomCenter).padding(16.dp),
                    containerColor = Color(0xFF2A1A2E),
            ) { Text(msg, color = MaterialTheme.colorScheme.onSurface) }
        }
    }
}

// ── MapTileView ───────────────────────────────────────────────────────────────

@Composable
fun MapTileView(mapTile: ByteArray?, modifier: Modifier = Modifier) {
    Box(
            modifier = modifier.fillMaxWidth().background(Color(0xFF0A0A1A)),
            contentAlignment = Alignment.Center,
    ) {
        if (mapTile != null) {
            val bitmap =
                    remember(mapTile) {
                        android.graphics.BitmapFactory.decodeByteArray(mapTile, 0, mapTile.size)
                                ?.asImageBitmap()
                    }
            if (bitmap != null) {
                Image(
                        bitmap = bitmap,
                        contentDescription = "Mapa",
                        modifier = Modifier.fillMaxSize(),
                        contentScale = ContentScale.Fit,
                )
            }
        } else {
            Text(
                    text = "Sin mapa — esperando tile…",
                    fontSize = 13.sp,
                    color = Color(0xFF445566),
            )
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
        onConnect: (ssid: String, password: String) -> Unit,
        modifier: Modifier = Modifier,
) {
    Card(
            modifier = modifier.fillMaxWidth(),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
            shape = RoundedCornerShape(12.dp),
    ) {
        Column(modifier = Modifier.padding(20.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(
                        text = "Dispositivos ESP32",
                        fontSize = 16.sp,
                        fontWeight = FontWeight.SemiBold,
                        color = MaterialTheme.colorScheme.onSurface,
                        modifier = Modifier.weight(1f),
                )
                IconButton(onClick = onScan, enabled = !isScanning && !isConnecting) {
                    if (isScanning) {
                        CircularProgressIndicator(
                                modifier = Modifier.size(20.dp),
                                color = MaterialTheme.colorScheme.secondary,
                                strokeWidth = 2.dp,
                        )
                    } else {
                        Icon(
                                imageVector = Icons.Default.Refresh,
                                contentDescription = "Buscar",
                                tint = MaterialTheme.colorScheme.secondary,
                        )
                    }
                }
            }

            // ── Aviso diálogo del sistema ─────────────────────────────
            if (isConnecting) {
                Spacer(modifier = Modifier.height(8.dp))
                Text(
                        text = "Buscá el diálogo del sistema y tocá \"Conectar\"",
                        fontSize = 13.sp,
                        color = MaterialTheme.colorScheme.primary,
                )
            }

            // ── Último dispositivo guardado ───────────────────────────
            if (lastSsid != null) {
                Spacer(modifier = Modifier.height(8.dp))
                DeviceRow(
                        ssid = lastSsid,
                        label = "Último usado",
                        isConnecting = isConnecting,
                        onConnect = onConnect,
                )
            }

            // ── Resultados del scan ───────────────────────────────────
            if (scanResults.isNotEmpty()) {
                val fresh = scanResults.filter { it != lastSsid }
                if (fresh.isNotEmpty()) {
                    if (lastSsid != null) {
                        Spacer(modifier = Modifier.height(8.dp))
                        HorizontalDivider(
                                color = MaterialTheme.colorScheme.outline.copy(alpha = 0.4f)
                        )
                        Spacer(modifier = Modifier.height(8.dp))
                    }
                    fresh.forEach { ssid ->
                        DeviceRow(
                                ssid = ssid,
                                label = null,
                                isConnecting = isConnecting,
                                onConnect = onConnect,
                        )
                        Spacer(modifier = Modifier.height(4.dp))
                    }
                }
            } else if (!isScanning && lastSsid == null) {
                Spacer(modifier = Modifier.height(12.dp))
                Text(
                        text = "Presioná el ícono para buscar dispositivos.",
                        fontSize = 13.sp,
                        color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.5f),
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
        onConnect: (ssid: String, password: String) -> Unit,
) {
    Row(modifier = Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Column(modifier = Modifier.weight(1f)) {
            Text(
                    text = ssid,
                    fontSize = 14.sp,
                    fontWeight = FontWeight.Medium,
                    color = MaterialTheme.colorScheme.onSurface
            )
            if (label != null) {
                Text(
                        text = label,
                        fontSize = 11.sp,
                        color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.5f)
                )
            }
        }
        Spacer(modifier = Modifier.width(8.dp))
        OutlinedButton(
                onClick = { onConnect(ssid, ESP32_PASSWORD) },
                enabled = !isConnecting,
                colors =
                        ButtonDefaults.outlinedButtonColors(
                                contentColor = MaterialTheme.colorScheme.primary
                        ),
        ) {
            if (isConnecting) {
                CircularProgressIndicator(
                        modifier = Modifier.size(14.dp),
                        strokeWidth = 2.dp,
                        color = MaterialTheme.colorScheme.primary
                )
            } else {
                Text("Conectar", fontSize = 13.sp)
            }
        }
    }
}

// ── Resto de composables ──────────────────────────────────────────────────────

@Composable
fun HeaderBar(status: String, zoom: Int) {
    Row(
            modifier =
                    Modifier.fillMaxWidth()
                            .background(MaterialTheme.colorScheme.surface)
                            .padding(horizontal = 16.dp, vertical = 12.dp),
            verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
                text = "ESP32 NAV",
                fontSize = 18.sp,
                fontWeight = FontWeight.Bold,
                color = MaterialTheme.colorScheme.primary
        )
        Spacer(modifier = Modifier.width(12.dp))
        Text(
                text = status,
                fontSize = 12.sp,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
                color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.7f),
                modifier = Modifier.weight(1f)
        )
        Text(text = "Z$zoom", fontSize = 12.sp, color = MaterialTheme.colorScheme.secondary)
    }
    Box(
            modifier =
                    Modifier.fillMaxWidth()
                            .height(2.dp)
                            .background(MaterialTheme.colorScheme.primary)
    )
}

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
                value = query,
                onValueChange = { query = it },
                modifier = Modifier.weight(1f),
                placeholder = { Text("Buscar destino…", fontSize = 14.sp) },
                leadingIcon = { Icon(Icons.Default.LocationOn, contentDescription = null) },
                trailingIcon =
                        if (route != null) {
                            {
                                IconButton(
                                        onClick = {
                                            query = ""
                                            onClear()
                                        }
                                ) { Icon(Icons.Default.Clear, contentDescription = "Limpiar ruta") }
                            }
                        } else null,
                singleLine = true,
                keyboardOptions = KeyboardOptions(imeAction = ImeAction.Search),
                keyboardActions =
                        KeyboardActions(onSearch = { if (query.isNotBlank()) onSearch(query) }),
                colors =
                        OutlinedTextFieldDefaults.colors(
                                focusedBorderColor = MaterialTheme.colorScheme.primary,
                                unfocusedBorderColor = MaterialTheme.colorScheme.outline,
                                cursorColor = MaterialTheme.colorScheme.primary,
                                focusedTextColor = MaterialTheme.colorScheme.onSurface,
                                unfocusedTextColor = MaterialTheme.colorScheme.onSurface,
                        ),
        )
        Spacer(modifier = Modifier.width(8.dp))
        IconButton(
                onClick = { if (query.isNotBlank()) onSearch(query) },
                enabled = !isSearching,
                modifier =
                        Modifier.background(
                                        MaterialTheme.colorScheme.primary,
                                        RoundedCornerShape(8.dp)
                                )
                                .size(48.dp),
        ) {
            if (isSearching) {
                CircularProgressIndicator(
                        modifier = Modifier.size(20.dp),
                        color = Color.White,
                        strokeWidth = 2.dp
                )
            } else {
                Icon(Icons.Default.Search, contentDescription = "Buscar", tint = Color.White)
            }
        }
    }
}

@Composable
fun NavStepCard(
        instruction: String,
        distanceM: Int,
        etaMin: Int,
        stepNum: Int,
        totalSteps: Int,
        modifier: Modifier = Modifier,
) {
    val distText =
            if (distanceM >= 1000) "${"%.1f".format(distanceM / 1000.0)} km" else "$distanceM m"

    Card(
            modifier = modifier.fillMaxWidth(),
            colors = CardDefaults.cardColors(containerColor = Color(0xFF0F3020)),
            shape = RoundedCornerShape(10.dp)
    ) {
        Row(modifier = Modifier.padding(14.dp), verticalAlignment = Alignment.CenterVertically) {
            Column(modifier = Modifier.weight(1f)) {
                Text(
                        text = instruction,
                        fontSize = 15.sp,
                        fontWeight = FontWeight.SemiBold,
                        color = Color(0xFFEEEEEE)
                )
                Spacer(modifier = Modifier.height(4.dp))
                Text(
                        text = "En $distText  •  Paso $stepNum de $totalSteps",
                        fontSize = 12.sp,
                        color = Color(0xFF778899)
                )
            }
            Spacer(modifier = Modifier.width(12.dp))
            Column(horizontalAlignment = Alignment.End) {
                Text(
                        text = "$etaMin min",
                        fontSize = 18.sp,
                        fontWeight = FontWeight.Bold,
                        color = Color(0xFF4DCC88)
                )
                Text(text = "ETA", fontSize = 11.sp, color = Color(0xFF778899))
            }
        }
    }
}

@Composable
fun LocationBar(lat: Double, lon: Double, acc: Float) {
    Row(
            modifier =
                    Modifier.fillMaxWidth()
                            .background(MaterialTheme.colorScheme.surface.copy(alpha = 0.9f))
                            .padding(horizontal = 16.dp, vertical = 8.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
    ) {
        Text(
                text = "%.5f, %.5f".format(lat, lon),
                fontSize = 12.sp,
                color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.7f)
        )
        Text(
                text = "±${"%.0f".format(acc)} m",
                fontSize = 12.sp,
                color = if (acc < 10f) Color(0xFF4DCC88) else Color(0xFFFFAA44)
        )
    }
}

@Composable
fun ZoomControls(
        zoom: Int,
        onZoomIn: () -> Unit,
        onZoomOut: () -> Unit,
        modifier: Modifier = Modifier
) {
    Row(
            modifier = modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.Center,
            verticalAlignment = Alignment.CenterVertically
    ) {
        ZoomButton("−", onZoomOut)
        Spacer(modifier = Modifier.width(16.dp))
        Text(
                text = "Zoom $zoom",
                fontSize = 14.sp,
                fontWeight = FontWeight.Medium,
                color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.7f)
        )
        Spacer(modifier = Modifier.width(16.dp))
        ZoomButton("+", onZoomIn)
    }
}

@Composable
fun ZoomButton(label: String, onClick: () -> Unit) {
    Box(
            modifier =
                    Modifier.size(40.dp)
                            .background(
                                    MaterialTheme.colorScheme.surfaceVariant,
                                    RoundedCornerShape(8.dp)
                            )
                            .clickable(onClick = onClick),
            contentAlignment = Alignment.Center,
    ) {
        Text(
                text = label,
                fontSize = 20.sp,
                fontWeight = FontWeight.Bold,
                color = MaterialTheme.colorScheme.secondary
        )
    }
}
