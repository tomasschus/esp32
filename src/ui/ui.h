#pragma once

#include <lvgl.h>

/**
 * Inicializa la interfaz y carga la pantalla principal.
 * Llamar una sola vez, después de lv_init().
 */
void ui_init();

/**
 * Pantallas disponibles (para navegación inter-pantalla).
 */
typedef enum {
  UI_SCREEN_MAIN_MENU = 0,
  UI_SCREEN_GAMES,
  UI_SCREEN_TOOLS,
  UI_SCREEN_SETTINGS,
  UI_SCREEN_WIFI,
  UI_SCREEN_WIFI_ANALYZER,
  UI_SCREEN_TIMER,
  UI_SCREEN_PLAYER,
  UI_SCREEN_MAPS,
} ui_screen_id_t;

/** Navegar a una pantalla (cambio instantáneo, sin animación). */
void ui_navigate_to(ui_screen_id_t screen_id, bool back = false);
