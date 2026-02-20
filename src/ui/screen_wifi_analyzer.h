#pragma once
#include <lvgl.h>

void      screen_wifi_analyzer_create();
lv_obj_t* screen_wifi_analyzer_get();

/** Llamar justo antes de navegar a esta pantalla. */
void      screen_wifi_analyzer_start();
