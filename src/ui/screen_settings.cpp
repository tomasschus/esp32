#include "screen_settings.h"
#include "ui.h"

static lv_obj_t *scr = nullptr;

void screen_settings_create() {
  scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  /* ── Header ─────────────────────────────────────────────── */
  lv_obj_t *header = lv_obj_create(scr);
  lv_obj_set_size(header, 480, 56);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_color(header, lv_color_hex(0x0A0A0A), 0);
  lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *accent = lv_obj_create(scr);
  lv_obj_set_size(accent, 480, 3);
  lv_obj_align(accent, LV_ALIGN_TOP_MID, 0, 56);
  lv_obj_set_style_bg_color(accent, lv_color_hex(0xE94560), 0);
  lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(accent, 0, 0);
  lv_obj_set_style_radius(accent, 0, 0);

  /* Botón Volver */
  lv_obj_t *btn_back = lv_button_create(header);
  lv_obj_set_size(btn_back, 80, 36);
  lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 8, 0);
  lv_obj_set_style_bg_color(btn_back, lv_color_hex(0xE94560), 0);
  lv_obj_set_style_radius(btn_back, 8, 0);
  lv_obj_add_event_cb(
      btn_back, [](lv_event_t *) { ui_navigate_to(UI_SCREEN_MAIN_MENU); },
      LV_EVENT_CLICKED, nullptr);

  lv_obj_t *lbl_back = lv_label_create(btn_back);
  lv_label_set_text(lbl_back, LV_SYMBOL_LEFT " Volver");
  lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl_back);

  /* Título */
  lv_obj_t *title = lv_label_create(header);
  lv_label_set_text(title, LV_SYMBOL_LIST "  Configuracion");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(0xEEEEEE), 0);
  lv_obj_center(title);

  /* ── Contenido ───────────────────────────────────────────── */
  lv_obj_t *btn_wifi = lv_button_create(scr);
  lv_obj_set_size(btn_wifi, 200, 64);
  lv_obj_align(btn_wifi, LV_ALIGN_CENTER, 0, 20);
  lv_obj_set_style_bg_color(btn_wifi, lv_color_hex(0x0F3460), 0);
  lv_obj_set_style_bg_opa(btn_wifi, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(btn_wifi, 12, 0);
  lv_obj_set_style_border_width(btn_wifi, 2, 0);
  lv_obj_set_style_border_color(btn_wifi, lv_color_hex(0xE94560), 0);
  lv_obj_add_event_cb(
      btn_wifi, [](lv_event_t *) { ui_navigate_to(UI_SCREEN_WIFI); },
      LV_EVENT_CLICKED, nullptr);

  lv_obj_t *lbl_wifi = lv_label_create(btn_wifi);
  lv_label_set_text(lbl_wifi, LV_SYMBOL_WIFI "  WiFi");
  lv_obj_set_style_text_font(lbl_wifi, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_wifi, lv_color_hex(0xEEEEEE), 0);
  lv_obj_center(lbl_wifi);
}

lv_obj_t *screen_settings_get() { return scr; }
