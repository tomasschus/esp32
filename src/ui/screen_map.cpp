/*
 * Pantalla Mapas – portrait 320×480.
 *
 * El tile JPEG 320×480 ocupa TODA la pantalla; el botón "Volver"
 * flota en la esquina superior izquierda con fondo semitransparente.
 * Buffer RGB565 en PSRAM; AP + WebSocket arranca al entrar.
 */
#include "screen_map.h"
#include "../dispcfg.h"
#include "maps_ws_server.h"
#include "ui.h"

#include <cstring>
#include <esp_heap_caps.h>
#include <lvgl.h>

#define COLOR_BTN_BG lv_color_hex(0x1A1A2E)
#define COLOR_ACCENT lv_color_hex(0xE94560)
#define COLOR_TEXT lv_color_hex(0xEEEEEE)

static lv_obj_t *scr = nullptr;
static lv_obj_t *img_map = nullptr;
static lv_obj_t *lbl_waiting = nullptr;
static uint16_t *s_map_buf = nullptr;
static lv_image_dsc_t s_map_dsc;
static volatile bool s_map_dirty = false;
static volatile bool s_has_received_frame = false;
static lv_timer_t *s_dirty_timer = nullptr;

static void on_map_frame(void) {
  s_has_received_frame = true;
  s_map_dirty = true;
}

static void dirty_timer_cb(lv_timer_t *t) {
  (void)t;
  if (!s_map_dirty || !img_map)
    return;
  if (s_has_received_frame && lbl_waiting &&
      !lv_obj_has_flag(lbl_waiting, LV_OBJ_FLAG_HIDDEN))
    lv_obj_add_flag(lbl_waiting, LV_OBJ_FLAG_HIDDEN);
  s_map_dirty = false;
  lv_obj_invalidate(img_map);
}

void screen_map_create(void) {
  scr = lv_obj_create(NULL);
  /* Tamaño explícito en portrait para que start() pueda redimensionar con
   * set_size. El tamaño definitivo (portrait 320×480) se aplica en
   * screen_map_start(). */
  lv_obj_set_size(scr, MAPS_WS_MAP_W, MAPS_WS_MAP_H);
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  /* ── Buffer del mapa ────────────────────────────────────────── */
  s_map_buf = (uint16_t *)heap_caps_malloc(MAPS_WS_MAP_BYTES,
                                           MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!s_map_buf)
    s_map_buf = (uint16_t *)heap_caps_malloc(
        MAPS_WS_MAP_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

  if (s_map_buf) {
    memset(s_map_buf, 0x00, MAPS_WS_MAP_BYTES);

    /* Descriptor LVGL: RGB565 320×480 */
    memset(&s_map_dsc, 0, sizeof(s_map_dsc));
    s_map_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    s_map_dsc.header.cf = (uint8_t)LV_COLOR_FORMAT_RGB565;
    s_map_dsc.header.w = MAPS_WS_MAP_W;
    s_map_dsc.header.h = MAPS_WS_MAP_H;
    s_map_dsc.header.stride = MAPS_WS_MAP_W * 2;
    s_map_dsc.data_size = MAPS_WS_MAP_BYTES;
    s_map_dsc.data = (const uint8_t *)s_map_buf;

    /* Imagen fullscreen (CAPA 0) */
    img_map = lv_image_create(scr);
    lv_image_set_src(img_map, &s_map_dsc);
    lv_obj_set_size(img_map, MAPS_WS_MAP_W, MAPS_WS_MAP_H);
    lv_obj_align(img_map, LV_ALIGN_TOP_LEFT, 0, 0);

    /* Mensaje de espera centrado */
    lbl_waiting = lv_label_create(scr);
    lv_label_set_text(lbl_waiting, "Conecta a WiFi ESP32-NAV\n"
                                   "y abre la app para ver el mapa.");
    lv_obj_set_style_text_font(lbl_waiting, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_waiting, lv_color_hex(0x778899), 0);
    lv_obj_set_style_text_align(lbl_waiting, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl_waiting, LV_ALIGN_CENTER, 0, 0);
  } else {
    lv_obj_t *err = lv_label_create(scr);
    lv_label_set_text(err, "Sin memoria para mapa");
    lv_obj_set_style_text_color(err, COLOR_ACCENT, 0);
    lv_obj_center(err);
  }

  /* ── Botón "Volver" flotante – esquina superior izquierda ───── */
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

  /* ── Timer de refresco ─────────────────────────────────────── */
  s_dirty_timer = lv_timer_create(dirty_timer_cb, 100, nullptr);
}

lv_obj_t *screen_map_get(void) { return scr; }

void screen_map_start(void) {
  /*
   * La rotación a portrait ya fue aplicada por ui_navigate_to() ANTES de
   * llamar aquí, usando display_set_rotation(DISP_ROT_PORTRAIT).
   * LVGL ya conoce las dimensiones 320×480; solo hay que asegurarse de que
   * los objetos de esta pantalla tengan el tamaño correcto.
   */
  lv_obj_set_size(scr, MAPS_WS_MAP_W, MAPS_WS_MAP_H); /* 320×480 */
  if (img_map)
    lv_obj_set_size(img_map, MAPS_WS_MAP_W, MAPS_WS_MAP_H);

  s_has_received_frame = false;
  if (lbl_waiting)
    lv_obj_clear_flag(lbl_waiting, LV_OBJ_FLAG_HIDDEN);
  if (s_map_buf)
    maps_ws_start(s_map_buf, on_map_frame);
}

void screen_map_stop(void) {
  /*
   * Detener el servidor WebSocket. La rotación de vuelta a landscape la
   * aplica ui_navigate_to() ANTES de llamar a lv_screen_load(), por lo que
   * no es necesario hacerlo aquí.
   */
  maps_ws_stop();
}
