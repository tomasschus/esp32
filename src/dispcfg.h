#pragma once

/* ── Orientación y resolución ───────────────────────────────── */
/* Rotación 1 = landscape: el canvas de 320×480 se gira 90° →
   la pantalla lógica queda 480 (ancho) × 320 (alto)           */
#define TFT_ROT   1
#define TFT_RES_W 320   // resolución física (portrait)
#define TFT_RES_H 480

/* Dimensiones lógicas tras la rotación (las usa LVGL) */
#define DISP_HOR_RES 480
#define DISP_VER_RES 320

/* ── Calibración del touch (valores en rotación 0) ─────────── */
#define Touch_X_MIN 12
#define Touch_X_MAX 310
#define Touch_Y_MIN 14
#define Touch_Y_MAX 461
