// Five in a Line. The rules are the same as in the browser version: move a marble along a free path,
// five or more of one colour in a row disappear, a move that clears a line brings no new marbles.
#include "app.h"
#include <stdio.h>
#include <string.h>

namespace lines {

static const int N = 9, CELLS = 81, COLORS = 7, START = 5, PER_TURN = 3;
static const int CELL = 26, X0 = 3, Y0 = 38, STATUS_Y = 274, R = 10;
static const ui::Rect B_MENU = {4, 292, 74, 26}, B_NEW = {83, 292, 153, 26};

static const uint16_t CELL_BG = RGB(24, 33, 43), GRID = RGB(44, 58, 72), SEL_BG = RGB(47, 66, 86);
// Three sets of marble colours. How a colour really looks depends on the panel, and these panels differ from
// one board to the next, so the set is picked on the device itself: Settings -> Marble colours. No picture of
// a computer screen can settle this.
//   0  Bright: seven hues with a white marble. Only two of them are cool, because a violet would sit between
//      the teal and the blue and all three would blur. Brightness separates them as well as hue: white is the
//      brightest, the teal sits in the middle, the blue is deliberately dark. The yellow is pulled towards
//      amber, since too much green in it reads as olive.
//   1  No white: the white marble becomes orange, so no marble competes with the light colours at all.
//   2  Okabe-Ito: the set drawn up for colour-blind readers, where every pair differs by more than its hue.
static const int PALETTES = 3;
static const uint8_t BALL[PALETTES][COLORS + 1][3] = {
  {{0, 0, 0}, {235, 50, 45}, {255, 165, 0}, {50, 195, 65}, {0, 175, 190}, {45, 85, 230}, {255, 95, 180}, {238, 242, 246}},
  {{0, 0, 0}, {230, 45, 40}, {255, 120, 10}, {250, 210, 50}, {55, 200, 70}, {0, 180, 200}, {60, 100, 245}, {255, 100, 190}},
  {{0, 0, 0}, {213, 94, 0}, {230, 159, 0}, {240, 228, 66}, {0, 158, 115}, {86, 180, 233}, {0, 114, 178}, {204, 121, 167}},
};
// After the ready-made sets comes one the player puts together on the Your own set screen. It starts as a copy
// of the first set and lives in flash, one colour per key.
static uint8_t custom[COLORS + 1][3];

static int comp(int pal, int c, int k) { return pal < PALETTES ? BALL[pal][c][k] : custom[c][k]; }

int paletteCount() { return PALETTES + 1; }
int presetCount() { return PALETTES; }
void customGet(int c, int& r, int& g, int& b) { r = custom[c][0]; g = custom[c][1]; b = custom[c][2]; }

static void saveCustom(int c) {
  char key[8];
  snprintf(key, sizeof key, "cc%d", c);
  hw::saveInt(key, (int32_t)(((int32_t)custom[c][0] << 16) | ((int32_t)custom[c][1] << 8) | custom[c][2]));
}
void customSet(int c, int r, int g, int b) {
  custom[c][0] = (uint8_t)r; custom[c][1] = (uint8_t)g; custom[c][2] = (uint8_t)b;
  saveCustom(c);
}
void customFromPreset(int pal) {
  for (int c = 1; c <= COLORS; c++) {
    for (int k = 0; k < 3; k++) custom[c][k] = BALL[pal][c][k];
    saveCustom(c);
  }
}
void loadCustom() {
  char key[8];
  for (int c = 1; c <= COLORS; c++) {
    snprintf(key, sizeof key, "cc%d", c);
    int32_t v = hw::loadInt(key, -1);
    for (int k = 0; k < 3; k++) custom[c][k] = v < 0 ? BALL[0][c][k] : (uint8_t)(v >> (16 - 8 * k));
  }
}
// The marble ink: dark on the light colours, light on the dark ones.
static const uint16_t MARK_DARK = RGB(10, 14, 18), MARK_LIGHT = RGB(242, 246, 250);

static uint8_t board[CELLS];
static uint8_t nextB[START];
static int nextN = 0, score = 0, bestV = 0, sel = -1;
static bool over = false, started = false, bestLoaded = false, overRecord = false;
static uint32_t confirmAt = 0;
static char statusText[72] = "";

/* ---------- drawing ---------- */

static uint16_t shade(int pal, int c, int percent) {   // 100 is the colour itself, less is darker, more is lighter
  int v[3];
  for (int k = 0; k < 3; k++) {
    int base = comp(pal, c, k);
    v[k] = percent <= 100 ? base * percent / 100 : base + (255 - base) * (percent - 100) / 100;
  }
  return RGB(v[0], v[1], v[2]);
}
uint16_t ballColor(int c) { return shade(app::palette, c, 100); }
// Bricks asks for the same colours a little darker and a little lighter, for the face and the edges of a brick.
uint16_t ballShade(int c, int percent) { return shade(app::palette, c, percent); }

// Besides its colour every marble carries its own little sign, so the colours can still be told apart on a
// panel that renders them poorly. Settings -> Marks turns the signs off. The marbles of the growing and
// shrinking animations are too small for a sign and go without.
static void drawMark(int cx, int cy, int r, int c, int pal) {
  if (!app::marks || r < 5) return;
  int luma = (comp(pal, c, 0) * 299 + comp(pal, c, 1) * 587 + comp(pal, c, 2) * 114) / 1000;
  uint16_t ink = luma > 150 ? MARK_DARK : MARK_LIGHT;
  int s = r * 2 / 3, t = r >= 8 ? 3 : 2, d = s - 1;   // half the width of the sign, and the stroke width
  switch (c) {
    case 1: hw::fillCircle(cx, cy, s / 2 + 1, ink); break;                                          // a dot
    case 2: hw::fillCircle(cx, cy, s, ink); hw::fillCircle(cx, cy, s - t, shade(pal, c, 100)); break;  // a ring
    case 3: hw::fillRect(cx - s, cy - t / 2, 2 * s + 1, t, ink); break;                              // a bar
    case 4: hw::fillRect(cx - s, cy - t / 2, 2 * s + 1, t, ink);                                     // a cross
            hw::fillRect(cx - t / 2, cy - s, t, 2 * s + 1, ink); break;
    case 5: hw::fillTriangle(cx, cy - s, cx - s, cy + s * 2 / 3, cx + s, cy + s * 2 / 3, ink); break;  // a triangle
    case 6: hw::fillRect(cx - s + 2, cy - s + 2, 2 * s - 3, 2 * s - 3, ink); break;                  // a square
    default: for (int k = 0; k < t; k++) hw::drawLine(cx - d + k, cy + d, cx + d + k - t + 1, cy - d, ink); break;  // a slash
  }
}
static void drawBall(int cx, int cy, int r, int c, int pal) {
  if (r < 1) return;
  hw::fillCircle(cx, cy, r, shade(pal, c, 72));   // the rim is only a little darker: too dark and the
                                                  // deeper colours sink into the background
  if (r > 2) hw::fillCircle(cx, cy, r - 1, shade(pal, c, 100));
  if (r > 4) hw::fillCircle(cx - r * 3 / 8, cy - r * 3 / 8, r / 4, shade(pal, c, 165));
  drawMark(cx, cy, r, c, pal);
}
void drawSample(int cx, int cy, int r, int c, int pal) { drawBall(cx, cy, r, c, pal); }
static int cellX(int i) { return X0 + (i % N) * CELL; }
static int cellY(int i) { return Y0 + (i / N) * CELL; }
// one cell: its background, the frame if it is the picked one, and a marble of the given colour and size
static void drawCell(int i, int color, int r) {
  int x = cellX(i), y = cellY(i);
  bool picked = i == sel;
  hw::fillRect(x, y, CELL, CELL, picked ? SEL_BG : CELL_BG);
  hw::fillRect(x + CELL - 1, y, 1, CELL, GRID);
  hw::fillRect(x, y + CELL - 1, CELL, 1, GRID);
  if (picked) {
    hw::fillRect(x, y, CELL - 1, 2, C_ACCENT); hw::fillRect(x, y + CELL - 3, CELL - 1, 2, C_ACCENT);
    hw::fillRect(x, y, 2, CELL - 1, C_ACCENT); hw::fillRect(x + CELL - 3, y, 2, CELL - 1, C_ACCENT);
  }
  if (color) drawBall(x + CELL / 2 - 1, y + CELL / 2 - 1, r, color, app::palette);
}
static void paintCell(int i) { drawCell(i, board[i], R); }
static void paintScore() {
  char text[12];
  snprintf(text, sizeof text, "%d", score);
  ui::label(6, 16, 76, 20, text, FONT_M, C_INK, C_BG, ui::LEFT);
}
static void paintBest() {
  char text[12];
  snprintf(text, sizeof text, "%d", bestV > score ? bestV : score);
  ui::label(158, 16, 76, 20, text, FONT_M, C_INK, C_BG, ui::RIGHT);
}
static void paintNext() {
  hw::fillRect(90, 17, 60, 19, C_BG);
  for (int k = 0; k < nextN && k < PER_TURN; k++) drawBall(104 + k * 16, 26, 6, nextB[k], app::palette);
}
static void paintStatus() {
  uint16_t color = confirmAt ? C_INK : (over ? (overRecord ? C_GOOD : C_DANGER) : C_MUTED);
  ui::label(0, STATUS_Y, hw::W, 16, confirmAt ? T(S_L_CONFIRM) : statusText, FONT_S, color, C_BG);
}
static void say(StrId id) { snprintf(statusText, sizeof statusText, "%s", T(id)); paintStatus(); }
static void say2(StrId id, int a, int b) { app::fillText(statusText, sizeof statusText, T(id), "", a, b); paintStatus(); }
static void paintBar() {
  ui::button(B_MENU.x, B_MENU.y, B_MENU.w, B_MENU.h, T(S_MENU), FONT_M, C_INK, C_PANEL);
  bool hot = confirmAt != 0;
  ui::button(B_NEW.x, B_NEW.y, B_NEW.w, B_NEW.h, T(S_L_NEW), FONT_M,
             hot ? C_ON_ACCENT : (over ? C_ON_ACCENT : C_INK), hot ? C_DANGER : (over ? C_ACCENT : C_PANEL));
}
static void paintAll() {
  hw::fillRect(0, 0, hw::W, hw::H, C_BG);
  ui::label(6, 2, 76, 14, T(S_L_SCORE), FONT_S, C_MUTED, C_BG, ui::LEFT);
  ui::label(84, 2, 72, 14, T(S_L_NEXT), FONT_S, C_MUTED, C_BG, ui::CENTER);
  ui::label(158, 2, 76, 14, T(S_BEST), FONT_S, C_MUTED, C_BG, ui::RIGHT);
  paintScore(); paintNext(); paintBest();
  for (int i = 0; i < CELLS; i++) paintCell(i);
  paintStatus(); paintBar();
}

/* ---------- rules ---------- */

static int rc() { return 1 + (int)(hw::rnd() % COLORS); }
static int empties(uint8_t* list) {
  int n = 0;
  for (int i = 0; i < CELLS; i++) if (!board[i]) list[n++] = (uint8_t)i;
  return n;
}
static int points(int n) {
  static const int POINTS[5] = {10, 12, 18, 28, 42};
  return n < 5 ? 0 : (n - 5 < 5 ? POINTS[n - 5] : 42 + (n - 9) * 16);
}
// marks every cell of every line of five or more that passes through cell i
static void linesAt(int i, uint8_t* out) {
  static const int DIRS[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
  int c = board[i];
  if (!c) return;
  int x = i % N, y = i / N, run[2 * N];
  for (int d = 0; d < 4; d++) {
    int dx = DIRS[d][0], dy = DIRS[d][1], len = 0;
    run[len++] = i;
    for (int s = -1; s <= 1; s += 2) {
      int cx = x + dx * s, cy = y + dy * s;
      while (cx >= 0 && cx < N && cy >= 0 && cy < N && board[cy * N + cx] == c) { run[len++] = cy * N + cx; cx += dx * s; cy += dy * s; }
    }
    if (len >= 5) for (int k = 0; k < len; k++) out[run[k]] = 1;
  }
}
static int countMarked(const uint8_t* out) {
  int n = 0;
  for (int i = 0; i < CELLS; i++) n += out[i];
  return n;
}
// removes the marked marbles, shrinking them first; returns the points
static int clearCells(const uint8_t* out) {
  static const int SHRINK[3] = {7, 4, 1};
  int n = countMarked(out);
  for (int step = 0; step < 3; step++) {
    for (int i = 0; i < CELLS; i++) if (out[i]) drawCell(i, board[i], SHRINK[step]);
    app::wait(45);
  }
  for (int i = 0; i < CELLS; i++) if (out[i]) { board[i] = 0; paintCell(i); }
  int p = points(n);
  score += p;
  snd::play(660, 70);
  for (int k = 1; k < n - 3 && k < 5; k++) snd::note(660 + k * 110, 70);
  return p;
}
// shortest way through free cells, moving up, down, left and right; returns its length, 0 if there is none
static int findPath(int src, int dst, uint8_t* path) {
  int8_t prev[CELLS];
  uint8_t queue[CELLS];
  int head = 0, tail = 0;
  memset(prev, -1, sizeof prev);
  prev[src] = (int8_t)src;
  queue[tail++] = (uint8_t)src;
  while (head < tail) {
    int i = queue[head++];
    if (i == dst) break;
    int x = i % N, y = i / N, nb[4], k = 0;
    if (x > 0) nb[k++] = i - 1;
    if (x < N - 1) nb[k++] = i + 1;
    if (y > 0) nb[k++] = i - N;
    if (y < N - 1) nb[k++] = i + N;
    for (int j = 0; j < k; j++) if (prev[nb[j]] == -1 && !board[nb[j]]) { prev[nb[j]] = (int8_t)i; queue[tail++] = (uint8_t)nb[j]; }
  }
  if (prev[dst] == -1) return 0;
  int len = 0;
  for (int i = dst; ; i = prev[i]) { len++; if (i == src) break; }
  int at = dst;
  for (int k = len - 1; k >= 0; k--) { path[k] = (uint8_t)at; at = prev[at]; }
  return len;
}
static void loadBest() {
  if (!bestLoaded) { bestV = hw::loadInt("lbest", 0); bestLoaded = true; }
}
int best() { loadBest(); return bestV; }
void resetBest() { bestV = 0; bestLoaded = true; hw::saveInt("lbest", 0); }

static void finish() {
  over = true; sel = -1;
  loadBest();
  overRecord = score > bestV && score > 0;
  if (score > bestV) { bestV = score; hw::saveInt("lbest", bestV); }
  app::fillText(statusText, sizeof statusText, T(overRecord ? S_L_OVER_BEST : S_L_OVER), "", score, 0);
  snd::play(330, 150); snd::note(220, 150); snd::note(110, 300);
}
// the waiting marbles land on random free cells and three more are drawn; returns the points if they made a line
static int computerTurn(bool show, int* lineLen) {
  static const int GROW[3] = {3, 6, R};
  uint8_t placed[START], free_[CELLS], out[CELLS];
  int count = 0;
  for (int k = 0; k < nextN; k++) {
    int e = empties(free_);
    if (!e) break;
    int i = free_[hw::rnd() % (uint32_t)e];
    board[i] = nextB[k];
    placed[count++] = (uint8_t)i;
  }
  if (show) for (int step = 0; step < 3; step++) {
    for (int k = 0; k < count; k++) drawCell(placed[k], board[placed[k]], GROW[step]);
    app::wait(40);
  }
  nextN = PER_TURN;
  for (int k = 0; k < PER_TURN; k++) nextB[k] = (uint8_t)rc();
  memset(out, 0, sizeof out);
  for (int k = 0; k < count; k++) linesAt(placed[k], out);
  int n = countMarked(out), gained = 0;
  if (n) { if (show) gained = clearCells(out); else { for (int i = 0; i < CELLS; i++) if (out[i]) board[i] = 0; gained = points(n); score += gained; } }
  if (lineLen) *lineLen = n;
  if (!empties(free_)) finish();
  return gained;
}
static void reset() {
  memset(board, 0, sizeof board);
  score = 0; sel = -1; over = false; overRecord = false; confirmAt = 0; started = true;
  nextN = START;
  for (int k = 0; k < START; k++) nextB[k] = (uint8_t)rc();
  // the opening five are placed without lining up: a lucky line here would only confuse
  for (;;) {
    int len = 0;
    memset(board, 0, sizeof board);
    score = 0; nextN = START;
    computerTurn(false, &len);
    if (!len) break;
    for (int k = 0; k < START; k++) nextB[k] = (uint8_t)rc();
  }
  snprintf(statusText, sizeof statusText, "%s", T(S_L_PROMPT));
}
static void afterMove(int dst) {
  uint8_t out[CELLS], free_[CELLS];
  memset(out, 0, sizeof out);
  linesAt(dst, out);
  int n = countMarked(out);
  if (n) {
    int p = clearCells(out);
    say2(S_L_LINE, n, p);
    if (empties(free_) == CELLS) computerTurn(true, 0);   // the board is empty: it gets new marbles anyway
  } else {
    int len = 0, gained = computerTurn(true, &len);
    if (!over) { if (gained) say2(S_L_LUCKY, len, gained); else say(S_L_PROMPT); }
  }
  paintScore(); paintBest(); paintNext();
  if (over) { paintStatus(); paintBar(); }
}
static void tap(int i) {
  if (over) return;
  if (board[i]) {
    int old = sel;
    if (sel == i) { sel = -1; say(S_L_PROMPT); }
    else { sel = i; say(S_L_PICKED); snd::play(560, 40); }
    if (old >= 0) paintCell(old);
    paintCell(i);
    return;
  }
  if (sel < 0) { say(S_L_PICK_FIRST); return; }
  uint8_t path[CELLS];
  int len = findPath(sel, i, path);
  if (!len) {
    say(S_L_BLOCKED);
    snd::play(140, 120);
    hw::fillRect(cellX(i) + 2, cellY(i) + 2, CELL - 5, CELL - 5, C_DANGER);   // a short flash on the cell that cannot be reached
    app::wait(120);
    paintCell(i);
    return;
  }
  int color = board[sel], from = sel;
  board[from] = 0; sel = -1;
  snd::play(300, 40);
  for (int k = 1; k < len; k++) {
    drawCell(path[k - 1], 0, 0);
    drawCell(path[k], color, R);
    app::wait(28);
  }
  board[i] = (uint8_t)color;
  paintCell(i);
  afterMove(i);
}

/* ---------- screen ---------- */

void enter() {
  if (!started) reset();
  confirmAt = 0;
  // the texts may be in another language now
  if (over) app::fillText(statusText, sizeof statusText, T(overRecord ? S_L_OVER_BEST : S_L_OVER), "", score, 0);
  else snprintf(statusText, sizeof statusText, "%s", T(sel >= 0 ? S_L_PICKED : S_L_PROMPT));
  loadBest();
  paintAll();
}
void update() {
  using app::touch;
  if (confirmAt && hw::ms() - confirmAt > 3500) { confirmAt = 0; paintStatus(); paintBar(); }
  if (!touch.pressed) return;
  int x = touch.x, y = touch.y;
  if (B_NEW.has(x, y)) {
    if (score > 0 && !over && !confirmAt) { confirmAt = hw::ms(); paintStatus(); paintBar(); snd::play(660, 40); }
    else { reset(); snd::play(880, 25); paintAll(); }
    return;
  }
  if (confirmAt) { confirmAt = 0; paintStatus(); paintBar(); }
  if (B_MENU.has(x, y)) { snd::play(880, 25); app::go(app::SCR_MENU); return; }
  if (x >= X0 && y >= Y0 && x < X0 + N * CELL && y < Y0 + N * CELL) tap((y - Y0) / CELL * N + (x - X0) / CELL);
}

#ifdef CYD_SIM   // views and shortcuts for the tests in sim/
uint8_t* simBoard() { return board; }
int simScore() { return score; }
bool simOver() { return over; }
int simSel() { return sel; }
int simPoints(int n) { return points(n); }
void simReset() { reset(); }
void simTap(int i) { tap(i); }
void simPowerOff() { started = false; bestLoaded = false; }
int simCellX(int i) { return cellX(i) + CELL / 2; }
int simCellY(int i) { return cellY(i) + CELL / 2; }
#endif

}  // namespace lines
