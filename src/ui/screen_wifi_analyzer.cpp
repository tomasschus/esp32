#include "screen_wifi_analyzer.h"
#include "../wifi_manager.h"
#include "ui.h"

#include <cstdio>

/* ── Paleta ──────────────────────────────────────────────────── */
#define COLOR_BG lv_color_hex(0x000000)
#define COLOR_HEADER lv_color_hex(0x0A0A0A)
#define COLOR_ACCENT lv_color_hex(0xE94560)
#define COLOR_TEXT lv_color_hex(0xEEEEEE)
#define COLOR_DIM lv_color_hex(0x778899)
#define COLOR_CARD lv_color_hex(0x0F2040)
#define COLOR_BORDER lv_color_hex(0x2A2A5A)

/* ── Widgets persistentes ────────────────────────────────────── */
static lv_obj_t *scr = nullptr;
static lv_obj_t *list_cont = nullptr; // contenedor scrollable
static lv_obj_t *lbl_count = nullptr; // "X redes"
static lv_timer_t *poll_timer = nullptr;

/* ── Helpers de señal ────────────────────────────────────────── */
static uint32_t rssi_color_hex(int rssi) {
  if (rssi >= -60)
    return 0x4DA6FF; /* excelente – azul */
  if (rssi >= -70)
    return 0x22AAFF; /* buena     – azul claro */
  if (rssi >= -80)
    return 0xFF8800; /* regular   – naranja */
  return 0xE94560;   /* débil     – rojo */
}

static const char *rssi_quality(int rssi) {
  if (rssi >= -60)
    return "Excelente";
  if (rssi >= -70)
    return "Buena";
  if (rssi >= -80)
    return "Regular";
  return "Debil";
}

static int rssi_pct(int rssi) {
  int p = (rssi + 100) * 100 / 70; // -100 → 0 %, -30 → 100 %
  return p < 0 ? 0 : p > 100 ? 100 : p;
}

/* ── Poblar lista ────────────────────────────────────────────── */
static void populate() {
  lv_obj_clean(list_cont);

  int n = wifi_mgr_get_network_count();

  /* Actualizar contador */
  char buf[32];
  snprintf(buf, sizeof(buf), "%d red%s encontrada%s", n, n == 1 ? "" : "es",
           n == 1 ? "" : "s");
  lv_label_set_text(lbl_count, buf);

  if (n == 0) {
    lv_obj_t *lbl = lv_label_create(list_cont);
    lv_label_set_text(lbl, "Sin redes disponibles");
    lv_obj_set_style_text_color(lbl, COLOR_DIM, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl);
    return;
  }

  for (int i = 0; i < n; i++) {
    int rssi = wifi_mgr_get_rssi(i);
    int ch = wifi_mgr_get_channel(i);
    const char *ssid = wifi_mgr_get_ssid(i);
    const char *enc = wifi_mgr_get_encryption(i);
    uint32_t sigc = rssi_color_hex(rssi);
    int pct = rssi_pct(rssi);

    /* ── Tarjeta ─────────────────────────────────────────── */
    lv_obj_t *card = lv_obj_create(list_cont);
    lv_obj_set_size(card, 454, 54);
    lv_obj_set_style_bg_color(card, COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    /* ── Barra de señal ──────────────────────────────────── */
    lv_obj_t *bar = lv_bar_create(card);
    lv_obj_set_size(bar, 60, 10);
    lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 8, 10);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, pct, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x1A2A4A), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(sigc), LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 3, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 3, LV_PART_INDICATOR);

    /* RSSI numérico bajo la barra */
    char rssi_buf[12];
    snprintf(rssi_buf, sizeof(rssi_buf), "%d dBm", rssi);
    lv_obj_t *lbl_rssi = lv_label_create(card);
    lv_label_set_text(lbl_rssi, rssi_buf);
    lv_obj_set_style_text_font(lbl_rssi, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_rssi, COLOR_DIM, 0);
    lv_obj_align(lbl_rssi, LV_ALIGN_BOTTOM_LEFT, 8, -6);

    /* ── SSID ────────────────────────────────────────────── */
    lv_obj_t *lbl_ssid = lv_label_create(card);
    lv_label_set_text(lbl_ssid, ssid[0] ? ssid : "(oculta)");
    lv_obj_set_style_text_font(lbl_ssid, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_ssid, ssid[0] ? COLOR_TEXT : COLOR_DIM, 0);
    lv_label_set_long_mode(lbl_ssid, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(lbl_ssid, 250);
    lv_obj_align(lbl_ssid, LV_ALIGN_TOP_LEFT, 76, 6);

    /* Calidad coloreada (debajo del SSID) */
    lv_obj_t *lbl_q = lv_label_create(card);
    lv_label_set_text(lbl_q, rssi_quality(rssi));
    lv_obj_set_style_text_font(lbl_q, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_q, lv_color_hex(sigc), 0);
    lv_obj_align(lbl_q, LV_ALIGN_BOTTOM_LEFT, 76, -6);

    /* Encriptación */
    lv_obj_t *lbl_enc = lv_label_create(card);
    lv_label_set_text(lbl_enc, enc);
    lv_obj_set_style_text_font(lbl_enc, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_enc, COLOR_DIM, 0);
    lv_obj_align(lbl_enc, LV_ALIGN_BOTTOM_LEFT, 168, -6);

    /* ── Canal (columna derecha) ─────────────────────────── */
    char ch_buf[8];
    snprintf(ch_buf, sizeof(ch_buf), "CH %d", ch);
    lv_obj_t *lbl_ch = lv_label_create(card);
    lv_label_set_text(lbl_ch, ch_buf);
    lv_obj_set_style_text_font(lbl_ch, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_ch, COLOR_DIM, 0);
    lv_obj_align(lbl_ch, LV_ALIGN_TOP_RIGHT, -10, 8);

    /* Número de señal % (columna derecha, abajo) */
    char pct_buf[8];
    snprintf(pct_buf, sizeof(pct_buf), "%d%%", pct);
    lv_obj_t *lbl_pct = lv_label_create(card);
    lv_label_set_text(lbl_pct, pct_buf);
    lv_obj_set_style_text_font(lbl_pct, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_pct, lv_color_hex(sigc), 0);
    lv_obj_align(lbl_pct, LV_ALIGN_BOTTOM_RIGHT, -10, -6);
  }
}

/* ── Timer de polling ────────────────────────────────────────── */
static void poll_cb(lv_timer_t *) {
  if (wifi_mgr_get_state() == WIFI_MGR_SCAN_DONE) {
    populate();
    lv_timer_pause(poll_timer);
  }
}

/* ── Crear pantalla ──────────────────────────────────────────── */
void screen_wifi_analyzer_create() {
  scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr, COLOR_BG, 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  /* ── Header ─────────────────────────────────────────────── */
  lv_obj_t *header = lv_obj_create(scr);
  lv_obj_set_size(header, 480, 56);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_color(header, COLOR_HEADER, 0);
  lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *accent = lv_obj_create(scr);
  lv_obj_set_size(accent, 480, 3);
  lv_obj_align(accent, LV_ALIGN_TOP_MID, 0, 56);
  lv_obj_set_style_bg_color(accent, COLOR_ACCENT, 0);
  lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(accent, 0, 0);
  lv_obj_set_style_radius(accent, 0, 0);

  /* Botón Volver */
  lv_obj_t *btn_back = lv_button_create(header);
  lv_obj_set_size(btn_back, 80, 36);
  lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 8, 0);
  lv_obj_set_style_bg_color(btn_back, COLOR_ACCENT, 0);
  lv_obj_set_style_radius(btn_back, 8, 0);
  lv_obj_add_event_cb(
      btn_back, [](lv_event_t *) { ui_navigate_to(UI_SCREEN_TOOLS, true); },
      LV_EVENT_CLICKED, nullptr);
  lv_obj_t *lbl_back = lv_label_create(btn_back);
  lv_label_set_text(lbl_back, LV_SYMBOL_LEFT " Volver");
  lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl_back);

  /* Botón Refrescar */
  lv_obj_t *btn_ref = lv_button_create(header);
  lv_obj_set_size(btn_ref, 44, 36);
  lv_obj_align(btn_ref, LV_ALIGN_RIGHT_MID, -8, 0);
  lv_obj_set_style_bg_color(btn_ref, COLOR_HEADER, 0);
  lv_obj_set_style_radius(btn_ref, 8, 0);
  lv_obj_set_style_border_width(btn_ref, 1, 0);
  lv_obj_set_style_border_color(btn_ref, COLOR_ACCENT, 0);
  lv_obj_add_event_cb(
      btn_ref,
      [](lv_event_t *) {
        lv_label_set_text(lbl_count, "Escaneando...");
        lv_obj_clean(list_cont);
        wifi_mgr_start_scan();
        lv_timer_resume(poll_timer);
      },
      LV_EVENT_CLICKED, nullptr);
  lv_obj_t *lbl_ref = lv_label_create(btn_ref);
  lv_label_set_text(lbl_ref, LV_SYMBOL_REFRESH);
  lv_obj_set_style_text_color(lbl_ref, COLOR_ACCENT, 0);
  lv_obj_center(lbl_ref);

  /* Título */
  lv_obj_t *title = lv_label_create(header);
  lv_label_set_text(title, LV_SYMBOL_WIFI "  WiFi Analyzer");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
  lv_obj_center(title);

  /* ── Barra de info (contador) ────────────────────────────── */
  lv_obj_t *toolbar = lv_obj_create(scr);
  lv_obj_set_size(toolbar, 480, 30);
  lv_obj_align(toolbar, LV_ALIGN_TOP_MID, 0, 59);
  lv_obj_set_style_bg_color(toolbar, lv_color_hex(0x0D1A30), 0);
  lv_obj_set_style_bg_opa(toolbar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(toolbar, 0, 0);
  lv_obj_set_style_radius(toolbar, 0, 0);
  lv_obj_set_style_pad_all(toolbar, 0, 0);
  lv_obj_clear_flag(toolbar, LV_OBJ_FLAG_SCROLLABLE);

  lbl_count = lv_label_create(toolbar);
  lv_label_set_text(lbl_count, "Escaneando...");
  lv_obj_set_style_text_font(lbl_count, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lbl_count, COLOR_DIM, 0);
  lv_obj_align(lbl_count, LV_ALIGN_LEFT_MID, 10, 0);

  /* ── Lista scrollable ────────────────────────────────────── */
  list_cont = lv_obj_create(scr);
  lv_obj_set_size(list_cont, 480, 231); // 320 - 59 - 30
  lv_obj_align(list_cont, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_opa(list_cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(list_cont, 0, 0);
  lv_obj_set_style_pad_left(list_cont, 12, 0);
  lv_obj_set_style_pad_right(list_cont, 12, 0);
  lv_obj_set_style_pad_top(list_cont, 6, 0);
  lv_obj_set_style_pad_bottom(list_cont, 6, 0);
  lv_obj_set_style_pad_row(list_cont, 5, 0);
  lv_obj_set_flex_flow(list_cont, LV_FLEX_FLOW_COLUMN);

  /* ── Timer de polling (cada 500 ms) ─────────────────────── */
  poll_timer = lv_timer_create(poll_cb, 500, nullptr);
  lv_timer_pause(poll_timer);
}

lv_obj_t *screen_wifi_analyzer_get() { return scr; }

void screen_wifi_analyzer_start() {
  lv_label_set_text(lbl_count, "Escaneando...");
  lv_obj_clean(list_cont);
  wifi_mgr_start_scan();
  lv_timer_resume(poll_timer);
}
