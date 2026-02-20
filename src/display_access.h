#pragma once
#include "AXS15231B_touch.h"
#include <Arduino_GFX_Library.h>

/** Retorna el puntero al canvas compartido (inicializado en main.cpp). */
Arduino_Canvas *get_canvas();

/** Retorna el puntero al controlador de touch (inicializado en main.cpp). */
AXS15231B_Touch *get_touch();

/**
 * Rotación disponible por pantalla.
 *  DISP_ROT_LANDSCAPE → 480×320 (default, TFT_ROT=1)
 *  DISP_ROT_PORTRAIT  → 320×480 (solo pantalla Mapas)
 */
typedef enum {
  DISP_ROT_LANDSCAPE = 0,
  DISP_ROT_PORTRAIT = 1,
} disp_rot_t;

/**
 * Cambia la orientación del display en runtime.
 *
 * Internamente:
 *   1. Limpia el canvas GFX con negro y hace flush → pantalla física queda
 *      completamente negra, sin rastro de píxeles anteriores.
 *   2. Llama a gfx->setRotation() para que el canvas cambie de dimensiones
 *      (hardwate/canvas level, sin rotación software de LVGL).
 *   3. Llama a lv_display_set_resolution() para que LVGL conozca las nuevas
 *      dimensiones lógicas y marque todo el display como dirty.
 *   4. Actualiza el flag interno que usa touch_read_cb para transformar
 *      coordenadas de touch.
 *
 * Llamar ANTES de lv_screen_load() para que la resolución LVGL sea correcta
 * cuando se renderice la nueva pantalla.
 */
void display_set_rotation(disp_rot_t rot);

/** Retorna la rotación actualmente aplicada. */
disp_rot_t display_get_rotation(void);
