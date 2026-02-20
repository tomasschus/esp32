#pragma once
#include "../common.h"

class PongGame {
public:
    PongGame(Arduino_Canvas *gfx, AXS15231B_Touch *touch);
    void begin();
    bool update();          // true = sigue corriendo, false = juego terminado
    int  getScore() const  { return _scorePlayer; }

    static const int PADDLE_W  = 12;   // público para uso en pong.cpp

private:
    Arduino_Canvas  *_gfx;
    AXS15231B_Touch *_touch;
    TouchState       _ts;
    static const int PADDLE_H  = 60;
    static const int BALL_R    = 7;
    static const int HEADER_H  = 28;
    static const int FIELD_Y   = HEADER_H;
    static const int FIELD_H   = DISP_H - HEADER_H;
    static const int WIN_SCORE = 7;
    static const int AI_SPEED  = 3;

    // Paddl izquierdo = AI, derecho = jugador
    float _aiY, _playerY;
    float _ballX, _ballY;
    float _ballVX, _ballVY;

    int _scoreAI, _scorePlayer;
    bool _gameOver;
    bool _scored;             // pequeña pausa tras gol
    unsigned long _scoredAt;

    void resetBall(int towardPlayer);
    void drawField();
    void drawHeader();
    void drawPaddle(float y, int px, uint16_t color, uint16_t erase);
    void drawBall(float x, float y, uint16_t color);
    void drawGameOver();
};
