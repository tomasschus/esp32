#include "pong.h"

static const uint16_t BG_C      = C_DARK_BG;
static const uint16_t BALL_C    = C_WHITE;
static const uint16_t AI_C      = 0x001F;   // azul
static const uint16_t PLAYER_C  = C_ACCENT; // rojo-rosado
static const uint16_t NET_C     = 0x2104;   // gris oscuro
static const uint16_t TEXT_C    = C_WHITE;

static const int AI_X    = 16;
static const int PLAYER_X = DISP_W - 16 - PongGame::PADDLE_W;

PongGame::PongGame(Arduino_Canvas *gfx, AXS15231B_Touch *touch)
    : _gfx(gfx), _touch(touch) {}

// ── begin ────────────────────────────────────────────────────────────────────
void PongGame::begin() {
    _scoreAI     = 0;
    _scorePlayer = 0;
    _gameOver    = false;
    _scored      = false;

    _aiY     = FIELD_Y + (FIELD_H - PADDLE_H) / 2.0f;
    _playerY = FIELD_Y + (FIELD_H - PADDLE_H) / 2.0f;

    resetBall(1);  // empieza yendo al jugador

    _gfx->fillScreen(BG_C);
    drawField();
    drawHeader();
    drawPaddle(_aiY,     AI_X,     AI_C,     BG_C);
    drawPaddle(_playerY, PLAYER_X, PLAYER_C, BG_C);
    drawBall(_ballX, _ballY, BALL_C);
    _gfx->flush();
}

// ── update ───────────────────────────────────────────────────────────────────
bool PongGame::update() {
    if (_gameOver) return false;

    // Pausa tras gol
    if (_scored) {
        if (millis() - _scoredAt < 1200) return true;
        _scored = false;
        resetBall(_scoreAI < _scorePlayer ? 1 : -1);
        drawField();
        drawPaddle(_aiY,     AI_X,     AI_C,     BG_C);
        drawPaddle(_playerY, PLAYER_X, PLAYER_C, BG_C);
        drawBall(_ballX, _ballY, BALL_C);
        _gfx->flush();
        return true;
    }

    _ts.update(_touch);

    // ── Jugador: dedo en mitad derecha mueve el paddle ──────────────────────
    float prevPlayerY = _playerY;
    if (_ts.pressed && _ts.x > DISP_W / 2) {
        float target = _ts.y - PADDLE_H / 2.0f;
        target = constrain(target, (float)FIELD_Y, (float)(DISP_H - PADDLE_H));
        // Interpolación suave
        _playerY += (target - _playerY) * 0.35f;
    }

    // ── AI: sigue la pelota ──────────────────────────────────────────────────
    float prevAiY = _aiY;
    float aiCenter = _aiY + PADDLE_H / 2.0f;
    if (_ballY > aiCenter + 4)      _aiY += AI_SPEED;
    else if (_ballY < aiCenter - 4) _aiY -= AI_SPEED;
    _aiY = constrain(_aiY, (float)FIELD_Y, (float)(DISP_H - PADDLE_H));

    // ── Mover pelota ─────────────────────────────────────────────────────────
    float prevBX = _ballX, prevBY = _ballY;
    _ballX += _ballVX;
    _ballY += _ballVY;

    // Rebote techo/suelo
    if (_ballY - BALL_R < FIELD_Y)         { _ballY = FIELD_Y + BALL_R;      _ballVY = -_ballVY; }
    if (_ballY + BALL_R > DISP_H)          { _ballY = DISP_H - BALL_R;       _ballVY = -_ballVY; }

    // Rebote con paddle AI (izquierdo)
    if (_ballVX < 0 &&
        _ballX - BALL_R < AI_X + PADDLE_W &&
        _ballX - BALL_R > AI_X &&
        _ballY + BALL_R > _aiY &&
        _ballY - BALL_R < _aiY + PADDLE_H) {
        _ballVX = -_ballVX * 1.04f;
        float rel = (_ballY - (_aiY + PADDLE_H / 2.0f)) / (PADDLE_H / 2.0f);
        _ballVY = rel * 5.0f;
        _ballX  = AI_X + PADDLE_W + BALL_R;
    }

    // Rebote con paddle jugador (derecho)
    if (_ballVX > 0 &&
        _ballX + BALL_R > PLAYER_X &&
        _ballX + BALL_R < PLAYER_X + PADDLE_W &&
        _ballY + BALL_R > _playerY &&
        _ballY - BALL_R < _playerY + PADDLE_H) {
        _ballVX = -_ballVX * 1.04f;
        float rel = (_ballY - (_playerY + PADDLE_H / 2.0f)) / (PADDLE_H / 2.0f);
        _ballVY = rel * 5.0f;
        _ballX  = PLAYER_X - BALL_R;
    }

    // Clamp velocidad máxima
    _ballVX = constrain(_ballVX, -9.0f, 9.0f);
    _ballVY = constrain(_ballVY, -7.0f, 7.0f);

    // Gol
    if (_ballX + BALL_R < 0) {
        _scorePlayer++;
        drawBall(prevBX, prevBY, BG_C);
        drawHeader();
        if (_scorePlayer >= WIN_SCORE) { drawGameOver(); _gameOver = true; return false; }
        _scored = true; _scoredAt = millis();
        _gfx->flush();
        return true;
    }
    if (_ballX - BALL_R > DISP_W) {
        _scoreAI++;
        drawBall(prevBX, prevBY, BG_C);
        drawHeader();
        if (_scoreAI >= WIN_SCORE) { drawGameOver(); _gameOver = true; return false; }
        _scored = true; _scoredAt = millis();
        _gfx->flush();
        return true;
    }

    // ── Redibujar solo lo que cambió ─────────────────────────────────────────
    if ((int)prevAiY != (int)_aiY)
        drawPaddle(prevAiY, AI_X, BG_C, BG_C);
    drawPaddle(_aiY, AI_X, AI_C, BG_C);

    if ((int)prevPlayerY != (int)_playerY)
        drawPaddle(prevPlayerY, PLAYER_X, BG_C, BG_C);
    drawPaddle(_playerY, PLAYER_X, PLAYER_C, BG_C);

    drawBall(prevBX, prevBY, BG_C);
    drawBall(_ballX, _ballY, BALL_C);

    _gfx->flush();
    return true;
}

// ── resetBall ────────────────────────────────────────────────────────────────
void PongGame::resetBall(int dir) {
    _ballX  = DISP_W / 2.0f;
    _ballY  = FIELD_Y + FIELD_H / 2.0f;
    _ballVX = dir * 4.5f;
    _ballVY = random(-30, 31) / 10.0f;
}

// ── drawField ────────────────────────────────────────────────────────────────
void PongGame::drawField() {
    _gfx->fillRect(0, FIELD_Y, DISP_W, FIELD_H, BG_C);
    // Red central punteada
    for (int y = FIELD_Y; y < DISP_H; y += 14)
        _gfx->fillRect(DISP_W / 2 - 1, y, 3, 8, NET_C);
}

// ── drawHeader ───────────────────────────────────────────────────────────────
void PongGame::drawHeader() {
    _gfx->fillRect(0, 0, DISP_W, HEADER_H, 0x0000);
    _gfx->setTextColor(AI_C);
    _gfx->setTextSize(2);
    _gfx->setCursor(40, 6);
    _gfx->print(_scoreAI);
    _gfx->setTextColor(TEXT_C);
    _gfx->setCursor(DISP_W / 2 - 20, 6);
    _gfx->print("PONG");
    _gfx->setTextColor(PLAYER_C);
    _gfx->setCursor(DISP_W - 60, 6);
    _gfx->print(_scorePlayer);
}

// ── drawPaddle ───────────────────────────────────────────────────────────────
void PongGame::drawPaddle(float y, int px, uint16_t color, uint16_t /*unused*/) {
    _gfx->fillRoundRect(px, (int)y, PADDLE_W, PADDLE_H, 4, color);
}

// ── drawBall ─────────────────────────────────────────────────────────────────
void PongGame::drawBall(float x, float y, uint16_t color) {
    _gfx->fillCircle((int)x, (int)y, BALL_R, color);
}

// ── drawGameOver ─────────────────────────────────────────────────────────────
void PongGame::drawGameOver() {
    bool playerWon = _scorePlayer >= WIN_SCORE;
    int bx = 100, by = 100, bw = 280, bh = 110;
    _gfx->fillRoundRect(bx, by, bw, bh, 10, 0x1082);
    _gfx->drawRoundRect(bx, by, bw, bh, 10, C_ACCENT);
    _gfx->setTextColor(C_WHITE);
    _gfx->setTextSize(2);
    _gfx->setCursor(bx + 30, by + 14);
    _gfx->print(playerWon ? "GANASTE!" : "PERDISTE...");
    _gfx->setTextSize(2);
    _gfx->setCursor(bx + 30, by + 46);
    _gfx->print("AI ");  _gfx->print(_scoreAI);
    _gfx->print("  -  TU "); _gfx->print(_scorePlayer);
    _gfx->flush();
}
