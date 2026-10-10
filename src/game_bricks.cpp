// Bricks. The rules are the same as in the browser version: three balls, a paddle that sends the ball off the
// steeper the further from its middle it lands, coloured bricks that break at one hit and strong ones at two,
// steel that never breaks and is not needed to clear a level, and capsules that fall out of broken bricks.
#include "app.h"
#include <math.h>
#include <stdio.h>

namespace bricks {

// Ten bricks of 22 pixels fill the width, and the browser's wall is ten bricks wide as well, so its six levels
// are copied in below letter for letter. Everything else is two thirds of its size there, because the screen is
// 240 x 320 against 360 x 480: the paddle, the ball, the capsules and every speed.
static const int COLS = 10, ROWS_MAX = 7, MAXB = COLS * ROWS_MAX, BW = 22, BH = 10, BX0 = 10, BY0 = 50;
static const int FX = 4, FY = 36, FW = 232, FH = 230, WALLT = 6;
static const int LEFT = FX + WALLT, RIGHT = FX + FW - WALLT;   // the ball turns back at these
static const int TOP = FY + WALLT, FLOOR = FY + FH;            // and is gone once it is below the floor
static const int PADDLE_Y = 248, PH = 8, PW = 40, PW_WIDE = 64, BR = 4;
static const int START_LIVES = 3, MAX_LIVES = 5, DROP_PERCENT = 13, WIDE_TIME = 18, SLOW_TIME = 14;
static const int CAP_W = 22, CAP_H = 12, MAXBALL = 12, MAXCAP = 8, MAXCHIP = 32;
static const int STATUS_Y = 270;
static const float BASE_SPEED = 160.0f, LEVEL_UP = 0.07f, HIT_UP = 1.015f, SPEED_CAP = 1.45f;
static const float SLOW_FACTOR = 0.68f, CAP_SPEED = 76.0f, DRAG_GAIN = 1.15f;
static const float DT = 0.02f, CHIP_GRAVITY = 400.0f, CHIP_LIFE = 0.8f, CAP_HALF = CAP_H / 2.0f;
static const uint32_t TICK = 20;          // one step of the game is this many milliseconds
static const int CLEAR_TICKS = 70;        // the pause on a cleared level, in steps
static const ui::Rect B_MENU = {4, 292, 74, 26}, B_NEW = {83, 292, 153, 26};
static const ui::Rect FIELD = {FX, FY, FW, FH};

// The chrome is grey and one yellow on purpose. The brick faces are the colours the player picked for the
// marbles, the set that was settled on the board itself, and nothing else here leans on telling hues apart:
// a strong brick is known by its crack and a steel one by its bolts, as in the browser.
static const uint16_t WALL_C = RGB(74, 82, 90), WALL_HI = RGB(104, 112, 120), FIELD_BG = RGB(20, 27, 35);
static const uint16_t MORTAR = RGB(46, 53, 62), STEEL = RGB(146, 154, 162), STEEL_HI = RGB(196, 202, 208);
static const uint16_t STEEL_DK = RGB(86, 94, 102), CLINKER = RGB(96, 80, 72), CLINKER_HI = RGB(136, 116, 104);
static const uint16_t CLINKER_DK = RGB(58, 46, 40), CRACK = RGB(24, 18, 14);
static const uint16_t HAZARD = RGB(236, 184, 46), HAZARD_DK = RGB(58, 44, 12);
static const uint16_t BALL_C = RGB(222, 228, 234), BALL_DK = RGB(112, 120, 128), BALL_HI = RGB(255, 255, 255);

// '.' empty, '1'..'6' a brick of that course colour, 'S' a strong brick, 'X' steel. Straight from the browser.
static const char* const LEVELS[6][ROWS_MAX] = {
  {"1111111111", "2222222222", "3333333333", "4444444444", "5555555555", "6666666666", 0},
  {"....11....", "...2222...", "..333333..", ".44444444.", "5555555555", "6666666666", "SSSSSSSSSS"},
  {"1.1.1.1.1.", ".2.2.2.2.2", "S3S3S3S3S3", "4S4S4S4S4S", "5.5.5.5.5.", ".6.6.6.6.6", 0},
  {"X........X", "1X......X1", "22X....X22", "333SSSS333", "4444444444", "5555555555", "6666666666"},
  {"SSSSSSSSSS", "1111111111", "X.XX..XX.X", "3333333333", "4444444444", "5555555555", "6666666666"},
  {"XXX....XXX", "X11SSSS11X", "X22222222X", "X33333333X", "X44S44S44X", "X55555555X", ".66666666."}
};

enum { KIND_PLAIN, KIND_STRONG, KIND_STEEL };
enum { CAP_WIDE, CAP_SLOW, CAP_MULTI, CAP_LIFE };
enum { ST_READY, ST_PLAY, ST_CLEAR, ST_OVER, ST_PAUSE };

struct Brick { int16_t x, y; uint8_t c, kind, hp; };
// px and py are where the thing was last drawn, so it can be rubbed out again; NOWHERE means it is not drawn.
static const int16_t NOWHERE = -1000;
struct Ball { float x, y, vx, vy; bool stuck; int16_t px, py; };
struct Cap { float x, y; uint8_t kind; int16_t px, py; };
struct Chip { float x, y, vx, vy, t; uint8_t c, kind; int16_t px, py; };

static Brick wall[MAXB];
static Ball balls[MAXBALL];
static Cap caps[MAXCAP];
static Chip chips[MAXCHIP];
static int wallN = 0, ballN = 0, capN = 0, chipN = 0, rowsN = 0;
static int level = 0, cycle = 0, score = 0, bestV = 0, lives = START_LIVES;
static float paddleX = hw::W / 2.0f, wideLeft = 0, slowLeft = 0, hitBoost = 1.0f;
static int state = ST_READY, clearLeft = 0;
static int drawnPadX = NOWHERE, drawnPadW = 0, drawnScore = -1;
static bool started = false, bestLoaded = false, overRecord = false, showMoves = true, noDrops = false;
static bool dragging = false;
static float dragAt = 0, dragMoved = 0;
static uint32_t confirmAt = 0, lastTick = 0;
static char statusText[80] = "";

static int paddleW() { return wideLeft > 0 ? PW_WIDE : PW; }
static float levelSpeed() { return BASE_SPEED * (1.0f + LEVEL_UP * (level + cycle * 6)); }
static float ballSpeed() {
  float boost = hitBoost > SPEED_CAP ? SPEED_CAP : hitBoost;
  return levelSpeed() * boost * (slowLeft > 0 ? SLOW_FACTOR : 1.0f);
}
static int levelNumber() { return level + 1 + cycle * 6; }
static bool breakable() {
  for (int k = 0; k < wallN; k++) if (wall[k].kind != KIND_STEEL) return true;
  return false;
}
static float rnd01() { return (float)(hw::rnd() % 10000u) / 10000.0f; }

/* ---------- drawing ---------- */

static void drawBrick(const Brick& b) {
  int x = b.x + 1, y = b.y + 1, w = BW - 2, h = BH - 2;     // the mortar shows through the gap all round
  if (b.kind == KIND_STEEL) {
    hw::fillRect(x, y, w, h, STEEL);
    hw::fillRect(x, y, w, 1, STEEL_HI);
    hw::fillRect(x, y + h - 1, w, 1, STEEL_DK);
    hw::fillRect(x + 3, y + h / 2 - 1, 2, 2, STEEL_DK);     // two bolts
    hw::fillRect(x + w - 5, y + h / 2 - 1, 2, 2, STEEL_DK);
    return;
  }
  bool strong = b.kind == KIND_STRONG;
  hw::fillRect(x, y, w, h, strong ? CLINKER : lines::ballColor(b.c));
  hw::fillRect(x, y, w, 1, strong ? CLINKER_HI : lines::ballShade(b.c, 135));   // glaze catching the light
  hw::fillRect(x, y + h - 1, w, 1, strong ? CLINKER_DK : lines::ballShade(b.c, 72));
  if (!strong) return;
  hw::fillRect(x + 3, y + 3, w - 6, 1, CLINKER_HI);
  if (b.hp == 1) {                                          // a strong brick shows a crack once it is hit
    hw::drawLine(x + 7, y, x + 9, y + 3, CRACK);
    hw::drawLine(x + 9, y + 3, x + 7, y + 5, CRACK);
    hw::drawLine(x + 7, y + 5, x + 10, y + h - 1, CRACK);
  }
}
// The paddle is built in a buffer and sent in one go: it is redrawn every time the finger moves, and seven
// slanted stripes laid down as separate rectangles flicker and cost far more than one blit.
static void paintPaddle() {
  static uint16_t buf[PW_WIDE * PH];
  int w = paddleW(), x = (int)(paddleX - w / 2.0f + 0.5f);
  for (int row = 0; row < PH; row++) {
    int inset = (row == 0 || row == PH - 1) ? 2 : ((row == 1 || row == PH - 2) ? 1 : 0);
    for (int col = 0; col < w; col++) {
      bool out = col < inset || col >= w - inset;
      buf[row * w + col] = out ? FIELD_BG : (((col + PH - 1 - row) % 8) < 4 ? HAZARD_DK : HAZARD);
    }
  }
  hw::blit(x, PADDLE_Y, w, PH, buf);
}
static ui::Rect paddleRect() {
  int w = paddleW();
  return ui::Rect{(int)(paddleX - w / 2.0f + 0.5f), PADDLE_Y, w, PH};
}
static void drawBall(int cx, int cy) {
  hw::fillCircle(cx, cy, BR, BALL_DK);
  hw::fillCircle(cx, cy, BR - 1, BALL_C);
  hw::fillRect(cx - 2, cy - 2, 2, 2, BALL_HI);
}
// A capsule says what it does with a picture rather than a letter, so it reads the same in both languages.
static void drawCap(int cx, int cy, int kind) {
  static const int FACE[4] = {4, 5, 6, 1};          // which of the ball colours each capsule wears
  uint16_t body = lines::ballColor(FACE[kind]);
  hw::fillRoundRect(cx - CAP_W / 2, cy - CAP_H / 2, CAP_W, CAP_H, 5, body);
  hw::fillRect(cx - 7, cy - 3, 14, 1, lines::ballShade(FACE[kind], 150));
  uint16_t ink = RGB(255, 255, 255);
  if (kind == CAP_WIDE) {                           // a bar with an arrow at both ends
    hw::fillRect(cx - 5, cy, 11, 1, ink);
    hw::fillTriangle(cx - 8, cy, cx - 4, cy - 3, cx - 4, cy + 3, ink);
    hw::fillTriangle(cx + 8, cy, cx + 4, cy - 3, cx + 4, cy + 3, ink);
  } else if (kind == CAP_SLOW) {                    // an hourglass
    hw::fillTriangle(cx - 3, cy - 3, cx + 3, cy - 3, cx, cy, ink);
    hw::fillTriangle(cx - 3, cy + 3, cx + 3, cy + 3, cx, cy, ink);
  } else if (kind == CAP_MULTI) {                   // three balls
    for (int d = -4; d <= 4; d += 4) hw::fillCircle(cx + d, cy, 1, ink);
  } else {                                          // a heart
    hw::fillCircle(cx - 2, cy - 1, 2, ink);
    hw::fillCircle(cx + 2, cy - 1, 2, ink);
    hw::fillTriangle(cx - 4, cy, cx + 4, cy, cx, cy + 4, ink);
  }
}
static void drawChip(const Chip& c) {
  uint16_t color = c.kind == KIND_STRONG ? CLINKER_HI : lines::ballColor(c.c);
  hw::fillRect((int)c.x - 1, (int)c.y - 1, 3, 3, color);
}

// Puts a piece of the field back: the background, the mortar bed, every brick that reaches into it, the paddle.
// Everything that moves is rubbed out this way, so no frame has to redraw the whole field.
static void repaintArea(int x, int y, int w, int h) {
  if (x < LEFT) { w += x - LEFT; x = LEFT; }
  if (y < TOP) { h += y - TOP; y = TOP; }
  if (x + w > RIGHT) w = RIGHT - x;
  if (y + h > FLOOR) h = FLOOR - y;
  if (w <= 0 || h <= 0) return;
  hw::fillRect(x, y, w, h, FIELD_BG);
  int bx = BX0 - 2, by = BY0 - 2, bw = COLS * BW + 4, bh = rowsN * BH + 4;
  int ix = x > bx ? x : bx, iy = y > by ? y : by;
  int iw = (x + w < bx + bw ? x + w : bx + bw) - ix, ih = (y + h < by + bh ? y + h : by + bh) - iy;
  if (iw > 0 && ih > 0) hw::fillRect(ix, iy, iw, ih, MORTAR);
  for (int k = 0; k < wallN; k++) {
    const Brick& b = wall[k];
    if (b.x + BW <= x || b.x >= x + w || b.y + BH <= y || b.y >= y + h) continue;
    drawBrick(b);
  }
  ui::Rect p = paddleRect();
  if (state != ST_OVER && x < p.x + p.w && x + w > p.x && y < p.y + p.h && y + h > p.y) paintPaddle();
}

static void paintScore() {
  char text[12];
  snprintf(text, sizeof text, "%d", score);
  ui::label(6, 15, 56, 19, text, FONT_M, C_INK, C_BG, ui::LEFT);
  drawnScore = score;
}
static void paintBest() {
  char text[12];
  snprintf(text, sizeof text, "%d", bestV > score ? bestV : score);
  ui::label(64, 15, 56, 19, text, FONT_M, C_INK, C_BG, ui::CENTER);
}
static void paintLevel() {
  char text[12];
  snprintf(text, sizeof text, "%d", levelNumber());
  ui::label(122, 15, 58, 19, text, FONT_M, C_INK, C_BG, ui::CENTER);
}
static void paintLives() {
  hw::fillRect(182, 15, 52, 19, C_BG);
  for (int k = 0; k < lives; k++) {
    int cx = 190 + k * 11;
    hw::fillCircle(cx, 24, 4, BALL_DK);
    hw::fillCircle(cx, 24, 3, lives > 1 ? BALL_C : C_DANGER);
  }
}
static void paintStatus() {
  uint16_t color = confirmAt ? C_INK : (state == ST_OVER ? (overRecord ? C_GOOD : C_DANGER)
                                                         : (state == ST_CLEAR ? C_GOOD : C_MUTED));
  ui::label(0, STATUS_Y, hw::W, 16, confirmAt ? T(S_L_CONFIRM) : statusText, FONT_S, color, C_BG);
}
static void say(StrId id) {
  snprintf(statusText, sizeof statusText, "%s", T(id));
  if (showMoves) paintStatus();
}
static void say2(StrId id, int a, int b) {
  app::fillText(statusText, sizeof statusText, T(id), "", a, b);
  if (showMoves) paintStatus();
}
static void paintBar() {
  ui::button(B_MENU.x, B_MENU.y, B_MENU.w, B_MENU.h, T(S_MENU), FONT_M, C_INK, C_PANEL);
  bool hot = confirmAt != 0;
  ui::button(B_NEW.x, B_NEW.y, B_NEW.w, B_NEW.h, T(S_L_NEW), FONT_M,
             hot || state == ST_OVER ? C_ON_ACCENT : C_INK,
             hot ? C_DANGER : (state == ST_OVER ? C_ACCENT : C_PANEL));
}
// Draws the ball, the capsules and the chips where they are now, and remembers where that was. A thing whose
// box is not wholly inside the field is left undrawn: repaintArea cannot rub out what falls outside it.
// Both the full repaint and the frame-by-frame one go through here, so the two agree pixel for pixel.
static void drawThings() {
  for (int k = 0; k < chipN; k++) {
    Chip& c = chips[k];
    bool in = c.x - 2 >= LEFT && c.x + 2 < RIGHT && c.y - 2 >= TOP && c.y + 2 < FLOOR;
    if (in) drawChip(c);
    c.px = in ? (int16_t)c.x : NOWHERE; c.py = (int16_t)c.y;
  }
  for (int k = 0; k < capN; k++) {
    Cap& c = caps[k];
    bool in = c.y - CAP_HALF >= TOP && c.y + CAP_HALF < FLOOR;
    if (in) drawCap((int)c.x, (int)c.y, c.kind);
    c.px = in ? (int16_t)c.x : NOWHERE; c.py = (int16_t)c.y;
  }
  for (int k = 0; k < ballN; k++) {
    Ball& b = balls[k];
    bool in = b.y + BR < FLOOR;
    if (in) drawBall((int)b.x, (int)b.y);
    b.px = in ? (int16_t)b.x : NOWHERE; b.py = (int16_t)b.y;
  }
}
// The whole field, walls and all. Everything that moves is drawn again where it is now.
static void paintField() {
  hw::fillRect(FX, FY, FW, WALLT, WALL_C);                      // the frame: left, right and over the top,
  hw::fillRect(FX, FY, WALLT, FH, WALL_C);                      // with the floor left open as in the browser
  hw::fillRect(RIGHT, FY, WALLT, FH, WALL_C);
  hw::fillRect(FX, FY, FW, 1, WALL_HI);
  hw::fillRect(LEFT, TOP, RIGHT - LEFT, FLOOR - TOP, FIELD_BG);
  hw::fillRect(BX0 - 2, BY0 - 2, COLS * BW + 4, rowsN * BH + 4, MORTAR);
  for (int k = 0; k < wallN; k++) drawBrick(wall[k]);
  if (state != ST_OVER) paintPaddle();
  ui::Rect p = paddleRect();
  drawnPadX = p.x; drawnPadW = p.w;
  drawThings();
}
static void paintAll() {
  if (!showMoves) return;
  hw::fillRect(0, 0, hw::W, hw::H, C_BG);
  ui::label(6, 2, 56, 13, T(S_L_SCORE), FONT_S, C_MUTED, C_BG, ui::LEFT);
  ui::label(64, 2, 56, 13, T(S_BEST), FONT_S, C_MUTED, C_BG, ui::CENTER);
  ui::label(122, 2, 58, 13, T(S_K_LEVEL), FONT_S, C_MUTED, C_BG, ui::CENTER);
  ui::label(182, 2, 52, 13, T(S_K_BALLS), FONT_S, C_MUTED, C_BG, ui::CENTER);
  paintScore(); paintBest(); paintLevel(); paintLives();
  paintField();
  paintStatus(); paintBar();
}
// Rubs out everything that moves where it was drawn last time, then draws it where it is now.
static void paintMoving() {
  if (!showMoves) return;
  if (score != drawnScore) { paintScore(); paintBest(); }     // bricks break between one frame and the next
  for (int k = 0; k < chipN; k++) if (chips[k].px != NOWHERE) repaintArea(chips[k].px - 2, chips[k].py - 2, 5, 5);
  for (int k = 0; k < capN; k++) if (caps[k].px != NOWHERE) repaintArea(caps[k].px - CAP_W / 2, caps[k].py - CAP_H / 2, CAP_W, CAP_H);
  for (int k = 0; k < ballN; k++) if (balls[k].px != NOWHERE) repaintArea(balls[k].px - BR, balls[k].py - BR, 2 * BR + 1, 2 * BR + 1);
  ui::Rect p = paddleRect();
  if (drawnPadX != p.x || drawnPadW != p.w) {
    int from = drawnPadX < p.x ? drawnPadX : p.x, to = drawnPadX + drawnPadW > p.x + p.w ? drawnPadX + drawnPadW : p.x + p.w;
    repaintArea(from, PADDLE_Y, to - from, PH);
    drawnPadX = p.x; drawnPadW = p.w;
  }
  drawThings();
}

// The menu icon: three courses of brick with a ball resting at the foot of them.
void drawIcon(int x, int y) {
  for (int row = 0; row < 3; row++) {
    int by = y + row * 9, off = row & 1 ? -8 : 0;
    for (int col = 0; col < 3; col++) {
      int bx = x + off + col * 17;
      int a = bx < x ? x : bx, b = bx + 16 > x + 34 ? x + 34 : bx + 16;
      if (b > a) {
        hw::fillRect(a, by, b - a, 8, lines::ballColor(row + 1));
        hw::fillRect(a, by, b - a, 1, lines::ballShade(row + 1, 135));
      }
    }
  }
  hw::fillCircle(x + 26, y + 30, 4, BALL_DK);
  hw::fillCircle(x + 26, y + 30, 3, BALL_C);
}

/* ---------- rules ---------- */

static void loadBest() {
  if (!bestLoaded) { bestV = hw::loadInt("kbest", 0); bestLoaded = true; }
}
int best() { loadBest(); return bestV; }
void resetBest() { bestV = 0; bestLoaded = true; hw::saveInt("kbest", 0); }

static void addScore(int n) {
  score += n;
  loadBest();
  if (score > bestV) { bestV = score; overRecord = true; hw::saveInt("kbest", bestV); }
}
static void buildLevel(int n) {
  const char* const* rows = LEVELS[n % 6];
  wallN = 0; rowsN = 0;
  for (int r = 0; r < ROWS_MAX && rows[r]; r++) {
    rowsN = r + 1;
    for (int c = 0; c < COLS; c++) {
      char ch = rows[r][c];
      if (ch == '.' || !ch) continue;
      Brick& b = wall[wallN++];
      b.x = (int16_t)(BX0 + c * BW); b.y = (int16_t)(BY0 + r * BH);
      b.kind = (uint8_t)(ch == 'S' ? KIND_STRONG : (ch == 'X' ? KIND_STEEL : KIND_PLAIN));
      b.c = (uint8_t)(b.kind == KIND_PLAIN ? ch - '0' : r % 6 + 1);
      b.hp = (uint8_t)(b.kind == KIND_STRONG ? 2 : 1);
    }
  }
}
static void serveBall() {
  ballN = 1;
  balls[0].x = paddleX; balls[0].y = PADDLE_Y - BR - 1;
  balls[0].vx = 0; balls[0].vy = 0; balls[0].stuck = true;
  balls[0].px = NOWHERE; balls[0].py = NOWHERE;
  capN = 0; wideLeft = 0; slowLeft = 0; hitBoost = 1.0f;
}
static void reset() {
  level = 0; cycle = 0; score = 0; lives = START_LIVES;
  overRecord = false; confirmAt = 0; chipN = 0; dragging = false;
  paddleX = hw::W / 2.0f;
  drawnPadX = NOWHERE;
  buildLevel(0); serveBall();
  state = ST_READY; clearLeft = 0;
  started = true;
  lastTick = hw::ms();
  say2(S_K_READY, 1, 0);
}
static void launch() {
  for (int k = 0; k < ballN; k++) {
    Ball& b = balls[k];
    if (!b.stuck) continue;
    float a = (rnd01() - 0.5f) * 0.6f, s = ballSpeed();
    b.stuck = false; b.vx = sinf(a) * s; b.vy = -cosf(a) * s;
  }
}
static void start() {
  if (state == ST_OVER) return;             // New game is the way out of a finished game
  if (state == ST_CLEAR) return;
  if (state == ST_PAUSE || state == ST_READY) {
    bool wasReady = state == ST_READY;
    state = ST_PLAY;
    if (wasReady) { launch(); snd::play(520, 40); }
    say(S_K_PLAY);
    if (showMoves) paintBar();
  }
}
static void dropCapsule(int kind, float x, float y) {
  if (capN >= MAXCAP) return;
  Cap& c = caps[capN++];
  c.x = x; c.y = y; c.kind = (uint8_t)kind; c.px = NOWHERE; c.py = NOWHERE;
}
static void breakBrick(int k) {
  Brick b = wall[k];
  for (int j = k; j < wallN - 1; j++) wall[j] = wall[j + 1];
  wallN--;
  addScore(b.kind == KIND_STRONG ? 20 : 10);
  if (showMoves) repaintArea(b.x, b.y, BW, BH);
  for (int j = 0; j < 4 && chipN < MAXCHIP; j++) {
    Chip& c = chips[chipN++];
    c.x = b.x + BW / 2.0f; c.y = b.y + BH / 2.0f;
    c.vx = (rnd01() - 0.5f) * 107.0f; c.vy = -rnd01() * 80.0f;
    c.t = 0; c.c = b.c; c.kind = b.kind; c.px = NOWHERE; c.py = NOWHERE;
  }
  if (!noDrops && (int)(hw::rnd() % 100u) < DROP_PERCENT) {
    static const int KINDS[7] = {CAP_WIDE, CAP_WIDE, CAP_SLOW, CAP_SLOW, CAP_MULTI, CAP_MULTI, CAP_LIFE};
    dropCapsule(KINDS[hw::rnd() % 7u], b.x + BW / 2.0f, b.y + BH / 2.0f);
  }
  snd::play(b.kind == KIND_STRONG ? 300 : 660 + (6 - b.c) * 40, 25);
}
// Steel turns the ball by a hair, so it cannot settle into an endless loop between the paddle and the steel.
static void nudge(Ball& b) {
  float a = (rnd01() - 0.5f) * 0.24f, c = cosf(a), s = sinf(a), vx = b.vx;
  b.vx = vx * c - b.vy * s; b.vy = vx * s + b.vy * c;
}
static void hitBrick(int k, Ball& b) {
  if (wall[k].kind == KIND_STEEL) { nudge(b); snd::play(1400, 15); return; }
  if (--wall[k].hp <= 0) { breakBrick(k); return; }
  snd::play(260, 25);
  if (showMoves) drawBrick(wall[k]);        // the crack appears
}
static void levelCleared() {
  state = ST_CLEAR; clearLeft = CLEAR_TICKS;
  ballN = 0; capN = 0;
  int bonus = 100 * levelNumber();
  addScore(bonus);
  say2(S_K_CLEAR, levelNumber(), bonus);
  snd::play(523, 130); snd::note(659, 130); snd::note(784, 130); snd::note(1047, 130); snd::note(1319, 220);
  if (showMoves) { paintScore(); paintBest(); paintField(); paintStatus(); }
}
static void nextLevel() {
  level++;
  if (level >= 6) { level = 0; cycle++; }
  buildLevel(level); serveBall();
  state = ST_READY;
  say2(S_K_READY, levelNumber(), 0);
  if (showMoves) { paintLevel(); paintField(); paintStatus(); }
}
static void finish() {
  state = ST_OVER;
  say2(overRecord ? S_K_OVER_BEST : S_K_OVER, score, 0);
  snd::play(300, 200); snd::note(220, 160); snd::note(110, 320);
  if (showMoves) { paintField(); paintStatus(); paintBar(); }
}
static void loseBall() {
  lives--;
  snd::play(300, 180); snd::note(150, 180);
  if (lives <= 0) { lives = 0; if (showMoves) paintLives(); finish(); return; }
  serveBall();
  state = ST_READY;
  say2(S_K_LOST, lives, 0);
  if (showMoves) { paintLives(); paintField(); paintStatus(); }
}
static void catchCapsule(int kind) {
  if (kind == CAP_WIDE) {
    wideLeft = (float)WIDE_TIME;        // paintMoving sees the paddle change width and redraws both boxes
    say2(S_K_WIDE, WIDE_TIME, 0);
  } else if (kind == CAP_SLOW) {
    slowLeft = (float)SLOW_TIME;
    say2(S_K_SLOW, SLOW_TIME, 0);
  } else if (kind == CAP_LIFE) {
    if (lives < MAX_LIVES) lives++;
    say(S_K_LIFE);
    if (showMoves) paintLives();
  } else {
    // every ball in play splits in three
    int was = ballN;
    for (int k = 0; k < was; k++) {
      Ball& b = balls[k];
      if (b.stuck) continue;
      float s = sqrtf(b.vx * b.vx + b.vy * b.vy), a = atan2f(b.vx, -b.vy);
      if (s <= 0) s = ballSpeed();
      for (int d = 0; d < 2 && ballN < MAXBALL; d++) {
        float na = a + (d ? 0.35f : -0.35f);
        Ball& n = balls[ballN++];
        n.x = b.x; n.y = b.y; n.stuck = false;
        n.vx = sinf(na) * s; n.vy = -fabsf(cosf(na) * s);
        n.px = NOWHERE; n.py = NOWHERE;
      }
    }
    say(S_K_MULTI);
  }
  snd::play(660, 50); snd::note(880, 50); snd::note(1100, 60);
}

// Moves one ball a short way and settles its collisions; false when the ball is gone.
static bool moveBall(Ball& b, float h) {
  float px = b.x, py = b.y;
  b.x += b.vx * h; b.y += b.vy * h;
  if (b.x < LEFT + BR) { b.x = 2 * (LEFT + BR) - b.x; b.vx = fabsf(b.vx); snd::play(200, 12); }
  else if (b.x > RIGHT - BR) { b.x = 2 * (RIGHT - BR) - b.x; b.vx = -fabsf(b.vx); snd::play(200, 12); }
  if (b.y < TOP + BR) { b.y = 2 * (TOP + BR) - b.y; b.vy = fabsf(b.vy); snd::play(200, 12); }
  // the paddle catches only from above, so a ball that slipped past is not scooped back up
  float half = paddleW() / 2.0f;
  if (b.vy > 0 && py + BR <= PADDLE_Y + 1 && b.y + BR >= PADDLE_Y &&
      b.x >= paddleX - half - BR && b.x <= paddleX + half + BR) {
    float rel = (b.x - paddleX) / (half + BR);
    if (rel < -1.0f) rel = -1.0f;
    if (rel > 1.0f) rel = 1.0f;
    float a = rel * 1.05f;
    // never straight up: a ball that goes up and down one column forever is no fun
    if (a == 0.0f) a = (hw::rnd() & 1u) ? 0.1f : -0.1f;
    else if (a > -0.1f && a < 0.1f) a = a < 0 ? -0.1f : 0.1f;
    hitBoost *= HIT_UP;
    if (hitBoost > SPEED_CAP) hitBoost = SPEED_CAP;
    float s = ballSpeed();
    b.vx = sinf(a) * s; b.vy = -cosf(a) * s;
    b.y = PADDLE_Y - BR;
    snd::play(420, 20);
  }
  // bricks: the first one the ball overlaps; the side it came from decides the bounce
  for (int k = 0; k < wallN; k++) {
    const Brick& br = wall[k];
    float nx = b.x < br.x ? br.x : (b.x > br.x + BW ? br.x + BW : b.x);
    float ny = b.y < br.y ? br.y : (b.y > br.y + BH ? br.y + BH : b.y);
    float dx = b.x - nx, dy = b.y - ny;
    if (dx * dx + dy * dy > (float)(BR * BR)) continue;
    bool fromSide = px + BR <= br.x || px - BR >= br.x + BW;
    bool fromEnd = py + BR <= br.y || py - BR >= br.y + BH;
    if (fromSide && !fromEnd) b.vx = -b.vx;
    else if (fromEnd && !fromSide) b.vy = -b.vy;
    else { b.vx = -b.vx; b.vy = -b.vy; }
    b.x = px; b.y = py;
    hitBrick(k, b);
    break;
  }
  return b.y - BR <= FLOOR;
}

// One step of the game, always DT of game time, so the board and the tests play it the same way.
static void step() {
  for (int k = chipN - 1; k >= 0; k--) {
    Chip& c = chips[k];
    c.t += DT; c.vy += CHIP_GRAVITY * DT; c.x += c.vx * DT; c.y += c.vy * DT;
    if (c.t > CHIP_LIFE) {
      if (showMoves && c.px != NOWHERE) repaintArea(c.px - 2, c.py - 2, 5, 5);
      chips[k] = chips[--chipN];
    }
  }
  if (state == ST_CLEAR) {
    if (--clearLeft <= 0) nextLevel();
    return;
  }
  if (state != ST_PLAY && state != ST_READY) return;
  float half = paddleW() / 2.0f;
  if (paddleX < LEFT + half) paddleX = LEFT + half;
  if (paddleX > RIGHT - half) paddleX = RIGHT - half;
  for (int k = 0; k < ballN; k++) if (balls[k].stuck) balls[k].x = paddleX;
  if (state != ST_PLAY) return;

  if (wideLeft > 0) wideLeft -= DT;
  if (slowLeft > 0) slowLeft -= DT;
  float s = ballSpeed();
  for (int k = ballN - 1; k >= 0; k--) {
    Ball& b = balls[k];
    if (b.stuck) continue;
    float cur = sqrtf(b.vx * b.vx + b.vy * b.vy);
    if (cur <= 0) cur = s;
    b.vx *= s / cur; b.vy *= s / cur;
    // never let a ball crawl almost sideways forever
    if (fabsf(b.vy) < s * 0.22f) {
      b.vy = (b.vy < 0 ? -1.0f : 1.0f) * s * 0.22f;
      b.vx = (b.vx < 0 ? -1.0f : 1.0f) * sqrtf(s * s - b.vy * b.vy);
    }
    int sub = (int)(s * DT / 1.8f) + 1;
    bool alive = true;
    for (int n = 0; n < sub && alive; n++) alive = moveBall(b, DT / sub);
    if (!alive) {
      if (showMoves && b.px != NOWHERE) repaintArea(b.px - BR, b.py - BR, 2 * BR + 1, 2 * BR + 1);
      balls[k] = balls[--ballN];
    }
    if (!breakable()) { levelCleared(); return; }
  }
  for (int k = capN - 1; k >= 0; k--) {
    Cap& c = caps[k];
    c.y += CAP_SPEED * DT;
    bool caught = c.y + CAP_HALF >= PADDLE_Y && c.y - CAP_HALF <= PADDLE_Y + PH &&
                  fabsf(c.x - paddleX) <= paddleW() / 2.0f + 8.0f;
    if (caught || c.y > FLOOR + CAP_H) {
      int kind = c.kind;
      if (showMoves && c.px != NOWHERE) repaintArea(c.px - CAP_W / 2, c.py - CAP_H / 2, CAP_W, CAP_H);
      caps[k] = caps[--capN];
      if (caught) catchCapsule(kind);
    }
  }
  if (!ballN) loseBall();
}

/* ---------- screen ---------- */

static void retell() {       // the status line again, in whatever language is set now
  if (state == ST_OVER) say2(overRecord ? S_K_OVER_BEST : S_K_OVER, score, 0);
  else if (state == ST_CLEAR) say2(S_K_CLEAR, levelNumber(), 100 * levelNumber());
  else if (state == ST_PLAY) say(S_K_PLAY);
  else if (state == ST_PAUSE) say(S_K_PAUSE);
  else say2(S_K_READY, levelNumber(), 0);
}

void enter() {
  if (!started) reset();
  // a game left for the menu waits where it was; the ball does not fly on while nobody is looking
  if (state == ST_PLAY) state = ST_PAUSE;
  confirmAt = 0;
  dragging = false;
  lastTick = hw::ms();
  loadBest();
  retell();
  paintAll();
}

void update() {
  using app::touch;
  if (confirmAt && hw::ms() - confirmAt > 3500) { confirmAt = 0; paintStatus(); paintBar(); }

  if (touch.pressed) {
    int x = touch.x, y = touch.y;
    if (B_NEW.has(x, y)) {
      if (score > 0 && state != ST_OVER && !confirmAt) {
        if (state == ST_PLAY) { state = ST_PAUSE; say(S_K_PAUSE); }
        confirmAt = hw::ms(); paintStatus(); paintBar(); snd::play(660, 40);
      } else { confirmAt = 0; reset(); snd::play(880, 25); paintAll(); }
      return;
    }
    if (confirmAt) { confirmAt = 0; paintStatus(); paintBar(); }
    if (B_MENU.has(x, y)) { snd::play(880, 25); app::go(app::SCR_MENU); return; }
    if (FIELD.has(x, y)) { dragging = true; dragAt = (float)touch.liveX; dragMoved = 0; }
  }
  // The finger drags the paddle by its own movement rather than sitting on top of it, which is what the
  // browser does on a phone. A press that hardly moved is a tap, and a tap launches the ball.
  if (dragging && touch.down) {
    float dx = (float)touch.liveX - dragAt;
    dragAt = (float)touch.liveX;
    dragMoved += dx < 0 ? -dx : dx;
    if (state == ST_PLAY || state == ST_READY) paddleX += dx * DRAG_GAIN;
  } else if (dragging && !touch.down) {
    dragging = false;
    if (dragMoved < 8.0f) start();
  }

  uint32_t now = hw::ms();
  uint32_t due = now - lastTick;
  if (due < TICK) return;
  int steps = (int)(due / TICK);
  lastTick += (uint32_t)steps * TICK;
  if (steps > 3) steps = 3;        // after a long frame the game catches up, it does not teleport
  for (int k = 0; k < steps; k++) step();
  paintMoving();
}

#ifdef CYD_SIM   // views and shortcuts for the tests in sim/
void simReset() { reset(); }
void simAnimate(bool on) { showMoves = on; }
void simNoDrops(bool on) { noDrops = on; }
void simStep(int ticks) { for (int k = 0; k < ticks; k++) step(); }
void simLaunch() { start(); }
void simPaddleTo(int x) {
  float half = paddleW() / 2.0f;
  paddleX = (float)x;
  if (paddleX < LEFT + half) paddleX = LEFT + half;
  if (paddleX > RIGHT - half) paddleX = RIGHT - half;
}
void simDrop(int kind, int x, int y) { dropCapsule(kind, (float)x, (float)y); }
int simPaddleX() { return (int)paddleX; }
int simPaddleW() { return paddleW(); }
int simBalls() { return ballN; }
int simBallX(int k) { return (int)balls[k].x; }
int simBallY(int k) { return (int)balls[k].y; }
bool simStuck(int k) { return balls[k].stuck; }
int simSpeed() { return (int)ballSpeed(); }
int simScore() { return score; }
int simLives() { return lives; }
int simLevel() { return levelNumber(); }
int simState() { return state; }
int simCapsules() { return capN; }
int simBricks(int kind) {
  int n = 0;
  for (int k = 0; k < wallN; k++) if (kind < 0 || wall[k].kind == kind) n++;
  return n;
}
int simLeft() { return LEFT; }
int simRight() { return RIGHT; }
int simTop() { return TOP; }
int simFloor() { return FLOOR; }
int simPaddleY() { return PADDLE_Y; }
int simBallR() { return BR; }
int simFieldY() { return FY; }
void simPowerOff() { started = false; bestLoaded = false; }
#endif

}  // namespace bricks
