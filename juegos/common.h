#pragma once
#include "../src/AXS15231B_touch.h"
#include <Arduino_GFX_Library.h>

// ── Colores RGB565 ──────────────────────────────────────────────────────────
#define C_BLACK 0x0000
#define C_WHITE 0xFFFF
#define C_RED 0xF800
#define C_GREEN 0x07E0
#define C_BLUE 0x001F
#define C_YELLOW 0xFFE0
#define C_CYAN 0x07FF
#define C_MAGENTA 0xF81F
#define C_ORANGE 0xFD20
#define C_PURPLE 0x8010
#define C_DARK_BG 0x0841 // fondo oscuro (casi negro azulado)
#define C_DARK_GRAY 0x4208
#define C_GRAY 0x8410
#define C_ACCENT 0xE8A3 // rojo-rosado del proyecto

// ── Display dimensions ──────────────────────────────────────────────────────
#define DISP_W 480
#define DISP_H 320

// ── Back-button global flag (definido en game_runner.cpp) ───────────────────
extern bool g_game_back_requested;

// ── Touch helper ────────────────────────────────────────────────────────────
struct TouchState {
  bool pressed = false;
  bool justPressed = false;
  bool justReleased = false;
  int16_t x = 0, y = 0;
  int16_t pressX = 0, pressY = 0;
  int16_t releaseX = 0, releaseY = 0;
  int16_t lastValidX = 0,
          lastValidY = 0; // última posición válida (no 0,0) para release

  enum Dir { NONE, LEFT, RIGHT, UP, DOWN, TAP };

  // Llamar una vez por frame
  void update(AXS15231B_Touch *touch) {
    bool now = touch->touched();
    if (now) {
      uint16_t tx, ty;
      touch->readData(&tx, &ty);
      x = (int16_t)tx;
      y = (int16_t)ty;
      if (x != 0 || y != 0) {
        lastValidX = x;
        lastValidY = y;
      }
    }
    justPressed = !pressed && now;
    justReleased = pressed && !now;
    if (justPressed) {
      pressX = x;
      pressY = y;
    }
    if (justReleased) {
      // Al soltar, el driver a veces reporta (0,0); usar última posición válida
      if (x == 0 && y == 0) {
        releaseX = lastValidX;
        releaseY = lastValidY;
      } else {
        releaseX = x;
        releaseY = y;
      }
    }
    pressed = now;
    // Zona de botón back: esquina superior-derecha (x>410, y<32), tap pequeño
    if (justReleased && releaseX > 410 && releaseY < 32 &&
        abs(releaseX - pressX) < 22 && abs(releaseY - pressY) < 22)
      g_game_back_requested = true;
  }

  // Dirección del swipe al soltar el dedo (o TAP si no se movió)
  Dir swipe(int minDist = 35) const {
    if (!justReleased)
      return NONE;
    int dx = releaseX - pressX;
    int dy = releaseY - pressY;
    int ax = abs(dx), ay = abs(dy);
    // Suprimir acciones de juego si fue tap de back button
    if (releaseX > 410 && releaseY < 32 && ax < 22 && ay < 22)
      return NONE;
    if (ax < minDist && ay < minDist)
      return TAP;
    return (ax >= ay) ? (dx > 0 ? RIGHT : LEFT) : (dy > 0 ? DOWN : UP);
  }

  // Dirección del arrastre mientras el dedo sigue tocando (sin soltar).
  // Permite cambiar dirección en cuanto arrastras, más responsivo que solo
  // swipe().
  Dir dragDir(int minDist = 22) const {
    if (!pressed)
      return NONE;
    int dx = x - pressX;
    int dy = y - pressY;
    int ax = abs(dx), ay = abs(dy);
    if (ax < minDist && ay < minDist)
      return NONE;
    // Ignorar zona del botón back
    if (x > 410 && y < 32)
      return NONE;
    return (ax >= ay) ? (dx > 0 ? RIGHT : LEFT) : (dy > 0 ? DOWN : UP);
  }
};
