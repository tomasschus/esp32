#pragma once

#include <stdint.h>

/**
 * Servidor AP "ESP32-NAV" + WebSocket en puerto 8080, path /ws.
 *
 * Protocolo v2 (vectorial):
 *   Texto {"t":"vec",...} → frame vectorial con calles + ruta + posición
 *   Texto {"t":"nav",...} → paso de navegación
 *   Binario              → tile JPEG legacy (sigue funcionando)
 */

/* Dimensiones de pantalla portrait */
#define MAPS_WS_MAP_W 320
#define MAPS_WS_MAP_H 480
#define MAPS_WS_MAP_BYTES (MAPS_WS_MAP_W * MAPS_WS_MAP_H * 2)

/* ── Tipos de datos del frame vectorial ─────────────────────────── */

#define VEC_MAX_ROAD_SEGS   80
#define VEC_MAX_PTS_PER_SEG 30
#define VEC_MAX_ROUTE_PTS  100

struct vec_point_t { int16_t x, y; };

struct vec_road_t {
    vec_point_t pts[VEC_MAX_PTS_PER_SEG];
    uint8_t n;   /* número de puntos */
    uint8_t w;   /* grosor: 1=menor, 2=secundaria, 3=autopista */
};

struct vec_frame_t {
    vec_road_t  roads[VEC_MAX_ROAD_SEGS];
    uint8_t     n_roads;
    vec_point_t route[VEC_MAX_ROUTE_PTS];
    uint16_t    n_route;
    int16_t     pos_x, pos_y;
    int16_t     heading;   /* -1 si no disponible */
};

struct nav_step_t {
    char step[128];
    char dist[20];
    char eta[20];
};

/* ── Callbacks ───────────────────────────────────────────────────── */
typedef void (*maps_ws_on_frame_t)(void);                   /* JPEG legacy */
typedef void (*maps_ws_on_vec_t)(const vec_frame_t &frame); /* frame vectorial */
typedef void (*maps_ws_on_nav_t)(const nav_step_t &step);   /* paso de nav */

/* ── API ─────────────────────────────────────────────────────────── */
bool maps_ws_start(uint16_t          *map_buf,
                   maps_ws_on_frame_t on_frame,
                   maps_ws_on_vec_t   on_vec  = nullptr,
                   maps_ws_on_nav_t   on_nav  = nullptr);
void maps_ws_stop(void);
bool maps_ws_is_running(void);
