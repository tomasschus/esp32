#include "screen_timer.h"
#include "ui.h"
#include <Arduino.h>

/* ═══════════════════════════════════════════════════════════
   Estado global
═══════════════════════════════════════════════════════════ */

/* Cronómetro */
static uint32_t sw_accum = 0; // ms acumulados en runs anteriores
static uint32_t sw_t0 = 0;    // millis() al iniciar el run actual
static bool sw_run = false;
static int sw_lap_n = 0;
static char sw_laps[512] = ""; // texto de vueltas (más nueva primero)

/* Temporizador */
enum class TMState { IDLE, RUNNING, PAUSED, DONE };
static TMState tm_state = TMState::IDLE;
static int tm_min = 5;
static int tm_sec_v = 0;
static uint32_t tm_set = 5 * 60000UL; // duración configurada
static uint32_t tm_rem = 5 * 60000UL; // ms restantes al pausar/idle
static uint32_t tm_t0 = 0;            // millis() al iniciar el run actual

/* ═══════════════════════════════════════════════════════════
   Widgets
═══════════════════════════════════════════════════════════ */
static lv_obj_t *scr = nullptr;
static lv_obj_t *view_sw = nullptr;
static lv_obj_t *view_tm = nullptr;
static lv_obj_t *btn_tab_sw = nullptr;
static lv_obj_t *btn_tab_tm = nullptr;

/* Cronómetro */
static lv_obj_t *lbl_sw_time = nullptr;
static lv_obj_t *lbl_sw_laps = nullptr;
static lv_obj_t *lbl_sw_play = nullptr; // etiqueta del botón play/pause

/* Temporizador */
static lv_obj_t *lbl_tm_time = nullptr;
static lv_obj_t *row_tm_adj = nullptr;    // fila de botones +/-
static lv_obj_t *lbl_tm_status = nullptr; // "¡TIEMPO!" al terminar
static lv_obj_t *lbl_tm_play = nullptr;   // etiqueta del botón play/pause

static lv_timer_t *tick_tmr = nullptr;

/* ═══════════════════════════════════════════════════════════
   Helpers de tiempo
═══════════════════════════════════════════════════════════ */
static uint32_t sw_ms() { return sw_accum + (sw_run ? millis() - sw_t0 : 0); }

static uint32_t tm_ms() {
  if (tm_state != TMState::RUNNING)
    return tm_rem;
  uint32_t e = millis() - tm_t0;
  return e >= tm_rem ? 0 : tm_rem - e;
}

static void fmt_sw(char *b, size_t n, uint32_t ms) {
  uint32_t h = ms / 3600000UL;
  uint32_t m = (ms % 3600000UL) / 60000UL;
  uint32_t s = (ms % 60000UL) / 1000UL;
  uint32_t d = (ms % 1000UL) / 100UL;
  /* Relleno a los lados para evitar que el primer/último carácter se dibuje mal
   * en pantalla */
  if (h)
    snprintf(b, n, "  %02lu:%02lu:%02lu.%lu  ", h, m, s, d);
  else
    snprintf(b, n, "  %02lu:%02lu.%lu  ", m, s, d);
}

static void fmt_tm(char *b, size_t n, uint32_t ms) {
  snprintf(b, n, "%02lu:%02lu", ms / 60000UL, (ms % 60000UL) / 1000UL);
}

/* Aplica el estado actual del temporizador a la UI */
static void tm_apply_ui() {
  char b[12];
  fmt_tm(b, sizeof(b), tm_rem);
  lv_label_set_text(lbl_tm_time, b);

  bool adj_vis = (tm_state == TMState::IDLE);
  bool sts_vis = (tm_state == TMState::DONE);
  if (adj_vis)
    lv_obj_clear_flag(row_tm_adj, LV_OBJ_FLAG_HIDDEN);
  else
    lv_obj_add_flag(row_tm_adj, LV_OBJ_FLAG_HIDDEN);
  if (sts_vis)
    lv_obj_clear_flag(lbl_tm_status, LV_OBJ_FLAG_HIDDEN);
  else
    lv_obj_add_flag(lbl_tm_status, LV_OBJ_FLAG_HIDDEN);

  const char *pt;
  switch (tm_state) {
  case TMState::IDLE:
    pt = "> Iniciar";
    break;
  case TMState::RUNNING:
    pt = "|| Pausar";
    break;
  case TMState::PAUSED:
    pt = "> Reanudar";
    break;
  default:
    pt = "O Reiniciar";
    break;
  }
  lv_label_set_text(lbl_tm_play, pt);
}

/* ═══════════════════════════════════════════════════════════
   Tick timer (100 ms)
═══════════════════════════════════════════════════════════ */
static void tick_cb(lv_timer_t *) {
  char b[20];

  /* Actualizar cronómetro */
  fmt_sw(b, sizeof(b), sw_ms());
  lv_label_set_text(lbl_sw_time, b);

  /* Actualizar temporizador (sólo cuando corre) */
  if (tm_state == TMState::RUNNING) {
    uint32_t rem = tm_ms();
    if (rem == 0) {
      tm_rem = 0;
      tm_state = TMState::DONE;
      lv_label_set_text(lbl_tm_status, "! ¡TIEMPO!");
      lv_obj_set_style_text_color(lbl_tm_status, lv_color_hex(0xE94560), 0);
      tm_apply_ui();
    } else {
      fmt_tm(b, sizeof(b), rem);
      lv_label_set_text(lbl_tm_time, b);
    }
  }
}

/* ═══════════════════════════════════════════════════════════
   Callbacks de pestañas
═══════════════════════════════════════════════════════════ */
static void tab_sw_cb(lv_event_t *) {
  lv_obj_clear_flag(view_sw, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(view_tm, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_bg_color(btn_tab_sw, lv_color_hex(0xE94560), 0);
  lv_obj_set_style_bg_color(btn_tab_tm, lv_color_hex(0x2A2A4A), 0);
}

static void tab_tm_cb(lv_event_t *) {
  lv_obj_add_flag(view_sw, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(view_tm, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_bg_color(btn_tab_sw, lv_color_hex(0x2A2A4A), 0);
  lv_obj_set_style_bg_color(btn_tab_tm, lv_color_hex(0xE94560), 0);
}

/* ═══════════════════════════════════════════════════════════
   Callbacks del cronómetro
═══════════════════════════════════════════════════════════ */
static void sw_play_cb(lv_event_t *) {
  if (sw_run) {
    sw_accum += millis() - sw_t0;
    sw_run = false;
    lv_label_set_text(lbl_sw_play, "> Iniciar");
  } else {
    sw_t0 = millis();
    sw_run = true;
    lv_label_set_text(lbl_sw_play, "|| Pausar");
  }
}

static void sw_lap_cb(lv_event_t *) {
  if (!sw_run)
    return;
  char tbuf[20], entry[40], tmp[600];
  fmt_sw(tbuf, sizeof(tbuf), sw_ms());
  snprintf(entry, sizeof(entry), "#%d  %s\n", ++sw_lap_n, tbuf);
  snprintf(tmp, sizeof(tmp), "%s%s", entry, sw_laps);
  strncpy(sw_laps, tmp, sizeof(sw_laps) - 1);
  lv_label_set_text(lbl_sw_laps, sw_laps);
}

static void sw_rst_cb(lv_event_t *) {
  sw_run = false;
  sw_accum = 0;
  sw_t0 = 0;
  sw_lap_n = 0;
  sw_laps[0] = '\0';
  lv_label_set_text(lbl_sw_time, "  00:00.0  ");
  lv_label_set_text(lbl_sw_laps, "");
  lv_label_set_text(lbl_sw_play, "> Iniciar");
}

/* ═══════════════════════════════════════════════════════════
   Callbacks del temporizador
═══════════════════════════════════════════════════════════ */
static void tm_adj_cb(lv_event_t *e) {
  if (tm_state == TMState::RUNNING || tm_state == TMState::PAUSED)
    return;
  int d = (int)(intptr_t)lv_event_get_user_data(e);
  int t = tm_min * 60 + tm_sec_v + d;
  if (t < 5)
    t = 5;
  if (t > 99 * 60 + 59)
    t = 99 * 60 + 59;
  tm_min = t / 60;
  tm_sec_v = t % 60;
  tm_set = (uint32_t)t * 1000UL;
  tm_rem = tm_set;
  tm_state = TMState::IDLE;
  tm_apply_ui();
}

static void tm_play_cb(lv_event_t *) {
  switch (tm_state) {
  case TMState::IDLE:
    tm_t0 = millis();
    tm_state = TMState::RUNNING;
    break;
  case TMState::RUNNING:
    tm_rem = tm_ms();
    tm_state = TMState::PAUSED;
    break;
  case TMState::PAUSED:
    tm_t0 = millis();
    tm_state = TMState::RUNNING;
    break;
  default:
    tm_rem = tm_set;
    tm_state = TMState::IDLE;
    break;
  }
  tm_apply_ui();
}

static void tm_rst_cb(lv_event_t *) {
  tm_rem = tm_set;
  tm_state = TMState::IDLE;
  tm_apply_ui();
}

/* ═══════════════════════════════════════════════════════════
   Constructores de widgets reutilizables
═══════════════════════════════════════════════════════════ */
static lv_obj_t *mk_panel(lv_obj_t *p, int w, int h, uint32_t col) {
  lv_obj_t *o = lv_obj_create(p);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_color(o, lv_color_hex(col), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_set_style_radius(o, 0, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  return o;
}

static void mk_adj(lv_obj_t *p, const char *txt, int delta_sec) {
  lv_obj_t *b = lv_button_create(p);
  lv_obj_set_size(b, 60, 36);
  lv_obj_set_style_bg_color(b, lv_color_hex(0x1E3A5F), 0);
  lv_obj_set_style_radius(b, 8, 0);
  lv_obj_set_style_border_width(b, 0, 0);
  lv_obj_add_event_cb(b, tm_adj_cb, LV_EVENT_CLICKED,
                      (void *)(intptr_t)delta_sec);
  lv_obj_t *l = lv_label_create(b);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(0xCCCCCC), 0);
  lv_obj_center(l);
}

static lv_obj_t *mk_ctrl(lv_obj_t *p, const char *txt, uint32_t col, int w,
                         lv_event_cb_t cb) {
  lv_obj_t *b = lv_button_create(p);
  lv_obj_set_size(b, w, 44);
  lv_obj_set_style_bg_color(b, lv_color_hex(col), 0);
  lv_obj_set_style_radius(b, 10, 0);
  lv_obj_set_style_border_width(b, 0, 0);
  if (cb)
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *l = lv_label_create(b);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(0xEEEEEE), 0);
  lv_obj_center(l);
  return b;
}

static lv_obj_t *mk_flex_row(lv_obj_t *p, int w, int h, uint32_t col) {
  lv_obj_t *o = mk_panel(p, w, h, col);
  lv_obj_set_flex_flow(o, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(o, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  return o;
}

/* ═══════════════════════════════════════════════════════════
   Creación de la pantalla
═══════════════════════════════════════════════════════════ */
void screen_timer_create() {
  if (scr)
    return;

  scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  /* ── Header ────────────────────────────────────────────── */
  lv_obj_t *hdr = mk_panel(scr, 480, 56, 0x0A0A0A);
  lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 0);

  lv_obj_t *bbk = lv_button_create(hdr);
  lv_obj_set_size(bbk, 80, 36);
  lv_obj_align(bbk, LV_ALIGN_LEFT_MID, 8, 0);
  lv_obj_set_style_bg_color(bbk, lv_color_hex(0xE94560), 0);
  lv_obj_set_style_radius(bbk, 8, 0);
  lv_obj_set_style_border_width(bbk, 0, 0);
  lv_obj_add_event_cb(
      bbk, [](lv_event_t *) { ui_navigate_to(UI_SCREEN_TOOLS, true); },
      LV_EVENT_CLICKED, nullptr);
  lv_obj_t *lbk = lv_label_create(bbk);
  lv_label_set_text(lbk, "< Volver");
  lv_obj_set_style_text_font(lbk, &lv_font_montserrat_14, 0);
  lv_obj_center(lbk);

  lv_obj_t *ttl = lv_label_create(hdr);
  lv_label_set_text(ttl, "Cronometro  /  Temporizador");
  lv_obj_set_style_text_font(ttl, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(ttl, lv_color_hex(0xEEEEEE), 0);
  lv_obj_center(ttl);

  /* Barra de acento */
  lv_obj_t *acc = mk_panel(scr, 480, 3, 0xE94560);
  lv_obj_align(acc, LV_ALIGN_TOP_MID, 0, 56);

  /* ── Barra de pestañas (y=59..99) ──────────────────────── */
  lv_obj_t *tabs = mk_panel(scr, 480, 40, 0x0A0A0A);
  lv_obj_align(tabs, LV_ALIGN_TOP_MID, 0, 59);

  btn_tab_sw = lv_button_create(tabs);
  lv_obj_set_size(btn_tab_sw, 236, 34);
  lv_obj_align(btn_tab_sw, LV_ALIGN_LEFT_MID, 4, 0);
  lv_obj_set_style_bg_color(btn_tab_sw, lv_color_hex(0xE94560), 0); // activa
  lv_obj_set_style_radius(btn_tab_sw, 8, 0);
  lv_obj_set_style_border_width(btn_tab_sw, 0, 0);
  lv_obj_add_event_cb(btn_tab_sw, tab_sw_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *tsw = lv_label_create(btn_tab_sw);
  lv_label_set_text(tsw, "> Cronometro");
  lv_obj_set_style_text_font(tsw, &lv_font_montserrat_14, 0);
  lv_obj_center(tsw);

  btn_tab_tm = lv_button_create(tabs);
  lv_obj_set_size(btn_tab_tm, 236, 34);
  lv_obj_align(btn_tab_tm, LV_ALIGN_RIGHT_MID, -4, 0);
  lv_obj_set_style_bg_color(btn_tab_tm, lv_color_hex(0x2A2A4A), 0); // inactiva
  lv_obj_set_style_radius(btn_tab_tm, 8, 0);
  lv_obj_set_style_border_width(btn_tab_tm, 0, 0);
  lv_obj_add_event_cb(btn_tab_tm, tab_tm_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *ttm = lv_label_create(btn_tab_tm);
  lv_label_set_text(ttm, "O Temporizador");
  lv_obj_set_style_text_font(ttm, &lv_font_montserrat_14, 0);
  lv_obj_center(ttm);

  /* ══════════════════════════════════════════════════════════
     Vista Cronómetro (y=99..320 = 221px)
  ══════════════════════════════════════════════════════════ */
  view_sw = mk_panel(scr, 480, 221, 0x000000);
  lv_obj_align(view_sw, LV_ALIGN_TOP_MID, 0, 99);

  /* Tiempo grande */
  lbl_sw_time = lv_label_create(view_sw);
  lv_label_set_text(lbl_sw_time, "  00:00.0  ");
  lv_obj_set_style_text_font(lbl_sw_time, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_sw_time, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(lbl_sw_time, LV_ALIGN_TOP_MID, 0, 8);

  /* Caja de vueltas */
  lv_obj_t *lbox = lv_obj_create(view_sw);
  lv_obj_set_size(lbox, 456, 78);
  lv_obj_align(lbox, LV_ALIGN_TOP_MID, 0, 66);
  lv_obj_set_style_bg_color(lbox, lv_color_hex(0x111126), 0);
  lv_obj_set_style_bg_opa(lbox, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(lbox, lv_color_hex(0x2A2A4A), 0);
  lv_obj_set_style_border_width(lbox, 1, 0);
  lv_obj_set_style_radius(lbox, 8, 0);
  lv_obj_set_style_pad_all(lbox, 6, 0);
  lbl_sw_laps = lv_label_create(lbox);
  lv_label_set_text(lbl_sw_laps, "");
  lv_obj_set_style_text_font(lbl_sw_laps, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lbl_sw_laps, lv_color_hex(0x9999BB), 0);
  lv_label_set_long_mode(lbl_sw_laps, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(lbl_sw_laps, 444);

  /* Fila de botones cronómetro */
  lv_obj_t *sw_row = mk_flex_row(view_sw, 456, 44, 0x000000);
  lv_obj_align(sw_row, LV_ALIGN_BOTTOM_MID, 0, -8);

  lv_obj_t *bplay = mk_ctrl(sw_row, "> Iniciar", 0x1E3A5F, 155, sw_play_cb);
  lbl_sw_play = lv_obj_get_child(bplay, 0);

  mk_ctrl(sw_row, "+ Vuelta", 0x1E3A5F, 130, sw_lap_cb);
  mk_ctrl(sw_row, "X Reset", 0x5A1A1A, 120, sw_rst_cb);

  /* ══════════════════════════════════════════════════════════
     Vista Temporizador (y=99..320 = 221px, oculta al inicio)
  ══════════════════════════════════════════════════════════ */
  view_tm = mk_panel(scr, 480, 221, 0x000000);
  lv_obj_align(view_tm, LV_ALIGN_TOP_MID, 0, 99);
  lv_obj_add_flag(view_tm, LV_OBJ_FLAG_HIDDEN);

  /* Tiempo grande */
  lbl_tm_time = lv_label_create(view_tm);
  lv_label_set_text(lbl_tm_time, "05:00");
  lv_obj_set_style_text_font(lbl_tm_time, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_tm_time, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(lbl_tm_time, LV_ALIGN_TOP_MID, 0, 8);

  /* Fila de ajuste +/- (visible sólo en IDLE) */
  row_tm_adj = mk_flex_row(view_tm, 456, 44, 0x000000);
  lv_obj_align(row_tm_adj, LV_ALIGN_TOP_MID, 0, 66);

  mk_adj(row_tm_adj, "-5m", -300);
  mk_adj(row_tm_adj, "-1m", -60);
  mk_adj(row_tm_adj, "-15s", -15);
  mk_adj(row_tm_adj, "+15s", +15);
  mk_adj(row_tm_adj, "+1m", +60);
  mk_adj(row_tm_adj, "+5m", +300);

  /* Etiqueta de estado "¡TIEMPO!" (oculta normalmente) */
  lbl_tm_status = lv_label_create(view_tm);
  lv_label_set_text(lbl_tm_status, "");
  lv_obj_set_style_text_font(lbl_tm_status, &lv_font_montserrat_16, 0);
  lv_obj_align(lbl_tm_status, LV_ALIGN_TOP_MID, 0, 122);
  lv_obj_add_flag(lbl_tm_status, LV_OBJ_FLAG_HIDDEN);

  /* Fila de botones temporizador */
  lv_obj_t *tm_row = mk_flex_row(view_tm, 456, 44, 0x000000);
  lv_obj_align(tm_row, LV_ALIGN_BOTTOM_MID, 0, -8);

  lv_obj_t *btmplay = mk_ctrl(tm_row, "> Iniciar", 0x1E3A5F, 210, tm_play_cb);
  lbl_tm_play = lv_obj_get_child(btmplay, 0);

  mk_ctrl(tm_row, "O Reset", 0x5A1A1A, 190, tm_rst_cb);

  /* Timer LVGL de 100 ms */
  tick_tmr = lv_timer_create(tick_cb, 100, nullptr);
}

lv_obj_t *screen_timer_get() { return scr; }
