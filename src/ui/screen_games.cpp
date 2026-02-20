#include "screen_games.h"
#include "../game_runner.h"
#include "ui.h"

#define COLOR_BG lv_color_hex(0x000000)
#define COLOR_HEADER lv_color_hex(0x0A0A0A)
#define COLOR_ACCENT lv_color_hex(0xE94560)
#define COLOR_TEXT lv_color_hex(0xEEEEEE)

static lv_obj_t *scr = nullptr;

/* ── Crea un botón de juego ──────────────────────────────────── */
static void create_game_button(lv_obj_t *parent, const char *icon,
                               const char *label_text, lv_color_t bg_color,
                               game_id_t game_id) {
  lv_obj_t *btn = lv_button_create(parent);
  lv_obj_set_size(btn, 200, 50);
  lv_obj_set_style_bg_color(btn, bg_color, 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(btn, 12, 0);
  lv_obj_set_style_border_width(btn, 2, 0);
  lv_obj_set_style_border_color(btn, COLOR_ACCENT, 0);

  lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(btn, 10, 0);

  lv_obj_t *ico = lv_label_create(btn);
  lv_label_set_text(ico, icon);
  lv_obj_set_style_text_font(ico, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(ico, COLOR_ACCENT, 0);

  lv_obj_t *lbl = lv_label_create(btn);
  lv_label_set_text(lbl, label_text);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);

  lv_obj_add_event_cb(
      btn,
      [](lv_event_t *e) {
        game_id_t id = (game_id_t)(uintptr_t)lv_event_get_user_data(e);
        game_runner_launch(id);
      },
      LV_EVENT_CLICKED, (void *)(uintptr_t)game_id);
}

void screen_games_create() {
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
      btn_back, [](lv_event_t *) { ui_navigate_to(UI_SCREEN_MAIN_MENU); },
      LV_EVENT_CLICKED, nullptr);

  lv_obj_t *lbl_back = lv_label_create(btn_back);
  lv_label_set_text(lbl_back, LV_SYMBOL_LEFT " Volver");
  lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl_back);

  /* Título */
  lv_obj_t *title = lv_label_create(header);
  lv_label_set_text(title, LV_SYMBOL_PLAY "  Juegos");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
  lv_obj_center(title);

  /* ── Grilla 2×2 de botones de juegos ─────────────────────── */
  lv_obj_t *grid = lv_obj_create(scr);
  lv_obj_set_size(grid, 480, 261);
  lv_obj_align(grid, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(grid, 0, 0);
  lv_obj_set_style_pad_all(grid, 16, 0);
  lv_obj_set_style_pad_row(grid, 14, 0);
  lv_obj_set_style_pad_column(grid, 14, 0);
  lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  create_game_button(grid, LV_SYMBOL_PLAY, "Snake", lv_color_hex(0x0F3460),
                     GAME_SNAKE);
  create_game_button(grid, LV_SYMBOL_PLAY, "Pong", lv_color_hex(0x1A3A5A),
                     GAME_PONG);
  create_game_button(grid, LV_SYMBOL_PLAY, "Tetris", lv_color_hex(0x2A1A5A),
                     GAME_TETRIS);
  create_game_button(grid, LV_SYMBOL_PLAY, "Flappy Bird",
                     lv_color_hex(0x1E3A5F), GAME_FLAPPY);
}

lv_obj_t *screen_games_get() { return scr; }
