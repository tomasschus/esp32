#include "screen_main_menu.h"
#include "../wifi_manager.h"
#include "ui.h"

/* ── Colores de la paleta del menú ──────────────────────────── */
#define COLOR_BG lv_color_hex(0x1E3A5F)        // azul oscuro
#define COLOR_HEADER lv_color_hex(0x0A0A0A)    // negro suave
#define COLOR_BTN_GAMES lv_color_hex(0x0F3460) // azul medio
#define COLOR_BTN_TOOLS lv_color_hex(0x1E3A5F) // azul
#define COLOR_BTN_CONF lv_color_hex(0x3D1A5A)  // violeta oscuro
#define COLOR_ACCENT lv_color_hex(0xE94560)    // rojo/rosa
#define COLOR_TEXT lv_color_hex(0xEEEEEE)      // blanco suave

static lv_obj_t *scr = nullptr;
static lv_obj_t *lbl_wifi_ind = nullptr; /* indicador WiFi en header */

/* ── Crea un botón grande con icono + texto ─────────────────── */
static lv_obj_t *create_menu_button(lv_obj_t *parent, const char *icon,
                                    const char *label_text, lv_color_t bg_color,
                                    ui_screen_id_t target) {
  lv_obj_t *btn = lv_button_create(parent);
  lv_obj_set_size(btn, 140, 220);

  /* Fondo del botón */
  lv_obj_set_style_bg_color(btn, bg_color, 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(btn, 16, 0);
  lv_obj_set_style_border_width(btn, 2, 0);
  lv_obj_set_style_border_color(btn, COLOR_ACCENT, 0);
  lv_obj_set_style_shadow_width(btn, 20, 0);
  lv_obj_set_style_shadow_color(btn, COLOR_ACCENT, 0);
  lv_obj_set_style_shadow_opa(btn, LV_OPA_30, 0);

  /* Estado presionado: la barra de borde se apaga (el tema maneja el efecto de
   * pulsado) */
  lv_obj_set_style_border_color(btn, lv_color_white(), LV_STATE_PRESSED);

  /* Layout vertical centrado */
  lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  /* Icono (símbolo LVGL grande) */
  lv_obj_t *ico = lv_label_create(btn);
  lv_label_set_text(ico, icon);
  lv_obj_set_style_text_font(ico, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(ico, COLOR_ACCENT, 0);

  /* Separador visual */
  lv_obj_t *sep = lv_obj_create(btn);
  lv_obj_set_size(sep, 60, 2);
  lv_obj_set_style_bg_color(sep, COLOR_ACCENT, 0);
  lv_obj_set_style_bg_opa(sep, LV_OPA_50, 0);
  lv_obj_set_style_border_width(sep, 0, 0);
  lv_obj_set_style_pad_all(sep, 0, 0);
  lv_obj_set_style_margin_top(sep, 8, 0);
  lv_obj_set_style_margin_bottom(sep, 8, 0);

  /* Etiqueta de texto */
  lv_obj_t *lbl = lv_label_create(btn);
  lv_label_set_text(lbl, label_text);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);

  /* Evento de click → navegar */
  lv_obj_add_event_cb(
      btn,
      [](lv_event_t *e) {
        ui_screen_id_t id =
            (ui_screen_id_t)(uintptr_t)lv_event_get_user_data(e);
        ui_navigate_to(id);
      },
      LV_EVENT_CLICKED, (void *)(uintptr_t)target);

  return btn;
}

/* ── screen_main_menu_create ────────────────────────────────── */
void screen_main_menu_create() {
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

  /* Barra de acento en la parte inferior del header */
  lv_obj_t *accent_bar = lv_obj_create(scr);
  lv_obj_set_size(accent_bar, 480, 3);
  lv_obj_align(accent_bar, LV_ALIGN_TOP_MID, 0, 56);
  lv_obj_set_style_bg_color(accent_bar, COLOR_ACCENT, 0);
  lv_obj_set_style_bg_opa(accent_bar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(accent_bar, 0, 0);
  lv_obj_set_style_radius(accent_bar, 0, 0);

  /* Título */
  lv_obj_t *title = lv_label_create(header);
  lv_label_set_text(title, LV_SYMBOL_HOME "  ESP32 Launcher");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
  lv_obj_center(title);

  /* Indicador WiFi (esquina superior derecha del header) */
  lbl_wifi_ind = lv_label_create(header);
  lv_label_set_text(lbl_wifi_ind, LV_SYMBOL_WIFI);
  lv_obj_set_style_text_font(lbl_wifi_ind, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lbl_wifi_ind, lv_color_hex(0x4DA6FF), 0);
  lv_obj_align(lbl_wifi_ind, LV_ALIGN_RIGHT_MID, -10, 0);
  lv_obj_add_flag(lbl_wifi_ind, LV_OBJ_FLAG_HIDDEN); /* oculto por defecto */

  /* Timer que actualiza el indicador cada 1 s */
  lv_timer_create(
      [](lv_timer_t *) {
        if (wifi_mgr_get_state() == WIFI_MGR_CONNECTED) {
          lv_obj_remove_flag(lbl_wifi_ind, LV_OBJ_FLAG_HIDDEN);
        } else {
          lv_obj_add_flag(lbl_wifi_ind, LV_OBJ_FLAG_HIDDEN);
        }
      },
      1000, nullptr);

  /* ── Contenedor de botones (flex row) ───────────────────── */
  lv_obj_t *btn_cont = lv_obj_create(scr);
  lv_obj_set_size(btn_cont, 480, 261); /* 320 - 56 header - 3 barra */
  lv_obj_align(btn_cont, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_opa(btn_cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(btn_cont, 0, 0);
  lv_obj_set_style_pad_all(btn_cont, 10, 0);
  lv_obj_set_style_pad_column(btn_cont, 15, 0);
  lv_obj_clear_flag(btn_cont, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  /* Botones */
  create_menu_button(btn_cont, LV_SYMBOL_PLAY, "JUEGOS", COLOR_BTN_GAMES,
                     UI_SCREEN_GAMES);

  create_menu_button(btn_cont, LV_SYMBOL_SETTINGS, "HERRAMIENTAS",
                     COLOR_BTN_TOOLS, UI_SCREEN_TOOLS);

  create_menu_button(btn_cont, LV_SYMBOL_LIST, "CONFIGURACION", COLOR_BTN_CONF,
                     UI_SCREEN_SETTINGS);
}

lv_obj_t *screen_main_menu_get() { return scr; }
