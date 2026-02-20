#pragma once
#include <lvgl.h>

void      screen_wifi_create();
lv_obj_t* screen_wifi_get();

/** Llamar justo antes de navegar a la pantalla WiFi. */
void      screen_wifi_start();
