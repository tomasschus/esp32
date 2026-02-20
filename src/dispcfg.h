#pragma once

/* ── Orientación y resolución ───────────────────────────────── */
/* Rotación 1 = landscape: el panel físico (320×480 portrait) se
   gira 90° → pantalla lógica 480 (ancho) × 320 (alto).
   La pantalla de Mapas cambia dinámicamente a portrait vía
   display_set_rotation(DISP_ROT_PORTRAIT) al navegar hacia/desde ella. */
#define TFT_ROT 1
#define TFT_RES_W 320 /* resolución física del panel */
#define TFT_RES_H 480

/* Dimensiones lógicas en landscape (las usa LVGL y el resto de pantallas) */
#define DISP_HOR_RES 480
#define DISP_VER_RES 320

/* ── Calibración del touch (valores en rotación 1 / landscape) ── */
#define Touch_X_MIN 12
#define Touch_X_MAX 310
#define Touch_Y_MIN 14
#define Touch_Y_MAX 461
