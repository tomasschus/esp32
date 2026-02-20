#include "screen_tools.h"
#include "ui.h"

static lv_obj_t *scr = nullptr;

void screen_tools_create() {
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
  lv_label_set_text(title, LV_SYMBOL_SETTINGS "  Herramientas");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(0xEEEEEE), 0);
  lv_obj_center(title);

  /* ── Contenido ────────────────────────────────────────────── */
  lv_obj_t *content = lv_obj_create(scr);
  lv_obj_set_size(content, 480, 261);
  lv_obj_align(content, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(content, 0, 0);
  lv_obj_set_style_pad_all(content, 16, 0);
  /* keep scrollable so a 3rd card fits */

  /* ── Tarjeta: Mapas (primera) ─────────────────────────────── */
  lv_obj_t *card = lv_obj_create(content);
  lv_obj_set_size(card, 448, 72);
  lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_color(card, lv_color_hex(0x0F2040), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(card, lv_color_hex(0x1A3A6A), 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_radius(card, 10, 0);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(
      card, [](lv_event_t *) { ui_navigate_to(UI_SCREEN_MAPS); },
      LV_EVENT_CLICKED, nullptr);

  lv_obj_t *ico = lv_label_create(card);
  lv_label_set_text(ico, LV_SYMBOL_GPS);
  lv_obj_set_style_text_font(ico, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(ico, lv_color_hex(0x4DA6FF), 0);
  lv_obj_align(ico, LV_ALIGN_LEFT_MID, 14, 0);

  lv_obj_t *lbl_name = lv_label_create(card);
  lv_label_set_text(lbl_name, "Mapas");
  lv_obj_set_style_text_font(lbl_name, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_name, lv_color_hex(0xEEEEEE), 0);
  lv_obj_align(lbl_name, LV_ALIGN_LEFT_MID, 58, -10);

  lv_obj_t *lbl_desc = lv_label_create(card);
  lv_label_set_text(lbl_desc, "AP ESP32-NAV + tile desde app Android");
  lv_obj_set_style_text_font(lbl_desc, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lbl_desc, lv_color_hex(0x778899), 0);
  lv_obj_align(lbl_desc, LV_ALIGN_LEFT_MID, 58, 12);

  lv_obj_t *arr = lv_label_create(card);
  lv_label_set_text(arr, LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_color(arr, lv_color_hex(0x4DA6FF), 0);
  lv_obj_align(arr, LV_ALIGN_RIGHT_MID, -12, 0);

  /* ── Tarjeta: WiFi Analyzer ───────────────────────────────── */
  lv_obj_t *card2 = lv_obj_create(content);
  lv_obj_set_size(card2, 448, 72);
  lv_obj_align(card2, LV_ALIGN_TOP_MID, 0, 84);
  lv_obj_set_style_bg_color(card2, lv_color_hex(0x0F2040), 0);
  lv_obj_set_style_bg_opa(card2, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(card2, lv_color_hex(0x1A3A6A), 0);
  lv_obj_set_style_border_width(card2, 1, 0);
  lv_obj_set_style_radius(card2, 10, 0);
  lv_obj_clear_flag(card2, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(card2, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(
      card2, [](lv_event_t *) { ui_navigate_to(UI_SCREEN_WIFI_ANALYZER); },
      LV_EVENT_CLICKED, nullptr);

  lv_obj_t *ico2 = lv_label_create(card2);
  lv_label_set_text(ico2, LV_SYMBOL_WIFI);
  lv_obj_set_style_text_font(ico2, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(ico2, lv_color_hex(0x4DA6FF), 0);
  lv_obj_align(ico2, LV_ALIGN_LEFT_MID, 14, 0);

  lv_obj_t *lbl2_name = lv_label_create(card2);
  lv_label_set_text(lbl2_name, "WiFi Analyzer");
  lv_obj_set_style_text_font(lbl2_name, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl2_name, lv_color_hex(0xEEEEEE), 0);
  lv_obj_align(lbl2_name, LV_ALIGN_LEFT_MID, 58, -10);

  lv_obj_t *lbl2_desc = lv_label_create(card2);
  lv_label_set_text(lbl2_desc, "Redes disponibles, canal y senal");
  lv_obj_set_style_text_font(lbl2_desc, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lbl2_desc, lv_color_hex(0x778899), 0);
  lv_obj_align(lbl2_desc, LV_ALIGN_LEFT_MID, 58, 12);

  lv_obj_t *arr2 = lv_label_create(card2);
  lv_label_set_text(arr2, LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_color(arr2, lv_color_hex(0x4DA6FF), 0);
  lv_obj_align(arr2, LV_ALIGN_RIGHT_MID, -12, 0);

  /* ── Tarjeta: Cronómetro / Temporizador ───────────────── */
  lv_obj_t *card3 = lv_obj_create(content);
  lv_obj_set_size(card3, 448, 72);
  lv_obj_align(card3, LV_ALIGN_TOP_MID, 0, 168);
  lv_obj_set_style_bg_color(card3, lv_color_hex(0x0F2040), 0);
  lv_obj_set_style_bg_opa(card3, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(card3, lv_color_hex(0x1A3A6A), 0);
  lv_obj_set_style_border_width(card3, 1, 0);
  lv_obj_set_style_radius(card3, 10, 0);
  lv_obj_clear_flag(card3, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(card3, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(
      card3, [](lv_event_t *) { ui_navigate_to(UI_SCREEN_TIMER); },
      LV_EVENT_CLICKED, nullptr);

  lv_obj_t *ico3 = lv_label_create(card3);
  lv_label_set_text(ico3, LV_SYMBOL_LOOP);
  lv_obj_set_style_text_font(ico3, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(ico3, lv_color_hex(0x4DA6FF), 0);
  lv_obj_align(ico3, LV_ALIGN_LEFT_MID, 14, 0);

  lv_obj_t *lbl3_name = lv_label_create(card3);
  lv_label_set_text(lbl3_name, "Cronometro / Temporizador");
  lv_obj_set_style_text_font(lbl3_name, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl3_name, lv_color_hex(0xEEEEEE), 0);
  lv_obj_align(lbl3_name, LV_ALIGN_LEFT_MID, 58, -10);

  lv_obj_t *lbl3_desc = lv_label_create(card3);
  lv_label_set_text(lbl3_desc, "Cronometro con vueltas y cuenta regresiva");
  lv_obj_set_style_text_font(lbl3_desc, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lbl3_desc, lv_color_hex(0x778899), 0);
  lv_obj_align(lbl3_desc, LV_ALIGN_LEFT_MID, 58, 12);

  lv_obj_t *arr3 = lv_label_create(card3);
  lv_label_set_text(arr3, LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_color(arr3, lv_color_hex(0x4DA6FF), 0);
  lv_obj_align(arr3, LV_ALIGN_RIGHT_MID, -12, 0);

  /* ── Tarjeta: Reproductor de Música ───────────────────── */
  lv_obj_t *card4 = lv_obj_create(content);
  lv_obj_set_size(card4, 448, 72);
  lv_obj_align(card4, LV_ALIGN_TOP_MID, 0, 252);
  lv_obj_set_style_bg_color(card4, lv_color_hex(0x2A1A0E), 0);
  lv_obj_set_style_bg_opa(card4, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(card4, lv_color_hex(0x4A2A1A), 0);
  lv_obj_set_style_border_width(card4, 1, 0);
  lv_obj_set_style_radius(card4, 10, 0);
  lv_obj_clear_flag(card4, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(card4, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(
      card4, [](lv_event_t *) { ui_navigate_to(UI_SCREEN_PLAYER); },
      LV_EVENT_CLICKED, nullptr);

  lv_obj_t *ico4 = lv_label_create(card4);
  lv_label_set_text(ico4, LV_SYMBOL_AUDIO);
  lv_obj_set_style_text_font(ico4, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(ico4, lv_color_hex(0xFFAA44), 0);
  lv_obj_align(ico4, LV_ALIGN_LEFT_MID, 14, 0);

  lv_obj_t *lbl4_name = lv_label_create(card4);
  lv_label_set_text(lbl4_name, "Reproductor de Musica");
  lv_obj_set_style_text_font(lbl4_name, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl4_name, lv_color_hex(0xEEEEEE), 0);
  lv_obj_align(lbl4_name, LV_ALIGN_LEFT_MID, 58, -10);

  lv_obj_t *lbl4_desc = lv_label_create(card4);
  lv_label_set_text(lbl4_desc, "MP3 / WAV / AAC desde tarjeta SD");
  lv_obj_set_style_text_font(lbl4_desc, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lbl4_desc, lv_color_hex(0x778899), 0);
  lv_obj_align(lbl4_desc, LV_ALIGN_LEFT_MID, 58, 12);

  lv_obj_t *arr4 = lv_label_create(card4);
  lv_label_set_text(arr4, LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_color(arr4, lv_color_hex(0xFFAA44), 0);
  lv_obj_align(arr4, LV_ALIGN_RIGHT_MID, -12, 0);
}

lv_obj_t *screen_tools_get() { return scr; }
