// 2048. The rules are the same as in the browser version: a move slides every tile as far as it goes, two
// equal tiles that meet merge into their sum and the sum is scored, a tile merges only once in a move, a new
// 2 or now and then a 4 appears after every move, 2048 wins and you may go on, and the game ends when the
// board is full with no two neighbours equal. One move can be taken back.
#include "app.h"
#include <stdio.h>
#include <string.h>

namespace g2048 {

// Four tiles of fifty pixels and five gaps of four fill the width, which leaves the board square and the
// bottom of the screen for the status line and the three buttons.
static const int N = 4, CELLS = N * N, GOAL = 2048;
static const int CELL = 50, PAD = 4, BX = 10, BY = 40, SIDE = N * CELL + (N + 1) * PAD;
static const int STATUS_Y = 266, SWIPE_MIN = 16;
static const ui::Rect BOARD = {BX, BY, SIDE, SIDE};
static const ui::Rect B_MENU = {4, 290, 68, 26}, B_UNDO = {76, 290, 78, 26}, B_NEW = {158, 290, 78, 26};

// The heat map of the browser's dark theme, tile colour and the ink on it, from 2 up to 4096 and one more
// for everything past that. Nothing here has to be told apart by hue — every tile says its own number — so
// unlike the marble colours this scale is simply copied over and needs no settling on the board.
static const uint16_t FACE[13] = {
  RGB(0x33, 0x40, 0x4D), RGB(0x2D, 0x4D, 0x6B), RGB(0x2F, 0x65, 0x96), RGB(0x37, 0x84, 0xC4),
  RGB(0x3F, 0xA0, 0xE0), RGB(0x28, 0xB3, 0xA2), RGB(0x5B, 0xC8, 0x6C), RGB(0xC5, 0xD4, 0x46),
  RGB(0xF5, 0xC8, 0x3D), RGB(0xF5, 0x9A, 0x3A), RGB(0xF0, 0x5A, 0x42), RGB(0xD0, 0x37, 0x8F),
  RGB(0x8B, 0x4B, 0xD0)
};
static const uint16_t INK[13] = {
  RGB(0xDC, 0xE6, 0xEF), RGB(0xE1, 0xEE, 0xFA), RGB(0xFF, 0xFF, 0xFF), RGB(0xFF, 0xFF, 0xFF),
  RGB(0x06, 0x19, 0x2A), RGB(0x04, 0x20, 0x1C), RGB(0x06, 0x21, 0x0B), RGB(0x22, 0x2A, 0x05),
  RGB(0x2E, 0x22, 0x00), RGB(0x2A, 0x15, 0x00), RGB(0xFF, 0xFF, 0xFF), RGB(0xFF, 0xFF, 0xFF),
  RGB(0xFF, 0xFF, 0xFF)
};
static const uint16_t BOARD_BG = RGB(0x1D, 0x21, 0x26), EMPTY_C = RGB(42, 48, 56);

static int cells[CELLS], beforeCells[CELLS], backCells[CELLS];
static int score = 0, bestV = 0, backScore = 0;
static bool won = false, over = false, backWon = false, backRecord = false, hasBack = false;
static bool started = false, bestLoaded = false, overRecord = false, showMoves = true;
static bool swiping = false;
static int swipeX = 0, swipeY = 0;
static uint32_t confirmAt = 0;
static char statusText[80] = "";

static int cellX(int i) { return BX + PAD + (i % N) * (CELL + PAD); }
static int cellY(int i) { return BY + PAD + (i / N) * (CELL + PAD); }
// 2 is step 0, 4 is step 1 and so on; everything past 4096 shares the last colour
static int step(int v) {
  int e = 0;
  while (v > 2 && e < 12) { v >>= 1; e++; }
  return e;
}
static uint16_t mix(uint16_t a, uint16_t b, int percent) {   // percent of a, the rest of b
  int r = (((a >> 11) & 31) * percent + ((b >> 11) & 31) * (100 - percent)) / 100;
  int g = (((a >> 5) & 63) * percent + ((b >> 5) & 63) * (100 - percent)) / 100;
  int bl = ((a & 31) * percent + (b & 31) * (100 - percent)) / 100;
  return (uint16_t)((r << 11) | (g << 5) | bl);
}

/* ---------- drawing ---------- */

// One cell: the hollow underneath and the tile on it, if there is one. `shrink` draws the tile smaller and
// without its number, which is the frame a merged or a freshly arrived tile grows out of.
static void paintCell(int i, int shrink) {
  int x = cellX(i), y = cellY(i), v = cells[i];
  hw::fillRoundRect(x, y, CELL, CELL, 6, EMPTY_C);
  if (!v) return;
  int s = step(v);
  uint16_t face = FACE[s], ink = INK[s];
  hw::fillRoundRect(x + shrink, y + shrink, CELL - 2 * shrink, CELL - 2 * shrink, 6, face);
  if (shrink) return;
  char text[12];
  snprintf(text, sizeof text, "%d", v);
  ui::label(x, y + CELL / 2 - 9, CELL, 18, text, FONT_M, ink, face, ui::CENTER);
  // the browser writes the tile as a power of two in the corner; at this size the exponent alone carries it
  snprintf(text, sizeof text, "%d", s + 1);
  ui::label(x + 4, y + 3, 18, 12, text, FONT_D, mix(ink, face, 55), face, ui::LEFT);
}
static void paintBoard() {
  hw::fillRoundRect(BX, BY, SIDE, SIDE, 8, BOARD_BG);
  for (int i = 0; i < CELLS; i++) paintCell(i, 0);
}
static void paintScore() {
  char text[12];
  snprintf(text, sizeof text, "%d", score);
  ui::label(6, 15, 104, 19, text, FONT_M, C_INK, C_BG, ui::LEFT);
}
static void paintBest() {
  char text[12];
  snprintf(text, sizeof text, "%d", bestV > score ? bestV : score);
  ui::label(130, 15, 104, 19, text, FONT_M, C_INK, C_BG, ui::RIGHT);
}
static void paintStatus() {
  uint16_t color = confirmAt ? C_INK : (over ? (overRecord ? C_GOOD : C_DANGER) : (won ? C_GOOD : C_MUTED));
  ui::label(0, STATUS_Y, hw::W, 16, confirmAt ? T(S_L_CONFIRM) : statusText, FONT_S, color, C_BG);
}
static void say(StrId id) {
  snprintf(statusText, sizeof statusText, "%s", T(id));
  if (showMoves) paintStatus();
}
static void say1(StrId id, int a) {
  app::fillText(statusText, sizeof statusText, T(id), "", a, 0);
  if (showMoves) paintStatus();
}
static void paintBar() {
  bool hot = confirmAt != 0;
  ui::button(B_MENU.x, B_MENU.y, B_MENU.w, B_MENU.h, T(S_MENU), FONT_M, C_INK, C_PANEL);
  ui::button(B_UNDO.x, B_UNDO.y, B_UNDO.w, B_UNDO.h, T(S_G_UNDO), FONT_M,
             hasBack ? C_INK : C_LINE, C_PANEL);
  ui::button(B_NEW.x, B_NEW.y, B_NEW.w, B_NEW.h, T(S_M_NEW), FONT_M,
             hot || over ? C_ON_ACCENT : C_INK, hot ? C_DANGER : (over ? C_ACCENT : C_PANEL));
}
static void paintAll() {
  if (!showMoves) return;
  hw::fillRect(0, 0, hw::W, hw::H, C_BG);
  ui::label(6, 2, 104, 13, T(S_L_SCORE), FONT_S, C_MUTED, C_BG, ui::LEFT);
  ui::label(130, 2, 104, 13, T(S_BEST), FONT_S, C_MUTED, C_BG, ui::RIGHT);
  paintScore(); paintBest();
  paintBoard();
  paintStatus(); paintBar();
}

// The menu icon: four tiles off the low end of the heat map, with their numbers.
void drawIcon(int x, int y) {
  static const int V[4] = {2, 4, 8, 16};
  for (int k = 0; k < 4; k++) {
    int tx = x + (k % 2) * 18, ty = y + (k / 2) * 18, s = step(V[k]);
    char text[4];
    hw::fillRoundRect(tx, ty, 16, 16, 3, FACE[s]);
    snprintf(text, sizeof text, "%d", V[k]);
    ui::label(tx, ty + 3, 16, 11, text, FONT_D, INK[s], FACE[s], ui::CENTER);
  }
}

/* ---------- rules ---------- */

static void loadBest() {
  if (!bestLoaded) { bestV = hw::loadInt("gbest", 0); bestLoaded = true; }
}
int best() { loadBest(); return bestV; }
void resetBest() { bestV = 0; bestLoaded = true; hw::saveInt("gbest", 0); }

static void addScore(int n) {
  score += n;
  loadBest();
  if (score > bestV) { bestV = score; overRecord = true; hw::saveInt("gbest", bestV); }
}
static int spawn() {
  int free[CELLS], n = 0;
  for (int i = 0; i < CELLS; i++) if (!cells[i]) free[n++] = i;
  if (!n) return -1;
  int i = free[hw::rnd() % (uint32_t)n];
  cells[i] = (hw::rnd() % 10u) ? 2 : 4;      // a 2 nine times out of ten
  return i;
}
static bool canMove() {
  for (int i = 0; i < CELLS; i++) if (!cells[i]) return true;
  for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) {
    int v = cells[r * N + c];
    if (c < N - 1 && cells[r * N + c + 1] == v) return true;
    if (r < N - 1 && cells[(r + 1) * N + c] == v) return true;
  }
  return false;
}
// Slides everything one way into `next`. False when nothing moves. dir: 0 up, 1 right, 2 down, 3 left.
static bool slide(int dir, int* next, int& gained, bool* merged) {
  bool moved = false;
  gained = 0;
  for (int i = 0; i < CELLS; i++) { next[i] = 0; merged[i] = false; }
  for (int line = 0; line < N; line++) {
    int path[N];
    // the cells of this line in the order the tiles reach them, nearest the wall they move toward first
    for (int k = 0; k < N; k++) {
      int r, c;
      if (dir == 0) { r = k; c = line; }
      else if (dir == 2) { r = N - 1 - k; c = line; }
      else if (dir == 3) { r = line; c = k; }
      else { r = line; c = N - 1 - k; }
      path[k] = r * N + c;
    }
    int out[N], from[N], n = 0;
    bool fused[N];
    for (int k = 0; k < N; k++) {
      int v = cells[path[k]];
      if (!v) continue;
      if (n > 0 && out[n - 1] == v && !fused[n - 1]) {
        out[n - 1] = v * 2; gained += v * 2; fused[n - 1] = true; moved = true;
      } else { out[n] = v; from[n] = path[k]; fused[n] = false; n++; }
    }
    for (int k = 0; k < N; k++) next[path[k]] = k < n ? out[k] : 0;
    for (int k = 0; k < n; k++) {
      if (from[k] != path[k]) moved = true;
      if (fused[k]) merged[path[k]] = true;
    }
  }
  return moved;
}
static bool reached() {
  for (int i = 0; i < CELLS; i++) if (cells[i] >= GOAL) return true;
  return false;
}

// Redraws the cells that are not what they were, and lets the merged tiles and the new one grow into place.
static void paintMove(const bool* merged, int fresh) {
  if (!showMoves) return;
  for (int i = 0; i < CELLS; i++) {
    bool young = merged[i] || i == fresh;
    if (cells[i] == beforeCells[i] && !young) continue;
    paintCell(i, young ? 9 : 0);
  }
  app::wait(55);
  for (int i = 0; i < CELLS; i++) if (merged[i] || i == fresh) paintCell(i, 0);
}

static void reset() {
  for (int i = 0; i < CELLS; i++) cells[i] = 0;
  score = 0; won = false; over = false; overRecord = false; hasBack = false;
  confirmAt = 0; swiping = false; started = true;
  spawn(); spawn();
  snprintf(statusText, sizeof statusText, "%s", T(S_G_START));
}
static void move(int dir) {
  if (over) return;
  int next[CELLS], gained = 0;
  bool merged[CELLS];
  if (!slide(dir, next, gained, merged)) { say(S_G_STUCK); snd::play(140, 60); return; }
  memcpy(beforeCells, cells, sizeof cells);
  memcpy(backCells, cells, sizeof cells);
  backScore = score; backWon = won; backRecord = overRecord; hasBack = true;
  memcpy(cells, next, sizeof cells);
  addScore(gained);
  int fresh = spawn();

  if (gained) {
    int top = 0;
    for (int i = 0; i < CELLS; i++) if (merged[i] && cells[i] > top) top = cells[i];
    say1(S_G_MOVED, gained);
    snd::play(300 + 60 * step(top), 70);
  } else {
    say(S_G_START);
    snd::play(520, 25);
  }
  paintMove(merged, fresh);
  if (showMoves) { paintScore(); paintBest(); paintBar(); }

  if (!won && reached()) {
    won = true;
    say1(S_G_WIN, score);
    snd::play(523, 150); snd::note(659, 150); snd::note(784, 150); snd::note(1047, 260);
  } else if (!canMove()) {
    over = true;
    say1(overRecord ? S_G_OVER_BEST : S_G_OVER, score);
    snd::play(330, 170); snd::note(220, 170); snd::note(165, 320);
    if (showMoves) paintBar();
  }
}
static void undo() {
  if (!hasBack) { say(S_G_NOUNDO); snd::play(180, 60); return; }
  memcpy(cells, backCells, sizeof cells);
  score = backScore; won = backWon; overRecord = backRecord;
  over = false; hasBack = false;
  snd::play(400, 60);
  say(S_G_UNDONE);
  if (showMoves) { paintBoard(); paintScore(); paintBest(); paintStatus(); paintBar(); }
}

/* ---------- screen ---------- */

void enter() {
  if (!started) reset();
  confirmAt = 0;
  swiping = false;
  loadBest();
  // the texts may be in another language now
  if (over) app::fillText(statusText, sizeof statusText, T(overRecord ? S_G_OVER_BEST : S_G_OVER), "", score, 0);
  else snprintf(statusText, sizeof statusText, "%s", T(S_G_START));
  paintAll();
}

void update() {
  using app::touch;
  if (confirmAt && hw::ms() - confirmAt > 3500) { confirmAt = 0; paintStatus(); paintBar(); }
  if (touch.pressed) {
    int x = touch.x, y = touch.y;
    if (B_NEW.has(x, y)) {
      if (score > 0 && !over && !confirmAt) { confirmAt = hw::ms(); paintStatus(); paintBar(); snd::play(660, 40); }
      else { confirmAt = 0; reset(); snd::play(880, 25); paintAll(); }
      return;
    }
    if (confirmAt) { confirmAt = 0; paintStatus(); paintBar(); }
    if (B_MENU.has(x, y)) { snd::play(880, 25); app::go(app::SCR_MENU); return; }
    if (B_UNDO.has(x, y)) { undo(); return; }
    if (BOARD.has(x, y)) { swiping = true; swipeX = touch.liveX; swipeY = touch.liveY; }
    return;
  }
  // A swipe is a press that travelled; the direction is whichever of the two it travelled further in.
  // Anything shorter is a tap on the board, which 2048 has nothing to do with.
  if (swiping && !touch.down) {
    swiping = false;
    int dx = touch.liveX - swipeX, dy = touch.liveY - swipeY;
    int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    if (ax < SWIPE_MIN && ay < SWIPE_MIN) return;
    move(ax > ay ? (dx > 0 ? 1 : 3) : (dy > 0 ? 2 : 0));
  }
}

#ifdef CYD_SIM   // views and shortcuts for the tests in sim/
void simReset() { reset(); }
void simAnimate(bool on) { showMoves = on; }
int* simCells() { return cells; }
void simMove(int dir) { move(dir); }
void simUndo() { undo(); }
bool simSlides(int dir) {
  int next[CELLS], gained = 0;
  bool merged[CELLS];
  return slide(dir, next, gained, merged);
}
bool simSlide(int dir, int* out, int& gained) {    // the slide on its own, with no new tile after it
  bool merged[CELLS];
  return slide(dir, out, gained, merged);
}
int simScore() { return score; }
bool simOver() { return over; }
bool simWon() { return won; }
bool simCanUndo() { return hasBack; }
int simCellX(int i) { return cellX(i) + CELL / 2; }
int simCellY(int i) { return cellY(i) + CELL / 2; }
void simPowerOff() { started = false; bestLoaded = false; }
#endif

}  // namespace g2048
