/*
 * Pantalla Mapas – portrait 320×480.
 *
 * Renderiza un frame vectorial recibido por WebSocket:
 *   - Fondo oscuro (#1C1C2E)
 *   - Calles grises (grosor 1-3 px según tipo)
 *   - Ruta azul (#4488FF, grosor 3 px)
 *   - Marcador de posición: círculo blanco + punto azul
 *   - Label de navegación en la parte inferior
 *
 * El canvas comparte el mismo buffer RGB565 en PSRAM que antes.
 * El botón "Volver" flota en la esquina superior izquierda.
 */
#include "screen_map.h"
#include "../dispcfg.h"
#include "maps_ws_server.h"
#include "ui.h"

#include <cstring>
#include <esp_heap_caps.h>
#include <lvgl.h>

#define COLOR_BG       lv_color_hex(0x1C1C2E)
#define COLOR_ROAD_1   lv_color_hex(0x3A3A5A)   /* calle menor */
#define COLOR_ROAD_2   lv_color_hex(0x555580)   /* calle secundaria */
#define COLOR_ROAD_3   lv_color_hex(0x7777AA)   /* autopista */
#define COLOR_ROUTE    lv_color_hex(0x4488FF)   /* ruta activa */
#define COLOR_POS_OUT  lv_color_hex(0xFFFFFF)   /* borde del marcador */
#define COLOR_POS_IN   lv_color_hex(0x4488FF)   /* centro del marcador */
#define COLOR_BTN_BG   lv_color_hex(0x1A1A2E)
#define COLOR_ACCENT   lv_color_hex(0xE94560)
#define COLOR_TEXT     lv_color_hex(0xEEEEEE)
#define COLOR_NAV_BG   lv_color_hex(0x12122A)

static lv_obj_t  *scr         = nullptr;
static lv_obj_t  *canvas      = nullptr;
static lv_obj_t  *lbl_waiting = nullptr;
static lv_obj_t  *lbl_nav     = nullptr;  /* instrucción actual */
static lv_obj_t  *lbl_dist    = nullptr;  /* distancia al próximo giro */
static uint16_t  *s_map_buf   = nullptr;

static volatile bool     s_vec_dirty = false;
static volatile bool     s_nav_dirty = false;
static volatile bool     s_has_received_frame = false;
static lv_timer_t       *s_dirty_timer = nullptr;

/* Copias seguras para acceso desde el timer (hilo LVGL) – en PSRAM */
static vec_frame_t *s_pending_vec = nullptr;
static nav_step_t   s_pending_nav;

/* ── Callbacks del WebSocket (ISR context) ───────────────────────── */
static void on_map_frame(void) {
  /* JPEG legacy: no hacemos nada en modo vectorial */
}

static void on_vec_frame(const vec_frame_t &f) {
  if (!s_pending_vec) return;
  memcpy(s_pending_vec, &f, sizeof(vec_frame_t));
  s_has_received_frame = true;
  s_vec_dirty = true;
}

static void on_nav_step(const nav_step_t &n) {
  memcpy(&s_pending_nav, &n, sizeof(nav_step_t));
  s_nav_dirty = true;
}

/* ── Dibujo del frame vectorial sobre el canvas ──────────────────── */
static void render_vec_frame(const vec_frame_t &f) {
  if (!canvas) return;

  lv_canvas_fill_bg(canvas, COLOR_BG, LV_OPA_COVER);

  lv_layer_t layer;
  lv_canvas_init_layer(canvas, &layer);

  /* Calles */
  lv_draw_line_dsc_t road_dsc;
  lv_draw_line_dsc_init(&road_dsc);
  road_dsc.opa = LV_OPA_COVER;

  /* En LVGL 9.2, p1/p2 son campos del descriptor (no argumentos separados) */
  for (uint8_t i = 0; i < f.n_roads; i++) {
    const vec_road_t &r = f.roads[i];
    switch (r.w) {
      case 3:  road_dsc.color = COLOR_ROAD_3; road_dsc.width = 6; break;
      case 2:  road_dsc.color = COLOR_ROAD_2; road_dsc.width = 4; break;
      default: road_dsc.color = COLOR_ROAD_1; road_dsc.width = 2; break;
    }
    for (uint8_t j = 0; j + 1 < r.n; j++) {
      road_dsc.p1.x = r.pts[j].x;
      road_dsc.p1.y = r.pts[j].y;
      road_dsc.p2.x = r.pts[j + 1].x;
      road_dsc.p2.y = r.pts[j + 1].y;
      lv_draw_line(&layer, &road_dsc);
    }
  }

  /* Ruta */
  if (f.n_route >= 2) {
    lv_draw_line_dsc_t rte_dsc;
    lv_draw_line_dsc_init(&rte_dsc);
    rte_dsc.color       = COLOR_ROUTE;
    rte_dsc.width       = 3;
    rte_dsc.opa         = LV_OPA_COVER;
    rte_dsc.round_start = 1;
    rte_dsc.round_end   = 1;
    for (uint16_t j = 0; j + 1 < f.n_route; j++) {
      rte_dsc.p1.x = f.route[j].x;
      rte_dsc.p1.y = f.route[j].y;
      rte_dsc.p2.x = f.route[j + 1].x;
      rte_dsc.p2.y = f.route[j + 1].y;
      lv_draw_line(&layer, &rte_dsc);
    }
  }

  /* Marcador de posición: círculo blanco (radio 8) + punto azul (radio 5) */
  lv_draw_arc_dsc_t arc;
  lv_draw_arc_dsc_init(&arc);
  arc.center.x   = f.pos_x;
  arc.center.y   = f.pos_y;
  arc.start_angle = 0;
  arc.end_angle   = 360;
  arc.opa         = LV_OPA_COVER;

  arc.color  = COLOR_POS_OUT;
  arc.radius = 8;
  arc.width  = 8;
  lv_draw_arc(&layer, &arc);

  arc.color  = COLOR_POS_IN;
  arc.radius = 5;
  arc.width  = 5;
  lv_draw_arc(&layer, &arc);

  lv_canvas_finish_layer(canvas, &layer);
}

/* ── Timer de refresco (hilo LVGL, 100 ms) ───────────────────────── */
static void dirty_timer_cb(lv_timer_t *t) {
  (void)t;

  /* Ocultar label de espera cuando llega el primer frame */
  if (s_has_received_frame && lbl_waiting &&
      !lv_obj_has_flag(lbl_waiting, LV_OBJ_FLAG_HIDDEN)) {
    lv_obj_add_flag(lbl_waiting, LV_OBJ_FLAG_HIDDEN);
  }

  /* Renderizar frame vectorial */
  if (s_vec_dirty && s_pending_vec) {
    s_vec_dirty = false;
    render_vec_frame(*s_pending_vec);
    lv_obj_invalidate(canvas);
  }

  /* Actualizar label de navegación */
  if (s_nav_dirty && lbl_nav && lbl_dist) {
    s_nav_dirty = false;
    lv_label_set_text(lbl_nav,  s_pending_nav.step);
    lv_label_set_text(lbl_dist, s_pending_nav.dist);
    lv_obj_clear_flag(lv_obj_get_parent(lbl_nav), LV_OBJ_FLAG_HIDDEN);
  }
}

/* ── screen_map_create ───────────────────────────────────────────── */
void screen_map_create(void) {
  if (!s_pending_vec)
    s_pending_vec = (vec_frame_t *)heap_caps_malloc(
        sizeof(vec_frame_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  scr = lv_obj_create(NULL);
  lv_obj_set_size(scr, MAPS_WS_MAP_W, MAPS_WS_MAP_H);
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  /* ── Buffer del mapa (PSRAM preferida) ──────────────────────── */
  s_map_buf = (uint16_t *)heap_caps_malloc(MAPS_WS_MAP_BYTES,
                                           MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!s_map_buf)
    s_map_buf = (uint16_t *)heap_caps_malloc(
        MAPS_WS_MAP_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

  if (s_map_buf) {
    memset(s_map_buf, 0x00, MAPS_WS_MAP_BYTES);

    /* Canvas fullscreen que usa el mismo buffer RGB565 */
    canvas = lv_canvas_create(scr);
    lv_canvas_set_buffer(canvas, s_map_buf,
                         MAPS_WS_MAP_W, MAPS_WS_MAP_H,
                         LV_COLOR_FORMAT_RGB565);
    lv_obj_set_size(canvas, MAPS_WS_MAP_W, MAPS_WS_MAP_H);
    lv_obj_align(canvas, LV_ALIGN_TOP_LEFT, 0, 0);

    /* Fondo inicial */
    lv_canvas_fill_bg(canvas, COLOR_BG, LV_OPA_COVER);

    /* Label de espera */
    lbl_waiting = lv_label_create(scr);
    lv_label_set_text(lbl_waiting,
                      "Conecta a WiFi ESP32-NAV\n"
                      "y abre la app para ver el mapa.");
    lv_obj_set_style_text_font(lbl_waiting, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_waiting, lv_color_hex(0x778899), 0);
    lv_obj_set_style_text_align(lbl_waiting, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl_waiting, LV_ALIGN_CENTER, 0, 0);

    /* ── Panel de navegación (parte inferior) ─────────────────── */
    lv_obj_t *nav_panel = lv_obj_create(scr);
    lv_obj_set_size(nav_panel, MAPS_WS_MAP_W, 70);
    lv_obj_align(nav_panel, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(nav_panel, COLOR_NAV_BG, 0);
    lv_obj_set_style_bg_opa(nav_panel, LV_OPA_90, 0);
    lv_obj_set_style_border_width(nav_panel, 0, 0);
    lv_obj_set_style_radius(nav_panel, 0, 0);
    lv_obj_set_style_pad_all(nav_panel, 6, 0);
    lv_obj_clear_flag(nav_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(nav_panel, LV_OBJ_FLAG_HIDDEN);  /* oculto hasta que haya nav */

    lbl_nav = lv_label_create(nav_panel);
    lv_label_set_text(lbl_nav, "");
    lv_label_set_long_mode(lbl_nav, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl_nav, MAPS_WS_MAP_W - 12);
    lv_obj_set_style_text_font(lbl_nav, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_nav, COLOR_TEXT, 0);
    lv_obj_align(lbl_nav, LV_ALIGN_TOP_LEFT, 0, 0);

    lbl_dist = lv_label_create(nav_panel);
    lv_label_set_text(lbl_dist, "");
    lv_obj_set_style_text_font(lbl_dist, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_dist, COLOR_ACCENT, 0);
    lv_obj_align(lbl_dist, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  } else {
    lv_obj_t *err = lv_label_create(scr);
    lv_label_set_text(err, "Sin memoria para mapa");
    lv_obj_set_style_text_color(err, COLOR_ACCENT, 0);
    lv_obj_center(err);
  }

  /* ── Botón "Volver" flotante ─────────────────────────────────── */
  lv_obj_t *btn_back = lv_button_create(scr);
  lv_obj_set_size(btn_back, 88, 38);
  lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 10, 10);
  lv_obj_set_style_bg_color(btn_back, COLOR_BTN_BG, 0);
  lv_obj_set_style_bg_opa(btn_back, LV_OPA_80, 0);
  lv_obj_set_style_border_color(btn_back, COLOR_ACCENT, 0);
  lv_obj_set_style_border_width(btn_back, 1, 0);
  lv_obj_set_style_radius(btn_back, 10, 0);
  lv_obj_add_event_cb(
      btn_back,
      [](lv_event_t *) {
        screen_map_stop();
        ui_navigate_to(UI_SCREEN_MAIN_MENU);
      },
      LV_EVENT_CLICKED, nullptr);

  lv_obj_t *lbl_back = lv_label_create(btn_back);
  lv_label_set_text(lbl_back, LV_SYMBOL_LEFT " Volver");
  lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lbl_back, COLOR_TEXT, 0);
  lv_obj_center(lbl_back);

  /* ── Timer de refresco ──────────────────────────────────────── */
  s_dirty_timer = lv_timer_create(dirty_timer_cb, 100, nullptr);
}

lv_obj_t *screen_map_get(void) { return scr; }

void screen_map_start(void) {
  lv_obj_set_size(scr, MAPS_WS_MAP_W, MAPS_WS_MAP_H);
  if (canvas)
    lv_obj_set_size(canvas, MAPS_WS_MAP_W, MAPS_WS_MAP_H);

  s_has_received_frame = false;
  s_vec_dirty          = false;
  s_nav_dirty          = false;
  if (lbl_waiting)
    lv_obj_clear_flag(lbl_waiting, LV_OBJ_FLAG_HIDDEN);
  if (s_map_buf)
    maps_ws_start(s_map_buf, on_map_frame, on_vec_frame, on_nav_step);
}

void screen_map_stop(void) {
  maps_ws_stop();
}
