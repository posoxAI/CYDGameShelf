#include "app.h"
#include <stdio.h>
#include <stdlib.h>

namespace app {

int lang = 0;
bool soundOn = true, flip = false, invert = false;
bool marks = true;
int palette = 0;
Touch touch = {false, false, false, false, false, 0, 0, 0, 0, 0};

static Screen current = SCR_MENU;

/* ---------- touch: from raw panel readings to screen pixels ---------- */

// screen = cal * raw, found by the calibration screen. It absorbs scale, offset and swapped or mirrored axes,
// so the code never needs to know how the panel is glued on.
static float cal[6] = {0, 0, 0, 0, 0, 0};
static bool calOk = false;

static void mapRaw(int rx, int ry, int& sx, int& sy) {
  float x = cal[0] * rx + cal[1] * ry + cal[2], y = cal[3] * rx + cal[4] * ry + cal[5];
  if (flip) { x = hw::W - 1 - x; y = hw::H - 1 - y; }
  sx = x < 0 ? 0 : (x > hw::W - 1 ? hw::W - 1 : (int)(x + 0.5f));
  sy = y < 0 ? 0 : (y > hw::H - 1 ? hw::H - 1 : (int)(y + 0.5f));
}
static void saveCal() {
  char key[8];
  for (int k = 0; k < 6; k++) { snprintf(key, sizeof key, "cal%d", k); hw::saveInt(key, (int32_t)(cal[k] * 65536.0f)); }
  hw::saveInt("calok", 1);
}
static void loadCal() {
  char key[8];
  calOk = hw::loadInt("calok", 0) == 1;
  for (int k = 0; k < 6; k++) { snprintf(key, sizeof key, "cal%d", k); cal[k] = hw::loadInt(key, 0) / 65536.0f; }
}

// A resistive panel is noisy at the moment the finger lands and lifts, so a press counts only after two
// readings that agree, and a release only after three empty ones.
static uint32_t lastPoll = 0;
static int okCount = 0, missCount = 0, lastX = 0, lastY = 0, sumN = 0;
static long sumX = 0, sumY = 0;
static bool longFired = false;

void pollTouch() {
  touch.pressed = touch.released = touch.longPress = false;
  uint32_t now = hw::ms();
  if (now - lastPoll < 8) return;
  lastPoll = now;
  int rx, ry, rz, sx = 0, sy = 0;
  bool valid = hw::touchRaw(rx, ry, rz);
  if (valid) mapRaw(rx, ry, sx, sy);
  if (valid) {
    missCount = 0;
    if (!touch.down) {
      if (okCount > 0 && (abs(sx - lastX) > 14 || abs(sy - lastY) > 14)) okCount = 0;
      lastX = sx; lastY = sy; okCount++;
      if (okCount >= 2) {
        touch.down = true; touch.pressed = true; touch.downAt = now; touch.wasLong = false;
        sumX = sx; sumY = sy; sumN = 1; touch.x = sx; touch.y = sy; longFired = false;
        touch.liveX = sx; touch.liveY = sy;
      }
    } else {
      // the first tenth of a second is averaged into the position; later readings are ignored
      if (now - touch.downAt < 110) { sumX += sx; sumY += sy; sumN++; touch.x = (int)(sumX / sumN); touch.y = (int)(sumY / sumN); }
      if (!longFired && now - touch.downAt >= 450) { longFired = true; touch.longPress = true; }
      // a screen that is dragged rather than tapped needs the finger followed; a panel this noisy needs the
      // readings smoothed on the way, or the aim shakes
      touch.liveX = (touch.liveX * 2 + sx) / 3; touch.liveY = (touch.liveY * 2 + sy) / 3;
    }
  } else {
    okCount = 0;
    if (touch.down && ++missCount >= 3) { touch.down = false; touch.released = true; touch.wasLong = longFired; }
  }
}

void fillText(char* out, int size, const char* pattern, const char* word, int a, int b) {
  int n = 0, numbers = 0;
  char num[12];
  while (*pattern && n < size - 1) {
    const char* piece = 0;
    if (pattern[0] == '%' && pattern[1] == 's') piece = word;
    else if (pattern[0] == '%' && pattern[1] == 'd') { snprintf(num, sizeof num, "%d", numbers++ ? b : a); piece = num; }
    if (!piece) { out[n++] = *pattern++; continue; }
    pattern += 2;
    while (*piece && n < size - 1) out[n++] = *piece++;
  }
  out[n] = 0;
}

void wait(uint32_t ms) {
  uint32_t start = hw::ms();
  while (hw::ms() - start < ms) { snd::tick(); hw::sleepMs(2); }
}

/* ---------- calibration ---------- */

static void cross(int x, int y, uint16_t color) {
  hw::fillRect(x - 14, y - 1, 29, 3, color);
  hw::fillRect(x - 1, y - 14, 3, 29, color);
  hw::fillRect(x - 2, y - 2, 5, 5, C_BG);
}

// Waits for a steady press and returns the averaged raw reading; then waits for the finger to lift.
static void readPress(int& rx, int& ry) {
  char line[48];
  long sx = 0, sy = 0;
  int n = 0, quiet = 0, x, y, z;
  uint32_t start = 0, shown = 0;
  for (;;) {
    hw::sleepMs(10);
    if (hw::touchRaw(x, y, z)) {
      quiet = 0;
      if (!start) start = hw::ms();
      if (hw::ms() - start > 80) { sx += x; sy += y; n++; }       // skip the landing
      if (hw::ms() - shown > 150) {
        // the raw numbers stay on screen: they are what to report if calibration keeps failing
        shown = hw::ms();
        snprintf(line, sizeof line, "x %d  y %d  z %d", x, y, z);
        ui::label(0, hw::H - 18, hw::W, 16, line, FONT_S, C_MUTED, C_BG);
      }
      if (n >= 20) break;
    } else if (start && ++quiet > 4) { start = 0; n = 0; sx = sy = 0; }   // lifted too early: start over
  }
  rx = (int)(sx / n); ry = (int)(sy / n);
  for (quiet = 0; quiet < 12;) { hw::sleepMs(10); if (hw::touchRaw(x, y, z)) quiet = 0; else quiet++; }
}

void calibrate() {
  static const int PX[4] = {30, 210, 60, 150}, PY[4] = {44, 150, 284, 226};   // three to solve with, one to check
  char line[48];
  for (;;) {
    int rx[4], ry[4];
    hw::fillRect(0, 0, hw::W, hw::H, C_BG);
    for (int k = 0; k < 4; k++) {
      ui::label(0, 96, hw::W, 20, "Нажмите на крестик", FONT_M, C_INK, C_BG);
      ui::label(0, 118, hw::W, 20, "Tap the cross", FONT_M, C_INK, C_BG);
      snprintf(line, sizeof line, "%d / 4", k + 1);
      ui::label(0, 180, hw::W, 18, line, FONT_S, C_MUTED, C_BG);
      cross(PX[k], PY[k], k < 3 ? C_ACCENT : C_GOOD);
      readPress(rx[k], ry[k]);
      cross(PX[k], PY[k], C_BG);
      snprintf(line, sizeof line, "cal point %d: raw %d %d", k + 1, rx[k], ry[k]);
      hw::log(line);
      snd::play(880, 40); wait(120);
    }
    // the crosses were drawn through the current flip; the stored mapping is for the unflipped screen
    double tx[4], ty[4], X[4], Y[4];
    for (int k = 0; k < 4; k++) {
      tx[k] = flip ? hw::W - 1 - PX[k] : PX[k]; ty[k] = flip ? hw::H - 1 - PY[k] : PY[k];
      X[k] = rx[k]; Y[k] = ry[k];
    }
    // three equations a*X + b*Y + c = t for each screen axis, solved by Cramer's rule
    double det = X[0] * (Y[1] - Y[2]) - Y[0] * (X[1] - X[2]) + (X[1] * Y[2] - X[2] * Y[1]);
    bool good = det > 1000.0 || det < -1000.0;
    if (good) {
      for (int axis = 0; axis < 2; axis++) {
        const double* t = axis ? ty : tx;
        double a = (t[0] * (Y[1] - Y[2]) - Y[0] * (t[1] - t[2]) + (t[1] * Y[2] - t[2] * Y[1])) / det;
        double b = (X[0] * (t[1] - t[2]) - t[0] * (X[1] - X[2]) + (X[1] * t[2] - X[2] * t[1])) / det;
        double c = (X[0] * (Y[1] * t[2] - Y[2] * t[1]) - Y[0] * (X[1] * t[2] - X[2] * t[1]) + t[0] * (X[1] * Y[2] - X[2] * Y[1])) / det;
        cal[axis * 3] = (float)a; cal[axis * 3 + 1] = (float)b; cal[axis * 3 + 2] = (float)c;
      }
      float ex = cal[0] * rx[3] + cal[1] * ry[3] + cal[2] - tx[3], ey = cal[3] * rx[3] + cal[4] * ry[3] + cal[5] - ty[3];
      snprintf(line, sizeof line, "cal check: off by %d %d px", (int)ex, (int)ey);
      hw::log(line);
      good = ex > -22 && ex < 22 && ey > -22 && ey < 22;
    }
    if (good) { calOk = true; saveCal(); return; }
    ui::label(0, 96, hw::W, 20, "Не получилось, ещё раз", FONT_M, C_DANGER, C_BG);
    ui::label(0, 118, hw::W, 20, "That did not work, again", FONT_M, C_DANGER, C_BG);
    snd::play(200, 200); wait(1400);
  }
}

/* ---------- first start: language ---------- */

static void chooseLanguage() {
  const ui::Rect ru = {30, 110, 180, 54}, en = {30, 180, 180, 54};
  hw::fillRect(0, 0, hw::W, hw::H, C_BG);
  ui::label(0, 50, hw::W, 20, "Язык · Language", FONT_M, C_MUTED, C_BG);
  ui::button(ru.x, ru.y, ru.w, ru.h, "Русский", FONT_M, C_INK, C_PANEL);
  ui::button(en.x, en.y, en.w, en.h, "English", FONT_M, C_INK, C_PANEL);
  for (;;) {
    pollTouch(); snd::tick(); hw::sleepMs(4);
    if (!touch.pressed) continue;
    if (ru.has(touch.x, touch.y)) { lang = 0; break; }
    if (en.has(touch.x, touch.y)) { lang = 1; break; }
  }
  hw::saveInt("lang", lang);
  snd::play(880, 30);
}

/* ---------- menu ---------- */

static const ui::Rect MENU_MINES = {12, 74, 216, 52}, MENU_LINES = {12, 130, 216, 52},
                      MENU_BUBBLES = {12, 186, 216, 52}, MENU_SET = {12, 246, 216, 40};

static void gameButton(const ui::Rect& r, const char* name, const char* sub) {
  hw::fillRoundRect(r.x, r.y, r.w, r.h, 6, C_PANEL);
  ui::label(r.x + 56, r.y + 7, r.w - 62, 20, name, FONT_M, C_INK, C_PANEL, ui::LEFT);
  ui::label(r.x + 56, r.y + 28, r.w - 62, 16, sub, FONT_S, C_MUTED, C_PANEL, ui::LEFT);
}
static void drawMenu() {
  char sub[48], t[16];
  hw::fillRect(0, 0, hw::W, hw::H, C_BG);
  ui::label(0, 12, hw::W, 34, T(S_TITLE), FONT_L, C_INK, C_BG);
  ui::label(0, 50, hw::W, 18, T(S_PICK), FONT_S, C_MUTED, C_BG);

  int b = mines::bestTenths(0);
  if (b) { mines::formatTime(b, t, sizeof t); snprintf(sub, sizeof sub, "%s 9×9: %s", T(S_BEST), t); }
  else snprintf(sub, sizeof sub, "%s", T(S_NO_BEST));
  gameButton(MENU_MINES, T(S_MINES), sub);
  // a little closed tile with a flag
  int ix = MENU_MINES.x + 12, iy = MENU_MINES.y + 9;
  hw::fillRect(ix, iy, 34, 34, RGB(58, 74, 92));
  hw::fillRect(ix, iy, 34, 2, RGB(88, 106, 127)); hw::fillRect(ix, iy, 2, 34, RGB(88, 106, 127));
  hw::fillRect(ix, iy + 32, 34, 2, RGB(36, 48, 61)); hw::fillRect(ix + 32, iy, 2, 34, RGB(36, 48, 61));
  hw::fillRect(ix + 18, iy + 7, 2, 19, C_INK);
  hw::fillTriangle(ix + 18, iy + 7, ix + 7, iy + 12, ix + 18, iy + 17, RGB(255, 90, 77));
  hw::fillRect(ix + 11, iy + 26, 14, 2, C_INK);

  if (lines::best()) snprintf(sub, sizeof sub, "%s: %d", T(S_BEST), lines::best());
  else snprintf(sub, sizeof sub, "%s", T(S_NO_BEST));
  gameButton(MENU_LINES, T(S_LINES), sub);
  for (int k = 0; k < 3; k++) hw::fillCircle(MENU_LINES.x + 14 + k * 14, MENU_LINES.y + 26, 6, lines::ballColor(k + 1));

  if (bubbles::best()) snprintf(sub, sizeof sub, "%s: %d", T(S_BEST), bubbles::best());
  else snprintf(sub, sizeof sub, "%s", T(S_NO_BEST));
  gameButton(MENU_BUBBLES, T(S_BUBBLES), sub);
  // three bubbles hanging over a fourth, the way they sit on the field
  for (int k = 0; k < 3; k++) hw::fillCircle(MENU_BUBBLES.x + 14 + k * 12, MENU_BUBBLES.y + 20, 5, lines::ballColor(k + 4));
  for (int k = 0; k < 2; k++) hw::fillCircle(MENU_BUBBLES.x + 20 + k * 12, MENU_BUBBLES.y + 30, 5, lines::ballColor(k + 1));

  ui::button(MENU_SET.x, MENU_SET.y, MENU_SET.w, MENU_SET.h, T(S_SETTINGS), FONT_M, C_INK, C_PANEL);
  ui::label(0, 296, hw::W, 16, T(S_FOOT), FONT_S, C_MUTED, C_BG);
}
static void updateMenu() {
  if (!touch.pressed) return;
  if (MENU_MINES.has(touch.x, touch.y)) { snd::play(880, 25); go(SCR_MINES); }
  else if (MENU_LINES.has(touch.x, touch.y)) { snd::play(880, 25); go(SCR_LINES); }
  else if (MENU_BUBBLES.has(touch.x, touch.y)) { snd::play(880, 25); go(SCR_BUBBLES); }
  else if (MENU_SET.has(touch.x, touch.y)) { snd::play(880, 25); go(SCR_SETTINGS); }
}

/* ---------- settings ---------- */

static const int ROW_Y[4] = {44, 78, 112, 146};
static const ui::Rect SET_RU = {100, 44, 60, 32}, SET_EN = {168, 44, 60, 32}, SET_SOUND = {100, 78, 128, 32},
                      SET_FLIP = {100, 112, 128, 32}, SET_INV = {100, 146, 128, 32},
                      SET_PAL = {12, 186, 216, 30}, SET_CAL = {12, 220, 216, 30},
                      SET_RESET = {12, 254, 216, 30}, SET_BACK = {12, 288, 216, 30};
static uint32_t resetArmedAt = 0;
static bool resetDone = false;

static void toggle(const ui::Rect& r, const char* text, bool on) {
  ui::button(r.x, r.y, r.w, r.h, text, FONT_M, on ? C_ON_ACCENT : C_INK, on ? C_ACCENT : C_PANEL);
}
static void drawReset() {
  bool armed = resetArmedAt != 0;
  ui::button(SET_RESET.x, SET_RESET.y, SET_RESET.w, SET_RESET.h,
             T(armed ? S_SET_RESET_SURE : (resetDone ? S_SET_RESET_DONE : S_SET_RESET)), FONT_M,
             armed ? C_ON_ACCENT : C_INK, armed ? C_DANGER : C_PANEL);
}
static void drawSettings() {
  static const StrId names[4] = {S_SET_LANG, S_SET_SOUND, S_SET_SCREEN, S_SET_COLORS};
  hw::fillRect(0, 0, hw::W, hw::H, C_BG);
  ui::label(0, 6, hw::W, 34, T(S_SETTINGS), FONT_L, C_INK, C_BG);
  for (int k = 0; k < 4; k++) ui::label(12, ROW_Y[k], 86, 32, T(names[k]), FONT_M, C_MUTED, C_BG, ui::LEFT);
  toggle(SET_RU, "RU", lang == 0);
  toggle(SET_EN, "EN", lang == 1);
  toggle(SET_SOUND, T(soundOn ? S_ON : S_OFF), soundOn);
  toggle(SET_FLIP, T(S_FLIP), flip);
  toggle(SET_INV, T(invert ? S_INVERTED : S_NORMAL), invert);
  toggle(SET_PAL, T(S_PAL_TITLE), false);
  toggle(SET_CAL, T(S_SET_CAL), false);
  drawReset();
  toggle(SET_BACK, T(S_MENU), false);
}
static void updateSettings() {
  if (resetArmedAt && hw::ms() - resetArmedAt > 3500) { resetArmedAt = 0; drawReset(); }
  if (!touch.pressed) return;
  int x = touch.x, y = touch.y;
  bool redraw = true;
  if (SET_RESET.has(x, y)) {
    if (!resetArmedAt) { resetArmedAt = hw::ms(); resetDone = false; }
    else { resetArmedAt = 0; resetDone = true; mines::resetBest(); lines::resetBest(); bubbles::resetBest(); snd::play(440, 120); }
    drawReset();
    return;
  }
  resetArmedAt = 0;
  if (SET_RU.has(x, y) || SET_EN.has(x, y)) { lang = SET_EN.has(x, y) ? 1 : 0; hw::saveInt("lang", lang); }
  else if (SET_SOUND.has(x, y)) { soundOn = !soundOn; hw::saveInt("sound", soundOn); }
  else if (SET_FLIP.has(x, y)) { flip = !flip; hw::saveInt("flip", flip); hw::setFlip(flip); }
  else if (SET_INV.has(x, y)) { invert = !invert; hw::saveInt("inv", invert); hw::setInvert(invert); }
  else if (SET_PAL.has(x, y)) { snd::play(880, 25); go(SCR_PALETTE); return; }
  else if (SET_CAL.has(x, y)) { snd::play(880, 25); calibrate(); }
  else if (SET_BACK.has(x, y)) { snd::play(880, 25); go(SCR_MENU); return; }
  else redraw = false;
  if (redraw) { snd::play(880, 25); drawSettings(); }
}

/* ---------- the marble colours ---------- */

// The sets of marble colours are shown side by side at the size they have in the game, because a cheap panel
// renders colours its own way and only the board itself can settle which set is readable on it.
static const int PAL_N = 4;        // three ready-made sets and the player's own
static const ui::Rect PAL_ROW[PAL_N] = {{8, 60, 224, 36}, {8, 100, 224, 36}, {8, 140, 224, 36}, {8, 180, 224, 36}};
static const ui::Rect PAL_MARKS = {100, 226, 128, 32}, PAL_BACK = {12, 274, 216, 32};

static void drawPaletteRow(int k) {
  const ui::Rect& r = PAL_ROW[k];
  bool on = palette == k;
  uint16_t bg = on ? C_PANEL : C_BG;
  char num[4];
  hw::fillRoundRect(r.x, r.y, r.w, r.h, 6, on ? C_ACCENT : C_LINE);
  hw::fillRoundRect(r.x + 2, r.y + 2, r.w - 4, r.h - 4, 5, bg);
  snprintf(num, sizeof num, "%d", k + 1);
  ui::label(r.x + 4, r.y + 10, 22, 16, num, FONT_S, on ? C_INK : C_MUTED, bg);
  for (int c = 1; c <= 7; c++) lines::drawSample(46 + (c - 1) * 26, r.y + 18, 10, c, k);
}
static void drawPalette() {
  hw::fillRect(0, 0, hw::W, hw::H, C_BG);
  ui::label(0, 4, hw::W, 34, T(S_PAL_TITLE), FONT_L, C_INK, C_BG);
  ui::label(0, 40, hw::W, 18, T(S_PAL_HINT), FONT_S, C_MUTED, C_BG);
  for (int k = 0; k < PAL_N; k++) drawPaletteRow(k);
  ui::label(12, 226, 86, 32, T(S_SET_MARKS), FONT_M, C_MUTED, C_BG, ui::LEFT);
  toggle(PAL_MARKS, T(marks ? S_ON : S_OFF), marks);
  toggle(PAL_BACK, T(S_SETTINGS), false);
}
static void updatePalette() {
  if (!touch.pressed) return;
  int x = touch.x, y = touch.y;
  for (int k = 0; k < PAL_N; k++) if (PAL_ROW[k].has(x, y)) {
    snd::play(880, 25);
    if (palette != k) {
      int was = palette;
      palette = k;
      hw::saveInt("pal", palette);
      drawPaletteRow(was); drawPaletteRow(k);
    }
    if (k >= lines::presetCount()) go(SCR_OWN);     // the last row is the player's own: open it for changing
    return;
  }
  if (PAL_MARKS.has(x, y)) {
    marks = !marks;
    hw::saveInt("marks", marks);
    snd::play(880, 25);
    toggle(PAL_MARKS, T(marks ? S_ON : S_OFF), marks);
    for (int k = 0; k < PAL_N; k++) drawPaletteRow(k);     // the signs come and go with it
    return;
  }
  if (PAL_BACK.has(x, y)) { snd::play(880, 25); go(SCR_SETTINGS); }
}

/* ---------- the player's own set of colours ---------- */

// A marble at the top, then a square from the grid below, and that marble has that colour. The grid is squares
// rather than sliders on purpose: a resistive panel hits a large square every time and a thin slider never.
static const ui::Rect OWN_RESET = {12, 272, 104, 32}, OWN_DONE = {124, 272, 104, 32};
static const int OWN_COLS = 8, OWN_ROWS = 5, OWN_SW = 28, OWN_GX = 8, OWN_GY = 96;
static const int HUES[OWN_COLS] = {0, 30, 55, 120, 175, 220, 275, 320};
static const int LEVEL_S[4] = {100, 100, 100, 50}, LEVEL_V[4] = {100, 72, 48, 100};
static const int GREY_V[OWN_COLS] = {22, 36, 50, 64, 76, 86, 94, 100};   // the bottom row: no colour at all
static int ownPick = 1;              // the marble being changed

static ui::Rect ownCell(int k) { return ui::Rect{15 + k * 30, 56, 30, 30}; }
static ui::Rect ownSwatch(int col, int row) { return ui::Rect{OWN_GX + col * OWN_SW, OWN_GY + row * 30, OWN_SW, 28}; }

// h 0..359, s and v 0..100
static void hsv(int h, int s, int v, int& r, int& g, int& b) {
  int rem = h % 60, p = v * (100 - s) / 100, q = v * (100 - s * rem / 60) / 100,
      t = v * (100 - s * (60 - rem) / 60) / 100, R, G, B;
  switch (h / 60) {
    case 0:  R = v; G = t; B = p; break;
    case 1:  R = q; G = v; B = p; break;
    case 2:  R = p; G = v; B = t; break;
    case 3:  R = p; G = q; B = v; break;
    case 4:  R = t; G = p; B = v; break;
    default: R = v; G = p; B = q; break;
  }
  r = R * 255 / 100; g = G * 255 / 100; b = B * 255 / 100;
}
static void ownSwatchColor(int col, int row, int& r, int& g, int& b) {
  if (row < 4) hsv(HUES[col], LEVEL_S[row], LEVEL_V[row], r, g, b);
  else r = g = b = GREY_V[col] * 255 / 100;
}

static void drawOwnCell(int k) {
  ui::Rect r = ownCell(k);
  bool on = ownPick == k + 1;
  hw::fillRoundRect(r.x, r.y, r.w, r.h, 5, on ? C_ACCENT : C_BG);
  hw::fillRoundRect(r.x + 2, r.y + 2, r.w - 4, r.h - 4, 4, C_BG);
  lines::drawSample(r.x + r.w / 2, r.y + r.h / 2, 10, k + 1, lines::paletteCount() - 1);
}
static void drawOwn() {
  int r, g, b;
  hw::fillRect(0, 0, hw::W, hw::H, C_BG);
  ui::label(0, 2, hw::W, 32, T(S_PAL_OWN), FONT_L, C_INK, C_BG);
  ui::label(0, 36, hw::W, 16, T(S_PAL_OWN_HINT), FONT_S, C_MUTED, C_BG);
  for (int k = 0; k < 7; k++) drawOwnCell(k);
  for (int row = 0; row < OWN_ROWS; row++) for (int col = 0; col < OWN_COLS; col++) {
    ui::Rect s = ownSwatch(col, row);
    ownSwatchColor(col, row, r, g, b);
    hw::fillRect(s.x, s.y, s.w - 1, s.h - 1, RGB(r, g, b));
  }
  ui::button(OWN_RESET.x, OWN_RESET.y, OWN_RESET.w, OWN_RESET.h, T(S_PAL_RESET), FONT_M, C_INK, C_PANEL);
  ui::button(OWN_DONE.x, OWN_DONE.y, OWN_DONE.w, OWN_DONE.h, T(S_PAL_DONE), FONT_M, C_ON_ACCENT, C_ACCENT);
}
static void updateOwn() {
  if (!touch.pressed) return;
  int x = touch.x, y = touch.y, r, g, b;
  for (int k = 0; k < 7; k++) if (ownCell(k).has(x, y)) {
    int was = ownPick;
    ownPick = k + 1;
    snd::play(560, 30);
    drawOwnCell(was - 1); drawOwnCell(k);
    return;
  }
  for (int row = 0; row < OWN_ROWS; row++) for (int col = 0; col < OWN_COLS; col++) {
    if (!ownSwatch(col, row).has(x, y)) continue;
    ownSwatchColor(col, row, r, g, b);
    lines::customSet(ownPick, r, g, b);
    snd::play(880, 25);
    drawOwnCell(ownPick - 1);
    return;
  }
  if (OWN_RESET.has(x, y)) {
    lines::customFromPreset(0);
    snd::play(440, 80);
    for (int k = 0; k < 7; k++) drawOwnCell(k);
    return;
  }
  if (OWN_DONE.has(x, y)) { snd::play(880, 25); go(SCR_PALETTE); }
}

/* ---------- screens ---------- */

void go(Screen s) {
  if (current == SCR_MINES && s != SCR_MINES) mines::leave();
  current = s;
  resetArmedAt = 0; resetDone = false;
  if (s == SCR_MENU) drawMenu();
  else if (s == SCR_SETTINGS) drawSettings();
  else if (s == SCR_PALETTE) drawPalette();
  else if (s == SCR_OWN) drawOwn();
  else if (s == SCR_MINES) mines::enter();
  else if (s == SCR_LINES) lines::enter();
  else bubbles::enter();
}

void setup() {
  hw::begin();
  lang = hw::loadInt("lang", -1);
  soundOn = hw::loadInt("sound", 1) != 0;
  flip = hw::loadInt("flip", 0) != 0;
  invert = hw::loadInt("inv", 0) != 0;
  marks = hw::loadInt("marks", 1) != 0;
  lines::loadCustom();
  palette = hw::loadInt("pal", 0);
  if (palette < 0 || palette >= lines::paletteCount()) palette = 0;
  loadCal();
  hw::setFlip(flip);
  hw::setInvert(invert);

  // Splash. Holding the BOOT button, or a finger on the screen, through it starts the touch calibration:
  // the way back in when a wrong calibration makes the menu unusable.
  hw::fillRect(0, 0, hw::W, hw::H, C_BG);
  ui::label(0, 130, hw::W, 34, "ИГРОТЕКА", FONT_L, C_INK, C_BG);
  ui::label(0, 166, hw::W, 18, "GAME SHELF", FONT_S, C_MUTED, C_BG);
  int held = 0, total = 0, x, y, z;
  bool button = false;
  for (uint32_t start = hw::ms(); hw::ms() - start < 900;) {
    hw::sleepMs(10);
    total++;
    if (hw::touchRaw(x, y, z)) held++;
    if (hw::bootButton()) button = true;
  }
  bool force = button || held * 10 >= total * 9;
  if (!calOk || force) calibrate();
  if (lang != 0 && lang != 1) chooseLanguage();
  hw::log("ready");
  go(SCR_MENU);
}

void loop() {
  pollTouch();
  snd::tick();
  if (current == SCR_MENU) updateMenu();
  else if (current == SCR_SETTINGS) updateSettings();
  else if (current == SCR_PALETTE) updatePalette();
  else if (current == SCR_OWN) updateOwn();
  else if (current == SCR_MINES) mines::update();
  else if (current == SCR_LINES) lines::update();
  else bubbles::update();
  hw::sleepMs(3);
}

#ifdef CYD_SIM
int simScreen() { return (int)current; }
#endif

}  // namespace app

/* ---------- sound: a short queue of notes on the speaker pin ---------- */

namespace snd {

struct Note { int freq, ms; };
static Note queue[8];
static int head = 0, count = 0;
static bool sounding = false;
static uint32_t endAt = 0;

void note(int freq, int ms) {
  if (!app::soundOn || count >= 8) return;
  queue[(head + count) % 8].freq = freq;
  queue[(head + count) % 8].ms = ms;
  count++;
}
void play(int freq, int ms) {
  count = 0;
  if (sounding) { hw::toneOff(); sounding = false; }
  note(freq, ms);
  tick();
}
void tick() {
  uint32_t now = hw::ms();
  if (sounding && (int32_t)(now - endAt) >= 0) { hw::toneOff(); sounding = false; }
  if (!sounding && count) {
    Note n = queue[head];
    head = (head + 1) % 8; count--;
    if (n.freq > 0) hw::toneOn(n.freq);
    sounding = true; endAt = now + n.ms;      // a zero frequency is a rest
  }
}

}  // namespace snd
