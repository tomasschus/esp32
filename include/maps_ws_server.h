#pragma once

#include <cstdint>

/**
 * Servidor AP "ESP32-NAV" + WebSocket en puerto 8080, path /ws.
 * Pensado para la app Android: conecta al AP, abre ws://192.168.4.1:8080/ws
 * y envía tiles JPEG (480×320) como mensajes binarios.
 *
 * Uso:
 *   - maps_ws_start() inicia el AP y el servidor (desde pantalla Mapas).
 *   - map_buf debe ser un buffer RGB565 de MAP_W*MAP_H*2 bytes (ej. en PSRAM).
 *   - Cuando llega un frame JPEG, se decodifica a map_buf y se llama
 * on_frame().
 *   - maps_ws_stop() apaga servidor y opcionalmente el AP.
 */

#define MAPS_WS_MAP_W 480
#define MAPS_WS_MAP_H 320
#define MAPS_WS_MAP_BYTES (MAPS_WS_MAP_W * MAPS_WS_MAP_H * 2)

/** Buffer RGB565 (MAP_W * MAP_H * 2). Debe vivir en PSRAM o RAM. */
typedef void (*maps_ws_on_frame_t)(void);

/**
 * Inicia AP "ESP32-NAV" y servidor WebSocket en :8080/ws.
 * map_buf: buffer para decodificar el tile (RGB565, MAPS_WS_MAP_BYTES).
 * on_frame: llamado cada vez que se decodifica un tile nuevo (para refrescar
 * UI). Devuelve true si OK.
 */
bool maps_ws_start(uint16_t *map_buf, maps_ws_on_frame_t on_frame);

/** Detiene servidor y desconecta el AP. */
void maps_ws_stop(void);

/** true si el servidor está activo. */
bool maps_ws_is_running(void);
