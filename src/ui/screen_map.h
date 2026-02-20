#pragma once

#include <lvgl.h>

void screen_map_create(void);
lv_obj_t *screen_map_get(void);
/** Llamar al entrar a la pantalla: inicia AP + WebSocket y muestra el mapa. */
void screen_map_start(void);
/** Llamar al salir (opcional): detiene servidor y AP. */
void screen_map_stop(void);
