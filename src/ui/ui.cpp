#include "ui.h"
#include "screen_games.h"
#include "screen_main_menu.h"
#include "screen_map.h"
#include "screen_player.h"
#include "screen_settings.h"
#include "screen_timer.h"
#include "screen_tools.h"
#include "screen_wifi.h"
#include "screen_wifi_analyzer.h"

void ui_init() {
  screen_main_menu_create();
  screen_games_create();
  screen_tools_create();
  screen_settings_create();
  screen_wifi_create();
  screen_wifi_analyzer_create();
  screen_timer_create();
  screen_player_create();
  screen_map_create();

  /* Cargar el menú principal al inicio */
  lv_screen_load(screen_main_menu_get());
}

void ui_navigate_to(ui_screen_id_t screen_id, bool back) {
  (void)back;
  lv_obj_t *target = nullptr;

  switch (screen_id) {
  case UI_SCREEN_MAIN_MENU:
    target = screen_main_menu_get();
    break;
  case UI_SCREEN_GAMES:
    target = screen_games_get();
    break;
  case UI_SCREEN_TOOLS:
    target = screen_tools_get();
    break;
  case UI_SCREEN_SETTINGS:
    target = screen_settings_get();
    break;
  case UI_SCREEN_WIFI:
    screen_wifi_start();
    target = screen_wifi_get();
    break;
  case UI_SCREEN_WIFI_ANALYZER:
    screen_wifi_analyzer_start();
    target = screen_wifi_analyzer_get();
    break;
  case UI_SCREEN_TIMER:
    target = screen_timer_get();
    break;
  case UI_SCREEN_PLAYER:
    target = screen_player_get();
    break;
  case UI_SCREEN_MAPS:
    screen_map_start();
    target = screen_map_get();
    break;
  default:
    return;
  }

  lv_screen_load(target); /* Cambio instantáneo, sin animación */
}
