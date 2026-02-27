package com.tschuster.esp32nav

import android.Manifest
import android.content.Intent
import android.os.Bundle
import android.provider.Settings
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
import androidx.compose.material.icons.filled.Notifications
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Search
import androidx.compose.material3.AlertDialog
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
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.view.WindowCompat
import com.tschuster.esp32nav.map.MapController
import com.tschuster.esp32nav.network.ESP32_PASSWORD
import com.tschuster.esp32nav.network.GeocodeSuggestion
import com.tschuster.esp32nav.ui.theme.ESP32NavTheme
import com.tschuster.esp32nav.util.formatEtaMinutes
import kotlinx.coroutines.delay

private const val TAG = "ESP32Nav/Main"

class MainActivity : ComponentActivity() {

    private val vm: MainViewModel by viewModels()
    private lateinit var mapController: MapController

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
                val denied = grants.filter { !it.value }.keys
                if (denied.isEmpty()) {
                    when (pendingAction) {
                        Action.SCAN -> vm.startScan()
                        Action.CONNECT -> vm.connectToDevice(pendingSsid, pendingPassword)
                    }
                } else {
                    Log.e(TAG, "permisos DENEGADOS: $denied")
                }
            }

    private fun requestPerms(action: Action, ssid: String = "", password: String = "") {
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

        mapController = MapController(this)

        setContent {
            LaunchedEffect(Unit) {
                vm.registerMapCallbacks(
                        enableFollow = { mapController.enableFollow() },
                        setCenter = { lat, lon -> mapController.animateTo(lat, lon) },
                        setZoom = { z -> mapController.setZoom(z) },
                        setRoute = { pts -> mapController.setRoute(pts) },
                        setNavMode = { enabled -> mapController.setNavMode(enabled) },
                        updateBearing = { bearing -> mapController.updateBearing(bearing) },
                )
                mapController.onMapTap = { lat, lon -> vm.routeToMapPoint(lat, lon) }
            }

            ESP32NavTheme {
                val state by vm.ui.collectAsState()
                MainScreen(
                        state = state,
                        mapController = mapController,
                        onScan = { requestPerms(Action.SCAN) },
                        onConnect = { ssid, pw -> requestPerms(Action.CONNECT, ssid, pw) },
                        onSearch = vm::searchSuggestions,
                        onSelectSuggestion = vm::routeToSuggestion,
                        onClearSuggestions = vm::clearSuggestions,
                        onClear = vm::clearRoute,
                        onZoomIn = { vm.setZoom(state.zoom + 1) },
                        onZoomOut = { vm.setZoom(state.zoom - 1) },
                        onCenterLocation = vm::centerOnLocation,
                        onDismissError = vm::dismissError,
                        onOpenNotifSettings = {
                            startActivity(Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS))
                        },
                )
            }
        }
    }

    override fun onResume() {
        super.onResume()
        mapController.onResume()
        vm.checkNotifPermission()   // re-chequear al volver de Ajustes
    }
    override fun onPause() {
        super.onPause()
        mapController.onPause()
    }
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
        onSelectSuggestion: (GeocodeSuggestion) -> Unit,
        onClearSuggestions: () -> Unit,
        onClear: () -> Unit,
        onZoomIn: () -> Unit,
        onZoomOut: () -> Unit,
        onCenterLocation: () -> Unit,
        onDismissError: () -> Unit,
        onOpenNotifSettings: () -> Unit,
) {
    // Dialog de permiso de notificaciones (solo si no fue concedido)
    var notifDialogDismissed by remember { mutableStateOf(false) }
    if (!state.notifPermissionGranted && !notifDialogDismissed) {
        AlertDialog(
            onDismissRequest = { notifDialogDismissed = true },
            icon = {
                Icon(
                    Icons.Default.Notifications,
                    contentDescription = null,
                    tint = MaterialTheme.colorScheme.primary,
                )
            },
            title = { Text("Acceso a notificaciones") },
            text = {
                Text(
                    "Para mostrar WhatsApp, Google Maps y controles de música en el ESP32, " +
                    "la app necesita acceso a las notificaciones del sistema.\n\n" +
                    "Se abrirá Ajustes → buscá \"ESP32 Nav\" y activalo."
                )
            },
            confirmButton = {
                TextButton(onClick = {
                    notifDialogDismissed = true
                    onOpenNotifSettings()
                }) {
                    Text("Ir a Ajustes", color = MaterialTheme.colorScheme.primary)
                }
            },
            dismissButton = {
                TextButton(onClick = { notifDialogDismissed = true }) {
                    Text("Ahora no", color = Color(0x88EEEEEE))
                }
            },
            containerColor = Color(0xFF1A1A2E),
            titleContentColor = Color(0xFFEEEEEE),
            textContentColor = Color(0xCCEEEEEE),
        )
    }

    Box(modifier = Modifier.fillMaxSize()) {

        // ── CAPA 0: Mapa ocupa toda la pantalla ──────────────────────────
        AndroidView(
                factory = { mapController.mapView },
                modifier = Modifier.fillMaxSize(),
        )

        // A partir de aquí todo flota con safeDrawingPadding
        Box(
                modifier = Modifier.fillMaxSize().safeDrawingPadding(),
        ) {

            // ── CAPA 1: Card superior unificada + card de paso nav ────────
            Column(
                    modifier =
                            Modifier.align(Alignment.TopStart)
                                    .fillMaxWidth()
                                    .padding(horizontal = 12.dp, vertical = 8.dp),
                    verticalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                // ── Card principal ────────────────────────────────────────
                Card(
                        modifier = Modifier.fillMaxWidth(),
                        colors = CardDefaults.cardColors(containerColor = Color(0xEE1A1A2E)),
                        shape = RoundedCornerShape(12.dp),
                ) {
                    Column(modifier = Modifier.padding(horizontal = 16.dp, vertical = 12.dp)) {

                        // 1. Título + estado + indicador de navegación activa
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Text(
                                    text = "ESP32 NAV",
                                    fontSize = 16.sp,
                                    fontWeight = FontWeight.Bold,
                                    color = MaterialTheme.colorScheme.primary,
                            )
                            if (state.route != null) {
                                Spacer(Modifier.width(8.dp))
                                Text(
                                        text = "• Navegación activa",
                                        fontSize = 12.sp,
                                        color = Color(0xFF4DCC88),
                                )
                            }
                            Spacer(Modifier.width(10.dp))
                            Text(
                                    text = state.status,
                                    fontSize = 12.sp,
                                    maxLines = 1,
                                    overflow = TextOverflow.Ellipsis,
                                    color = Color(0xCCEEEEEE),
                                    modifier = Modifier.weight(1f),
                            )
                            if (state.isConnected) {
                                Text(
                                        text = "Z${state.zoom}",
                                        fontSize = 12.sp,
                                        color = MaterialTheme.colorScheme.secondary,
                                )
                            }
                        }

                        // 2. Búsqueda (cuando hay GPS)
                        if (state.location != null || state.route != null) {
                            HorizontalDivider(
                                    modifier = Modifier.padding(vertical = 10.dp),
                                    color = Color(0x22FFFFFF),
                            )
                            NavSearchBar(
                                    route = state.route,
                                    destination = state.navDestination,
                                    isSearching =
                                            state.isSearchingRoute || state.isSearchingSuggestions,
                                    suggestions = state.suggestions,
                                    recentSearches = state.recentSearches,
                                    onSearch = onSearch,
                                    onSelectSuggestion = onSelectSuggestion,
                                    onClearSuggestions = onClearSuggestions,
                                    onClear = onClear,
                            )
                        }

                        // 3. Dispositivos (solo cuando no está conectado)
                        if (!state.isConnected) {
                            HorizontalDivider(
                                    modifier = Modifier.padding(vertical = 10.dp),
                                    color = Color(0x22FFFFFF),
                            )
                            Row(verticalAlignment = Alignment.CenterVertically) {
                                Text(
                                        text = "Dispositivos",
                                        fontSize = 14.sp,
                                        fontWeight = FontWeight.SemiBold,
                                        color = Color(0xFFEEEEEE),
                                        modifier = Modifier.weight(1f),
                                )
                                IconButton(
                                        onClick = onScan,
                                        enabled = !state.isScanning && !state.isConnecting,
                                ) {
                                    if (state.isScanning) {
                                        CircularProgressIndicator(
                                                modifier = Modifier.size(20.dp),
                                                color = MaterialTheme.colorScheme.secondary,
                                                strokeWidth = 2.dp,
                                        )
                                    } else {
                                        Icon(
                                                Icons.Default.Refresh,
                                                contentDescription = "Buscar dispositivos",
                                                tint = MaterialTheme.colorScheme.secondary,
                                        )
                                    }
                                }
                            }

                            if (state.isConnecting) {
                                Text(
                                        text = "Buscá el diálogo del sistema y tocá \"Conectar\"",
                                        fontSize = 13.sp,
                                        color = MaterialTheme.colorScheme.primary,
                                )
                            }

                            if (state.lastSsid != null) {
                                Spacer(Modifier.height(4.dp))
                                DeviceRow(
                                        ssid = state.lastSsid,
                                        label = "Último usado",
                                        isConnecting = state.isConnecting,
                                        isConnected = false,
                                        connectedSsid = null,
                                        onConnect = onConnect,
                                )
                            }

                            val fresh = state.scanResults.filter { it != state.lastSsid }
                            if (fresh.isNotEmpty()) {
                                Spacer(Modifier.height(4.dp))
                                fresh.forEach { ssid ->
                                    DeviceRow(
                                            ssid = ssid,
                                            label = null,
                                            isConnecting = state.isConnecting,
                                            isConnected = false,
                                            connectedSsid = null,
                                            onConnect = onConnect
                                    )
                                    Spacer(Modifier.height(4.dp))
                                }
                            } else if (!state.isScanning && state.lastSsid == null) {
                                Spacer(Modifier.height(4.dp))
                                Text(
                                        text = "Presioná el ícono para buscar dispositivos.",
                                        fontSize = 13.sp,
                                        color = Color(0x88EEEEEE),
                                )
                            }
                        }
                    }
                }

                // ── Card de paso de navegación o "Llegaste" ─────────────────
                state.route?.let { route ->
                    if (state.currentStepIndex < route.steps.size) {
                        val step = route.steps[state.currentStepIndex]
                        NavStepCard(
                                instruction = step.instruction,
                                distanceM = step.distanceM,
                                etaMin = route.etaMin,
                                stepNum = state.currentStepIndex + 1,
                                totalSteps = route.steps.size,
                        )
                    } else {
                        ArrivedCard()
                    }
                }
            }

            // ── CAPA 2: Badge de coordenadas GPS (inferior izquierdo) ────
            state.location?.let { loc ->
                LocationBadge(
                        lat = loc.latitude,
                        lon = loc.longitude,
                        acc = loc.accuracy,
                        modifier =
                                Modifier.align(Alignment.BottomStart)
                                        .padding(start = 12.dp, bottom = 12.dp),
                )
            }

            // ── CAPA 3: Botones de mapa (inferior derecho) ───────────────
            if (state.isConnected) {
                Column(
                        modifier =
                                Modifier.align(Alignment.BottomEnd)
                                        .padding(end = 12.dp, bottom = 12.dp),
                        verticalArrangement = Arrangement.spacedBy(6.dp),
                        horizontalAlignment = Alignment.CenterHorizontally,
                ) {
                    if (state.location != null) {
                        MapFab(onClick = onCenterLocation) {
                            Icon(
                                    imageVector = Icons.Default.LocationOn,
                                    contentDescription = "Mi ubicación",
                                    tint = Color.White,
                                    modifier = Modifier.size(20.dp),
                            )
                        }
                    }
                    MapFab(onClick = onZoomIn) {
                        Text(
                                "+",
                                fontSize = 20.sp,
                                fontWeight = FontWeight.Bold,
                                color = Color.White
                        )
                    }
                    ZoomBadge(zoom = state.zoom)
                    MapFab(onClick = onZoomOut) {
                        Text(
                                "−",
                                fontSize = 20.sp,
                                fontWeight = FontWeight.Bold,
                                color = Color.White
                        )
                    }
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
                        modifier = Modifier.align(Alignment.BottomCenter).padding(16.dp),
                        containerColor = Color(0xFF2A1A2E),
                ) { Text(msg, color = Color(0xFFEEEEEE)) }
            }
        }
    }
}

@Composable
fun DeviceRow(
        ssid: String,
        label: String?,
        isConnecting: Boolean,
        isConnected: Boolean,
        connectedSsid: String?,
        onConnect: (String, String) -> Unit,
) {
    val isThisDeviceConnected = isConnected && ssid == connectedSsid
    val canConnect = !isConnecting && !isConnected

    Row(modifier = Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Column(modifier = Modifier.weight(1f)) {
            Text(
                    text = ssid,
                    fontSize = 14.sp,
                    fontWeight = FontWeight.Medium,
                    color = Color(0xFFEEEEEE)
            )
            if (label != null) {
                Text(text = label, fontSize = 11.sp, color = Color(0x88EEEEEE))
            }
        }
        Spacer(Modifier.width(8.dp))
        OutlinedButton(
                onClick = { onConnect(ssid, ESP32_PASSWORD) },
                enabled = canConnect,
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
                Text(
                        text = if (isThisDeviceConnected) "Conectado" else "Conectar",
                        fontSize = 13.sp
                )
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
        suggestions: List<GeocodeSuggestion>,
        recentSearches: List<GeocodeSuggestion>,
        onSearch: (String) -> Unit,
        onSelectSuggestion: (GeocodeSuggestion) -> Unit,
        onClearSuggestions: () -> Unit,
        onClear: () -> Unit,
        modifier: Modifier = Modifier,
) {
    var query by remember { mutableStateOf(destination) }
    // Flag para distinguir cambios del usuario vs. cambios programáticos (evita auto-search al
    // setear destino)
    var userTyped by remember { mutableStateOf(false) }

    // Sync cuando el VM actualiza navDestination (e.g. sugerencia seleccionada o ruta borrada)
    LaunchedEffect(destination) {
        if (destination != query) {
            userTyped = false
            query = destination
        }
    }

    // Debounce: auto-busca 300 ms después de que el usuario deja de tipear
    LaunchedEffect(query) {
        if (userTyped && query.length >= 2) {
            delay(300L)
            onSearch(query)
        }
    }

    Column(modifier = modifier.fillMaxWidth()) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            OutlinedTextField(
                    value = query,
                    onValueChange = {
                        userTyped = true
                        query = it
                        if (suggestions.isNotEmpty()) onClearSuggestions()
                    },
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
                                    ) {
                                        Icon(
                                                Icons.Default.Clear,
                                                contentDescription = "Limpiar ruta"
                                        )
                                    }
                                }
                            } else null,
                    singleLine = true,
                    keyboardOptions = KeyboardOptions(imeAction = ImeAction.Search),
                    keyboardActions =
                            KeyboardActions(onSearch = { if (query.isNotBlank()) onSearch(query) }),
                    colors =
                            OutlinedTextFieldDefaults.colors(
                                    focusedBorderColor = MaterialTheme.colorScheme.primary,
                                    unfocusedBorderColor = Color(0x66FFFFFF),
                                    cursorColor = MaterialTheme.colorScheme.primary,
                                    focusedTextColor = Color(0xFFEEEEEE),
                                    unfocusedTextColor = Color(0xFFEEEEEE),
                                    focusedContainerColor = Color(0xDD1A1A2E),
                                    unfocusedContainerColor = Color(0xCC1A1A2E),
                            ),
                    shape = RoundedCornerShape(10.dp),
            )
            Spacer(Modifier.width(8.dp))
            IconButton(
                    onClick = { if (query.isNotBlank()) onSearch(query) },
                    enabled = !isSearching,
                    modifier =
                            Modifier.background(
                                            MaterialTheme.colorScheme.primary,
                                            RoundedCornerShape(10.dp)
                                    )
                                    .size(52.dp),
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

        // Últimas búsquedas (cuando el campo está vacío y hay recientes)
        if (query.isEmpty() && recentSearches.isNotEmpty()) {
            Spacer(Modifier.height(4.dp))
            Text(
                    text = "Últimas búsquedas",
                    fontSize = 12.sp,
                    color = Color(0x88EEEEEE),
                    modifier = Modifier.padding(vertical = 4.dp),
            )
            Column {
                recentSearches.forEachIndexed { i, suggestion ->
                    if (i > 0) HorizontalDivider(color = Color(0x22FFFFFF))
                    Row(
                            modifier =
                                    Modifier.fillMaxWidth()
                                            .clickable { onSelectSuggestion(suggestion) }
                                            .padding(vertical = 10.dp),
                            verticalAlignment = Alignment.CenterVertically,
                    ) {
                        Icon(
                                Icons.Default.LocationOn,
                                contentDescription = null,
                                tint = Color(0x88FFFFFF),
                                modifier = Modifier.size(16.dp),
                        )
                        Spacer(Modifier.width(10.dp))
                        Text(
                                text = suggestion.label,
                                fontSize = 13.sp,
                                color = Color(0xFFEEEEEE),
                                maxLines = 2,
                                overflow = TextOverflow.Ellipsis,
                        )
                    }
                }
            }
        }

        // Dropdown de sugerencias de la API
        if (suggestions.isNotEmpty()) {
            Spacer(Modifier.height(4.dp))
            Column {
                suggestions.forEachIndexed { i, suggestion ->
                    if (i > 0) HorizontalDivider(color = Color(0x22FFFFFF))
                    Row(
                            modifier =
                                    Modifier.fillMaxWidth()
                                            .clickable { onSelectSuggestion(suggestion) }
                                            .padding(vertical = 10.dp),
                            verticalAlignment = Alignment.CenterVertically,
                    ) {
                        Icon(
                                Icons.Default.LocationOn,
                                contentDescription = null,
                                tint = MaterialTheme.colorScheme.primary,
                                modifier = Modifier.size(16.dp),
                        )
                        Spacer(Modifier.width(10.dp))
                        Text(
                                text = suggestion.label,
                                fontSize = 13.sp,
                                color = Color(0xFFEEEEEE),
                                maxLines = 2,
                                overflow = TextOverflow.Ellipsis,
                        )
                    }
                }
            }
        }
    }
}

// ── ArrivedCard (estilo Google Maps: llegada al destino) ─────────────────────

@Composable
fun ArrivedCard(modifier: Modifier = Modifier) {
    Card(
            modifier = modifier.fillMaxWidth(),
            colors = CardDefaults.cardColors(containerColor = Color(0xEE0F3020)),
            shape = RoundedCornerShape(10.dp),
    ) {
        Row(modifier = Modifier.padding(14.dp), verticalAlignment = Alignment.CenterVertically) {
            Icon(
                    Icons.Default.LocationOn,
                    contentDescription = null,
                    tint = Color(0xFF4DCC88),
                    modifier = Modifier.size(28.dp),
            )
            Spacer(Modifier.width(12.dp))
            Text(
                    text = "Llegaste al destino",
                    fontSize = 15.sp,
                    fontWeight = FontWeight.SemiBold,
                    color = Color(0xFFEEEEEE),
            )
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
    val distText =
            if (distanceM >= 1000) "${"%.1f".format(distanceM / 1000.0)} km" else "$distanceM m"

    Card(
            modifier = modifier.fillMaxWidth(),
            colors = CardDefaults.cardColors(containerColor = Color(0xEE0F3020)),
            shape = RoundedCornerShape(10.dp),
    ) {
        Row(modifier = Modifier.padding(14.dp), verticalAlignment = Alignment.CenterVertically) {
            Column(modifier = Modifier.weight(1f)) {
                Text(
                        text = instruction,
                        fontSize = 15.sp,
                        fontWeight = FontWeight.SemiBold,
                        color = Color(0xFFEEEEEE)
                )
                Spacer(Modifier.height(4.dp))
                Text(
                        text = "En $distText  •  Paso $stepNum de $totalSteps",
                        fontSize = 12.sp,
                        color = Color(0xFF778899)
                )
            }
            Spacer(Modifier.width(12.dp))
            Column(horizontalAlignment = Alignment.End) {
                Text(
                        text = formatEtaMinutes(etaMin),
                        fontSize = 18.sp,
                        fontWeight = FontWeight.Bold,
                        color = Color(0xFF4DCC88)
                )
                Text(text = "ETA", fontSize = 11.sp, color = Color(0xFF778899))
            }
        }
    }
}

// ── LocationBadge ─────────────────────────────────────────────────────────────

@Composable
fun LocationBadge(lat: Double, lon: Double, acc: Float, modifier: Modifier = Modifier) {
    Row(
            modifier =
                    modifier.background(Color(0xCC1A1A2E), RoundedCornerShape(8.dp))
                            .padding(horizontal = 10.dp, vertical = 5.dp),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
            verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(text = "%.5f, %.5f".format(lat, lon), fontSize = 11.sp, color = Color(0xCCEEEEEE))
        Text(
                text = "±${"%.0f".format(acc)}m",
                fontSize = 11.sp,
                color = if (acc < 10f) Color(0xFF4DCC88) else Color(0xFFFFAA44),
        )
    }
}

// ── MapFab ────────────────────────────────────────────────────────────────────

@Composable
fun MapFab(onClick: () -> Unit, content: @Composable () -> Unit) {
    Box(
            modifier =
                    Modifier.size(44.dp)
                            .background(Color(0xDD1A1A2E), RoundedCornerShape(12.dp))
                            .clickable(onClick = onClick),
            contentAlignment = Alignment.Center,
    ) { content() }
}

// ── ZoomBadge ─────────────────────────────────────────────────────────────────

@Composable
fun ZoomBadge(zoom: Int) {
    Box(
            modifier =
                    Modifier.background(Color(0x881A1A2E), RoundedCornerShape(6.dp))
                            .padding(horizontal = 8.dp, vertical = 3.dp),
            contentAlignment = Alignment.Center,
    ) {
        Text(
                text = "Z$zoom",
                fontSize = 11.sp,
                fontWeight = FontWeight.SemiBold,
                color = Color(0xCCEEEEEE)
        )
    }
}
