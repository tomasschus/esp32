#include "tetris.h"

// ── Piezas: 7 tipos × 4 rotaciones × 4 bloques × (dx,dy) ───────────────────
const int8_t TetrisGame::PIECES[7][4][4][2] = {
    // I
    {{{0,1},{1,1},{2,1},{3,1}}, {{2,0},{2,1},{2,2},{2,3}},
     {{0,2},{1,2},{2,2},{3,2}}, {{1,0},{1,1},{1,2},{1,3}}},
    // J
    {{{0,0},{0,1},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{1,2}},
     {{0,1},{1,1},{2,1},{2,2}}, {{1,0},{1,1},{0,2},{1,2}}},
    // L
    {{{2,0},{0,1},{1,1},{2,1}}, {{1,0},{1,1},{1,2},{2,2}},
     {{0,1},{1,1},{2,1},{0,2}}, {{0,0},{1,0},{1,1},{1,2}}},
    // O
    {{{0,0},{1,0},{0,1},{1,1}}, {{0,0},{1,0},{0,1},{1,1}},
     {{0,0},{1,0},{0,1},{1,1}}, {{0,0},{1,0},{0,1},{1,1}}},
    // S
    {{{1,0},{2,0},{0,1},{1,1}}, {{1,0},{1,1},{2,1},{2,2}},
     {{1,1},{2,1},{0,2},{1,2}}, {{0,0},{0,1},{1,1},{1,2}}},
    // T
    {{{1,0},{0,1},{1,1},{2,1}}, {{1,0},{1,1},{2,1},{1,2}},
     {{0,1},{1,1},{2,1},{1,2}}, {{1,0},{0,1},{1,1},{1,2}}},
    // Z
    {{{0,0},{1,0},{1,1},{2,1}}, {{2,0},{1,1},{2,1},{1,2}},
     {{0,1},{1,1},{1,2},{2,2}}, {{1,0},{0,1},{1,1},{0,2}}}
};

// índice 0 = vacío, 1-7 = piezas I,J,L,O,S,T,Z
const uint16_t TetrisGame::COLORS[8] = {
    C_DARK_BG,   // 0 vacío
    0x07FF,      // 1 I – cyan
    0x001F,      // 2 J – azul
    0xFD20,      // 3 L – naranja
    0xFFE0,      // 4 O – amarillo
    0x07E0,      // 5 S – verde
    0x801F,      // 6 T – violeta
    0xF800       // 7 Z – rojo
};

static const uint16_t BORDER_C = 0x4208;
static const uint16_t TEXT_C   = C_WHITE;
static const uint16_t PANEL_BG = 0x0000;

TetrisGame::TetrisGame(Arduino_Canvas *gfx, AXS15231B_Touch *touch)
    : _gfx(gfx), _touch(touch) {}

// ── begin ────────────────────────────────────────────────────────────────────
void TetrisGame::begin() {
    memset(_board, 0, sizeof(_board));
    _score    = 0;
    _lines    = 0;
    _level    = 1;
    _gameOver = false;
    _lastDrop = 0;
    _dropInterval = 800;

    _nextType = random(0, 7);
    spawnPiece();

    _gfx->fillScreen(C_DARK_BG);
    drawBoard();
    drawSidePanel();
    drawPiece(_pieceType, _pieceRot, _pieceX, _pieceY, false);
    _gfx->flush();
}

// ── update ───────────────────────────────────────────────────────────────────
bool TetrisGame::update() {
    if (_gameOver) return false;

    _ts.update(_touch);
    applyInput();
    if (_gameOver) return false;  // applyInput puede setear _gameOver (hard drop)

    unsigned long now = millis();
    if (now - _lastDrop >= (unsigned long)_dropInterval) {
        _lastDrop = now;

        drawPiece(_pieceType, _pieceRot, _pieceX, _pieceY, true); // erase

        if (!collides(_pieceType, _pieceRot, _pieceX, _pieceY + 1)) {
            _pieceY++;
        } else {
            place();
            int cleared = clearLines();
            if (cleared) addScore(cleared);
            spawnPiece();
            if (collides(_pieceType, _pieceRot, _pieceX, _pieceY)) {
                drawGameOver();
                _gameOver = true;
                return false;
            }
            drawBoard();
            drawSidePanel();
        }
        drawPiece(_pieceType, _pieceRot, _pieceX, _pieceY, false);
        _gfx->flush();
    }
    return true;
}

// ── applyInput ───────────────────────────────────────────────────────────────
void TetrisGame::applyInput() {
    TouchState::Dir d = _ts.swipe(35);
    if (d == TouchState::NONE) return;

    drawPiece(_pieceType, _pieceRot, _pieceX, _pieceY, true);

    switch (d) {
        case TouchState::LEFT:
            if (!collides(_pieceType, _pieceRot, _pieceX - 1, _pieceY))
                _pieceX--;
            break;
        case TouchState::RIGHT:
            if (!collides(_pieceType, _pieceRot, _pieceX + 1, _pieceY))
                _pieceX++;
            break;
        case TouchState::UP: {
            int newRot = (_pieceRot + 1) % 4;
            if (!collides(_pieceType, newRot, _pieceX, _pieceY))
                _pieceRot = newRot;
            break;
        }
        case TouchState::DOWN:
            // Hard drop: requiere al menos 70px para evitar drops accidentales
            if (_ts.releaseY - _ts.pressY < 70) break;
            while (!collides(_pieceType, _pieceRot, _pieceX, _pieceY + 1))
                _pieceY++;
            place();
            {
                int cleared = clearLines();
                if (cleared) addScore(cleared);
            }
            spawnPiece();
            _lastDrop = millis();  // dar intervalo completo a la nueva pieza
            if (collides(_pieceType, _pieceRot, _pieceX, _pieceY)) {
                drawPiece(_pieceType, _pieceRot, _pieceX, _pieceY, false);
                _gfx->flush();
                drawGameOver();
                _gameOver = true;
                return;
            }
            drawBoard();
            drawSidePanel();
            break;
        case TouchState::TAP: {
            // Tap también rota
            int newRot = (_pieceRot + 1) % 4;
            if (!collides(_pieceType, newRot, _pieceX, _pieceY))
                _pieceRot = newRot;
            break;
        }
        default: break;
    }

    drawPiece(_pieceType, _pieceRot, _pieceX, _pieceY, false);
    _gfx->flush();
}

// ── collides ─────────────────────────────────────────────────────────────────
bool TetrisGame::collides(int type, int rot, int px, int py) const {
    for (int b = 0; b < 4; b++) {
        int bx = px + PIECES[type][rot][b][0];
        int by = py + PIECES[type][rot][b][1];
        if (bx < 0 || bx >= COLS || by >= ROWS) return true;
        if (by >= 0 && _board[by][bx] != 0) return true;
    }
    return false;
}

// ── place ────────────────────────────────────────────────────────────────────
void TetrisGame::place() {
    for (int b = 0; b < 4; b++) {
        int bx = _pieceX + PIECES[_pieceType][_pieceRot][b][0];
        int by = _pieceY + PIECES[_pieceType][_pieceRot][b][1];
        if (by >= 0 && by < ROWS && bx >= 0 && bx < COLS)
            _board[by][bx] = (uint8_t)(_pieceType + 1);
    }
}

// ── clearLines ───────────────────────────────────────────────────────────────
int TetrisGame::clearLines() {
    int cleared = 0;
    for (int row = ROWS - 1; row >= 0; ) {
        bool full = true;
        for (int c = 0; c < COLS; c++)
            if (_board[row][c] == 0) { full = false; break; }
        if (full) {
            for (int r = row; r > 0; r--)
                memcpy(_board[r], _board[r-1], COLS);
            memset(_board[0], 0, COLS);
            cleared++;
        } else {
            row--;
        }
    }
    return cleared;
}

// ── spawnPiece ───────────────────────────────────────────────────────────────
void TetrisGame::spawnPiece() {
    _pieceType = _nextType;
    _nextType  = random(0, 7);
    _pieceRot  = 0;
    _pieceX    = COLS / 2 - 2;
    _pieceY    = 0;
}

// ── addScore ─────────────────────────────────────────────────────────────────
void TetrisGame::addScore(int cleared) {
    static const int pts[5] = {0, 100, 300, 500, 800};
    _lines += cleared;
    _level  = _lines / 10 + 1;
    _score += pts[cleared] * _level;
    _dropInterval = max(100, 800 - (_level - 1) * 70);
}

// ── drawBoard ────────────────────────────────────────────────────────────────
void TetrisGame::drawBoard() {
    // Borde del tablero
    int x0 = BOARD_X - 2, y0 = BOARD_Y - 2;
    int bw  = COLS * CELL + 4, bh = ROWS * CELL + 4;
    _gfx->drawRect(x0, y0, bw, bh, BORDER_C);

    // Celdas del tablero
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            drawCell(c, r, COLORS[_board[r][c]]);
}

// ── drawPiece ────────────────────────────────────────────────────────────────
void TetrisGame::drawPiece(int type, int rot, int px, int py, bool erase) {
    uint16_t color = erase ? COLORS[0] : COLORS[type + 1];
    for (int b = 0; b < 4; b++) {
        int bx = px + PIECES[type][rot][b][0];
        int by = py + PIECES[type][rot][b][1];
        if (by >= 0) drawCell(bx, by, color);
    }
}

// ── drawCell ─────────────────────────────────────────────────────────────────
void TetrisGame::drawCell(int bx, int by, uint16_t color) {
    int px = BOARD_X + bx * CELL;
    int py = BOARD_Y + by * CELL;
    _gfx->fillRect(px + 1, py + 1, CELL - 2, CELL - 2, color);
    if (color != COLORS[0]) {
        // borde claro para efecto 3D
        _gfx->drawRect(px, py, CELL, CELL, (uint16_t)(color >> 1 | 0x8410));
    } else {
        _gfx->drawRect(px, py, CELL, CELL, 0x0841);
    }
}

// ── drawSidePanel ─────────────────────────────────────────────────────────────
void TetrisGame::drawSidePanel() {
    int px = BOARD_X + COLS * CELL + 14;
    int py = BOARD_Y;
    int pw = DISP_W - px - 6;

    _gfx->fillRect(px, 0, pw + 6, DISP_H, C_DARK_BG);

    // Título
    _gfx->setTextColor(C_ACCENT);
    _gfx->setTextSize(2);
    _gfx->setCursor(px, 4);
    _gfx->print("TETRIS");

    // Score
    _gfx->setTextColor(TEXT_C);
    _gfx->setTextSize(1);
    _gfx->setCursor(px, py + 10);  _gfx->print("SCORE");
    _gfx->setTextSize(2);
    _gfx->setCursor(px, py + 22);  _gfx->print(_score);

    _gfx->setTextSize(1);
    _gfx->setCursor(px, py + 54);  _gfx->print("LINES");
    _gfx->setTextSize(2);
    _gfx->setCursor(px, py + 66);  _gfx->print(_lines);

    _gfx->setTextSize(1);
    _gfx->setCursor(px, py + 98);  _gfx->print("LEVEL");
    _gfx->setTextSize(2);
    _gfx->setCursor(px, py + 110); _gfx->print(_level);

    // Siguiente pieza
    _gfx->setTextSize(1);
    _gfx->setCursor(px, py + 148); _gfx->print("NEXT");
    _gfx->fillRect(px, py + 160, 60, 60, 0x0000);
    _gfx->drawRect(px, py + 160, 60, 60, BORDER_C);
    drawNextPiece();

    // Controles
    _gfx->setTextSize(1);
    _gfx->setTextColor(C_GRAY);
    _gfx->setCursor(px, DISP_H - 60); _gfx->print("< > mover");
    _gfx->setCursor(px, DISP_H - 48); _gfx->print("^ rotar");
    _gfx->setCursor(px, DISP_H - 36); _gfx->print("v drop");
    _gfx->setCursor(px, DISP_H - 24); _gfx->print("tap rotar");
}

// ── drawNextPiece ─────────────────────────────────────────────────────────────
void TetrisGame::drawNextPiece() {
    int ox = BOARD_X + COLS * CELL + 14;
    int oy = BOARD_Y + 168;
    int cs = 12;
    uint16_t color = COLORS[_nextType + 1];
    for (int b = 0; b < 4; b++) {
        int bx = PIECES[_nextType][0][b][0];
        int by = PIECES[_nextType][0][b][1];
        _gfx->fillRect(ox + bx * cs + 4, oy + by * cs, cs - 1, cs - 1, color);
    }
}

// ── drawGameOver ──────────────────────────────────────────────────────────────
void TetrisGame::drawGameOver() {
    int bx = 60, by = 110, bw = 260, bh = 100;
    _gfx->fillRoundRect(bx, by, bw, bh, 10, 0x1082);
    _gfx->drawRoundRect(bx, by, bw, bh, 10, C_ACCENT);
    _gfx->setTextColor(C_WHITE);
    _gfx->setTextSize(3);
    _gfx->setCursor(bx + 20, by + 12);
    _gfx->print("GAME OVER");
    _gfx->setTextSize(2);
    _gfx->setCursor(bx + 20, by + 54);
    _gfx->print("Score: "); _gfx->print(_score);
    _gfx->setCursor(bx + 20, by + 76);
    _gfx->print("Lines: "); _gfx->print(_lines);
    _gfx->flush();
}
