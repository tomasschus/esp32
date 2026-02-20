#pragma once
#include "../common.h"

class FlappyBirdGame {
public:
    FlappyBirdGame(Arduino_Canvas *gfx, AXS15231B_Touch *touch);
    void begin();
    bool update();          // true = sigue corriendo, false = game over
    int  getScore() const { return _score; }

private:
    Arduino_Canvas  *_gfx;
    AXS15231B_Touch *_touch;
    TouchState       _ts;

    static const int BIRD_X    = 90;
    static const int BIRD_R    = 11;
    static const int PIPE_W    = 42;
    static const int GAP_H     = 100;
    static const int MAX_PIPES = 3;
    static const int PIPE_SPEED = 3;
    static const int HEADER_H  = 28;
    static const int GROUND_Y  = DISP_H - 22;

    struct Pipe {
        int x;
        int gapY;   // y del tope del hueco
        bool passed;
    };

    float  _birdY;
    float  _birdVY;
    Pipe   _pipes[MAX_PIPES];
    int    _score;
    bool   _gameOver;
    bool   _started;        // espera primer tap para arrancar
    int    _pipeSpacing;
    unsigned long _lastFrame;

    void spawnPipe(int idx, int x);
    void drawBird(float y, uint16_t color);
    void drawPipe(const Pipe &p, uint16_t color);
    void drawBackground();
    void drawGround();
    void drawHeader();
    void drawGetReady();
    void drawGameOver();
    bool checkCollision() const;
};
