#include "game_runner.h"
#include "display_access.h"

#include <lvgl.h>
#include <Arduino.h>

#include "../juegos/snake/snake.h"
#include "../juegos/pong/pong.h"
#include "../juegos/tetris/tetris.h"
#include "../juegos/flappy_bird/flappy_bird.h"

/* Definición del flag global declarado en common.h */
bool g_game_back_requested = false;

/* Dibuja el botón "< SALIR" en esquina superior-derecha del canvas. */
static void draw_back_btn(Arduino_Canvas *gfx) {
    gfx->fillRect(413, 3, 64, 22, 0x1082);   // fondo oscuro
    gfx->setTextColor(0x8C51);               // gris claro
    gfx->setTextSize(1);
    gfx->setCursor(417, 10);
    gfx->print("< SALIR");
}

/* Muestra "Toca para continuar" y espera un tap. */
static void wait_for_tap(Arduino_Canvas *gfx, AXS15231B_Touch *touch) {
    delay(1200);   /* deja ver el game over */

    /* Texto sobre el game over */
    gfx->setTextColor(0xC618);   /* gris claro */
    gfx->setTextSize(2);
    gfx->setCursor(110, DISP_H - 30);
    gfx->print("Toca para continuar");
    gfx->flush();

    /* Esperar release del dedo actual (si lo hay) */
    while (touch->touched()) delay(10);
    /* Esperar nuevo tap */
    while (!touch->touched()) delay(10);
    while (touch->touched())  delay(10);
}

void game_runner_launch(game_id_t id) {
    Arduino_Canvas    *gfx   = get_canvas();
    AXS15231B_Touch   *touch = get_touch();

    switch (id) {
        case GAME_SNAKE: {
            g_game_back_requested = false;
            SnakeGame g(gfx, touch);
            g.begin();
            draw_back_btn(gfx);
            gfx->flush();
            while (g.update() && !g_game_back_requested) {
                draw_back_btn(gfx);
                yield();
            }
            if (!g_game_back_requested) wait_for_tap(gfx, touch);
            break;
        }
        case GAME_PONG: {
            g_game_back_requested = false;
            PongGame g(gfx, touch);
            g.begin();
            draw_back_btn(gfx);
            gfx->flush();
            while (g.update() && !g_game_back_requested) {
                draw_back_btn(gfx);
                yield();
            }
            if (!g_game_back_requested) wait_for_tap(gfx, touch);
            break;
        }
        case GAME_TETRIS: {
            g_game_back_requested = false;
            TetrisGame g(gfx, touch);
            g.begin();
            draw_back_btn(gfx);
            gfx->flush();
            while (g.update() && !g_game_back_requested) {
                draw_back_btn(gfx);
                yield();
            }
            if (!g_game_back_requested) wait_for_tap(gfx, touch);
            break;
        }
        case GAME_FLAPPY: {
            g_game_back_requested = false;
            FlappyBirdGame g(gfx, touch);
            g.begin();
            draw_back_btn(gfx);
            gfx->flush();
            while (g.update() && !g_game_back_requested) {
                draw_back_btn(gfx);
                yield();
            }
            if (!g_game_back_requested) wait_for_tap(gfx, touch);
            break;
        }
    }

    /* Forzar a LVGL a redibujar toda la pantalla */
    lv_obj_invalidate(lv_screen_active());
}
