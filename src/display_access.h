#pragma once
#include <Arduino_GFX_Library.h>
#include "AXS15231B_touch.h"

/** Retorna el puntero al canvas compartido (inicializado en main.cpp). */
Arduino_Canvas    *get_canvas();

/** Retorna el puntero al controlador de touch (inicializado en main.cpp). */
AXS15231B_Touch   *get_touch();
