#pragma once
#include <lvgl.h>
#include "../../include/maps_ws_server.h"

/**
 * Pantalla "Teléfono" — 480×320 landscape.
 *
 * Muestra en tiempo real:
 *  - Indicaciones de Google Maps (maniobra, calle, distancia, ETA)
 *  - Controles de música (play/pause/prev/next/vol)
 *  - Últimas 3 notificaciones (WhatsApp, Gmail, etc.)
 */

void screen_phone_create(void);
lv_obj_t *screen_phone_get(void);
void screen_phone_start(void);
