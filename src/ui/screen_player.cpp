#include "screen_player.h"
#include "../audio_mgr.h"
#include "ui.h"
#include <Arduino.h>

/* ── Screen-level objects ────────────────────────────────────── */
static lv_obj_t *scr = nullptr;
static lv_obj_t *view_files = nullptr;  // scrollable track list
static lv_obj_t *view_player = nullptr; // now-playing panel

/* ── Player-view widgets updated by timer ────────────────────── */
static lv_obj_t *lbl_title = nullptr;
static lv_obj_t *lbl_artist = nullptr;
static lv_obj_t *bar_progress = nullptr;
static lv_obj_t *lbl_time_cur = nullptr;
static lv_obj_t *lbl_time_tot = nullptr;
static lv_obj_t *lbl_play_ico = nullptr; // play/pause icon label
static lv_obj_t *lbl_vol = nullptr;

/* ── State ───────────────────────────────────────────────────── */
static bool in_player_view = false;

/* ── Forward declarations ────────────────────────────────────── */
static void show_files_view();
static void show_player_view();

/* ── View switchers ──────────────────────────────────────────── */
static void show_files_view() {
  in_player_view = false;
  lv_obj_clear_flag(view_files, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(view_player, LV_OBJ_FLAG_HIDDEN);
}

static void show_player_view() {
  in_player_view = true;
  lv_obj_add_flag(view_files, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(view_player, LV_OBJ_FLAG_HIDDEN);
}

/* ── Time formatter ──────────────────────────────────────────── */
static void fmt_time(char *buf, uint32_t s) {
  snprintf(buf, 8, "%u:%02u", s / 60, s % 60);
}

/* ── 500 ms refresh timer ────────────────────────────────────── */
static void player_tick_cb(lv_timer_t *) {
  if (!in_player_view)
    return;

  audio_mgr_state_t st = audio_mgr_get_state();
  uint32_t pos = audio_mgr_get_position_s();
  uint32_t dur = audio_mgr_get_duration_s();

  /* Title & artist */
  const char *title = audio_mgr_get_title();
  const char *artist = audio_mgr_get_artist();
  int idx = audio_mgr_get_current_index();

  lv_label_set_text(
      lbl_title, (title && title[0])
                     ? title
                     : (idx >= 0 ? audio_mgr_get_filename(idx) : "Sin pista"));
  lv_label_set_text(lbl_artist, (artist && artist[0]) ? artist : "---");

  /* Progress bar */
  lv_bar_set_value(bar_progress, dur > 0 ? (int)(pos * 100 / dur) : 0,
                   LV_ANIM_OFF);

  /* Time labels */
  char buf[8];
  fmt_time(buf, pos);
  lv_label_set_text(lbl_time_cur, buf);
  fmt_time(buf, dur);
  lv_label_set_text(lbl_time_tot, buf);

  /* Play / pause icon */
  lv_label_set_text(lbl_play_ico,
                    st == AUDIO_PLAYING ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);

  /* Volume */
  char vbuf[4];
  snprintf(vbuf, sizeof(vbuf), "%d", audio_mgr_get_volume());
  lv_label_set_text(lbl_vol, vbuf);
}

/* ── screen_player_create ────────────────────────────────────── */
void screen_player_create() {
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
  lv_obj_set_style_bg_color(accent, lv_color_hex(0xFFAA44), 0);
  lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(accent, 0, 0);
  lv_obj_set_style_radius(accent, 0, 0);

  /* Back button */
  lv_obj_t *btn_back = lv_button_create(header);
  lv_obj_set_size(btn_back, 80, 36);
  lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 8, 0);
  lv_obj_set_style_bg_color(btn_back, lv_color_hex(0xFFAA44), 0);
  lv_obj_set_style_radius(btn_back, 8, 0);
  lv_obj_set_style_border_width(btn_back, 0, 0);
  lv_obj_add_event_cb(
      btn_back,
      [](lv_event_t *) {
        if (in_player_view) {
          show_files_view();
        } else {
          ui_navigate_to(UI_SCREEN_TOOLS);
        }
      },
      LV_EVENT_CLICKED, nullptr);

  lv_obj_t *lbl_back = lv_label_create(btn_back);
  lv_label_set_text(lbl_back, LV_SYMBOL_LEFT " Volver");
  lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl_back);

  /* Title */
  lv_obj_t *hdr_title = lv_label_create(header);
  lv_label_set_text(hdr_title, LV_SYMBOL_AUDIO "  Reproductor");
  lv_obj_set_style_text_font(hdr_title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(hdr_title, lv_color_hex(0xEEEEEE), 0);
  lv_obj_center(hdr_title);

  /* ── FILES VIEW ──────────────────────────────────────────── */
  view_files = lv_obj_create(scr);
  lv_obj_set_size(view_files, 480, 261);
  lv_obj_align(view_files, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_color(view_files, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(view_files, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(view_files, 0, 0);
  lv_obj_set_style_radius(view_files, 0, 0);
  lv_obj_set_style_pad_all(view_files, 0, 0);
  lv_obj_set_style_pad_row(view_files, 0, 0);
  lv_obj_set_flex_flow(view_files, LV_FLEX_FLOW_COLUMN);
  /* leave scrollable (default) */

  int n = audio_mgr_get_file_count();
  if (n == 0) {
    lv_obj_t *msg = lv_label_create(view_files);
    lv_label_set_text(
        msg, audio_mgr_get_state() == AUDIO_NO_SD
                 ? "Sin tarjeta SD"
                 : "No hay archivos de audio en la SD\n(MP3 / WAV / AAC)");
    lv_obj_set_style_text_color(msg, lv_color_hex(0x778899), 0);
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(msg, LV_ALIGN_CENTER, 0, 0);
  } else {
    for (int i = 0; i < n; i++) {
      lv_obj_t *row = lv_obj_create(view_files);
      lv_obj_set_size(row, 480, 52);
      lv_obj_set_style_bg_color(
          row, i % 2 == 0 ? lv_color_hex(0x0F2040) : lv_color_hex(0x0A1830), 0);
      lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
      lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
      lv_obj_set_style_border_color(row, lv_color_hex(0x1A3A6A), 0);
      lv_obj_set_style_border_width(row, 1, 0);
      lv_obj_set_style_radius(row, 0, 0);
      lv_obj_set_style_pad_all(row, 0, 0);
      lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_event_cb(
          row,
          [](lv_event_t *e) {
            int idx = (int)(intptr_t)lv_event_get_user_data(e);
            audio_mgr_play_index(idx);
            show_player_view();
          },
          LV_EVENT_CLICKED, (void *)(intptr_t)i);

      /* Music icon */
      lv_obj_t *ico = lv_label_create(row);
      lv_label_set_text(ico, LV_SYMBOL_AUDIO);
      lv_obj_set_style_text_color(ico, lv_color_hex(0xFFAA44), 0);
      lv_obj_set_style_text_font(ico, &lv_font_montserrat_16, 0);
      lv_obj_align(ico, LV_ALIGN_LEFT_MID, 12, 0);

      /* Filename */
      lv_obj_t *lbl = lv_label_create(row);
      lv_label_set_text(lbl, audio_mgr_get_filename(i));
      lv_obj_set_style_text_color(lbl, lv_color_hex(0xEEEEEE), 0);
      lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
      lv_obj_set_width(lbl, 400);
      lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
      lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 42, 0);

      /* Arrow */
      lv_obj_t *arr = lv_label_create(row);
      lv_label_set_text(arr, LV_SYMBOL_RIGHT);
      lv_obj_set_style_text_color(arr, lv_color_hex(0x4DA6FF), 0);
      lv_obj_align(arr, LV_ALIGN_RIGHT_MID, -10, 0);
    }
  }

  /* ── PLAYER VIEW ─────────────────────────────────────────── */
  view_player = lv_obj_create(scr);
  lv_obj_set_size(view_player, 480, 261);
  lv_obj_align(view_player, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_color(view_player, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(view_player, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(view_player, 0, 0);
  lv_obj_set_style_radius(view_player, 0, 0);
  lv_obj_set_style_pad_all(view_player, 0, 0);
  lv_obj_clear_flag(view_player, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(view_player, LV_OBJ_FLAG_HIDDEN); // hidden initially

  /* Artist label */
  lbl_artist = lv_label_create(view_player);
  lv_label_set_text(lbl_artist, "---");
  lv_obj_set_style_text_font(lbl_artist, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lbl_artist, lv_color_hex(0x778899), 0);
  lv_obj_set_width(lbl_artist, 444);
  lv_label_set_long_mode(lbl_artist, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(lbl_artist, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(lbl_artist, LV_ALIGN_TOP_MID, 0, 14);

  /* Title label */
  lbl_title = lv_label_create(view_player);
  lv_label_set_text(lbl_title, "Sin pista");
  lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xEEEEEE), 0);
  lv_obj_set_width(lbl_title, 444);
  lv_label_set_long_mode(lbl_title, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(lbl_title, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 38);

  /* Progress bar */
  bar_progress = lv_bar_create(view_player);
  lv_obj_set_size(bar_progress, 444, 10);
  lv_bar_set_range(bar_progress, 0, 100);
  lv_bar_set_value(bar_progress, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(bar_progress, lv_color_hex(0x2A3A5A), 0);
  lv_obj_set_style_bg_color(bar_progress, lv_color_hex(0xFFAA44),
                            LV_PART_INDICATOR);
  lv_obj_set_style_radius(bar_progress, 4, 0);
  lv_obj_set_style_radius(bar_progress, 4, LV_PART_INDICATOR);
  lv_obj_align(bar_progress, LV_ALIGN_TOP_MID, 0, 78);

  /* Time row */
  lv_obj_t *row_time = lv_obj_create(view_player);
  lv_obj_set_size(row_time, 444, 20);
  lv_obj_align(row_time, LV_ALIGN_TOP_MID, 0, 94);
  lv_obj_set_style_bg_opa(row_time, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row_time, 0, 0);
  lv_obj_set_style_radius(row_time, 0, 0);
  lv_obj_set_style_pad_all(row_time, 0, 0);
  lv_obj_clear_flag(row_time, LV_OBJ_FLAG_SCROLLABLE);

  lbl_time_cur = lv_label_create(row_time);
  lv_label_set_text(lbl_time_cur, "0:00");
  lv_obj_set_style_text_font(lbl_time_cur, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lbl_time_cur, lv_color_hex(0x778899), 0);
  lv_obj_align(lbl_time_cur, LV_ALIGN_LEFT_MID, 0, 0);

  lbl_time_tot = lv_label_create(row_time);
  lv_label_set_text(lbl_time_tot, "0:00");
  lv_obj_set_style_text_font(lbl_time_tot, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lbl_time_tot, lv_color_hex(0x778899), 0);
  lv_obj_align(lbl_time_tot, LV_ALIGN_RIGHT_MID, 0, 0);

  /* ── Control buttons (PREV / PLAY / NEXT) ─────────────────── */
  lv_obj_t *row_ctrl = lv_obj_create(view_player);
  lv_obj_set_size(row_ctrl, 320, 60);
  lv_obj_align(row_ctrl, LV_ALIGN_TOP_MID, 0, 124);
  lv_obj_set_style_bg_opa(row_ctrl, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row_ctrl, 0, 0);
  lv_obj_set_style_radius(row_ctrl, 0, 0);
  lv_obj_set_style_pad_all(row_ctrl, 0, 0);
  lv_obj_clear_flag(row_ctrl, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(row_ctrl, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row_ctrl, LV_FLEX_ALIGN_SPACE_EVENLY,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  /* PREV */
  lv_obj_t *btn_prev = lv_button_create(row_ctrl);
  lv_obj_set_size(btn_prev, 72, 52);
  lv_obj_set_style_bg_color(btn_prev, lv_color_hex(0x16213E), 0);
  lv_obj_set_style_radius(btn_prev, 12, 0);
  lv_obj_set_style_border_width(btn_prev, 0, 0);
  lv_obj_add_event_cb(
      btn_prev, [](lv_event_t *) { audio_mgr_prev(); }, LV_EVENT_CLICKED,
      nullptr);
  lv_obj_t *ico_prev = lv_label_create(btn_prev);
  lv_label_set_text(ico_prev, LV_SYMBOL_PREV);
  lv_obj_set_style_text_font(ico_prev, &lv_font_montserrat_16, 0);
  lv_obj_center(ico_prev);

  /* PLAY / PAUSE */
  lv_obj_t *btn_play = lv_button_create(row_ctrl);
  lv_obj_set_size(btn_play, 88, 52);
  lv_obj_set_style_bg_color(btn_play, lv_color_hex(0xFFAA44), 0);
  lv_obj_set_style_radius(btn_play, 12, 0);
  lv_obj_set_style_border_width(btn_play, 0, 0);
  lv_obj_add_event_cb(
      btn_play, [](lv_event_t *) { audio_mgr_toggle_pause(); },
      LV_EVENT_CLICKED, nullptr);
  lbl_play_ico = lv_label_create(btn_play);
  lv_label_set_text(lbl_play_ico, LV_SYMBOL_PLAY);
  lv_obj_set_style_text_font(lbl_play_ico, &lv_font_montserrat_16, 0);
  lv_obj_center(lbl_play_ico);

  /* NEXT */
  lv_obj_t *btn_next = lv_button_create(row_ctrl);
  lv_obj_set_size(btn_next, 72, 52);
  lv_obj_set_style_bg_color(btn_next, lv_color_hex(0x16213E), 0);
  lv_obj_set_style_radius(btn_next, 12, 0);
  lv_obj_set_style_border_width(btn_next, 0, 0);
  lv_obj_add_event_cb(
      btn_next, [](lv_event_t *) { audio_mgr_next(); }, LV_EVENT_CLICKED,
      nullptr);
  lv_obj_t *ico_next = lv_label_create(btn_next);
  lv_label_set_text(ico_next, LV_SYMBOL_NEXT);
  lv_obj_set_style_text_font(ico_next, &lv_font_montserrat_16, 0);
  lv_obj_center(ico_next);

  /* ── Volume row ─────────────────────────────────────────── */
  lv_obj_t *row_vol = lv_obj_create(view_player);
  lv_obj_set_size(row_vol, 220, 40);
  lv_obj_align(row_vol, LV_ALIGN_TOP_MID, 0, 198);
  lv_obj_set_style_bg_opa(row_vol, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row_vol, 0, 0);
  lv_obj_set_style_radius(row_vol, 0, 0);
  lv_obj_set_style_pad_all(row_vol, 0, 0);
  lv_obj_clear_flag(row_vol, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(row_vol, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row_vol, LV_FLEX_ALIGN_SPACE_EVENLY,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  /* Vol- */
  lv_obj_t *btn_vd = lv_button_create(row_vol);
  lv_obj_set_size(btn_vd, 48, 34);
  lv_obj_set_style_bg_color(btn_vd, lv_color_hex(0x16213E), 0);
  lv_obj_set_style_radius(btn_vd, 8, 0);
  lv_obj_set_style_border_width(btn_vd, 0, 0);
  lv_obj_add_event_cb(
      btn_vd,
      [](lv_event_t *) { audio_mgr_set_volume(audio_mgr_get_volume() - 1); },
      LV_EVENT_CLICKED, nullptr);
  lv_obj_t *lbl_vd = lv_label_create(btn_vd);
  lv_label_set_text(lbl_vd, LV_SYMBOL_MINUS);
  lv_obj_set_style_text_font(lbl_vd, &lv_font_montserrat_16, 0);
  lv_obj_center(lbl_vd);

  /* Volume value */
  lbl_vol = lv_label_create(row_vol);
  lv_label_set_text(lbl_vol, "10");
  lv_obj_set_style_text_font(lbl_vol, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_vol, lv_color_hex(0xEEEEEE), 0);
  lv_obj_set_width(lbl_vol, 36);
  lv_obj_set_style_text_align(lbl_vol, LV_TEXT_ALIGN_CENTER, 0);

  /* Vol+ */
  lv_obj_t *btn_vu = lv_button_create(row_vol);
  lv_obj_set_size(btn_vu, 48, 34);
  lv_obj_set_style_bg_color(btn_vu, lv_color_hex(0x16213E), 0);
  lv_obj_set_style_radius(btn_vu, 8, 0);
  lv_obj_set_style_border_width(btn_vu, 0, 0);
  lv_obj_add_event_cb(
      btn_vu,
      [](lv_event_t *) { audio_mgr_set_volume(audio_mgr_get_volume() + 1); },
      LV_EVENT_CLICKED, nullptr);
  lv_obj_t *lbl_vu = lv_label_create(btn_vu);
  lv_label_set_text(lbl_vu, LV_SYMBOL_PLUS);
  lv_obj_set_style_text_font(lbl_vu, &lv_font_montserrat_16, 0);
  lv_obj_center(lbl_vu);

  /* ── Refresh timer ──────────────────────────────────────── */
  lv_timer_create(player_tick_cb, 500, nullptr);
}

lv_obj_t *screen_player_get() { return scr; }
