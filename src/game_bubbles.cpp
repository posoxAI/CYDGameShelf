// Bubbles. The rules are the same as in the browser version: a shot bubble sticks where it lands, three or more
// of one colour touching pop, whatever is left hanging falls, and five shots in a row that pop nothing bring a
// new row down from the top.
#include "app.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace bubbles {

// The field is a honeycomb: every other row is shifted half a bubble along and holds one bubble fewer.
// Eleven columns of ten-pixel bubbles fill the width; eleven rows are what is left once the cannon has its
// place at the bottom, so the field is shorter than the browser's thirteen and starts with five rows instead
// of six. The proportion of full field to empty space is what matters, and it is about the same.
static const int COLS = 11, FIELD_ROWS = 11, MAXR = FIELD_ROWS + 2;
static const int COLORS = 6, START_ROWS = 5, MISS_LIMIT = 5;
static const int R = 10, D = 2 * R, RH = 17;    // RH is 10*sqrt(3) rounded down: the rows overlap by a third of a pixel
static const int FX = 10, FY = 36, FW = COLS * D, FH = 234;
static const int LINE_Y = FY + D + (FIELD_ROWS - 1) * RH;   // a bubble that reaches below this line ends the game
static const int LX = FX + FW / 2, LY = LINE_Y + 26;        // where the cannon sits
static const int STATUS_Y = 274;
static const ui::Rect B_MENU = {4, 292, 74, 26}, B_NEW = {83, 292, 153, 26};
static const ui::Rect HUD_NEXT = {182, 14, 52, 22};         // the waiting bubble; a tap on it swaps the two
static const ui::Rect CANNON = {LX - 26, LY - 22, 53, 36};  // the box the cannon is redrawn in, clear of the field

static const uint16_t FIELD_BG = RGB(21, 34, 42), GUIDE = RGB(120, 140, 152), BARREL = RGB(95, 119, 129);
static const float PI_F = 3.14159265f, SPEED = 420.0f, DT = 0.016f, MIN_ANGLE = 0.16f;

// The bubbles borrow the marble colours of Five in a Line, signs and all. The panel renders colours its own
// way and that set was settled on the board itself; a second set picked from a screenshot would only repeat a
// mistake this project has already made twice. Settings -> Ball colours changes both games at once.
static void drawBubble(int cx, int cy, int r, int c) { lines::drawSample(cx, cy, r, c, app::palette); }

static uint8_t grid[MAXR][COLS];
static bool shiftFirst = false;
static int cur = 1, waiting = 1;
static int score = 0, bestV = 0, misses = 0;
static bool over = false, won = false, started = false, bestLoaded = false, overRecord = false;
static bool aiming = false, flying = false, showMoves = true;
static float angle = PI_F / 2;
static uint32_t confirmAt = 0;
static char statusText[80] = "";
static int guideN = 0;
static int16_t guideX[20], guideY[20];

// Bubbles that have left the field but are still on the screen, while they shrink or fall.
struct Bub { float x, y, vx, vy; uint8_t c; };
static Bub pile[MAXR * COLS];

/* ---------- the honeycomb ---------- */

static bool shifted(int r) { return ((r & 1) == 1) != shiftFirst; }
static int rowLen(int r) { return shifted(r) ? COLS - 1 : COLS; }
static int bx(int r, int c) { return FX + R + c * D + (shifted(r) ? R : 0); }
static int by(int r) { return FY + R + r * RH; }

// The six cells that touch (r, c). Returns how many are on the field.
static int neighbours(int r, int c, int* nr, int* nc) {
  int s = shifted(r) ? 1 : 0, n = 0;
  const int dr[6] = {0, 0, -1, -1, 1, 1}, dc[6] = {-1, 1, s - 1, s, s - 1, s};
  for (int k = 0; k < 6; k++) {
    int rr = r + dr[k], cc = c + dc[k];
    if (rr >= 0 && rr < MAXR && cc >= 0 && cc < rowLen(rr)) { nr[n] = rr; nc[n] = cc; n++; }
  }
  return n;
}
static int countBubbles() {
  int n = 0;
  for (int r = 0; r < MAXR; r++) for (int c = 0; c < rowLen(r); c++) if (grid[r][c]) n++;
  return n;
}
static int lowestRow() {
  for (int r = MAXR - 1; r >= 0; r--) for (int c = 0; c < rowLen(r); c++) if (grid[r][c]) return r;
  return -1;
}
// The cannon is only loaded with colours that are still on the field, so no shot is wasted by definition.
static int pickColor() {
  uint8_t seen[COLORS + 1];
  int list[COLORS], n = 0;
  memset(seen, 0, sizeof seen);
  for (int r = 0; r < MAXR; r++) for (int c = 0; c < rowLen(r); c++) {
    int v = grid[r][c];
    if (v && !seen[v]) { seen[v] = 1; list[n++] = v; }
  }
  if (!n) return 1 + (int)(hw::rnd() % COLORS);
  return list[hw::rnd() % (uint32_t)n];
}

/* ---------- drawing ---------- */

static void paintDanger() {
  for (int x = FX + 4; x < FX + FW - 6; x += 10) hw::fillRect(x, LINE_Y + 2, 6, 2, C_DANGER);
}
static void paintBubblesIn(int x, int y, int w, int h) {
  for (int r = 0; r < MAXR; r++) for (int c = 0; c < rowLen(r); c++) {
    if (!grid[r][c]) continue;
    int cx = bx(r, c), cy = by(r);
    if (cx + R < x || cx - R >= x + w || cy + R < y || cy - R >= y + h) continue;
    drawBubble(cx, cy, R, grid[r][c]);
  }
}
static void paintCannon() {
  float ax = cosf(angle), ay = -sinf(angle);
  hw::fillRect(CANNON.x, CANNON.y, CANNON.w, CANNON.h, FIELD_BG);
  // once the game is lost the field reaches down into the cannon's place, and what is there to see is the
  // bubble that crossed the line, not a cannon with nothing left to shoot at
  if (over) { paintBubblesIn(CANNON.x, CANNON.y, CANNON.w, CANNON.h); return; }
  for (int k = 0; k <= 4; k++) hw::fillCircle(LX + (int)(ax * 4 * k), LY + (int)(ay * 4 * k), 6, BARREL);
  hw::fillCircle(LX, LY, 12, BARREL);
  if (!flying) drawBubble(LX, LY, 8, cur);
}
// Puts a piece of the field back: the background, the red line, every bubble that reaches into it, the cannon.
// Everything that moves is rubbed out this way, so nothing has to redraw the whole field.
// A bubble of radius R covers 2R + 1 pixels, not 2R — rub out one pixel less and every frame of a flight
// leaves a sliver of colour behind, which on the board reads as a dotted trail across the field.
static void repaintArea(int x, int y, int w, int h) {
  if (x < FX) { w += x - FX; x = FX; }
  if (y < FY) { h += y - FY; y = FY; }
  if (x + w > FX + FW) w = FX + FW - x;
  if (y + h > FY + FH) h = FY + FH - y;
  if (w <= 0 || h <= 0) return;
  hw::fillRect(x, y, w, h, FIELD_BG);
  if (y < LINE_Y + 4 && y + h > LINE_Y + 2) paintDanger();
  paintBubblesIn(x, y, w, h);
  if (x < CANNON.x + CANNON.w && x + w > CANNON.x && y < CANNON.y + CANNON.h && y + h > CANNON.y) paintCannon();
}

// true when a flying bubble at (x, y) touches the ceiling or a bubble already on the field
static bool touches(float x, float y) {
  if (y <= FY + R) return true;
  for (int r = 0; r < MAXR; r++) {
    float dy = (float)by(r) - y;
    if (dy > D || dy < -D) continue;
    for (int c = 0; c < rowLen(r); c++) {
      if (!grid[r][c]) continue;
      float dx = (float)bx(r, c) - x;
      if (dx * dx + dy * dy < 296.0f) return true;     // (0.86 * D) squared, as in the browser
    }
  }
  return false;
}

static void eraseGuide() {
  for (int k = 0; k < guideN; k++) repaintArea(guideX[k] - 3, guideY[k] - 3, 7, 7);
  guideN = 0;
}
// A dotted preview of the flight, bouncing off the walls and stopping at the first bubble.
static void paintGuide() {
  float x = (float)LX, y = (float)LY, dx = cosf(angle), dy = -sinf(angle);
  int travelled = 0;
  guideN = 0;
  while (travelled < 210 && guideN < 20) {
    x += dx * 3; y += dy * 3; travelled += 3;
    if (x < FX + R) { x = 2 * (FX + R) - x; dx = -dx; }
    else if (x > FX + FW - R) { x = 2 * (FX + FW - R) - x; dx = -dx; }
    if (travelled > 20 && touches(x, y)) break;
    if (travelled > 20 && travelled % 15 == 0) {
      guideX[guideN] = (int16_t)x; guideY[guideN] = (int16_t)y; guideN++;
      hw::fillCircle((int)x, (int)y, 1, GUIDE);
    }
  }
}

static void paintScore() {
  char text[12];
  snprintf(text, sizeof text, "%d", score);
  ui::label(6, 15, 56, 19, text, FONT_M, C_INK, C_BG, ui::LEFT);
}
static void paintBest() {
  char text[12];
  snprintf(text, sizeof text, "%d", bestV > score ? bestV : score);
  ui::label(64, 15, 56, 19, text, FONT_M, C_INK, C_BG, ui::CENTER);
}
static void paintMisses() {
  char text[12];
  snprintf(text, sizeof text, "%d", MISS_LIMIT - misses);
  ui::label(122, 15, 58, 19, text, FONT_M, misses + 1 >= MISS_LIMIT ? C_DANGER : C_INK, C_BG, ui::CENTER);
}
static void paintWaiting() {
  hw::fillRect(HUD_NEXT.x, HUD_NEXT.y, HUD_NEXT.w, HUD_NEXT.h, C_BG);
  if (!over) drawBubble(HUD_NEXT.x + HUD_NEXT.w / 2, HUD_NEXT.y + HUD_NEXT.h / 2, 8, waiting);
}
static void paintStatus() {
  uint16_t color = confirmAt ? C_INK : (over ? (overRecord || won ? C_GOOD : C_DANGER) : C_MUTED);
  ui::label(0, STATUS_Y, hw::W, 16, confirmAt ? T(S_L_CONFIRM) : statusText, FONT_S, color, C_BG);
}
static void say(StrId id) { snprintf(statusText, sizeof statusText, "%s", T(id)); paintStatus(); }
static void say2(StrId id, int a, int b) { app::fillText(statusText, sizeof statusText, T(id), "", a, b); paintStatus(); }
static void paintBar() {
  ui::button(B_MENU.x, B_MENU.y, B_MENU.w, B_MENU.h, T(S_MENU), FONT_M, C_INK, C_PANEL);
  bool hot = confirmAt != 0;
  ui::button(B_NEW.x, B_NEW.y, B_NEW.w, B_NEW.h, T(S_L_NEW), FONT_M,
             hot || over ? C_ON_ACCENT : C_INK, hot ? C_DANGER : (over ? C_ACCENT : C_PANEL));
}
static void paintAll() {
  hw::fillRect(0, 0, hw::W, hw::H, C_BG);
  ui::label(6, 2, 56, 13, T(S_L_SCORE), FONT_S, C_MUTED, C_BG, ui::LEFT);
  ui::label(64, 2, 56, 13, T(S_BEST), FONT_S, C_MUTED, C_BG, ui::CENTER);
  ui::label(122, 2, 58, 13, T(S_B_ROW_IN), FONT_S, C_MUTED, C_BG, ui::CENTER);
  ui::label(182, 2, 52, 13, T(S_L_NEXT), FONT_S, C_MUTED, C_BG, ui::CENTER);
  paintScore(); paintBest(); paintMisses(); paintWaiting();
  hw::fillRect(FX, FY, FW, FH, FIELD_BG);
  paintDanger();
  paintBubblesIn(FX, FY, FW, FH);
  paintCannon();
  // the guide stays up between shots, as it does in the browser: it is the only thing that says where the
  // cannon points, and a finger covers the place it is aiming at
  guideN = 0;
  if (!over) paintGuide();
  paintStatus(); paintBar();
}

/* ---------- rules ---------- */

// The free cell nearest to where the bubble stopped, among the cells that hang from something.
static bool landingCell(float x, float y, int& br, int& bc) {
  int guess = (int)((y - (FY + R)) / RH + 0.5f);
  int lo = guess - 2 < 0 ? 0 : guess - 2, hi = guess + 2 > MAXR - 1 ? MAXR - 1 : guess + 2;
  float bestD = 1e9f;
  bool found = false;
  for (int r = lo; r <= hi; r++) for (int c = 0; c < rowLen(r); c++) {
    if (grid[r][c]) continue;
    int nr[6], nc[6], n = neighbours(r, c, nr, nc);
    bool held = r == 0;
    for (int k = 0; k < n && !held; k++) if (grid[nr[k]][nc[k]]) held = true;
    if (!held) continue;
    float dx = (float)bx(r, c) - x, dy = (float)by(r) - y, d2 = dx * dx + dy * dy;
    if (d2 < bestD) { bestD = d2; br = r; bc = c; found = true; }
  }
  return found;
}
// Marks every bubble of the same colour that (r, c) is connected to, and returns how many there are.
static int group(int r, int c, uint8_t mark[MAXR][COLS]) {
  int qr[MAXR * COLS], qc[MAXR * COLS], head = 0, tail = 0, color = grid[r][c];
  mark[r][c] = 1; qr[tail] = r; qc[tail] = c; tail++;
  while (head < tail) {
    int nr[6], nc[6], n = neighbours(qr[head], qc[head], nr, nc);
    head++;
    for (int k = 0; k < n; k++)
      if (!mark[nr[k]][nc[k]] && grid[nr[k]][nc[k]] == color) { mark[nr[k]][nc[k]] = 1; qr[tail] = nr[k]; qc[tail] = nc[k]; tail++; }
  }
  return tail;
}
// Marks the bubbles that no longer hang from the top row, directly or through others.
static int looseCells(uint8_t mark[MAXR][COLS]) {
  uint8_t held[MAXR][COLS];
  int qr[MAXR * COLS], qc[MAXR * COLS], head = 0, tail = 0, n = 0;
  memset(held, 0, sizeof held);
  for (int c = 0; c < rowLen(0); c++) if (grid[0][c]) { held[0][c] = 1; qr[tail] = 0; qc[tail] = c; tail++; }
  while (head < tail) {
    int nr[6], nc[6], k = neighbours(qr[head], qc[head], nr, nc);
    head++;
    for (int j = 0; j < k; j++)
      if (!held[nr[j]][nc[j]] && grid[nr[j]][nc[j]]) { held[nr[j]][nc[j]] = 1; qr[tail] = nr[j]; qc[tail] = nc[j]; tail++; }
  }
  for (int r = 0; r < MAXR; r++) for (int c = 0; c < rowLen(r); c++)
    if (grid[r][c] && !held[r][c]) { mark[r][c] = 1; n++; }
  return n;
}
// Takes the marked bubbles off the field and remembers where they were, so an animation can still draw them
// while the field no longer does.
static int harvest(const uint8_t mark[MAXR][COLS]) {
  int n = 0;
  for (int r = 0; r < MAXR; r++) for (int c = 0; c < rowLen(r); c++) {
    if (!mark[r][c] || !grid[r][c]) continue;
    pile[n].x = (float)bx(r, c); pile[n].y = (float)by(r); pile[n].c = grid[r][c];
    pile[n].vx = ((int)(hw::rnd() % 41) - 20) * 0.6f;
    pile[n].vy = -30.0f - (float)(hw::rnd() % 50);
    n++;
    grid[r][c] = 0;
  }
  return n;
}
static void wipePile(int n) {
  for (int k = 0; k < n; k++) repaintArea((int)pile[k].x - R, (int)pile[k].y - R, D + 1, D + 1);
}
static void shrinkPile(int n) {
  static const int STEP[3] = {7, 4, 1};
  for (int s = 0; s < 3; s++) {
    wipePile(n);
    for (int k = 0; k < n; k++) drawBubble((int)pile[k].x, (int)pile[k].y, STEP[s], pile[k].c);
    app::wait(45);
  }
  wipePile(n);
}
static void dropPile(int n) {
  for (int step = 0; step < 32; step++) {
    wipePile(n);                           // where they were drawn last time round
    bool any = false;
    for (int k = 0; k < n; k++) {
      pile[k].vy += 1000.0f * 0.025f;
      pile[k].x += pile[k].vx * 0.025f; pile[k].y += pile[k].vy * 0.025f;
      if (pile[k].y - R >= FY + FH) continue;        // past the bottom of the field and out of the game
      any = true;
      // only while the whole bubble is inside the field: repaintArea cannot rub out what falls outside it
      if (pile[k].x - R >= FX && pile[k].x + R < FX + FW && pile[k].y + R < FY + FH)
        drawBubble((int)pile[k].x, (int)pile[k].y, R, pile[k].c);
    }
    app::wait(16);
    if (!any) break;
  }
  wipePile(n);
}

static void loadBest() {
  if (!bestLoaded) { bestV = hw::loadInt("bbest", 0); bestLoaded = true; }
}
int best() { loadBest(); return bestV; }
void resetBest() { bestV = 0; bestLoaded = true; hw::saveInt("bbest", 0); }

static void keepBest() {
  loadBest();
  if (score > bestV) { bestV = score; overRecord = true; hw::saveInt("bbest", bestV); }
}
// A new row comes down from the top: everything moves one row along and the shift changes hands with it,
// which leaves every old row with the offset it already had.
static void addRow() {
  for (int r = MAXR - 1; r > 0; r--) memcpy(grid[r], grid[r - 1], COLS);
  shiftFirst = !shiftFirst;
  memset(grid[0], 0, COLS);
  for (int c = 0; c < rowLen(0); c++) grid[0][c] = (uint8_t)pickColor();
}
static void finish(bool cleared) {
  over = true; won = cleared;
  keepBest();
  app::fillText(statusText, sizeof statusText,
                T(cleared ? (overRecord ? S_B_WON_BEST : S_B_WON) : (overRecord ? S_B_LOST_BEST : S_B_LOST)), "", score, 0);
  if (cleared) { snd::play(523, 160); snd::note(659, 160); snd::note(784, 160); snd::note(1047, 260); }
  else { snd::play(330, 160); snd::note(220, 160); snd::note(110, 320); }
}

static void land(float x, float y) {
  uint8_t mark[MAXR][COLS];
  int lr = 0, lc = 0;
  // nowhere to stick: the shot is simply lost, as in the browser
  if (!landingCell(x, y, lr, lc)) { say(S_B_PROMPT); if (showMoves) paintCannon(); return; }
  grid[lr][lc] = (uint8_t)cur;
  if (showMoves) drawBubble(bx(lr, lc), by(lr), R, cur);

  memset(mark, 0, sizeof mark);
  int popped = group(lr, lc, mark), fell = 0;
  if (popped >= 3) {
    int n = harvest(mark);
    if (showMoves) shrinkPile(n);
    memset(mark, 0, sizeof mark);
    fell = looseCells(mark);
    if (fell) {
      int m = harvest(mark);
      if (showMoves) dropPile(m);
    }
    int gain = popped * 10 + fell * 20;
    score += gain;
    for (int k = 0; k < popped && k < 5; k++) snd::note(620 + k * 90, 70);
    if (fell) snd::note(300, 180);
    if (fell) say2(S_B_POP_DROP, popped, fell); else say2(S_B_POP, popped, gain);
  } else {
    misses++;
    snd::play(200, 60);
    if (misses >= MISS_LIMIT) {
      misses = 0;
      addRow();
      say(S_B_ROW);
      snd::note(150, 200);
      if (showMoves) paintAll();
    } else say(S_B_PROMPT);
  }

  if (!countBubbles()) finish(true);
  else if (lowestRow() >= FIELD_ROWS) finish(false);
  else {
    cur = waiting;
    uint8_t seen = 0;
    for (int r = 0; r < MAXR && !seen; r++) for (int c = 0; c < rowLen(r); c++) if (grid[r][c] == cur) { seen = 1; break; }
    if (!seen) cur = pickColor();
    waiting = pickColor();
  }
  if (showMoves) {
    paintScore(); paintBest(); paintMisses(); paintWaiting(); paintCannon();
    eraseGuide();                         // the field has changed under it
    if (over) { paintStatus(); paintBar(); } else paintGuide();
  }
}

static void shoot() {
  float x = (float)LX, y = (float)LY, vx = cosf(angle) * SPEED, vy = -sinf(angle) * SPEED;
  int steps = (int)(SPEED * DT / 3.0f) + 1, prevX = LX, prevY = LY;
  float h = DT / steps;
  flying = true;
  snd::play(520, 60);
  for (;;) {
    bool landed = false;
    for (int k = 0; k < steps; k++) {
      x += vx * h; y += vy * h;
      if (x < FX + R) { x = 2 * (FX + R) - x; vx = -vx; }
      else if (x > FX + FW - R) { x = 2 * (FX + FW - R) - x; vx = -vx; }
      if (touches(x, y)) { landed = true; break; }
    }
    if (showMoves) {
      repaintArea(prevX - R, prevY - R, D + 1, D + 1);
      if (!landed) drawBubble((int)x, (int)y, R, cur);
      prevX = (int)x; prevY = (int)y;
      app::wait(12);
    }
    if (landed) break;
  }
  flying = false;
  land(x, y);
}
static void swapNext() {
  int t = cur; cur = waiting; waiting = t;
  snd::play(440, 40);
  paintWaiting(); paintCannon();
}
// Keeps the aim on the finger. Below the cannon there is nothing to aim at, so the direction is left alone.
static bool setAim(int x, int y) {
  if (y > LY - 4) return false;
  float a = atan2f((float)(LY - y), (float)(x - LX));
  if (a < MIN_ANGLE) a = MIN_ANGLE;
  if (a > PI_F - MIN_ANGLE) a = PI_F - MIN_ANGLE;
  if (fabsf(a - angle) < 0.004f) return true;
  angle = a;
  eraseGuide(); paintCannon(); paintGuide();
  return true;
}

static void reset() {
  memset(grid, 0, sizeof grid);
  shiftFirst = false;
  score = 0; misses = 0; over = false; won = false; overRecord = false; confirmAt = 0;
  aiming = false; flying = false; started = true;
  angle = PI_F / 2;
  guideN = 0;
  for (int r = 0; r < START_ROWS; r++) for (int c = 0; c < rowLen(r); c++) {
    // lean towards a neighbour's colour, so the field starts with groups worth aiming at
    int near[7], n = 0, nr[6], nc[6];
    if (c > 0) near[n++] = grid[r][c - 1];
    int k = neighbours(r, c, nr, nc);
    for (int j = 0; j < k; j++) if (nr[j] == r - 1 && grid[nr[j]][nc[j]]) near[n++] = grid[nr[j]][nc[j]];
    grid[r][c] = (uint8_t)((n && hw::rnd() % 100 < 42) ? near[hw::rnd() % (uint32_t)n] : 1 + (int)(hw::rnd() % COLORS));
  }
  cur = pickColor(); waiting = pickColor();
  snprintf(statusText, sizeof statusText, "%s", T(S_B_PROMPT));
}

/* ---------- screen ---------- */

void enter() {
  if (!started) reset();
  confirmAt = 0;
  aiming = false;
  // the texts may be in another language now
  if (over) app::fillText(statusText, sizeof statusText,
                          T(won ? (overRecord ? S_B_WON_BEST : S_B_WON) : (overRecord ? S_B_LOST_BEST : S_B_LOST)), "", score, 0);
  else snprintf(statusText, sizeof statusText, "%s", T(S_B_PROMPT));
  loadBest();
  paintAll();
}

void update() {
  using app::touch;
  if (confirmAt && hw::ms() - confirmAt > 3500) { confirmAt = 0; paintStatus(); paintBar(); }
  if (aiming) {
    if (touch.down) { setAim(touch.liveX, touch.liveY); return; }
    aiming = false;
    eraseGuide();
    // a finger lifted below the cannon takes the shot back, as it does in the browser
    if (touch.liveY < LY - 4) shoot(); else { paintCannon(); paintGuide(); }
    return;
  }
  if (!touch.pressed) return;
  int x = touch.x, y = touch.y;
  if (B_NEW.has(x, y)) {
    if (score > 0 && !over && !confirmAt) { confirmAt = hw::ms(); paintStatus(); paintBar(); snd::play(660, 40); }
    else { reset(); snd::play(880, 25); paintAll(); }
    return;
  }
  if (confirmAt) { confirmAt = 0; paintStatus(); paintBar(); }
  if (B_MENU.has(x, y)) { snd::play(880, 25); app::go(app::SCR_MENU); return; }
  if (HUD_NEXT.has(x, y)) { if (!over) swapNext(); return; }
  if (!over && x >= FX && x < FX + FW && y >= FY && y < LY - 4) { aiming = true; setAim(x, y); }
}

#ifdef CYD_SIM   // views and shortcuts for the tests in sim/
uint8_t* simGrid() { return &grid[0][0]; }
int simCols() { return COLS; }
int simMaxRows() { return MAXR; }
int simFieldRows() { return FIELD_ROWS; }
int simRowLen(int r) { return rowLen(r); }
int simScore() { return score; }
int simMisses() { return misses; }
int simCur() { return cur; }
int simNext() { return waiting; }
bool simOver() { return over; }
bool simWon() { return won; }
void simReset() { reset(); }
void simLoaded(int a, int b) { cur = a; waiting = b; }
void simAnimate(bool on) { showMoves = on; }
void simAim(int x, int y) { setAim(x, y); }
void simShoot() { if (!over) shoot(); }
void simPowerOff() { started = false; bestLoaded = false; }
int simCannonY() { return LY; }
#endif

}  // namespace bubbles
