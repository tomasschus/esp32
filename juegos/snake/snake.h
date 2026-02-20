#pragma once
#include "../common.h"

class SnakeGame {
public:
    SnakeGame(Arduino_Canvas *gfx, AXS15231B_Touch *touch);
    void begin();
    bool update();          // true = sigue corriendo, false = game over
    int  getScore() const { return _score; }

private:
    Arduino_Canvas    *_gfx;
    AXS15231B_Touch   *_touch;
    TouchState         _ts;

    static const int CELL     = 16;
    static const int HEADER_H = 24;
    static const int COLS     = DISP_W / CELL;            // 30
    static const int ROWS     = (DISP_H - HEADER_H) / CELL; // 18
    static const int MAX_LEN  = COLS * ROWS;

    struct Point { int8_t x, y; };

    Point _body[MAX_LEN];
    int   _len;
    Point _dir, _nextDir;
    Point _food;
    int   _score;
    bool  _gameOver;
    unsigned long _lastMove;
    int   _speed;   // ms entre movimientos

    void spawnFood();
    void applyInput();
    void drawGrid();
    void drawHeader();
    void drawCell(int x, int y, uint16_t color);
    void eraseCell(int x, int y);
    void drawFood(uint16_t color);
    void drawGameOver();
};
