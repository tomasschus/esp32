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

#define VEC_MAX_LABELS   20
#define VEC_LABEL_LEN    20   /* máx. chars del nombre (sin null) */

struct vec_label_t {
    int16_t x, y;
    char    name[VEC_LABEL_LEN + 1];
};

struct vec_frame_t {
    vec_road_t  roads[VEC_MAX_ROAD_SEGS];
    uint8_t     n_roads;
    vec_point_t route[VEC_MAX_ROUTE_PTS];
    uint16_t    n_route;
    vec_label_t labels[VEC_MAX_LABELS];
    uint8_t     n_labels;
    int16_t     pos_x, pos_y;
    int16_t     heading;   /* -1 si no disponible */
};

struct nav_step_t {
    char step[128];
    char dist[20];
    char eta[20];
};

/* ── Tipos de la pantalla de teléfono ───────────────────────────── */

struct gmaps_step_t {
    char step[64];      /* instrucción de maniobra */
    char street[48];    /* nombre de calle actual */
    char dist[16];      /* distancia al próximo paso */
    char eta[16];       /* tiempo estimado restante */
    char maneuver[32];  /* "turn-right", "turn-left", "straight", etc. */
};

struct media_state_t {
    char    app[24];    /* nombre de la app (YouTube Music, Spotify…) */
    char    title[48];  /* título de la canción */
    char    artist[32]; /* artista */
    bool    playing;    /* true = reproduciendo */
    uint8_t vol;        /* 0-100 */
};

struct phone_notif_t {
    char app[24];    /* nombre de la app */
    char title[32];  /* remitente / asunto */
    char text[64];   /* primeras palabras del mensaje */
};

/* ── Callbacks ───────────────────────────────────────────────────── */
typedef void (*maps_ws_on_frame_t)(void);                        /* JPEG legacy */
typedef void (*maps_ws_on_vec_t)(const vec_frame_t &frame);      /* frame vectorial */
typedef void (*maps_ws_on_nav_t)(const nav_step_t &step);        /* paso de nav interno */
typedef void (*maps_ws_on_gps_t)(int speed_kmh);                 /* velocidad GPS */
typedef void (*maps_ws_on_gmaps_t)(const gmaps_step_t &step);    /* paso Google Maps */
typedef void (*maps_ws_on_media_t)(const media_state_t &media);  /* estado de media */
typedef void (*maps_ws_on_notif_t)(const phone_notif_t &notif);  /* notificación */

/* ── API ─────────────────────────────────────────────────────────── */
bool maps_ws_init(void);               /* arranca AP + WS server (sin buffer de mapa) */
bool maps_ws_start(uint16_t          *map_buf,
                   maps_ws_on_frame_t on_frame,
                   maps_ws_on_vec_t   on_vec  = nullptr,
                   maps_ws_on_nav_t   on_nav  = nullptr);
void maps_ws_set_gps_cb(maps_ws_on_gps_t cb);
void maps_ws_set_gmaps_cb(maps_ws_on_gmaps_t cb);
void maps_ws_set_media_cb(maps_ws_on_media_t cb);
void maps_ws_set_notif_cb(maps_ws_on_notif_t cb);
void maps_ws_send_media_cmd(const char *cmd);   /* envía {"t":"media_cmd","cmd":"..."} al Android */
void maps_ws_stop(void);
bool maps_ws_is_running(void);
bool maps_ws_has_client(void);
