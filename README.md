# ESP32 NAV

Sistema de navegación GPS con pantalla táctil para el dispositivo **JC3248W535EN** (ESP32-S3 + pantalla QSPI 480×320) y app Android companion. Opcionalmente, un backend NestJS provee geocoding y cálculo de rutas (GraphHopper).

## Descripción

El ESP32 actúa como pantalla de navegación: muestra mapas, instrucciones de giro y reproduce audio. La app Android se conecta al ESP32 por WiFi, descarga tiles del mapa por datos móviles y los envía al dispositivo por WebSocket junto con la posición GPS y los pasos de navegación. Las búsquedas de dirección y el cálculo de rutas se hacen contra un backend (por defecto `https://maps.tomasschuster.com`) que usa GraphHopper.

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

        Celular ──> Backend (geocode + route) ──> GraphHopper API
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
│   ├── game_runner.cpp        # Launcher de juegos embebidos
│   ├── wifi_manager.cpp
│   ├── AXS15231B_touch.*      # Driver touch
│   └── ui/
│       ├── screen_main_menu.* # Menú principal
│       ├── screen_map.*       # Pantalla de mapas
│       ├── screen_tools.*     # Herramientas (incl. Mapas)
│       ├── screen_player.*    # Reproductor
│       ├── screen_timer.*     # Cronómetro
│       ├── screen_wifi.*      # Config WiFi
│       ├── screen_wifi_analyzer.*
│       ├── screen_settings.*
│       ├── screen_games.*     # Menú de juegos
│       └── ...
├── include/
│   ├── lv_conf.h              # Config LVGL
│   └── maps_ws_server.h
├── juegos/
│   ├── snake/
│   ├── pong/
│   ├── tetris/
│   └── flappy_bird/
├── boards/
│   └── esp32-s3-n16r8v.json  # Board custom
└── platformio.ini
```

### Dependencias (PlatformIO)

- `lvgl/lvgl@9.2.2`
- `moononournation/GFX Library for Arduino@1.5.0`
- `ESP32Async/ESPAsyncWebServer`
- `ESP8266Audio`
- `TJpg_Decoder`
- `bblanchon/ArduinoJson`

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
├── MainActivity.kt            # UI principal (Jetpack Compose)
├── MainViewModel.kt           # Lógica de negocio
├── ESP32NavApp.kt             # Application
├── location/
│   └── LocationTracker.kt     # GPS en tiempo real
├── map/
│   └── MapController.kt       # Control del mapa (OSM, overlays, rutas)
├── network/
│   ├── NetworkManager.kt      # Gestión de redes (WiFi ESP32 + celular)
│   ├── Esp32Client.kt         # WebSocket hacia el ESP32
│   ├── MapFetcher.kt          # Descarga de tiles (Mapbox/OSM) por celular
│   ├── NavRouter.kt           # Geocode + rutas vía backend
│   └── VectorFetcher.kt       # Overpass API (ej. POIs)
├── util/
│   └── VectorRenderer.kt      # Render de geometrías vectoriales
└── ui/theme/
    └── Theme.kt
```

### Flujo

1. La app pide datos móviles y los mantiene activos.
2. El usuario conecta el WiFi al AP **ESP32-NAV** (pass: `esp32nav12`).
3. Android mantiene ambas redes: WiFi → ESP32, celular → internet.
4. Cada 5 s descarga un tile JPEG 480×320 (Mapbox o OSM) por datos móviles y lo envía al ESP32 por WebSocket.
5. El GPS se envía continuamente al ESP32 en JSON.
6. Al buscar una dirección, la app llama al **backend** (`/geocode`); al calcular una ruta, llama a `/route`. Por defecto usa `https://maps.tomasschuster.com` (ver [Backend](#backend) para correr el backend en local).

### Fuentes de mapas (tiles)

| Fuente | Notas |
|---|---|
| Mapbox Static API | Requiere token en `MapFetcher.MAPBOX_TOKEN` |
| staticmap.openstreetmap.de | Sin API key (puede fallar DNS) |
| tile.openstreetmap.org | Sin API key, fallback principal |

Para usar Mapbox reemplazá `YOUR_MAPBOX_TOKEN_HERE` en `MapFetcher.kt`.

---

## Backend

Backend NestJS opcional para **geocoding** y **routing**. La app Android está configurada por defecto para usar `https://maps.tomasschuster.com`; si querés usar tu propia instancia, cambiá `BASE_URL` en `NavRouter.kt`.

### Requisitos

- Node.js 18+
- API key de [GraphHopper](https://www.graphhopper.com/) (hay tier gratis)

### Instalación y ejecución

```bash
cd backend
npm install
export GRAPHHOPPER_API_KEY=tu_api_key
npm run start:dev
```

Por defecto escucha en el puerto **4500** (`PORT` opcional).

### Endpoints

| Método | Ruta | Descripción |
|---|---|---|
| GET | `/geocode?q=...&lat=&lon=` | Búsqueda de direcciones (GraphHopper Geocoding). `lat`/`lon` opcionales para priorizar resultados cercanos. |
| POST | `/route` | Cálculo de ruta. Body: `{ "from": [lon, lat], "to": [lon, lat], "profile": "car" }`. Respuesta: distancia, tiempo, polyline, instrucciones. |

### Variables de entorno

| Variable | Descripción |
|---|---|
| `PORT` | Puerto HTTP (default: 4500) |
| `GRAPHHOPPER_API_KEY` | API key de GraphHopper (geocode + route) |

Para usar el backend en local desde la app, desplegá en un servidor accesible por el celular (ej. ngrok, VPS) y actualizá `BASE_URL` en `android/.../network/NavRouter.kt`.

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
5. Buscar un destino en la app (usa el backend por defecto) para activar la navegación paso a paso.

---

## Licencia

MIT
