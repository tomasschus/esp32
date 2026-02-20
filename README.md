# ESP32 NAV

Sistema de navegación GPS con pantalla táctil para el dispositivo **JC3248W535EN** (ESP32-S3 + pantalla QSPI 480×320) y app Android companion.

## Descripción

El ESP32 actúa como pantalla de navegación: muestra mapas, instrucciones de giro y reproduce audio. La app Android se conecta al ESP32 por WiFi, descarga tiles del mapa por datos móviles y los envía al dispositivo por WebSocket junto con la posición GPS y los pasos de navegación.

```
Android (GPS + datos móviles)
        │
        ├── WiFi AP "ESP32-NAV" ──── WebSocket ws://192.168.4.1:8080/ws
        │                                  │
        │                           JPEG tiles (480×320)
        │                           GPS JSON
        │                           Nav steps JSON
        │                                  │
        └─────────────────────────> ESP32-S3 (pantalla + audio)
```

---

## Hardware

| Componente | Detalle |
|---|---|
| Placa | JC3248W535EN |
| SoC | ESP32-S3 N16R8 (16 MB Flash, 8 MB PSRAM) |
| Pantalla | 480×320 QSPI, controlador AXS15231B |
| Touch | Capacitivo I2C |
| Audio | I2S + SD card |
| Alimentación | USB-C |

---

## Proyecto ESP32

### Requisitos

- [PlatformIO](https://platformio.org/) (CLI o extensión VS Code / Cursor)
- Cable USB-C para flash y monitor serial

### Compilar y flashear

```bash
cd /ruta/al/proyecto
pio run -t upload
```

### Monitor serial

```bash
pio device monitor -b 115200
```

### Estructura

```
esp32/
├── src/
│   ├── main.cpp                # Setup + loop principal
│   ├── maps_ws_server.cpp      # AP WiFi + WebSocket + decoder JPEG
│   ├── audio_mgr.cpp           # Audio desde SD por I2S
│   └── ui/
│       ├── screen_main_menu.*  # Menú principal
│       ├── screen_map.*        # Pantalla de mapas
│       ├── screen_tools.*      # Herramientas
│       ├── screen_player.*     # Reproductor
│       ├── screen_timer.*      # Cronómetro
│       ├── screen_wifi.*       # Config WiFi
│       └── ...
├── include/
│   ├── lv_conf.h               # Config LVGL
│   └── maps_ws_server.h
├── juegos/
│   ├── snake/
│   ├── pong/
│   ├── tetris/
│   └── flappy_bird/
├── boards/
│   └── esp32-s3-n16r8v.json   # Board custom
└── platformio.ini
```

### Dependencias (PlatformIO)

- `lvgl/lvgl@9.2.2`
- `moononournation/GFX Library for Arduino@1.5.0`
- `ESP32Async/ESPAsyncWebServer`
- `ESP8266Audio`
- `TJpg_Decoder`

---

## App Android

### Requisitos

- Android Studio o Gradle 8+
- Android SDK 34
- Dispositivo Android con datos móviles y WiFi

### Compilar

```bash
cd android
./gradlew assembleDebug
```

O abrí la carpeta `android/` directamente en Android Studio.

### Estructura

```
android/app/src/main/java/com/tschuster/esp32nav/
├── MainActivity.kt         # UI principal (Jetpack Compose)
├── MainViewModel.kt        # Lógica de negocio
├── ESP32NavApp.kt          # Application
├── location/
│   └── LocationTracker.kt  # GPS en tiempo real
├── network/
│   ├── NetworkManager.kt   # Gestión de redes (WiFi ESP32 + celular)
│   ├── Esp32Client.kt      # WebSocket hacia el ESP32
│   ├── MapFetcher.kt       # Descarga de tiles OSM por celular
│   └── NavRouter.kt        # Geocoding + rutas (OSRM/Nominatim)
└── ui/theme/
    └── Theme.kt
```

### Flujo

1. La app pide datos móviles y los mantiene activos.
2. El usuario conecta el WiFi al AP **ESP32-NAV** (pass: `esp32nav12`).
3. Android mantiene ambas redes simultáneamente: WiFi → ESP32, celular → internet.
4. Cada 5 s descarga un tile JPEG 480×320 de OpenStreetMap por datos móviles y lo envía al ESP32 por WebSocket.
5. El GPS se envía continuamente al ESP32 en JSON.
6. Al buscar una ruta, usa Nominatim (geocoding) y OSRM (routing), ambos por datos móviles.

### Fuentes de mapas

| Fuente | Notas |
|---|---|
| Mapbox Static API | Requiere token en `MapFetcher.MAPBOX_TOKEN` |
| staticmap.openstreetmap.de | Sin API key (puede fallar DNS) |
| tile.openstreetmap.org | Sin API key, fallback principal |

Para usar Mapbox reemplazá `YOUR_MAPBOX_TOKEN_HERE` en `MapFetcher.kt`.

---

## Protocolo WebSocket

El ESP32 escucha en `ws://192.168.4.1:8080/ws`.

| Tipo | Formato | Descripción |
|---|---|---|
| Binario | JPEG bytes | Tile de mapa 480×320 |
| Texto | `{"t":"gps","lat":0.0,"lon":0.0}` | Posición GPS |
| Texto | `{"t":"nav","step":"...","dist":"200m","eta":"12 min"}` | Paso de navegación |

---

## Uso

1. Encender el ESP32 y navegar a **Herramientas → Mapas**.
2. El ESP32 levanta el AP `ESP32-NAV` y el servidor WebSocket.
3. Conectar el teléfono Android al WiFi `ESP32-NAV` (contraseña: `esp32nav12`).
4. Abrir la app → conectar al dispositivo → el mapa aparece en el ESP32.
5. Buscar un destino en la app para activar la navegación paso a paso.

---

## Licencia

MIT
