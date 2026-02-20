#include "flappy_bird.h"

static const uint16_t SKY_C    = 0x3DDF;   // celeste
static const uint16_t GROUND_C = 0xD5A0;   // tierra
static const uint16_t GRASS_C  = 0x0600;   // verde pasto
static const uint16_t PIPE_C   = 0x0640;   // verde tubería
static const uint16_t PIPE_E_C = 0x03E0;   // borde tubo más claro
static const uint16_t BIRD_C   = 0xFFE0;   // amarillo
static const uint16_t BIRD_E_C = 0xFD20;   // naranja (ala)
static const uint16_t EYE_C    = C_WHITE;
static const uint16_t TEXT_C   = C_WHITE;

FlappyBirdGame::FlappyBirdGame(Arduino_Canvas *gfx, AXS15231B_Touch *touch)
    : _gfx(gfx), _touch(touch) {}

// ── begin ────────────────────────────────────────────────────────────────────
void FlappyBirdGame::begin() {
    _birdY      = DISP_H / 2.0f;
    _birdVY     = 0;
    _score      = 0;
    _gameOver   = false;
    _started    = false;
    _pipeSpacing = 190;
    _lastFrame  = 0;

    spawnPipe(0, DISP_W + 20);
    spawnPipe(1, DISP_W + 20 + _pipeSpacing);
    spawnPipe(2, DISP_W + 20 + _pipeSpacing * 2);

    _gfx->fillScreen(SKY_C);
    drawBackground();
    drawGround();
    drawHeader();
    drawBird(_birdY, BIRD_C);
    for (int i = 0; i < MAX_PIPES; i++) drawPipe(_pipes[i], PIPE_C);
    drawGetReady();
    _gfx->flush();
}

// ── update ───────────────────────────────────────────────────────────────────
bool FlappyBirdGame::update() {
    if (_gameOver) return false;

    // Frame rate cap ~60fps
    unsigned long now = millis();
    if (now - _lastFrame < 16) return true;
    _lastFrame = now;

    _ts.update(_touch);

    // Espera primer tap
    if (!_started) {
        if (_ts.justPressed) {
            _started = true;
            _birdVY = -6.5f;
            _gfx->fillScreen(SKY_C);
            drawBackground();
            drawGround();
            drawHeader();
            for (int i = 0; i < MAX_PIPES; i++) drawPipe(_pipes[i], PIPE_C);
            drawBird(_birdY, BIRD_C);
            _gfx->flush();
        }
        return true;
    }

    // Flap al tocar
    if (_ts.justPressed) _birdVY = -6.5f;

    float prevBirdY = _birdY;

    // Física del pájaro
    _birdVY += 0.38f;
    _birdVY  = constrain(_birdVY, -8.0f, 9.0f);
    _birdY  += _birdVY;

    // Límite superior
    if (_birdY - BIRD_R < HEADER_H) {
        _birdY  = HEADER_H + BIRD_R;
        _birdVY = 0;
    }

    // Mover tuberías
    for (int i = 0; i < MAX_PIPES; i++) {
        drawPipe(_pipes[i], SKY_C);   // borrar

        _pipes[i].x -= PIPE_SPEED;

        // Scoring
        if (!_pipes[i].passed && _pipes[i].x + PIPE_W < BIRD_X - BIRD_R) {
            _pipes[i].passed = true;
            _score++;
            // Aumentar dificultad cada 5 puntos
            if (_score % 5 == 0 && _pipeSpacing > 140)
                _pipeSpacing -= 8;
            drawHeader();
        }

        // Reciclar tubería que salió por la izquierda
        if (_pipes[i].x + PIPE_W < 0) {
            // Encontrar el tubo más a la derecha
            int maxX = _pipes[0].x;
            for (int j = 1; j < MAX_PIPES; j++)
                if (_pipes[j].x > maxX) maxX = _pipes[j].x;
            spawnPipe(i, maxX + _pipeSpacing);
        }

        drawPipe(_pipes[i], PIPE_C);  // redibujar
    }

    // Redibujar pájaro
    drawBird(prevBirdY, SKY_C);
    drawBird(_birdY, BIRD_C);

    // Redibujar suelo encima de tuberías que lo pisan
    drawGround();

    // Colisión con suelo
    if (_birdY + BIRD_R >= GROUND_Y) {
        drawBird(_birdY, BIRD_C);
        _gfx->flush();
        delay(400);
        drawGameOver();
        _gameOver = true;
        return false;
    }

    // Colisión con tuberías
    if (checkCollision()) {
        _gfx->flush();
        delay(300);
        drawGameOver();
        _gameOver = true;
        return false;
    }

    _gfx->flush();
    return true;
}

// ── spawnPipe ────────────────────────────────────────────────────────────────
void FlappyBirdGame::spawnPipe(int idx, int x) {
    _pipes[idx].x      = x;
    _pipes[idx].gapY   = random(HEADER_H + 30, GROUND_Y - GAP_H - 30);
    _pipes[idx].passed = false;
}

// ── checkCollision ───────────────────────────────────────────────────────────
bool FlappyBirdGame::checkCollision() const {
    int bx = BIRD_X, by = (int)_birdY;
    for (int i = 0; i < MAX_PIPES; i++) {
        const Pipe &p = _pipes[i];
        // Rango horizontal de la tubería
        if (bx + BIRD_R > p.x && bx - BIRD_R < p.x + PIPE_W) {
            // Tubería superior
            if (by - BIRD_R < p.gapY) return true;
            // Tubería inferior
            if (by + BIRD_R > p.gapY + GAP_H) return true;
        }
    }
    return false;
}

// ── drawBird ─────────────────────────────────────────────────────────────────
void FlappyBirdGame::drawBird(float y, uint16_t color) {
    int by = (int)y;
    if (color == SKY_C) {
        // Borrado: rellenar zona del pájaro con cielo
        _gfx->fillCircle(BIRD_X, by, BIRD_R + 2, SKY_C);
        return;
    }
    // Cuerpo
    _gfx->fillCircle(BIRD_X, by, BIRD_R, color);
    // Ala (triángulo en la parte baja)
    _gfx->fillTriangle(BIRD_X - 5, by + 4,
                       BIRD_X + 5, by + 4,
                       BIRD_X,     by + BIRD_R + 4, BIRD_E_C);
    // Ojo
    _gfx->fillCircle(BIRD_X + 5, by - 3, 3, EYE_C);
    _gfx->fillCircle(BIRD_X + 6, by - 3, 1, C_DARK_BG);
    // Pico
    _gfx->fillTriangle(BIRD_X + BIRD_R - 1, by,
                       BIRD_X + BIRD_R + 6, by - 2,
                       BIRD_X + BIRD_R + 6, by + 3, C_ORANGE);
}

// ── drawPipe ─────────────────────────────────────────────────────────────────
void FlappyBirdGame::drawPipe(const Pipe &p, uint16_t color) {
    if (color == SKY_C) {
        // Borrado rápido
        _gfx->fillRect(p.x, HEADER_H, PIPE_W, GROUND_Y - HEADER_H, SKY_C);
        return;
    }
    // Tubería superior
    if (p.gapY > HEADER_H) {
        _gfx->fillRect(p.x + 3, HEADER_H, PIPE_W - 6, p.gapY - HEADER_H, color);
        // Cap
        _gfx->fillRect(p.x, p.gapY - 14, PIPE_W, 14, PIPE_E_C);
        _gfx->drawRect(p.x, p.gapY - 14, PIPE_W, 14, C_DARK_BG);
    }
    // Tubería inferior
    int bot = p.gapY + GAP_H;
    if (bot < GROUND_Y) {
        _gfx->fillRect(p.x + 3, bot + 14, PIPE_W - 6, GROUND_Y - bot - 14, color);
        // Cap
        _gfx->fillRect(p.x, bot, PIPE_W, 14, PIPE_E_C);
        _gfx->drawRect(p.x, bot, PIPE_W, 14, C_DARK_BG);
    }
}

// ── drawBackground ───────────────────────────────────────────────────────────
void FlappyBirdGame::drawBackground() {
    _gfx->fillRect(0, HEADER_H, DISP_W, GROUND_Y - HEADER_H, SKY_C);
    // Nubes decorativas
    _gfx->fillRoundRect(60,  55, 70, 28, 12, C_WHITE);
    _gfx->fillRoundRect(200, 45, 55, 22, 10, C_WHITE);
    _gfx->fillRoundRect(350, 65, 80, 26, 12, C_WHITE);
}

// ── drawGround ───────────────────────────────────────────────────────────────
void FlappyBirdGame::drawGround() {
    _gfx->fillRect(0, GROUND_Y, DISP_W, DISP_H - GROUND_Y, GROUND_C);
    _gfx->fillRect(0, GROUND_Y, DISP_W, 6, GRASS_C);
}

// ── drawHeader ───────────────────────────────────────────────────────────────
void FlappyBirdGame::drawHeader() {
    _gfx->fillRect(0, 0, DISP_W, HEADER_H, 0x0000);
    _gfx->setTextColor(TEXT_C);
    _gfx->setTextSize(2);
    _gfx->setCursor(6, 6);
    _gfx->print("FLAPPY BIRD");
    _gfx->setCursor(340, 6);
    _gfx->print("Score:");
    _gfx->print(_score);
}

// ── drawGetReady ─────────────────────────────────────────────────────────────
void FlappyBirdGame::drawGetReady() {
    _gfx->fillRoundRect(130, 120, 220, 60, 10, 0x0000);
    _gfx->drawRoundRect(130, 120, 220, 60, 10, C_ACCENT);
    _gfx->setTextColor(TEXT_C);
    _gfx->setTextSize(2);
    _gfx->setCursor(148, 134);
    _gfx->print("TAP PARA JUGAR");
}

// ── drawGameOver ─────────────────────────────────────────────────────────────
void FlappyBirdGame::drawGameOver() {
    int bx = 100, by = 105, bw = 280, bh = 100;
    _gfx->fillRoundRect(bx, by, bw, bh, 10, 0x1082);
    _gfx->drawRoundRect(bx, by, bw, bh, 10, C_ACCENT);
    _gfx->setTextColor(C_WHITE);
    _gfx->setTextSize(3);
    _gfx->setCursor(bx + 20, by + 12);
    _gfx->print("GAME OVER");
    _gfx->setTextSize(2);
    _gfx->setCursor(bx + 40, by + 58);
    _gfx->print("Score: ");
    _gfx->print(_score);
    _gfx->flush();
}
