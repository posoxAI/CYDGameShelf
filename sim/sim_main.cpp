// Runs the same game code as the board on a computer: checks the rules, drives the screens with a scripted
// finger and saves pictures of them. Build and run with sim/run.sh.
#include "../src/app.h"
#include "sim.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static int failed = 0, passed = 0;
static void check(bool ok, const char* what) {
  if (ok) passed++; else { failed++; printf("  FAILED: %s\n", what); }
}
static void run(uint32_t ms) { for (uint32_t end = hw::ms() + ms; hw::ms() < end;) app::loop(); }
static void finishQueue() { while (hw::ms() < sim::queuedUntil() + 120) app::loop(); }
static void tap(int x, int y) { sim::tapAt(40, 90, x, y); finishQueue(); }
static void hold(int x, int y, uint32_t ms) { sim::tapAt(40, ms, x, y); finishQueue(); }

static const int CAL_X[4] = {30, 210, 60, 150}, CAL_Y[4] = {44, 150, 284, 226};
static void queueCalibration(uint32_t first) {
  for (int k = 0; k < 4; k++) sim::tapAt(k ? 500 : first, 420, CAL_X[k], CAL_Y[k]);
}
// power on with empty flash: calibration, then the language question
static void powerOff() { mines::simPowerOff(); lines::simPowerOff(); }   // what the games keep in memory is gone
static void firstBoot(uint32_t seed, int panel, int language, bool pictures) {
  powerOff();
  sim::reset(seed, false);
  sim::setPanel(panel);
  if (pictures) sim::shotAt(1200, "01-calibration");
  queueCalibration(1400);
  if (pictures) sim::shotAt(450, "02-language");
  sim::tapAt(700, 100, 120, language ? 207 : 137);
  app::setup();
  run(150);
}

/* ---------- minesweeper rules ---------- */

static int around(int i, int cols, int rows, int* out) {
  int n = 0, x = i % cols, y = i / cols;
  for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
    int nx = x + dx, ny = y + dy;
    if ((dx || dy) && nx >= 0 && ny >= 0 && nx < cols && ny < rows) out[n++] = ny * cols + nx;
  }
  return n;
}
static void testMines() {
  printf("minesweeper rules\n");
  int unsafeFirst = 0, badCount = 0, badNumbers = 0, badFlood = 0, notWon = 0, notLost = 0, wins = 0;
  for (int size = 0; size < 2; size++) for (int g = 0; g < 300; g++) {
    mines::simNew(size);
    int cols = mines::simCols(), rows = mines::simRows(), n = cols * rows, nb[8];
    int first = (int)(hw::rnd() % (uint32_t)n);
    mines::simPrimary(first);
    const uint8_t* mine = mines::simMine(); const uint8_t* adj = mines::simAdj(); const uint8_t* opn = mines::simOpen();
    int total = 0;
    for (int i = 0; i < n; i++) total += mine[i];
    if (total != mines::simMines()) badCount++;
    if (mine[first]) unsafeFirst++;
    int k = around(first, cols, rows, nb);
    for (int j = 0; j < k; j++) if (mine[nb[j]]) unsafeFirst++;
    for (int i = 0; i < n; i++) {
      int c = 0; k = around(i, cols, rows, nb);
      for (int j = 0; j < k; j++) c += mine[nb[j]];
      if (c != adj[i]) badNumbers++;
      if (opn[i] && mine[i]) badFlood++;
      if (opn[i] && !adj[i]) for (int j = 0; j < k; j++) if (!opn[nb[j]]) badFlood++;
    }
    if (g % 2 == 0) {            // open every safe cell: must end in a win with every mine flagged
      for (int i = 0; i < n; i++) if (!mine[i] && !opn[i]) mines::simPrimary(i);
      if (mines::simStatus() != 2 || mines::simFlags() != mines::simMines()) notWon++; else wins++;
    } else {                      // step on a mine: must end in a loss, and nothing may change afterwards
      int m = 0; while (!mine[m]) m++;
      mines::simPrimary(m);
      if (mines::simStatus() != 3) notLost++;
      int safe = 0; while (safe < n && (mine[safe] || opn[safe])) safe++;
      if (safe < n) { mines::simPrimary(safe); if (opn[safe]) notLost++; }
    }
  }
  check(!unsafeFirst, "first opened cell and its neighbours are never mined");
  check(!badCount, "the field holds exactly the stated number of mines");
  check(!badNumbers, "every number equals the mines around it");
  check(!badFlood, "an open empty cell has all its neighbours open, no mine is ever open");
  check(!notWon && wins == 300, "opening every safe cell wins and flags the mines");
  check(!notLost, "a mine ends the game and freezes the field");

  // flags and the tap on a number
  int chordOk = 0, chordIgnored = 0, chordLost = 0, tried = 0;
  for (int g = 0; g < 200 && tried < 60; g++) {
    mines::simNew(1);
    int cols = mines::simCols(), rows = mines::simRows(), n = cols * rows, nb[8];
    mines::simPrimary(120);
    const uint8_t* mine = mines::simMine(); const uint8_t* adj = mines::simAdj(); const uint8_t* opn = mines::simOpen();
    for (int i = 0; i < n; i++) {
      if (!opn[i] || !adj[i]) continue;
      int k = around(i, cols, rows, nb), closedSafe = 0;
      for (int j = 0; j < k; j++) if (!opn[nb[j]] && !mine[nb[j]]) closedSafe++;
      if (!closedSafe) continue;
      tried++;
      int before = 0; for (int j = 0; j < n; j++) before += opn[j];
      mines::simPrimary(i);                                   // no flags yet: nothing may happen
      int after = 0; for (int j = 0; j < n; j++) after += opn[j];
      if (after == before) chordIgnored++;
      if (g % 2 == 0) {
        for (int j = 0; j < k; j++) if (mine[nb[j]]) mines::simSecondary(nb[j]);
        mines::simPrimary(i);
        bool all = true; for (int j = 0; j < k; j++) if (!mine[nb[j]] && !opn[nb[j]]) all = false;
        if (all && mines::simStatus() != 3) chordOk++;
      } else if (adj[i] == 1) {                               // a wrong flag: the tap must set off the mine
        for (int j = 0; j < k; j++) if (!opn[nb[j]] && !mine[nb[j]]) { mines::simSecondary(nb[j]); break; }
        mines::simPrimary(i);
        if (mines::simStatus() == 3) chordLost++;
      } else { chordLost++; }
      break;
    }
  }
  check(tried >= 40 && chordIgnored == tried, "a tap on a number does nothing without the right flags");
  check(chordOk + chordLost == tried, "a tap on a number opens its neighbours, or loses on a wrong flag");
}

/* ---------- five in a line rules ---------- */

static int longestRun(const uint8_t* b) {
  static const int D[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
  int best = 0;
  for (int i = 0; i < 81; i++) if (b[i]) for (int d = 0; d < 4; d++) {
    int x = i % 9, y = i / 9, len = 0;
    while (x >= 0 && x < 9 && y >= 0 && y < 9 && b[y * 9 + x] == b[i]) { len++; x += D[d][0]; y += D[d][1]; }
    if (len > best) best = len;
  }
  return best;
}
static bool reachable(const uint8_t* b, int src, int dst) {
  bool seen[81] = {false}; int q[81], h = 0, t = 0;
  seen[src] = true; q[t++] = src;
  while (h < t) {
    int i = q[h++], x = i % 9, y = i / 9;
    if (i == dst) return true;
    const int nb[4] = {x > 0 ? i - 1 : -1, x < 8 ? i + 1 : -1, y > 0 ? i - 9 : -1, y < 8 ? i + 9 : -1};
    for (int k = 0; k < 4; k++) if (nb[k] >= 0 && !seen[nb[k]] && !b[nb[k]]) { seen[nb[k]] = true; q[t++] = nb[k]; }
  }
  return false;
}
static void testLines() {
  printf("five in a line rules\n");
  int badStart = 0, badGrowth = 0, badScore = 0, leftover = 0, badBlocked = 0, badEnd = 0, games = 0, moves = 0, cleared = 0, blockedTried = 0;
  for (int g = 0; g < 150; g++) {
    lines::simReset(); games++;
    uint8_t* b = lines::simBoard();
    int count = 0; for (int i = 0; i < 81; i++) count += b[i] != 0;
    if (count != 5 || lines::simScore() != 0 || longestRun(b) >= 5) badStart++;
    for (int step = 0; step < 4000 && !lines::simOver(); step++) {
      int src, dst;
      do { src = (int)(hw::rnd() % 81); } while (!b[src]);
      do { dst = (int)(hw::rnd() % 81); } while (b[dst]);
      uint8_t before[81]; memcpy(before, b, 81);
      int n0 = 0; for (int i = 0; i < 81; i++) n0 += b[i] != 0;
      int s0 = lines::simScore();
      bool can = reachable(b, src, dst);
      lines::simTap(src); lines::simTap(dst);
      int n1 = 0; for (int i = 0; i < 81; i++) n1 += b[i] != 0;
      int gain = lines::simScore() - s0;
      if (!can) {
        blockedTried++;
        if (memcmp(before, b, 81) != 0 || gain) badBlocked++;
        lines::simTap(src);                      // put the marble down again
        continue;
      }
      moves++;
      if (b[src] && before[src] && n1 == n0 && !gain && memcmp(before, b, 81) == 0) badBlocked++;   // a legal move did nothing
      if (gain) { cleared++; if (n1 >= n0 + 3) badScore++; }
      else if (n1 != n0 + 3 && !(lines::simOver() && n1 == 81)) badGrowth++;
      if (longestRun(b) >= 5) leftover++;
    }
    int full = 0; for (int i = 0; i < 81; i++) full += b[i] != 0;
    if (!lines::simOver() || full != 81) badEnd++;
  }
  check(!badStart, "a new game starts with five marbles, no score and no ready line");
  check(!badBlocked && blockedTried > 100, "a marble cannot jump over others, and a free path always works");
  check(!badGrowth, "a move without a line brings exactly three new marbles");
  check(!badScore && cleared > 5, "a line scores and brings no new marbles");
  check(!leftover, "no line of five or more is ever left on the board");
  check(!badEnd, "the game ends exactly when the board is full");
  check(lines::simPoints(4) == 0 && lines::simPoints(5) == 10 && lines::simPoints(6) == 12 && lines::simPoints(7) == 18 &&
        lines::simPoints(8) == 28 && lines::simPoints(9) == 42 && lines::simPoints(10) == 58, "points match the browser version");
  printf("  %d games, %d moves, %d lines cleared, %d blocked moves tried\n", games, moves, cleared, blockedTried);
}

/* ---------- texts ---------- */

static void testTexts() {
  printf("texts\n");
  int tooWide = 0;
  static const StrId STATUS[] = {S_M_READY, S_M_PLAY, S_M_PLAY_FLAG, S_M_WON, S_M_WON_BEST, S_M_LOST, S_M_CONFIRM, S_L_PROMPT, S_L_PICKED,
                                 S_L_PICK_FIRST, S_L_BLOCKED, S_L_LINE, S_L_LUCKY, S_L_OVER, S_L_OVER_BEST, S_L_CONFIRM, S_FOOT, S_PICK};
  struct Fit { StrId id; int width; const Font* font; };
  static const Fit BUTTONS[] = {{S_MENU, 64, &FONT_M}, {S_M_NEW, 62, &FONT_M}, {S_M_DIG, 64, &FONT_M}, {S_M_FLAG, 64, &FONT_M}, {S_L_NEW, 143, &FONT_M},
                                {S_MINES, 154, &FONT_M}, {S_LINES, 154, &FONT_M}, {S_SETTINGS, 206, &FONT_M}, {S_TITLE, 236, &FONT_L}, {S_SET_LANG, 86, &FONT_M},
                                {S_SET_SOUND, 86, &FONT_M}, {S_SET_SCREEN, 86, &FONT_M}, {S_SET_COLORS, 86, &FONT_M}, {S_SET_MARKS, 86, &FONT_M}, {S_FLIP, 118, &FONT_M},
                                {S_NORMAL, 118, &FONT_M}, {S_INVERTED, 118, &FONT_M}, {S_SET_CAL, 206, &FONT_M}, {S_SET_RESET, 206, &FONT_M},
                                {S_SET_RESET_SURE, 206, &FONT_M}, {S_SET_RESET_DONE, 206, &FONT_M}, {S_L_SCORE, 76, &FONT_S}, {S_L_NEXT, 72, &FONT_S}, {S_BEST, 76, &FONT_S}};
  for (app::lang = 0; app::lang < 2; app::lang++) {
    for (size_t k = 0; k < sizeof STATUS / sizeof STATUS[0]; k++) {
      char text[96];
      const char* f = T(STATUS[k]);
      const char* firstD = strstr(f, "%d");
      bool two = firstD && strstr(firstD + 2, "%d");
      app::fillText(text, sizeof text, f, "99:59.9", two ? 13 : 12345, 106);   // a line of 13 is the longest possible
      int w = ui::textWidth(text, FONT_S);
      if (w > 236) { tooWide++; printf("  too wide (%d px): %s\n", w, text); }
    }
    for (size_t k = 0; k < sizeof BUTTONS / sizeof BUTTONS[0]; k++) {
      int w = ui::textWidth(T(BUTTONS[k].id), *BUTTONS[k].font);
      if (w > BUTTONS[k].width) { tooWide++; printf("  too wide (%d of %d px): %s\n", w, BUTTONS[k].width, T(BUTTONS[k].id)); }
    }
  }
  app::lang = 0;
  check(!tooWide, "every text fits its place in both languages");
}

/* ---------- the screens, driven by touch ---------- */

static void testScreens() {
  printf("screens\n");
  // three differently mounted touch panels must all calibrate and then hit the menu buttons
  for (int panel = 0; panel < 3; panel++) {
    firstBoot(100 + panel, panel, 0, panel == 0);
    check(app::simScreen() == app::SCR_MENU, "boot ends in the menu");
    check(sim::logText().find("ready") != std::string::npos, "calibration succeeded on the first try");
    if (panel == 0) { sim::shot("03-menu-ru"); printf("%s", sim::logText().c_str()); }
    tap(120, 107); check(app::simScreen() == app::SCR_MINES, "the Minesweeper button opens Minesweeper");
    tap(40, 305); check(app::simScreen() == app::SCR_MENU, "Menu leads back");
    tap(120, 173); check(app::simScreen() == app::SCR_LINES, "the Five in a Line button opens it");
    tap(40, 305);
    tap(120, 236); check(app::simScreen() == app::SCR_SETTINGS, "the Settings button opens settings");
    tap(120, 298); check(app::simScreen() == app::SCR_MENU, "settings lead back");
  }

  // Minesweeper by touch: a tap opens, a long press flags, the mode switch swaps them
  firstBoot(7, 0, 0, false);
  tap(120, 107);
  sim::shot("04-mines-start");
  tap(mines::simCellX(40), mines::simCellY(40));
  check(mines::simStatus() == 1 && mines::simOpen()[40], "a tap opens a cell and starts the game");
  int closed = -1, mined = -1;
  for (int i = 0; i < 81; i++) { if (!mines::simOpen()[i] && mines::simMine()[i] && mined < 0) mined = i; if (!mines::simOpen()[i] && !mines::simMine()[i] && closed < 0) closed = i; }
  hold(mines::simCellX(mined), mines::simCellY(mined), 700);
  check(mines::simFlag()[mined] && !mines::simOpen()[mined] && mines::simStatus() == 1, "a long press plants a flag and does not open the cell");
  tap(mines::simCellX(mined), mines::simCellY(mined));
  check(mines::simFlag()[mined] && mines::simStatus() == 1, "a flagged cell does not open on a tap");
  tap(199, 305);                                               // Dig -> Flag
  if (closed >= 0) {
    tap(mines::simCellX(closed), mines::simCellY(closed));
    check(mines::simFlag()[closed] && !mines::simOpen()[closed], "in flag mode a tap plants a flag");
    tap(mines::simCellX(closed), mines::simCellY(closed));
    check(!mines::simFlag()[closed], "and a second tap removes it");
  }
  tap(199, 305);                                               // back to Dig
  // open a few more safe cells for a fuller picture
  for (int i = 0, done = 0; i < 81 && done < 6; i++) if (!mines::simOpen()[i] && !mines::simMine()[i]) { mines::simPrimary(i); done++; }
  tap(40, 305); tap(120, 107);                                 // out to the menu and back: the game must still be there
  check(mines::simStatus() == 1 && mines::simOpen()[40], "the game survives a trip to the menu");
  run(2100);
  sim::shot("05-mines-play");
  tap(120, 305);                                               // size button during a game: asks first
  check(mines::simCols() == 9, "changing the size mid-game asks first");
  sim::shot("06-mines-confirm");
  run(3800);
  // lose
  for (int i = 0; i < 81; i++) if (mines::simMine()[i] && !mines::simFlag()[i]) { tap(mines::simCellX(i), mines::simCellY(i)); break; }
  check(mines::simStatus() == 3, "tapping a mine loses");
  sim::shot("07-mines-lost");
  tap(120, 18);                                                // New
  check(mines::simStatus() == 0, "New starts a fresh game");
  tap(120, 305);                                               // size: no game running, switches at once
  check(mines::simCols() == 16, "the size button switches to 16 x 16");
  tap(mines::simCellX(120), mines::simCellY(120));
  for (int i = 0, done = 0; i < 256 && done < 25; i++) if (!mines::simOpen()[i] && !mines::simMine()[i]) { mines::simPrimary(i); done++; }
  for (int i = 0, done = 0; i < 256 && done < 9; i++) if (mines::simMine()[i]) { mines::simSecondary(i); done++; }
  tap(40, 305); tap(120, 107);
  sim::shot("08-mines-16");
  // win on 9 x 9 and check the best time is kept
  tap(120, 305); run(200); tap(120, 305);
  check(mines::simCols() == 9, "the second press confirms the size change");
  tap(mines::simCellX(40), mines::simCellY(40));
  run(1500);
  for (int i = 0; i < 81; i++) if (!mines::simOpen()[i] && !mines::simMine()[i]) mines::simPrimary(i);
  tap(40, 305); tap(120, 107);
  check(mines::simStatus() == 2 && mines::bestTenths(0) > 0, "a win is kept as the best time");
  sim::shot("09-mines-won");
  tap(40, 305);
  sim::shot("10-menu-with-best");

  // Five in a Line by touch
  tap(120, 173);
  sim::shot("11-lines-start");
  uint8_t* b = lines::simBoard();
  int src = -1, dst = -1;
  for (int i = 0; i < 81 && src < 0; i++) if (b[i]) for (int j = 0; j < 81; j++) if (!b[j] && reachable(b, i, j)) { src = i; dst = j; break; }
  tap(lines::simCellX(src), lines::simCellY(src));
  check(lines::simSel() == src, "a tap picks a marble");
  sim::shot("12-lines-picked");
  int color = b[src];
  tap(lines::simCellX(dst), lines::simCellY(dst));
  check(b[dst] == color && !b[src] && lines::simSel() < 0, "a tap on a free cell moves it there");
  int count = 0; for (int i = 0; i < 81; i++) count += b[i] != 0;
  check(count == 8, "three new marbles arrive after the move");
  // build a line by hand: four in a row plus the mover
  memset(b, 0, 81);
  b[9 * 4 + 0] = b[9 * 4 + 1] = b[9 * 4 + 2] = b[9 * 4 + 3] = 3; b[9 * 6 + 4] = 3; b[0] = 1; b[8] = 2; b[80] = 5; b[72] = 6; b[44] = 7; b[13] = 4;
  tap(40, 305); tap(120, 173);
  sim::shot("13-lines-before-line");
  tap(lines::simCellX(9 * 6 + 4), lines::simCellY(9 * 6 + 4));
  tap(lines::simCellX(9 * 4 + 4), lines::simCellY(9 * 4 + 4));
  check(lines::simScore() == 10 && !b[9 * 4 + 2], "five in a row disappear for 10 points");
  count = 0; for (int i = 0; i < 81; i++) count += b[i] != 0;
  check(count == 6, "and no new marbles arrive after a line");
  sim::shot("14-lines-after-line");
  tap(160, 305);                                               // New game with points: asks first
  check(lines::simScore() == 10, "New game asks before dropping a score");
  sim::shot("15-lines-confirm");
  // fill the board to see the end of a game
  for (int i = 0; i < 81; i++) b[i] = (uint8_t)(1 + (i * 3 + i / 9 * 2) % 7);
  b[0] = 0; b[1] = 0; b[9] = 0;
  run(3800);
  tap(40, 305); tap(120, 173);
  tap(lines::simCellX(10), lines::simCellY(10)); tap(lines::simCellX(9), lines::simCellY(9));
  check(lines::simOver(), "the game ends when the board fills up");
  sim::shot("16-lines-over");
  tap(40, 305);

  // settings: language, flip (touch must still land), the marks on the marbles, reset
  tap(120, 236);
  sim::shot("17-settings-ru");
  tap(198, 56);
  check(app::lang == 1, "EN switches the language");
  sim::shot("18-settings-en");
  tap(164, 124);
  check(app::flip, "Turn over flips the screen");
  tap(120, 300);
  check(app::simScreen() == app::SCR_MENU, "after the flip the touch still lands where the picture is");
  sim::shot("19-menu-en");
  tap(120, 107); sim::shot("20-mines-en"); tap(40, 305);
  tap(120, 173); sim::shot("21-lines-en"); tap(40, 305);
  tap(120, 236); tap(164, 124);
  check(!app::flip, "and flips back");
  check(app::marks, "the marks on the marbles start on");
  tap(164, 192);
  check(!app::marks, "Marks turns the signs on the marbles off");
  tap(120, 300); tap(120, 173); sim::shot("22-lines-no-marks"); tap(40, 305);
  tap(120, 236); tap(164, 192);
  check(app::marks, "and on again");
  tap(120, 262); sim::shot("23-settings-reset-armed"); tap(120, 262);
  check(mines::bestTenths(0) == 0 && lines::best() == 0, "Reset best results clears them after a second press");
  tap(120, 300);

  // power cycle: nothing is asked again, the language is kept
  int tonesBefore = sim::tones();
  check(tonesBefore > 20, "sounds are played");
  powerOff();
  sim::reset(9, true);
  app::setup(); run(100);
  check(app::simScreen() == app::SCR_MENU && app::lang == 1, "after a restart the calibration and the language are remembered");
  tap(120, 107);
  check(app::simScreen() == app::SCR_MINES, "and the stored calibration still works");

  // a finger held on the screen through the splash forces a new calibration
  powerOff();
  sim::reset(11, true);
  sim::tapAt(0, 1000, 120, 160);
  queueCalibration(800);
  app::setup(); run(100);
  check(sim::logText().find("cal point 4") != std::string::npos && app::simScreen() == app::SCR_MENU, "holding the screen at power-on starts calibration");
}

int main(int argc, char** argv) {
  if (argc > 1) sim::outDir = argv[1];
  sim::reset(12345, false);
  app::lang = 0;
  testMines();
  testLines();
  testTexts();
  testScreens();
  printf("%d checks passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
