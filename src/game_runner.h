#pragma once

typedef enum {
    GAME_SNAKE,
    GAME_PONG,
    GAME_TETRIS,
    GAME_FLAPPY
} game_id_t;

/** Lanza el juego indicado en un loop bloqueante.
 *  Retorna cuando el usuario toca la pantalla después del game over. */
void game_runner_launch(game_id_t id);
