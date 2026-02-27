#include "screen_phone.h"
#include "../../include/maps_ws_server.h"
#include "ui.h"
#include <cstring>
#include <cstdio>

/* ── Paleta ──────────────────────────────────────────────────────── */
#define CLR_BG        lv_color_hex(0x111827)
#define CLR_HEADER    lv_color_hex(0x0A0A0A)
#define CLR_SECTION   lv_color_hex(0x1F2937)
#define CLR_DIVIDER   lv_color_hex(0x374151)
#define CLR_TEXT      lv_color_hex(0xEEEEEE)
#define CLR_SUBTEXT   lv_color_hex(0x9CA3AF)
#define CLR_ACCENT    lv_color_hex(0x3B82F6)   /* azul nav */
#define CLR_GREEN     lv_color_hex(0x22C55E)
#define CLR_MUSIC     lv_color_hex(0xA855F7)   /* violeta música */
#define CLR_BTN       lv_color_hex(0x374151)
#define CLR_BTN_PR    lv_color_hex(0x4B5563)
#define CLR_NOTIF_BG  lv_color_hex(0x1A2332)

/* ── Dimensiones del layout (480×320) ───────────────────────────── */
#define H_HEADER   28
#define H_NAV      76
#define H_MEDIA    80
#define H_NOTIF   136   /* 3 × 44px + 4px padding */

#define BTN_W  52
#define BTN_H  52
#define BTN_R  10

/* ── Widgets estáticos ───────────────────────────────────────────── */
static lv_obj_t *scr = nullptr;

/* Header */
static lv_obj_t *lbl_conn = nullptr;

/* Sección Google Maps */
static lv_obj_t *lbl_maneuver  = nullptr;
static lv_obj_t *lbl_step      = nullptr;
static lv_obj_t *lbl_nav_sub   = nullptr;  /* calle + dist/eta */

/* Sección música */
static lv_obj_t *lbl_track     = nullptr;
static lv_obj_t *lbl_play_icon = nullptr;  /* ▶ / ⏸ */

/* Sección notificaciones */
static lv_obj_t *lbl_notif[3] = {};

/* Buffer de notificaciones (ring buffer, posición 0 = la más reciente) */
static phone_notif_t s_notifs[3] = {};
static uint8_t       s_notif_count = 0;

/* ── Símbolo de maniobra ─────────────────────────────────────────── */
static const char *maneuver_symbol(const char *m) {
    if (!m || !m[0]) return LV_SYMBOL_UP;
    if (strstr(m, "uturn"))      return LV_SYMBOL_LOOP;
    if (strstr(m, "roundabout")) return LV_SYMBOL_REFRESH;
    if (strstr(m, "left"))       return LV_SYMBOL_LEFT;
    if (strstr(m, "right"))      return LV_SYMBOL_RIGHT;
    if (strstr(m, "arrive"))     return LV_SYMBOL_HOME;
    return LV_SYMBOL_UP;   /* straight / default */
}

/* ── Helpers para crear controles ───────────────────────────────── */
static lv_obj_t *make_btn(lv_obj_t *parent, const char *icon,
                           lv_color_t bg, lv_event_cb_t cb, void *user_data) {
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_set_size(btn, BTN_W, BTN_H);
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_bg_color(btn, CLR_BTN_PR, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, BTN_R, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, icon);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl, CLR_TEXT, 0);
    lv_obj_center(lbl);

    if (cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    return btn;
}

/* ── Callbacks de botones de música ─────────────────────────────── */
static void on_btn_prev(lv_event_t *)  { maps_ws_send_media_cmd("prev"); }
static void on_btn_play(lv_event_t *)  { maps_ws_send_media_cmd("play"); }
static void on_btn_pause(lv_event_t *) { maps_ws_send_media_cmd("pause"); }
static void on_btn_next(lv_event_t *)  { maps_ws_send_media_cmd("next"); }
static void on_btn_vold(lv_event_t *)  { maps_ws_send_media_cmd("vol_down"); }
static void on_btn_volu(lv_event_t *)  { maps_ws_send_media_cmd("vol_up"); }

/* Play/Pause unificado: inspecciona el icono actual para saber estado */
static bool s_playing = false;
static void on_btn_play_pause(lv_event_t *) {
    maps_ws_send_media_cmd(s_playing ? "pause" : "play");
}

/* ── screen_phone_create ─────────────────────────────────────────── */
void screen_phone_create() {
    scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, CLR_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* ═══════════════════════════════════════════════════════════════
     * HEADER (y=0, h=28)
     * ═══════════════════════════════════════════════════════════════ */
    lv_obj_t *header = lv_obj_create(scr);
    lv_obj_set_size(header, 480, H_HEADER);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, CLR_HEADER, 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    /* Botón volver */
    lv_obj_t *btn_back = lv_obj_create(header);
    lv_obj_set_size(btn_back, 70, H_HEADER);
    lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_opa(btn_back, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_back, 0, 0);
    lv_obj_set_style_pad_all(btn_back, 0, 0);
    lv_obj_clear_flag(btn_back, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn_back,
        [](lv_event_t *) { ui_navigate_to(UI_SCREEN_MAIN_MENU, true); },
        LV_EVENT_CLICKED, nullptr);

    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT " MENU");
    lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_back, CLR_SUBTEXT, 0);
    lv_obj_center(lbl_back);

    /* Título */
    lv_obj_t *lbl_title = lv_label_create(header);
    lv_label_set_text(lbl_title, LV_SYMBOL_CALL "  TELEFONO");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_title, CLR_TEXT, 0);
    lv_obj_center(lbl_title);

    /* Indicador de conexión */
    lbl_conn = lv_label_create(header);
    lv_label_set_text(lbl_conn, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(lbl_conn, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_conn, CLR_GREEN, 0);
    lv_obj_align(lbl_conn, LV_ALIGN_RIGHT_MID, -8, 0);

    /* ═══════════════════════════════════════════════════════════════
     * SECCIÓN GOOGLE MAPS (y=28, h=76)
     * ═══════════════════════════════════════════════════════════════ */
    lv_obj_t *nav_sec = lv_obj_create(scr);
    lv_obj_set_size(nav_sec, 480, H_NAV);
    lv_obj_align(nav_sec, LV_ALIGN_TOP_MID, 0, H_HEADER);
    lv_obj_set_style_bg_color(nav_sec, CLR_SECTION, 0);
    lv_obj_set_style_bg_opa(nav_sec, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(nav_sec, 0, 0);
    lv_obj_set_style_radius(nav_sec, 0, 0);
    lv_obj_set_style_pad_all(nav_sec, 0, 0);
    lv_obj_clear_flag(nav_sec, LV_OBJ_FLAG_SCROLLABLE);

    /* Caja de ícono de maniobra */
    lv_obj_t *maneuver_box = lv_obj_create(nav_sec);
    lv_obj_set_size(maneuver_box, 60, 60);
    lv_obj_align(maneuver_box, LV_ALIGN_LEFT_MID, 8, 0);
    lv_obj_set_style_bg_color(maneuver_box, CLR_ACCENT, 0);
    lv_obj_set_style_bg_opa(maneuver_box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(maneuver_box, 10, 0);
    lv_obj_set_style_border_width(maneuver_box, 0, 0);
    lv_obj_set_style_pad_all(maneuver_box, 0, 0);
    lv_obj_clear_flag(maneuver_box, LV_OBJ_FLAG_SCROLLABLE);

    lbl_maneuver = lv_label_create(maneuver_box);
    lv_label_set_text(lbl_maneuver, LV_SYMBOL_UP);
    lv_obj_set_style_text_font(lbl_maneuver, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_maneuver, lv_color_white(), 0);
    lv_obj_center(lbl_maneuver);

    /* Texto del paso (instrucción) */
    lbl_step = lv_label_create(nav_sec);
    lv_label_set_text(lbl_step, "Sin navegacion activa");
    lv_obj_set_style_text_font(lbl_step, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_step, CLR_TEXT, 0);
    lv_label_set_long_mode(lbl_step, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lbl_step, 390);
    lv_obj_align(lbl_step, LV_ALIGN_LEFT_MID, 78, -12);

    /* Subtexto: calle + distancia/ETA */
    lbl_nav_sub = lv_label_create(nav_sec);
    lv_label_set_text(lbl_nav_sub, "");
    lv_obj_set_style_text_font(lbl_nav_sub, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_nav_sub, CLR_SUBTEXT, 0);
    lv_label_set_long_mode(lbl_nav_sub, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lbl_nav_sub, 390);
    lv_obj_align(lbl_nav_sub, LV_ALIGN_LEFT_MID, 78, 12);

    /* Separador */
    lv_obj_t *div1 = lv_obj_create(scr);
    lv_obj_set_size(div1, 480, 2);
    lv_obj_align(div1, LV_ALIGN_TOP_MID, 0, H_HEADER + H_NAV);
    lv_obj_set_style_bg_color(div1, CLR_DIVIDER, 0);
    lv_obj_set_style_bg_opa(div1, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(div1, 0, 0);
    lv_obj_set_style_radius(div1, 0, 0);

    /* ═══════════════════════════════════════════════════════════════
     * SECCIÓN MÚSICA (y=106, h=80)
     * ═══════════════════════════════════════════════════════════════ */
    const int y_media = H_HEADER + H_NAV + 2;

    lv_obj_t *media_sec = lv_obj_create(scr);
    lv_obj_set_size(media_sec, 480, H_MEDIA);
    lv_obj_align(media_sec, LV_ALIGN_TOP_MID, 0, y_media);
    lv_obj_set_style_bg_color(media_sec, CLR_BG, 0);
    lv_obj_set_style_bg_opa(media_sec, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(media_sec, 0, 0);
    lv_obj_set_style_radius(media_sec, 0, 0);
    lv_obj_set_style_pad_all(media_sec, 0, 0);
    lv_obj_clear_flag(media_sec, LV_OBJ_FLAG_SCROLLABLE);

    /* Ícono nota musical */
    lv_obj_t *note_box = lv_obj_create(media_sec);
    lv_obj_set_size(note_box, 32, 32);
    lv_obj_align(note_box, LV_ALIGN_LEFT_MID, 8, -12);
    lv_obj_set_style_bg_color(note_box, CLR_MUSIC, 0);
    lv_obj_set_style_bg_opa(note_box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(note_box, 8, 0);
    lv_obj_set_style_border_width(note_box, 0, 0);
    lv_obj_set_style_pad_all(note_box, 0, 0);
    lv_obj_clear_flag(note_box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *note_ico = lv_label_create(note_box);
    lv_label_set_text(note_ico, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_font(note_ico, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(note_ico, lv_color_white(), 0);
    lv_obj_center(note_ico);

    /* Texto canción/artista */
    lbl_track = lv_label_create(media_sec);
    lv_label_set_text(lbl_track, "Sin musica activa");
    lv_obj_set_style_text_font(lbl_track, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_track, CLR_TEXT, 0);
    lv_label_set_long_mode(lbl_track, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lbl_track, 178);
    lv_obj_align(lbl_track, LV_ALIGN_LEFT_MID, 46, -12);

    /* Botones de control: posicionados a la derecha */
    /* Prev */
    lv_obj_t *btn_prev = make_btn(media_sec, LV_SYMBOL_PREV, CLR_BTN, on_btn_prev, nullptr);
    lv_obj_align(btn_prev, LV_ALIGN_RIGHT_MID, -(BTN_W * 4 + 20), 0);

    /* Play/Pause */
    lv_obj_t *btn_pp = make_btn(media_sec, LV_SYMBOL_PLAY, CLR_ACCENT, on_btn_play_pause, nullptr);
    lv_obj_align(btn_pp, LV_ALIGN_RIGHT_MID, -(BTN_W * 3 + 14), 0);
    /* Guardamos referencia al label del botón para cambiar el ícono */
    lbl_play_icon = lv_obj_get_child(btn_pp, 0);

    /* Next */
    lv_obj_t *btn_next = make_btn(media_sec, LV_SYMBOL_NEXT, CLR_BTN, on_btn_next, nullptr);
    lv_obj_align(btn_next, LV_ALIGN_RIGHT_MID, -(BTN_W * 2 + 8), 0);

    /* Vol- */
    lv_obj_t *btn_vd = make_btn(media_sec, LV_SYMBOL_VOLUME_MID, CLR_BTN, on_btn_vold, nullptr);
    lv_obj_align(btn_vd, LV_ALIGN_RIGHT_MID, -(BTN_W + 4), 0);

    /* Vol+ */
    lv_obj_t *btn_vu = make_btn(media_sec, LV_SYMBOL_VOLUME_MAX, CLR_BTN, on_btn_volu, nullptr);
    lv_obj_align(btn_vu, LV_ALIGN_RIGHT_MID, 0, 0);

    /* Separador */
    lv_obj_t *div2 = lv_obj_create(scr);
    lv_obj_set_size(div2, 480, 2);
    lv_obj_align(div2, LV_ALIGN_TOP_MID, 0, y_media + H_MEDIA);
    lv_obj_set_style_bg_color(div2, CLR_DIVIDER, 0);
    lv_obj_set_style_bg_opa(div2, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(div2, 0, 0);
    lv_obj_set_style_radius(div2, 0, 0);

    /* ═══════════════════════════════════════════════════════════════
     * SECCIÓN NOTIFICACIONES (y=188, h=130)
     * ═══════════════════════════════════════════════════════════════ */
    const int y_notif = y_media + H_MEDIA + 2;
    const int row_h   = (320 - y_notif) / 3;

    for (int i = 0; i < 3; i++) {
        lv_obj_t *row = lv_obj_create(scr);
        lv_obj_set_size(row, 480, row_h);
        lv_obj_align(row, LV_ALIGN_TOP_MID, 0, y_notif + i * row_h);
        lv_obj_set_style_bg_color(row, (i % 2 == 0) ? CLR_NOTIF_BG : CLR_BG, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_set_style_pad_hor(row, 8, 0);
        lv_obj_set_style_pad_ver(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lbl_notif[i] = lv_label_create(row);
        lv_label_set_text(lbl_notif[i], "");
        lv_obj_set_style_text_font(lbl_notif[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl_notif[i], CLR_SUBTEXT, 0);
        lv_label_set_long_mode(lbl_notif[i], LV_LABEL_LONG_DOT);
        lv_obj_set_width(lbl_notif[i], 464);
        lv_obj_align(lbl_notif[i], LV_ALIGN_LEFT_MID, 0, 0);
    }
}

/* ── screen_phone_get ────────────────────────────────────────────── */
lv_obj_t *screen_phone_get() { return scr; }

/* ── Callbacks del WebSocket ─────────────────────────────────────── */
static void on_gmaps(const gmaps_step_t &step) {
    if (!lbl_step) return;
    lv_label_set_text(lbl_maneuver, maneuver_symbol(step.maneuver));

    lv_label_set_text(lbl_step, step.step[0] ? step.step : "Navegando...");

    /* Sub: "Av. Corrientes  |  200m  |  15 min" */
    char sub[80];
    if (step.street[0] && step.dist[0] && step.eta[0])
        snprintf(sub, sizeof(sub), "%s  |  %s  |  %s", step.street, step.dist, step.eta);
    else if (step.street[0])
        snprintf(sub, sizeof(sub), "%s", step.street);
    else if (step.dist[0])
        snprintf(sub, sizeof(sub), "%s  |  %s", step.dist, step.eta);
    else
        sub[0] = '\0';
    lv_label_set_text(lbl_nav_sub, sub);
}

static void on_media(const media_state_t &media) {
    if (!lbl_track) return;

    /* Título y artista */
    char track[80];
    if (media.artist[0])
        snprintf(track, sizeof(track), "%s - %s", media.title, media.artist);
    else
        snprintf(track, sizeof(track), "%s", media.title);
    lv_label_set_text(lbl_track, track[0] ? track : "Sin musica activa");

    /* Icono play/pause */
    s_playing = media.playing;
    if (lbl_play_icon)
        lv_label_set_text(lbl_play_icon,
                          media.playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
}

static void on_notif(const phone_notif_t &notif) {
    /* Shift: notif más reciente va al índice 0 */
    memmove(&s_notifs[1], &s_notifs[0], sizeof(phone_notif_t) * 2);
    s_notifs[0] = notif;
    if (s_notif_count < 3) s_notif_count++;

    /* Actualizar labels */
    for (int i = 0; i < 3; i++) {
        if (!lbl_notif[i]) continue;
        if (i >= s_notif_count) {
            lv_label_set_text(lbl_notif[i], "");
            continue;
        }
        char line[96];
        if (s_notifs[i].text[0])
            snprintf(line, sizeof(line), "[%s] %s: %s",
                     s_notifs[i].app, s_notifs[i].title, s_notifs[i].text);
        else
            snprintf(line, sizeof(line), "[%s] %s",
                     s_notifs[i].app, s_notifs[i].title);
        lv_label_set_text(lbl_notif[i], line);
        lv_obj_set_style_text_color(lbl_notif[i], CLR_TEXT, 0);
    }
}

/* ── screen_phone_start ──────────────────────────────────────────── */
void screen_phone_start() {
    maps_ws_init();   /* arranca AP + WS si no estaba ya activo */
    maps_ws_set_gmaps_cb(on_gmaps);
    maps_ws_set_media_cb(on_media);
    maps_ws_set_notif_cb(on_notif);
}
