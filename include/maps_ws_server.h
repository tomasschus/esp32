#pragma once

#include <cstdint>

/**
 * Servidor AP "ESP32-NAV" + WebSocket en puerto 8080, path /ws.
 * La app Android conecta al AP, abre ws://192.168.4.1:8080/ws
 * y envía tiles JPEG (320×480 portrait) como mensajes binarios.
 *
 * Uso:
 *   - maps_ws_start() inicia el AP y el servidor (desde pantalla Mapas).
 *   - map_buf debe ser un buffer RGB565 de MAP_W*MAP_H*2 bytes (en PSRAM).
 *   - Cuando llega un frame JPEG, se decodifica a map_buf y se llama
 * on_frame().
 *   - maps_ws_stop() apaga servidor y AP.
 */

/* Tile portrait: 320×480 — mismo nº de píxeles que landscape (153 600),
   mismo tamaño de buffer. El mapa llena toda la pantalla. */
#define MAPS_WS_MAP_W 320
#define MAPS_WS_MAP_H 480
#define MAPS_WS_MAP_BYTES (MAPS_WS_MAP_W * MAPS_WS_MAP_H * 2)

typedef void (*maps_ws_on_frame_t)(void);

bool maps_ws_start(uint16_t *map_buf, maps_ws_on_frame_t on_frame);
void maps_ws_stop(void);
bool maps_ws_is_running(void);
