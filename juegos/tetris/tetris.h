#pragma once
#include "../common.h"

class TetrisGame {
public:
    TetrisGame(Arduino_Canvas *gfx, AXS15231B_Touch *touch);
    void begin();
    bool update();          // true = sigue corriendo, false = game over
    int  getScore() const { return _score; }

private:
    Arduino_Canvas  *_gfx;
    AXS15231B_Touch *_touch;
    TouchState       _ts;

    // Tablero
    static const int CELL    = 14;
    static const int COLS    = 10;
    static const int ROWS    = 20;
    static const int BOARD_X = 10;
    static const int BOARD_Y = 20;

    uint8_t  _board[ROWS][COLS];   // 0 = vacío, 1-7 = color de pieza

    // Pieza activa
    int _pieceType;
    int _pieceRot;
    int _pieceX, _pieceY;

    // Siguiente pieza
    int _nextType;

    // Estado del juego
    int  _score, _lines, _level;
    bool _gameOver;

    unsigned long _lastDrop;
    int  _dropInterval;   // ms entre caídas automáticas

    // Herramientas internas
    static const int8_t PIECES[7][4][4][2];
    static const uint16_t COLORS[8];

    bool  collides(int type, int rot, int px, int py) const;
    void  place();
    int   clearLines();
    void  spawnPiece();
    void  applyInput();

    void  drawBoard();
    void  drawPiece(int type, int rot, int px, int py, bool erase);
    void  drawCell(int bx, int by, uint16_t color);
    void  drawSidePanel();
    void  drawNextPiece();
    void  drawGameOver();
    void  addScore(int cleared);
};
