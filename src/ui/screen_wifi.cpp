#include "screen_wifi.h"
#include "../wifi_manager.h"
#include "ui.h"

#include <cstdio>
#include <cstring>

/* ── Paleta (igual que el resto de pantallas) ────────────────── */
#define COLOR_BG lv_color_hex(0x000000)
#define COLOR_HEADER lv_color_hex(0x0A0A0A)
#define COLOR_ACCENT lv_color_hex(0xE94560)
#define COLOR_TEXT lv_color_hex(0xEEEEEE)
#define COLOR_DIM lv_color_hex(0x778899)
#define COLOR_OK lv_color_hex(0x4DA6FF) /* azul, sin verde */
#define COLOR_ERR lv_color_hex(0xE94560)

static lv_obj_t *scr = nullptr;

/* Vistas */
static lv_obj_t *view_scan = nullptr;
static lv_obj_t *view_list = nullptr;   /* lista de redes */
static lv_obj_t *view_pass = nullptr;   /* ingreso de contraseña */
static lv_obj_t *view_status = nullptr; /* resultado conexión */

/* Widgets de contraseña */
static lv_obj_t *lbl_ssid = nullptr;
static lv_obj_t *ta_pass = nullptr;
static lv_obj_t *kb = nullptr;

/* Widgets de estado */
static lv_obj_t *lbl_status = nullptr;
static lv_obj_t *lbl_ip = nullptr;
static lv_obj_t *btn_status = nullptr; /* botón "Volver" o "Reintentar" */
static lv_obj_t *lbl_btn_st = nullptr;

/* Lista de redes */
static lv_obj_t *net_list = nullptr;

/* SSID seleccionado */
static char selected_ssid[64] = {0};
static bool s_pass_visible = false; /* toggle mostrar contraseña */
static lv_obj_t *lbl_eye = nullptr; /* icono del botón ojo */

/* Timer de polling */
static lv_timer_t *poll_timer = nullptr;

/* Estado de vista actual */
typedef enum { V_SCAN, V_LIST, V_PASS, V_STATUS } view_id_t;
static view_id_t current_view = V_SCAN;

/* ── Utilidades ──────────────────────────────────────────────── */
static void show_view(view_id_t v) {
  lv_obj_add_flag(view_scan, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(view_list, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(view_pass, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(view_status, LV_OBJ_FLAG_HIDDEN);

  switch (v) {
  case V_SCAN:
    lv_obj_remove_flag(view_scan, LV_OBJ_FLAG_HIDDEN);
    break;
  case V_LIST:
    lv_obj_remove_flag(view_list, LV_OBJ_FLAG_HIDDEN);
    break;
  case V_PASS:
    lv_obj_remove_flag(view_pass, LV_OBJ_FLAG_HIDDEN);
    break;
  case V_STATUS:
    lv_obj_remove_flag(view_status, LV_OBJ_FLAG_HIDDEN);
    break;
  }
  current_view = v;
}

/* ── Poblar lista de redes ───────────────────────────────────── */
static void populate_list() {
  lv_obj_clean(net_list);

  int n = wifi_mgr_get_network_count();
  if (n == 0) {
    lv_obj_t *item = lv_list_add_text(net_list, "No se encontraron redes");
    lv_obj_set_style_text_color(item, COLOR_DIM, 0);
    return;
  }

  for (int i = 0; i < n; i++) {
    int rssi = wifi_mgr_get_rssi(i);
    /* Elegir icono según RSSI */
    const char *sig = (rssi >= -60)   ? LV_SYMBOL_WIFI
                      : (rssi >= -75) ? LV_SYMBOL_WIFI
                                      : LV_SYMBOL_WIFI;

    /* Construir label "SSID  (-XX dBm)" */
    char label[80];
    snprintf(label, sizeof(label), "%s  (%d dBm)", wifi_mgr_get_ssid(i), rssi);

    lv_obj_t *btn_item = lv_list_add_button(net_list, sig, label);
    lv_obj_set_style_text_color(btn_item, COLOR_TEXT, 0);
    lv_obj_set_style_bg_color(btn_item, COLOR_HEADER, 0);
    lv_obj_set_style_bg_opa(btn_item, LV_OPA_COVER, 0);

    /* Guardar índice como user-data para identificar cuál se pulsó */
    lv_obj_add_event_cb(
        btn_item,
        [](lv_event_t *e) {
          lv_obj_t *b = lv_event_get_target_obj(e);
          /* Obtener label del botón para extraer el SSID */
          lv_obj_t *child = lv_obj_get_child(
              b, 1); /* primer label es el icono, segundo es el texto */
          const char *txt = lv_label_get_text(child);
          /* txt tiene formato "SSID  (-XX dBm)" → tomar hasta "  (" */
          strncpy(selected_ssid, txt, sizeof(selected_ssid) - 1);
          selected_ssid[sizeof(selected_ssid) - 1] = '\0';
          char *p = strstr(selected_ssid, "  (");
          if (p)
            *p = '\0';

          /* Actualizar label de SSID en vista de contraseña */
          char buf[80];
          snprintf(buf, sizeof(buf), "Red: %s", selected_ssid);
          lv_label_set_text(lbl_ssid, buf);

          /* Limpiar textarea y resetear visibilidad de contraseña */
          lv_textarea_set_text(ta_pass, "");
          s_pass_visible = false;
          lv_textarea_set_password_mode(ta_pass, true);
          lv_label_set_text(lbl_eye, LV_SYMBOL_EYE_CLOSE);
          lv_obj_set_style_text_color(lbl_eye, COLOR_DIM, 0);

          show_view(V_PASS);
        },
        LV_EVENT_CLICKED, nullptr);
  }
}

/* ── Timer de polling (500 ms) ───────────────────────────────── */
static void poll_cb(lv_timer_t *) {
  wifi_mgr_state_t st = wifi_mgr_get_state();

  if (current_view == V_SCAN && st == WIFI_MGR_SCAN_DONE) {
    populate_list();
    show_view(V_LIST);
    return;
  }

  if (current_view == V_STATUS) {
    if (st == WIFI_MGR_CONNECTED) {
      char buf[60];
      snprintf(buf, sizeof(buf), LV_SYMBOL_OK "  Conectado!");
      lv_label_set_text(lbl_status, buf);
      lv_obj_set_style_text_color(lbl_status, COLOR_OK, 0);

      snprintf(buf, sizeof(buf), "IP: %s", wifi_mgr_get_ip());
      lv_label_set_text(lbl_ip, buf);
      lv_obj_remove_flag(lbl_ip, LV_OBJ_FLAG_HIDDEN);

      lv_label_set_text(lbl_btn_st, LV_SYMBOL_LEFT "  Volver");
    } else if (st == WIFI_MGR_FAILED) {
      lv_label_set_text(lbl_status, LV_SYMBOL_CLOSE "  Error de conexion");
      lv_obj_set_style_text_color(lbl_status, COLOR_ERR, 0);
      lv_obj_add_flag(lbl_ip, LV_OBJ_FLAG_HIDDEN);
      lv_label_set_text(lbl_btn_st, LV_SYMBOL_REFRESH "  Reintentar");
    }
  }
}

/* ── Crear pantalla WiFi ─────────────────────────────────────── */
void screen_wifi_create() {
  scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr, COLOR_BG, 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  /* ── Header ────────────────────────────────────────────── */
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
      btn_back,
      [](lv_event_t *) {
        if (current_view == V_PASS || current_view == V_STATUS) {
          show_view(V_LIST);
        } else {
          /* V_SCAN o V_LIST → volver a configuración */
          ui_navigate_to(UI_SCREEN_SETTINGS, true);
        }
      },
      LV_EVENT_CLICKED, nullptr);

  lv_obj_t *lbl_back = lv_label_create(btn_back);
  lv_label_set_text(lbl_back, LV_SYMBOL_LEFT " Volver");
  lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl_back);

  /* Título */
  lv_obj_t *title = lv_label_create(header);
  lv_label_set_text(title, LV_SYMBOL_WIFI "  WiFi");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
  lv_obj_center(title);

  /* ── Área de contenido (480 × 261) ──────────────────────── */
  /* Las 4 vistas se posicionan dentro de este área */
  int16_t cy = 59;  /* Y de inicio del contenido */
  int16_t ch = 261; /* Alto del contenido */

  /* ── Vista 1: Escaneando ─────────────────────────────── */
  view_scan = lv_obj_create(scr);
  lv_obj_set_size(view_scan, 480, ch);
  lv_obj_align(view_scan, LV_ALIGN_TOP_MID, 0, cy);
  lv_obj_set_style_bg_opa(view_scan, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(view_scan, 0, 0);
  lv_obj_set_style_pad_all(view_scan, 0, 0);
  lv_obj_clear_flag(view_scan, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *lbl_scan = lv_label_create(view_scan);
  lv_label_set_text(lbl_scan, "Buscando redes...");
  lv_obj_set_style_text_font(lbl_scan, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_scan, COLOR_DIM, 0);
  lv_obj_center(lbl_scan);

  /* ── Vista 2: Lista de redes ─────────────────────────── */
  view_list = lv_obj_create(scr);
  lv_obj_set_size(view_list, 480, ch);
  lv_obj_align(view_list, LV_ALIGN_TOP_MID, 0, cy);
  lv_obj_set_style_bg_opa(view_list, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(view_list, 0, 0);
  lv_obj_set_style_pad_all(view_list, 4, 0);
  lv_obj_clear_flag(view_list, LV_OBJ_FLAG_SCROLLABLE);

  /* Botón refrescar */
  lv_obj_t *btn_refresh = lv_button_create(view_list);
  lv_obj_set_size(btn_refresh, 40, 36);
  lv_obj_align(btn_refresh, LV_ALIGN_TOP_RIGHT, -4, 4);
  lv_obj_set_style_bg_color(btn_refresh, COLOR_HEADER, 0);
  lv_obj_set_style_radius(btn_refresh, 8, 0);
  lv_obj_add_event_cb(
      btn_refresh,
      [](lv_event_t *) {
        show_view(V_SCAN);
        wifi_mgr_start_scan();
      },
      LV_EVENT_CLICKED, nullptr);

  lv_obj_t *ico_ref = lv_label_create(btn_refresh);
  lv_label_set_text(ico_ref, LV_SYMBOL_REFRESH);
  lv_obj_set_style_text_color(ico_ref, COLOR_ACCENT, 0);
  lv_obj_center(ico_ref);

  /* Lista scrollable */
  net_list = lv_list_create(view_list);
  lv_obj_set_size(net_list, 468, ch - 12);
  lv_obj_align(net_list, LV_ALIGN_TOP_LEFT, 0, 48);
  lv_obj_set_style_bg_color(net_list, COLOR_BG, 0);
  lv_obj_set_style_border_width(net_list, 0, 0);
  lv_obj_set_style_pad_row(net_list, 4, 0);

  /* ── Vista 3: Ingreso de contraseña ──────────────────── */
  view_pass = lv_obj_create(scr);
  lv_obj_set_size(view_pass, 480, ch);
  lv_obj_align(view_pass, LV_ALIGN_TOP_MID, 0, cy);
  lv_obj_set_style_bg_opa(view_pass, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(view_pass, 0, 0);
  lv_obj_set_style_pad_all(view_pass, 0, 0);
  lv_obj_clear_flag(view_pass, LV_OBJ_FLAG_SCROLLABLE);

  /* Label SSID seleccionado */
  lbl_ssid = lv_label_create(view_pass);
  lv_label_set_text(lbl_ssid, "Red: --");
  lv_obj_set_style_text_font(lbl_ssid, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_ssid, COLOR_TEXT, 0);
  lv_obj_align(lbl_ssid, LV_ALIGN_TOP_LEFT, 10, 8);

  /* Textarea para la contraseña (más angosto para dejar lugar al botón ojo) */
  ta_pass = lv_textarea_create(view_pass);
  lv_obj_set_size(ta_pass, 270, 40);
  lv_obj_align(ta_pass, LV_ALIGN_TOP_LEFT, 10, 38);
  lv_textarea_set_placeholder_text(ta_pass, "Contraseña...");
  lv_textarea_set_password_mode(ta_pass, true);
  lv_textarea_set_one_line(ta_pass, true);
  lv_obj_set_style_bg_color(ta_pass, COLOR_HEADER, 0);
  lv_obj_set_style_text_color(ta_pass, COLOR_TEXT, 0);

  /* Botón mostrar/ocultar contraseña */
  lv_obj_t *btn_eye = lv_button_create(view_pass);
  lv_obj_set_size(btn_eye, 44, 40);
  lv_obj_align(btn_eye, LV_ALIGN_TOP_LEFT, 288, 38);
  lv_obj_set_style_bg_color(btn_eye, COLOR_HEADER, 0);
  lv_obj_set_style_radius(btn_eye, 8, 0);
  lv_obj_set_style_border_width(btn_eye, 1, 0);
  lv_obj_set_style_border_color(btn_eye, COLOR_DIM, 0);

  lbl_eye = lv_label_create(btn_eye);
  lv_label_set_text(lbl_eye, LV_SYMBOL_EYE_CLOSE);
  lv_obj_set_style_text_color(lbl_eye, COLOR_DIM, 0);
  lv_obj_center(lbl_eye);

  lv_obj_add_event_cb(
      btn_eye,
      [](lv_event_t *) {
        s_pass_visible = !s_pass_visible;
        lv_textarea_set_password_mode(ta_pass, !s_pass_visible);
        lv_label_set_text(lbl_eye, s_pass_visible ? LV_SYMBOL_EYE_OPEN
                                                  : LV_SYMBOL_EYE_CLOSE);
        lv_obj_set_style_text_color(
            lbl_eye, s_pass_visible ? COLOR_ACCENT : COLOR_DIM, 0);
      },
      LV_EVENT_CLICKED, nullptr);

  /* Botón Conectar */
  lv_obj_t *btn_conn = lv_button_create(view_pass);
  lv_obj_set_size(btn_conn, 110, 40);
  lv_obj_align(btn_conn, LV_ALIGN_TOP_RIGHT, -10, 38);
  lv_obj_set_style_bg_color(btn_conn, COLOR_OK, 0);
  lv_obj_set_style_radius(btn_conn, 8, 0);
  lv_obj_add_event_cb(
      btn_conn,
      [](lv_event_t *) {
        const char *pass = lv_textarea_get_text(ta_pass);
        wifi_mgr_connect(selected_ssid, pass);

        /* Mostrar vista de estado */
        lv_label_set_text(lbl_status, LV_SYMBOL_REFRESH "  Conectando...");
        lv_obj_set_style_text_color(lbl_status, COLOR_DIM, 0);
        lv_label_set_text(lbl_ip, "");
        lv_obj_add_flag(lbl_ip, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(lbl_btn_st, "...");
        show_view(V_STATUS);
      },
      LV_EVENT_CLICKED, nullptr);

  lv_obj_t *lbl_conn = lv_label_create(btn_conn);
  lv_label_set_text(lbl_conn, LV_SYMBOL_WIFI " Conectar");
  lv_obj_set_style_text_font(lbl_conn, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl_conn);

  /* Teclado en pantalla */
  kb = lv_keyboard_create(view_pass);
  lv_obj_set_size(kb, 480, 180);
  lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_keyboard_set_textarea(kb, ta_pass);
  lv_obj_set_style_bg_color(kb, COLOR_HEADER, 0);

  /* ── Vista 4: Estado de conexión ─────────────────────── */
  view_status = lv_obj_create(scr);
  lv_obj_set_size(view_status, 480, ch);
  lv_obj_align(view_status, LV_ALIGN_TOP_MID, 0, cy);
  lv_obj_set_style_bg_opa(view_status, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(view_status, 0, 0);
  lv_obj_set_style_pad_all(view_status, 0, 0);
  lv_obj_clear_flag(view_status, LV_OBJ_FLAG_SCROLLABLE);

  lbl_status = lv_label_create(view_status);
  lv_label_set_text(lbl_status, LV_SYMBOL_REFRESH "  Conectando...");
  lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_status, COLOR_DIM, 0);
  lv_obj_align(lbl_status, LV_ALIGN_CENTER, 0, -30);

  lbl_ip = lv_label_create(view_status);
  lv_label_set_text(lbl_ip, "");
  lv_obj_set_style_text_font(lbl_ip, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_ip, COLOR_OK, 0);
  lv_obj_align(lbl_ip, LV_ALIGN_CENTER, 0, 10);
  lv_obj_add_flag(lbl_ip, LV_OBJ_FLAG_HIDDEN);

  btn_status = lv_button_create(view_status);
  lv_obj_set_size(btn_status, 160, 40);
  lv_obj_align(btn_status, LV_ALIGN_CENTER, 0, 60);
  lv_obj_set_style_bg_color(btn_status, COLOR_HEADER, 0);
  lv_obj_set_style_radius(btn_status, 8, 0);
  lv_obj_add_event_cb(
      btn_status,
      [](lv_event_t *) {
        wifi_mgr_state_t st = wifi_mgr_get_state();
        if (st == WIFI_MGR_CONNECTED) {
          /* Ya conectado: volver a la lista */
          show_view(V_LIST);
        } else {
          /* Error o todavía conectando: reintentar (volver a lista) */
          wifi_mgr_disconnect();
          populate_list();
          show_view(V_LIST);
        }
      },
      LV_EVENT_CLICKED, nullptr);

  lbl_btn_st = lv_label_create(btn_status);
  lv_label_set_text(lbl_btn_st, "...");
  lv_obj_set_style_text_font(lbl_btn_st, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl_btn_st);

  /* Arrancar en vista de escaneo (ocultar las demás) */
  show_view(V_SCAN);
}

lv_obj_t *screen_wifi_get() { return scr; }

void screen_wifi_start() {
  /* Parar timer anterior si existía */
  if (poll_timer) {
    lv_timer_delete(poll_timer);
    poll_timer = nullptr;
  }

  /* Mostrar spinner y arrancar escaneo */
  show_view(V_SCAN);
  wifi_mgr_start_scan();

  /* Crear timer de polling */
  poll_timer = lv_timer_create(poll_cb, 500, nullptr);
}
