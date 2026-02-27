# Plan: Pantalla "Teléfono" — ESP32Nav

**Objetivo**: Nueva pantalla en el ESP32 que muestre en tiempo real, desde el teléfono:
- Indicaciones de Google Maps (paso, maniobra, distancia, ETA)
- Controles de música (YouTube Music / Spotify / cualquier media)
- Notificaciones entrantes (WhatsApp, etc.)

---

## 1. Diseño de pantalla (480×320 landscape)

```
┌─────────────────────────────────────────────────────────────────┐ 480×320
│ [←]  TELÉFONO                              ● conectado          │ 28px
├─────────────────────────────────────────────────────────────────┤
│ NAVEGACIÓN  [ICONO 64×64]  Girar a la derecha                   │
│             en Av. Corrientes          200m | ETA 15 min        │ 76px
├────────────────────────────────┬────────────────────────────────┤
│ MÚSICA                         │                                 │
│ 🎵 Bohemian Rhapsody - Queen   │  [◀◀]  [▶/⏸]  [▶▶]           │ 64px
│                                │   [-vol]      [+vol]            │
├────────────────────────────────┴────────────────────────────────┤
│ 💬 Juan García (WhatsApp): Hola, llegás en...                   │ 38px
│ 🔔 Gmail: Newsletter de Medium - Top 5 stories...               │ 38px
│ 💬 Grupo Motos (WhatsApp): ¿Van mañana al...                   │ 38px
└─────────────────────────────────────────────────────────────────┘
```

**Criterios de UX para moto:**
- Fuente mínima 22px para legibilidad a 70cm con casco
- Botones de música mínimo 60×60px para uso con guantes
- Contraste alto, fondo oscuro (tema nocturno)
- Sin scroll: toda la info visible de un vistazo

---

## 2. Arquitectura técnica

```
[Android App]                    [ESP32]
     │                               │
     ├─ NotificationListenerService  │
     │   ├─ Google Maps notifs  ──────►  parse_gmaps()  → screen_phone render
     │   ├─ WhatsApp notifs     ──────►  parse_notif()  → notifications list
     │   └─ Otras apps          ──────►  parse_notif()  → notifications list
     │                               │
     ├─ MediaSessionManager      ──────►  parse_media()  → music player
     │   ├─ Metadata (título/artista) │
     │   ├─ Estado (play/pause)       │
     │   └─ Volumen actual            │
     │                               │
     └─ Esp32Client (WS)             │
         envía JSON ──────────────────►  maps_ws_server.cpp
```

### Nuevos tipos de mensaje WebSocket

**Mensaje de Google Maps** (`"t":"gmaps"`):
```json
{
  "t": "gmaps",
  "step": "Girar a la derecha",
  "street": "Av. Corrientes",
  "dist": "200 m",
  "eta": "15 min",
  "maneuver": "turn-right"
}
```

**Mensaje de media** (`"t":"media"`):
```json
{
  "t": "media",
  "app": "YouTube Music",
  "title": "Bohemian Rhapsody",
  "artist": "Queen",
  "playing": true,
  "vol": 75
}
```

**Mensaje de notificación** (`"t":"notif"`):
```json
{
  "t": "notif",
  "app": "WhatsApp",
  "title": "Juan García",
  "text": "Hola, llegás en...",
  "id": 12345
}
```

**Comando de control de media** (Android recibe del ESP32):
```json
{
  "t": "media_cmd",
  "cmd": "play" | "pause" | "next" | "prev" | "vol_up" | "vol_down"
}
```

---

## 3. Implementación — Android

### 3.1 Nuevo: `PhoneNotificationService.kt`
- Extiende `NotificationListenerService`
- Filtros por paquete:
  - `com.google.android.apps.maps` → tipo `gmaps`
  - `com.whatsapp`, `com.whatsapp.w4b` → tipo `notif`
  - cualquier otra app → tipo `notif`
- Extrae de cada notificación: título, texto, nombre de app
- Para Google Maps: parsea el campo `text` de la notificación que contiene el paso de navegación y la distancia

**Permiso especial requerido:**
```
Configuración → Apps → Acceso especial → Acceder a notificaciones → ESP32Nav ✓
```

**Registro en `AndroidManifest.xml`:**
```xml
<service
    android:name=".PhoneNotificationService"
    android:label="ESP32Nav Notificaciones"
    android:permission="android.permission.BIND_NOTIFICATION_LISTENER_SERVICE"
    android:exported="true">
    <intent-filter>
        <action android:name="android.service.notification.NotificationListenerService" />
    </intent-filter>
</service>
```

### 3.2 Nuevo: `MediaSessionManager.kt` (o integrar en `MainViewModel`)
- Usa `android.media.session.MediaSessionManager` para obtener sesiones activas
- Obtiene `MediaController` de la sesión activa (YouTube Music / Spotify)
- Metadata: `MediaMetadata.METADATA_KEY_TITLE`, `METADATA_KEY_ARTIST`
- Estado: `PlaybackState.STATE_PLAYING` / `STATE_PAUSED`
- Envía updates por WebSocket cada vez que cambia el estado
- Ejecuta comandos recibidos del ESP32:
  - `play/pause` → `controller.getTransportControls().play()/pause()`
  - `next/prev` → `skipToNext()/skipToPrevious()`
  - `vol_up/down` → `AudioManager.adjustStreamVolume()`

**Permiso necesario** (ya está en Android 13+ sin permiso extra):
```
android.permission.MEDIA_CONTENT_CONTROL  (o vía NotificationListener implícita)
```

### 3.3 Modificar: `Esp32Client.kt`
- Recibir mensajes del ESP32 de tipo `media_cmd`
- Despachar a `MediaSessionManager`

### 3.4 Modificar: `MainViewModel.kt`
- Iniciar `PhoneNotificationService` cuando se conecta al ESP32
- Verificar permisos de NotificationListenerService al inicio
- Si no tiene permiso: mostrar diálogo pidiendo activarlo

---

## 4. Implementación — ESP32 Firmware

### 4.1 Modificar: `maps_ws_server.h`

Nuevas estructuras de datos:
```c
#define GMAPS_STEP_MAX   64
#define GMAPS_STREET_MAX 48
#define GMAPS_DIST_MAX   16
#define GMAPS_ETA_MAX    16
#define GMAPS_MANEUVER_MAX 32

typedef struct {
    char step[GMAPS_STEP_MAX];
    char street[GMAPS_STREET_MAX];
    char dist[GMAPS_DIST_MAX];
    char eta[GMAPS_ETA_MAX];
    char maneuver[GMAPS_MANEUVER_MAX];
} gmaps_step_t;

#define MEDIA_TITLE_MAX  48
#define MEDIA_ARTIST_MAX 32
#define MEDIA_APP_MAX    24

typedef struct {
    char app[MEDIA_APP_MAX];
    char title[MEDIA_TITLE_MAX];
    char artist[MEDIA_ARTIST_MAX];
    bool playing;
    uint8_t vol;  // 0-100
} media_state_t;

#define NOTIF_APP_MAX   24
#define NOTIF_TITLE_MAX 32
#define NOTIF_TEXT_MAX  64
#define NOTIF_QUEUE_MAX  3

typedef struct {
    char app[NOTIF_APP_MAX];
    char title[NOTIF_TITLE_MAX];
    char text[NOTIF_TEXT_MAX];
} notif_t;

// Callbacks
typedef void (*maps_ws_on_gmaps_t)(const gmaps_step_t *step);
typedef void (*maps_ws_on_media_t)(const media_state_t *media);
typedef void (*maps_ws_on_notif_t)(const notif_t *notif);
```

### 4.2 Modificar: `maps_ws_server.cpp`

Nuevas funciones de parseo:
```c
static void parse_gmaps(const char *json);   // "t":"gmaps"
static void parse_media(const char *json);   // "t":"media"
static void parse_notif(const char *json);   // "t":"notif"
```

Función para enviar comando de media al cliente Android:
```c
void maps_ws_send_media_cmd(const char *cmd);
// Envía: {"t":"media_cmd","cmd":"<cmd>"}
```

### 4.3 Modificar: `ui.h`

Agregar nueva pantalla:
```c
#define UI_SCREEN_PHONE  9   // Nueva pantalla teléfono

// En k_screen_rot[]:
// UI_SCREEN_PHONE → landscape (mismo que menú principal)
```

### 4.4 Modificar: `ui.cpp`

- Agregar `UI_SCREEN_PHONE` al array de rotaciones
- Agregar `screen_phone_get()` al switch de `ui_navigate_to()`

### 4.5 Nuevo: `screen_phone.h`

```c
#pragma once
#include "lvgl.h"
#include "maps_ws_server.h"

lv_obj_t *screen_phone_get(void);
void screen_phone_start(void);
void screen_phone_update_gmaps(const gmaps_step_t *step);
void screen_phone_update_media(const media_state_t *media);
void screen_phone_add_notif(const notif_t *notif);
```

### 4.6 Nuevo: `screen_phone.cpp`

Estructura de la UI con LVGL:

```c
// Layout principal (480×320)
// ┌─ header_bar (480×28): título + indicador conexión
// ├─ nav_section (480×76): ícono maniobra + paso + dist/eta
// ├─ media_section (480×64): info canción | botones control
// └─ notif_section (480×114): lista 3 notificaciones
```

**Iconos de maniobra**: mapear string `maneuver` a símbolo Unicode de LVGL built-in o imágenes pequeñas embebidas en flash (30×30px).

Maneuvers comunes de Google Maps:
| `maneuver` | Símbolo |
|---|---|
| `turn-right` | ↱ |
| `turn-left` | ↰ |
| `straight` | ↑ |
| `turn-sharp-right` | ↪ |
| `turn-sharp-left` | ↩ |
| `roundabout-right` | ↻ |
| `uturn-right` / `uturn-left` | ↺ |
| *(default)* | ↑ |

**Botones de media**: al presionar, llaman a `maps_ws_send_media_cmd()`.

---

## 5. Acceso a Google Maps por notificación

Google Maps publica sus indicaciones de navegación como notificación del sistema. El `NotificationListenerService` puede leerla sin acceso especial a Google Maps.

Formato típico de la notificación:
- **Título**: `"Girar a la derecha en 200 m"` o similar
- **Texto**: `"Av. Corrientes · 15 min"`
- **Acciones**: ninguna relevante

El parseo en Android extrae:
1. Del título: el texto del paso (hasta el "en X m/km")
2. Del texto: nombre de calle + ETA
3. El `maneuver` se infiere del texto del paso (contiene "derecha", "izquierda", "recto", etc.)

---

## 6. Menú principal — agregar acceso

En `screen_main_menu.cpp`, agregar ítem "TELÉFONO" al menú con ícono de teléfono/campana que navega a `UI_SCREEN_PHONE`.

---

## 7. Orden de implementación recomendado

1. **[Android]** `PhoneNotificationService.kt` — base del servicio, solo leer y loguear
2. **[Android]** Integrar envío de `notif` y `gmaps` en `Esp32Client.kt`
3. **[ESP32]** Estructuras nuevas en `maps_ws_server.h` + parseo en `.cpp`
4. **[ESP32]** `screen_phone.cpp` esqueleto — pantalla estática de prueba
5. **[ESP32]** Conectar callbacks: notificaciones aparecen en pantalla
6. **[Android]** `MediaSessionManager.kt` — metadata y estado
7. **[ESP32]** Sección de música + botones de control
8. **[Android]** Recibir `media_cmd` del ESP32 y ejecutar controles
9. **[ESP32]** Íconos de maniobra en sección Google Maps
10. **[Android/ESP32]** Agregar ítem al menú principal + pruebas end-to-end

---

## 8. Archivos a crear / modificar

| Archivo | Acción |
|---|---|
| `android/.../PhoneNotificationService.kt` | **Crear** |
| `android/.../MediaSessionManager.kt` | **Crear** |
| `android/.../network/Esp32Client.kt` | Modificar: recibir `media_cmd` |
| `android/.../MainViewModel.kt` | Modificar: iniciar servicio, verificar permisos |
| `android/.../AndroidManifest.xml` | Modificar: declarar servicio |
| `src/ui/screen_phone.h` | **Crear** |
| `src/ui/screen_phone.cpp` | **Crear** |
| `src/ui/ui.h` | Modificar: agregar `UI_SCREEN_PHONE` |
| `src/ui/ui.cpp` | Modificar: routing a nueva pantalla |
| `include/maps_ws_server.h` | Modificar: nuevas structs + callbacks |
| `src/maps_ws_server.cpp` | Modificar: parseo de 3 nuevos tipos |
| `src/ui/screen_main_menu.cpp` | Modificar: agregar ítem "Teléfono" |

---

## 9. Notas y consideraciones

- **Frecuencia de envío**: notificaciones se envían on-demand (event-driven); media state cada 2s o en cambio; gmaps on-demand desde la notificación.
- **Buffer de notificaciones**: el ESP32 guarda las últimas 3 en un ring buffer de `notif_t[NOTIF_QUEUE_MAX]`.
- **Desconexión**: si el WebSocket cae, la pantalla muestra "Sin conexión" en el header.
- **Pantalla bloqueada en Android**: `NotificationListenerService` funciona incluso con pantalla apagada gracias al `WakeLock` ya existente en `NavigationService`.
- **Privacy**: filtrar notificaciones de apps sensibles (banca, contraseñas) si se desea.
