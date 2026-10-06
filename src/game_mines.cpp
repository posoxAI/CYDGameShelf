// Minesweeper. The rules are the same as in the browser version: the first opened cell and its neighbours
// are never mined, a tap on an open number opens its neighbours once that many flags surround it.
#include "app.h"
#include <stdio.h>
#include <string.h>

namespace mines {

struct Size { int cols, rows, mines, cell; const char* name; };
static const Size SIZES[2] = {{9, 9, 10, 26, "9×9"}, {16, 16, 40, 14, "16×16"}};

enum { READY, PLAY, WON, LOST };
// what a cell looks like on screen; a cell is redrawn only when this changes
enum { V_CLOSED = 1, V_FLAG = 2, V_OPEN = 3 /* + number 0..8 */, V_MINE = 12, V_BOOM = 13, V_WRONG = 14 };

static const int BOARD_TOP = 38, BOARD_SIDE = 234, STATUS_Y = 274;
static const ui::Rect B_NEW = {84, 4, 72, 28}, B_MENU = {4, 292, 74, 26}, B_SIZE = {83, 292, 74, 26}, B_MODE = {162, 292, 74, 26};

static const uint16_t TILE = RGB(58, 74, 92), TILE_HI = RGB(88, 106, 127), TILE_LO = RGB(36, 48, 61);
static const uint16_t OPEN_BG = RGB(24, 33, 43), GRID = RGB(44, 58, 72), FLAG_RED = RGB(255, 90, 77), BOOM_BG = RGB(190, 40, 34);
static const uint16_t MINE_INK = RGB(205, 214, 222);
static const uint16_t NUM[9] = {0, RGB(127, 176, 255), RGB(116, 208, 138), RGB(255, 138, 128), RGB(179, 173, 255),
                                RGB(232, 160, 122), RGB(99, 214, 214), RGB(230, 236, 238), RGB(154, 166, 172)};

static uint8_t mine[256], adj[256], opn[256], flg[256], shown[256];
static int sizeIdx = 0, cols = 9, rows = 9, mineCount = 10, cell = 26, x0 = 3, y0 = BOARD_TOP;
static int status = READY, opened = 0, flags = 0, boomAt = -1;
static bool flagMode = false, started = false, pressOnBoard = false, running = false;
static uint32_t tickStart = 0, baseMs = 0, confirmAt = 0;
static int shownSec = -1;
static int best[2] = {0, 0};
static bool bestLoaded = false;
static char statusText[72] = "";

/* ---------- rules ---------- */

static int neighbours(int i, int* out) {
  int n = 0, x = i % cols, y = i / cols;
  for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
    if (!dx && !dy) continue;
    int nx = x + dx, ny = y + dy;
    if (nx >= 0 && ny >= 0 && nx < cols && ny < rows) out[n++] = ny * cols + nx;
  }
  return n;
}
static uint32_t elapsed() { return baseMs + (running ? hw::ms() - tickStart : 0); }

// Mines go down after the first tap, away from that cell and its neighbours.
static void placeMines(int first) {
  static int16_t pool[256];
  static uint8_t keep[256];
  int total = cols * rows, nb[8], n = 0;
  memset(keep, 0, sizeof keep);
  keep[first] = 1;
  int k = neighbours(first, nb);
  for (int j = 0; j < k; j++) keep[nb[j]] = 1;
  for (int i = 0; i < total; i++) if (!keep[i]) pool[n++] = (int16_t)i;
  for (int i = 0; i < mineCount; i++) {
    int pick = i + (int)(hw::rnd() % (uint32_t)(n - i));
    int16_t t = pool[i]; pool[i] = pool[pick]; pool[pick] = t;
    mine[pool[i]] = 1;
  }
  for (int i = 0; i < total; i++) {
    int c = 0;
    k = neighbours(i, nb);
    for (int j = 0; j < k; j++) c += mine[nb[j]];
    adj[i] = (uint8_t)c;
  }
}
static void openFrom(int i) {
  static int16_t stack[2100];
  int top = 0, nb[8];
  stack[top++] = (int16_t)i;
  while (top) {
    int c = stack[--top];
    if (opn[c] || flg[c]) continue;
    opn[c] = 1; opened++;
    if (!adj[c]) {
      int k = neighbours(c, nb);
      for (int j = 0; j < k; j++) if (!opn[nb[j]] && !flg[nb[j]] && top < 2100) stack[top++] = (int16_t)nb[j];
    }
  }
}

void formatTime(int tenths, char* out, int outSize) {
  snprintf(out, outSize, "%d:%02d.%d", tenths / 600, (tenths / 10) % 60, tenths % 10);
}
static void loadBest() {
  if (bestLoaded) return;
  best[0] = hw::loadInt("mbest0", 0);
  best[1] = hw::loadInt("mbest1", 0);
  bestLoaded = true;
}
int bestTenths(int size) { loadBest(); return best[size ? 1 : 0]; }
void resetBest() { best[0] = best[1] = 0; bestLoaded = true; hw::saveInt("mbest0", 0); hw::saveInt("mbest1", 0); }

/* ---------- drawing ---------- */

static void drawFlag(int x, int y, int c) {
  int t = c >= 20 ? 2 : 1, px = x + c * 9 / 16, top = y + c * 3 / 16, bottom = y + c * 12 / 16;
  hw::fillRect(px, top, t, bottom - top, C_INK);
  hw::fillTriangle(px + t - 1, top, px - c * 3 / 8, top + c * 5 / 32 + 1, px + t - 1, top + c * 5 / 16 + 1, FLAG_RED);
  hw::fillRect(x + c * 5 / 16, bottom, c / 2, t, C_INK);
}
static void drawMine(int x, int y, int c, uint16_t color) {
  int cx = x + c / 2, cy = y + c / 2, r = c * 9 / 32, t = c >= 20 ? 2 : 1;
  hw::fillRect(cx - r - 2, cy - t / 2, 2 * r + 5, t, color);
  hw::fillRect(cx - t / 2, cy - r - 2, t, 2 * r + 5, color);
  hw::drawLine(cx - r, cy - r, cx + r, cy + r, color);
  hw::drawLine(cx - r, cy + r, cx + r, cy - r, color);
  hw::fillCircle(cx, cy, r, color);
  if (c >= 20) hw::fillRect(cx - r / 2 - 1, cy - r / 2 - 1, 2, 2, C_INK);
}
static void drawTile(int x, int y) {
  int e = cell >= 20 ? 2 : 1;
  hw::fillRect(x, y, cell, cell, TILE);
  hw::fillRect(x, y, cell, e, TILE_HI);
  hw::fillRect(x, y, e, cell, TILE_HI);
  hw::fillRect(x, y + cell - e, cell, e, TILE_LO);
  hw::fillRect(x + cell - e, y, e, cell, TILE_LO);
}
static void drawOpen(int x, int y, uint16_t bg) {
  hw::fillRect(x, y, cell, cell, bg);
  hw::fillRect(x + cell - 1, y, 1, cell, GRID);
  hw::fillRect(x, y + cell - 1, cell, 1, GRID);
}
static void drawCell(int i, int v) {
  int x = x0 + (i % cols) * cell, y = y0 + (i / cols) * cell;
  if (v == V_CLOSED) drawTile(x, y);
  else if (v == V_FLAG) { drawTile(x, y); drawFlag(x, y, cell); }
  else if (v == V_WRONG) {
    drawTile(x, y); drawFlag(x, y, cell);
    hw::drawLine(x + 3, y + 3, x + cell - 4, y + cell - 4, C_DANGER);
    hw::drawLine(x + 3, y + cell - 4, x + cell - 4, y + 3, C_DANGER);
    if (cell >= 20) {
      hw::drawLine(x + 4, y + 3, x + cell - 3, y + cell - 4, C_DANGER);
      hw::drawLine(x + 4, y + cell - 4, x + cell - 3, y + 3, C_DANGER);
    }
  }
  else if (v == V_MINE) { drawOpen(x, y, OPEN_BG); drawMine(x, y, cell, MINE_INK); }
  else if (v == V_BOOM) { drawOpen(x, y, BOOM_BG); drawMine(x, y, cell, RGB(12, 16, 20)); }
  else {
    int n = v - V_OPEN;
    drawOpen(x, y, OPEN_BG);
    if (n > 0) {
      char digit[2] = {(char)('0' + n), 0};
      ui::label(x + 1, y + 1, cell - 3, cell - 3, digit, cell >= 20 ? FONT_M : FONT_D, NUM[n], OPEN_BG);
    }
  }
}
static int visualOf(int i) {
  if (opn[i]) return V_OPEN + adj[i];
  if (status == LOST) {
    if (i == boomAt) return V_BOOM;
    if (mine[i] && !flg[i]) return V_MINE;
    if (flg[i] && !mine[i]) return V_WRONG;
  }
  return flg[i] ? V_FLAG : V_CLOSED;
}
static void paintBoard(bool all) {
  for (int i = 0; i < cols * rows; i++) {
    int v = visualOf(i);
    if (all || shown[i] != v) { drawCell(i, v); shown[i] = (uint8_t)v; }
  }
}
static void paintLeft() {
  char text[8];
  snprintf(text, sizeof text, "%d", mineCount - flags);
  ui::label(30, 4, 50, 28, text, FONT_M, C_INK, C_BG, ui::LEFT);
}
static void paintTime(bool force) {
  int sec = (int)(elapsed() / 1000);
  if (sec > 5999) sec = 5999;
  if (!force && sec == shownSec) return;
  shownSec = sec;
  char text[12];
  snprintf(text, sizeof text, "%d:%02d", sec / 60, sec % 60);
  ui::label(160, 4, 74, 28, text, FONT_M, C_INK, C_BG, ui::RIGHT);
}
static void paintStatus() {
  uint16_t color = confirmAt ? C_INK : (status == WON ? C_GOOD : (status == LOST ? C_DANGER : C_MUTED));
  ui::label(0, STATUS_Y, hw::W, 16, confirmAt ? T(S_M_CONFIRM) : statusText, FONT_S, color, C_BG);
}
static void say(StrId id) { snprintf(statusText, sizeof statusText, "%s", T(id)); paintStatus(); }
static void sayPlaying() { say(status == READY ? S_M_READY : (flagMode ? S_M_PLAY_FLAG : S_M_PLAY)); }
static void paintBar() {
  ui::button(B_MENU.x, B_MENU.y, B_MENU.w, B_MENU.h, T(S_MENU), FONT_M, C_INK, C_PANEL);
  ui::button(B_SIZE.x, B_SIZE.y, B_SIZE.w, B_SIZE.h, SIZES[sizeIdx].name, FONT_M,
             confirmAt ? C_ON_ACCENT : C_INK, confirmAt ? C_DANGER : C_PANEL);
  ui::button(B_MODE.x, B_MODE.y, B_MODE.w, B_MODE.h, T(flagMode ? S_M_FLAG : S_M_DIG), FONT_M,
             flagMode ? C_ON_ACCENT : C_INK, flagMode ? C_ACCENT : C_PANEL);
}
static void paintAll() {
  hw::fillRect(0, 0, hw::W, hw::H, C_BG);
  drawFlag(6, 8, 20);
  ui::button(B_NEW.x, B_NEW.y, B_NEW.w, B_NEW.h, T(S_M_NEW), FONT_M, C_INK, C_PANEL);
  paintLeft(); paintTime(true);
  paintBoard(true);
  paintStatus(); paintBar();
}

/* ---------- moves ---------- */

static void newGame(int idx) {
  sizeIdx = idx ? 1 : 0;
  const Size& s = SIZES[sizeIdx];
  cols = s.cols; rows = s.rows; mineCount = s.mines; cell = s.cell;
  x0 = (hw::W - cols * cell) / 2;
  y0 = BOARD_TOP + (BOARD_SIDE - rows * cell) / 2;
  memset(mine, 0, sizeof mine); memset(adj, 0, sizeof adj); memset(opn, 0, sizeof opn); memset(flg, 0, sizeof flg);
  status = READY; opened = 0; flags = 0; boomAt = -1; baseMs = 0; running = false; confirmAt = 0; started = true;
  snprintf(statusText, sizeof statusText, "%s", T(S_M_READY));
}
static void lose(int i) {
  baseMs = elapsed(); running = false;
  status = LOST; boomAt = i;
  say(S_M_LOST);
  snd::play(220, 140); snd::note(160, 160); snd::note(110, 260);
}
static void win() {
  baseMs = elapsed(); running = false;
  status = WON;
  for (int i = 0; i < cols * rows; i++) if (mine[i] && !flg[i]) { flg[i] = 1; flags++; }
  int tenths = (int)((baseMs + 50) / 100);
  if (tenths < 1) tenths = 1;
  loadBest();
  bool record = !best[sizeIdx] || tenths < best[sizeIdx];
  if (record) { best[sizeIdx] = tenths; hw::saveInt(sizeIdx ? "mbest1" : "mbest0", tenths); }
  char t[16];
  formatTime(tenths, t, sizeof t);
  app::fillText(statusText, sizeof statusText, T(record ? S_M_WON_BEST : S_M_WON), t, 0, 0);
  paintStatus();
  snd::play(523, 110); snd::note(659, 110); snd::note(784, 110); snd::note(1047, 220);
}
static void afterMove() {
  if (opened == cols * rows - mineCount) win();
}
static void reveal(int i) {
  if (status == WON || status == LOST || opn[i] || flg[i]) return;
  if (status == READY) {
    placeMines(i);
    status = PLAY; baseMs = 0; tickStart = hw::ms(); running = true;
    sayPlaying();
  }
  if (mine[i]) { lose(i); return; }
  openFrom(i);
  snd::play(420, 30);
  afterMove();
}
static void chord(int i) {
  if (status != PLAY || !opn[i] || !adj[i]) return;
  int nb[8], todo[8], marked = 0, n = 0, k = neighbours(i, nb);
  for (int j = 0; j < k; j++) { if (flg[nb[j]]) marked++; else if (!opn[nb[j]]) todo[n++] = nb[j]; }
  if (marked != adj[i] || !n) return;
  for (int j = 0; j < n; j++) if (mine[todo[j]]) { lose(todo[j]); return; }
  for (int j = 0; j < n; j++) openFrom(todo[j]);
  snd::play(420, 30);
  afterMove();
}
static void toggleFlag(int i) {
  if (status == WON || status == LOST || opn[i]) return;
  flg[i] = flg[i] ? 0 : 1;
  flags += flg[i] ? 1 : -1;
  snd::play(flg[i] ? 660 : 500, 40);
}
// what a tap does, and what a long press does
static void primary(int i) {
  if (opn[i]) chord(i);
  else if (flagMode) toggleFlag(i);
  else reveal(i);
}
static void secondary(int i) {
  if (opn[i]) return;
  if (flagMode) reveal(i); else toggleFlag(i);
}
static int cellAt(int x, int y) {
  if (x < x0 || y < y0) return -1;
  int cx = (x - x0) / cell, cy = (y - y0) / cell;
  return (cx < cols && cy < rows) ? cy * cols + cx : -1;
}
static void refresh() { paintBoard(false); paintLeft(); paintTime(true); }

/* ---------- screen ---------- */

void enter() {
  if (!started) newGame(hw::loadInt("msize", 0));
  if (status == PLAY && !running) { tickStart = hw::ms(); running = true; }
  confirmAt = 0;
  // the texts may be in another language now
  if (status == READY || status == PLAY) snprintf(statusText, sizeof statusText, "%s", T(status == READY ? S_M_READY : (flagMode ? S_M_PLAY_FLAG : S_M_PLAY)));
  else if (status == LOST) snprintf(statusText, sizeof statusText, "%s", T(S_M_LOST));
  pressOnBoard = false;
  paintAll();
}
void leave() {
  if (running) { baseMs = elapsed(); running = false; }   // the clock stops while the game is off screen
}
void update() {
  using app::touch;
  if (confirmAt && hw::ms() - confirmAt > 3500) { confirmAt = 0; paintStatus(); paintBar(); }
  if (touch.pressed) {
    int x = touch.x, y = touch.y;
    pressOnBoard = false;
    if (B_SIZE.has(x, y)) {
      if (status == PLAY && !confirmAt) { confirmAt = hw::ms(); paintStatus(); paintBar(); snd::play(660, 40); }
      else { newGame(1 - sizeIdx); hw::saveInt("msize", sizeIdx); snd::play(880, 25); paintAll(); }
      return;
    }
    if (confirmAt) { confirmAt = 0; paintStatus(); paintBar(); }
    if (B_MENU.has(x, y)) { snd::play(880, 25); app::go(app::SCR_MENU); return; }
    if (B_NEW.has(x, y)) { newGame(sizeIdx); snd::play(880, 25); paintAll(); return; }
    if (B_MODE.has(x, y)) {
      flagMode = !flagMode; snd::play(880, 25); paintBar();
      if (status == READY || status == PLAY) sayPlaying();
      return;
    }
    pressOnBoard = cellAt(x, y) >= 0;
  }
  if (pressOnBoard && touch.longPress) {
    int i = cellAt(touch.x, touch.y);
    if (i >= 0) { secondary(i); refresh(); }
  }
  if (pressOnBoard && touch.released) {
    pressOnBoard = false;
    int i = cellAt(touch.x, touch.y);
    if (i >= 0 && !touch.wasLong) { primary(i); refresh(); }
  }
  if (status == PLAY) paintTime(false);
}

#ifdef CYD_SIM   // views and shortcuts for the tests in sim/
const uint8_t* simMine() { return mine; }
const uint8_t* simAdj() { return adj; }
const uint8_t* simOpen() { return opn; }
const uint8_t* simFlag() { return flg; }
int simCols() { return cols; }
int simRows() { return rows; }
int simMines() { return mineCount; }
int simStatus() { return status; }
int simFlags() { return flags; }
int simCellX(int i) { return x0 + (i % cols) * cell + cell / 2; }
int simCellY(int i) { return y0 + (i / cols) * cell + cell / 2; }
void simNew(int idx) { newGame(idx); }
void simPrimary(int i) { primary(i); }
void simSecondary(int i) { secondary(i); }
void simPowerOff() { started = false; flagMode = false; bestLoaded = false; running = false; }
#endif

}  // namespace mines
