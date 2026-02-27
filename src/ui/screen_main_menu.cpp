#include "screen_main_menu.h"
#include "../wifi_manager.h"
#include "ui.h"

/* ── Colores de la paleta del menú ──────────────────────────── */
#define COLOR_BG      lv_color_hex(0x111827)   // fondo oscuro
#define COLOR_HEADER  lv_color_hex(0x0A0A0A)   // negro suave
#define COLOR_ACCENT  lv_color_hex(0xE94560)   // rojo/rosa
#define COLOR_TEXT    lv_color_hex(0xEEEEEE)   // blanco suave
#define COLOR_SUBTEXT lv_color_hex(0x888888)   // gris

static lv_obj_t *scr          = nullptr;
static lv_obj_t *lbl_wifi_ind = nullptr;

/* ── Crea un ítem de lista estilo WiFi-analyzer ─────────────── */
static void create_list_item(lv_obj_t *parent,
                              const char *icon, lv_color_t icon_bg,
                              const char *name, const char *desc,
                              ui_screen_id_t target) {

    /* Tarjeta */
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, 454, 44);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    /* Hacer clickeable para tap y para que el scroll chain funcione */
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);

    /* Efecto hover/pressed */
    lv_obj_set_style_bg_color(card, lv_color_hex(0x374151), LV_STATE_PRESSED);

    /* Caja de ícono (izquierda) */
    lv_obj_t *icon_box = lv_obj_create(card);
    lv_obj_set_size(icon_box, 34, 34);
    lv_obj_align(icon_box, LV_ALIGN_LEFT_MID, 8, 0);
    lv_obj_set_style_bg_color(icon_box, icon_bg, 0);
    lv_obj_set_style_bg_opa(icon_box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(icon_box, 10, 0);
    lv_obj_set_style_border_width(icon_box, 0, 0);
    lv_obj_set_style_pad_all(icon_box, 0, 0);
    lv_obj_clear_flag(icon_box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ico_lbl = lv_label_create(icon_box);
    lv_label_set_text(ico_lbl, icon);
    lv_obj_set_style_text_font(ico_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(ico_lbl, lv_color_white(), 0);
    lv_obj_center(ico_lbl);

    /* Nombre */
    lv_obj_t *lbl_name = lv_label_create(card);
    lv_label_set_text(lbl_name, name);
    lv_obj_set_style_text_font(lbl_name, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_name, COLOR_TEXT, 0);
    lv_obj_align(lbl_name, LV_ALIGN_LEFT_MID, 52, -7);

    /* Descripción */
    lv_obj_t *lbl_desc = lv_label_create(card);
    lv_label_set_text(lbl_desc, desc);
    lv_obj_set_style_text_font(lbl_desc, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_desc, COLOR_SUBTEXT, 0);
    lv_obj_align(lbl_desc, LV_ALIGN_LEFT_MID, 52, 7);

    /* Flecha derecha */
    lv_obj_t *arrow = lv_label_create(card);
    lv_label_set_text(arrow, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(arrow, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(arrow, COLOR_SUBTEXT, 0);
    lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -10, 0);

    /* Evento click → navegar */
    lv_obj_add_event_cb(
        card,
        [](lv_event_t *e) {
            ui_screen_id_t id = (ui_screen_id_t)(uintptr_t)lv_event_get_user_data(e);
            ui_navigate_to(id);
        },
        LV_EVENT_CLICKED, (void *)(uintptr_t)target);
}

/* ── screen_main_menu_create ────────────────────────────────── */
void screen_main_menu_create() {
    scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* ── Header ─────────────────────────────────────────────── */
    lv_obj_t *header = lv_obj_create(scr);
    lv_obj_set_size(header, 480, 50);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, COLOR_HEADER, 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, LV_SYMBOL_HOME "  ESP32 Launcher");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
    lv_obj_center(title);

    /* Indicador WiFi */
    lbl_wifi_ind = lv_label_create(header);
    lv_label_set_text(lbl_wifi_ind, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(lbl_wifi_ind, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_wifi_ind, lv_color_hex(0x4DA6FF), 0);
    lv_obj_align(lbl_wifi_ind, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_add_flag(lbl_wifi_ind, LV_OBJ_FLAG_HIDDEN);

    /* Timer WiFi */
    lv_timer_create(
        [](lv_timer_t *) {
            if (wifi_mgr_get_state() == WIFI_MGR_CONNECTED)
                lv_obj_remove_flag(lbl_wifi_ind, LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(lbl_wifi_ind, LV_OBJ_FLAG_HIDDEN);
        },
        1000, nullptr);

    /* ── Barra de acento ─────────────────────────────────────── */
    lv_obj_t *accent_bar = lv_obj_create(scr);
    lv_obj_set_size(accent_bar, 480, 2);
    lv_obj_align(accent_bar, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_style_bg_color(accent_bar, COLOR_ACCENT, 0);
    lv_obj_set_style_bg_opa(accent_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(accent_bar, 0, 0);
    lv_obj_set_style_radius(accent_bar, 0, 0);

    /* ── Lista scrolleable ──────────────────────────────────── */
    lv_obj_t *list = lv_obj_create(scr);
    lv_obj_set_size(list, 480, 268); /* 320 - 50 - 2 */
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_hor(list, 13, 0);
    lv_obj_set_style_pad_ver(list, 8, 0);
    lv_obj_set_style_pad_row(list, 6, 0);
    lv_obj_clear_flag(list, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* ── Ítems ──────────────────────────────────────────────── */
    create_list_item(list,
        LV_SYMBOL_CALL,     lv_color_hex(0x1A7A5A),
        "TELEFONO",         "Notificaciones y musica",
        UI_SCREEN_PHONE);

    create_list_item(list,
        LV_SYMBOL_GPS,      lv_color_hex(0x1A4DA6),
        "MAPA",             "Navegacion GPS",
        UI_SCREEN_MAPS);

    create_list_item(list,
        LV_SYMBOL_PLAY,     lv_color_hex(0xA6001A),
        "JUEGOS",           "Minijuegos",
        UI_SCREEN_GAMES);

    create_list_item(list,
        LV_SYMBOL_SETTINGS, lv_color_hex(0x1A6A3A),
        "HERRAMIENTAS",     "Utilidades del dispositivo",
        UI_SCREEN_TOOLS);

    create_list_item(list,
        LV_SYMBOL_LIST,     lv_color_hex(0x5A1A7A),
        "CONFIGURACION",    "Ajustes del sistema",
        UI_SCREEN_SETTINGS);
}

lv_obj_t *screen_main_menu_get() { return scr; }
