#include "snake.h"

// Paleta de colores
static const uint16_t BG = C_DARK_BG;
static const uint16_t HEAD_C = 0x07E0; // verde brillante
static const uint16_t BODY_C = 0x0340; // verde oscuro
static const uint16_t FOOD_C = C_RED;
static const uint16_t GRID_C = 0x1082; // gris muy oscuro
static const uint16_t TEXT_C = C_WHITE;
static const uint16_t HDR_C = 0x0000; // negro

// ── Constructor ─────────────────────────────────────────────────────────────
SnakeGame::SnakeGame(Arduino_Canvas *gfx, AXS15231B_Touch *touch)
    : _gfx(gfx), _touch(touch) {}

// ── begin ───────────────────────────────────────────────────────────────────
void SnakeGame::begin() {
  _len = 4;
  _body[0] = {15, 9};
  _body[1] = {14, 9};
  _body[2] = {13, 9};
  _body[3] = {12, 9};
  _dir = {1, 0};
  _nextDir = {1, 0};
  _score = 0;
  _gameOver = false;
  _lastMove = 0;
  _speed = 160;

  _gfx->fillScreen(BG);
  drawGrid();
  drawHeader();
  spawnFood();
  drawFood(FOOD_C);
  for (int i = 0; i < _len; i++)
    drawCell(_body[i].x, _body[i].y, i == 0 ? HEAD_C : BODY_C);
  _gfx->flush();
}

// ── update ──────────────────────────────────────────────────────────────────
bool SnakeGame::update() {
  if (_gameOver)
    return false;

  _ts.update(_touch);
  applyInput();

  unsigned long now = millis();
  if (now - _lastMove < (unsigned long)_speed)
    return true;
  _lastMove = now;

  _dir = _nextDir;

  // Nueva cabeza
  Point newHead = {(int8_t)(_body[0].x + _dir.x),
                   (int8_t)(_body[0].y + _dir.y)};

  // Colisión con bordes
  if (newHead.x < 0 || newHead.x >= COLS || newHead.y < 0 ||
      newHead.y >= ROWS) {
    drawGameOver();
    _gameOver = true;
    return false;
  }

  // Colisión consigo mismo
  for (int i = 0; i < _len; i++) {
    if (_body[i].x == newHead.x && _body[i].y == newHead.y) {
      drawGameOver();
      _gameOver = true;
      return false;
    }
  }

  bool ate = (newHead.x == _food.x && newHead.y == _food.y);

  // Borrar cola si no comió
  if (!ate) {
    eraseCell(_body[_len - 1].x, _body[_len - 1].y);
  }

  // Desplazar cuerpo
  for (int i = _len - 1; i > 0; i--)
    _body[i] = _body[i - 1];
  _body[0] = newHead;

  if (ate) {
    if (_len < MAX_LEN)
      _len++;
    _score += 10;
    if (_speed > 60)
      _speed -= 2;
    // Redibujar comida vieja como cuerpo, nueva comida
    drawFood(BG); // borrar comida anterior
    spawnFood();
    drawFood(FOOD_C);
    drawHeader(); // actualizar score
  }

  // Redibujar serpiente (cabeza + 2do segmento para el cambio de color)
  drawCell(_body[0].x, _body[0].y, HEAD_C);
  if (_len > 1)
    drawCell(_body[1].x, _body[1].y, BODY_C);

  _gfx->flush();
  return true;
}

// ── applyInput ──────────────────────────────────────────────────────────────
void SnakeGame::applyInput() {
  // Prioridad: dirección mientras arrastras (más responsivo) y si no, al soltar
  TouchState::Dir d = _ts.dragDir(20);
  if (d == TouchState::NONE)
    d = _ts.swipe(20);
  switch (d) {
  case TouchState::RIGHT:
    if (_dir.x != -1)
      _nextDir = {1, 0};
    break;
  case TouchState::LEFT:
    if (_dir.x != 1)
      _nextDir = {-1, 0};
    break;
  case TouchState::DOWN:
    if (_dir.y != -1)
      _nextDir = {0, 1};
    break;
  case TouchState::UP:
    if (_dir.y != 1)
      _nextDir = {0, -1};
    break;
  default:
    break;
  }
}

// ── spawnFood ───────────────────────────────────────────────────────────────
void SnakeGame::spawnFood() {
  bool valid = false;
  while (!valid) {
    _food.x = (int8_t)random(0, COLS);
    _food.y = (int8_t)random(0, ROWS);
    valid = true;
    for (int i = 0; i < _len; i++) {
      if (_body[i].x == _food.x && _body[i].y == _food.y) {
        valid = false;
        break;
      }
    }
  }
}

// ── Draw helpers ─────────────────────────────────────────────────────────────
void SnakeGame::drawGrid() {
  for (int x = 0; x <= COLS; x++)
    _gfx->drawFastVLine(x * CELL, HEADER_H, ROWS * CELL, GRID_C);
  for (int y = 0; y <= ROWS; y++)
    _gfx->drawFastHLine(0, HEADER_H + y * CELL, COLS * CELL, GRID_C);
}

void SnakeGame::drawHeader() {
  _gfx->fillRect(0, 0, DISP_W, HEADER_H, HDR_C);
  _gfx->setTextColor(TEXT_C);
  _gfx->setTextSize(2);
  _gfx->setCursor(6, 4);
  _gfx->print("SNAKE");
  _gfx->setCursor(180, 4);
  _gfx->print("Score: ");
  _gfx->print(_score);
}

void SnakeGame::drawCell(int x, int y, uint16_t color) {
  _gfx->fillRect(x * CELL + 2, HEADER_H + y * CELL + 2, CELL - 3, CELL - 3,
                 color);
}

void SnakeGame::eraseCell(int x, int y) {
  _gfx->fillRect(x * CELL + 1, HEADER_H + y * CELL + 1, CELL - 1, CELL - 1, BG);
  // Redibujar lineas de grilla sobre la celda borrada
  _gfx->drawFastVLine(x * CELL, HEADER_H + y * CELL, CELL, GRID_C);
  _gfx->drawFastVLine((x + 1) * CELL, HEADER_H + y * CELL, CELL, GRID_C);
  _gfx->drawFastHLine(x * CELL, HEADER_H + y * CELL, CELL, GRID_C);
  _gfx->drawFastHLine(x * CELL, HEADER_H + (y + 1) * CELL, CELL, GRID_C);
}

void SnakeGame::drawFood(uint16_t color) {
  int cx = _food.x * CELL + CELL / 2;
  int cy = HEADER_H + _food.y * CELL + CELL / 2;
  _gfx->fillCircle(cx, cy, CELL / 2 - 2, color);
}

void SnakeGame::drawGameOver() {
  int bx = 120, by = 110, bw = 240, bh = 90;
  _gfx->fillRoundRect(bx, by, bw, bh, 10, 0x1082);
  _gfx->drawRoundRect(bx, by, bw, bh, 10, C_ACCENT);
  _gfx->setTextColor(C_WHITE);
  _gfx->setTextSize(3);
  _gfx->setCursor(bx + 30, by + 14);
  _gfx->print("GAME OVER");
  _gfx->setTextSize(2);
  _gfx->setCursor(bx + 40, by + 56);
  _gfx->print("Score: ");
  _gfx->print(_score);
  _gfx->flush();
}
