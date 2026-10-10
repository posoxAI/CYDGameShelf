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
// what the games keep in memory is gone
static void powerOff() { mines::simPowerOff(); lines::simPowerOff(); bubbles::simPowerOff(); }
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

/* ---------- bubbles rules ---------- */

// The honeycomb is walked here from scratch, out of nothing but the row lengths the game reports, so the
// checks do not lean on the game's own idea of which cells touch which.
static int bCols, bMaxRows;
static const uint8_t* bGrid;
static void bubLoad() { bGrid = bubbles::simGrid(); bCols = bubbles::simCols(); bMaxRows = bubbles::simMaxRows(); }
static int bAt(int r, int c) { return bGrid[r * bCols + c]; }
static bool bShifted(int r) { return bubbles::simRowLen(r) == bCols - 1; }
static int bNb(int r, int c, int* nr, int* nc) {
  int s = bShifted(r) ? 1 : 0, n = 0;
  const int dr[6] = {0, 0, -1, -1, 1, 1}, dc[6] = {-1, 1, s - 1, s, s - 1, s};
  for (int k = 0; k < 6; k++) {
    int rr = r + dr[k], cc = c + dc[k];
    if (rr >= 0 && rr < bMaxRows && cc >= 0 && cc < bubbles::simRowLen(rr)) { nr[n] = rr; nc[n] = cc; n++; }
  }
  return n;
}
static int bTotal() {
  int n = 0;
  for (int r = 0; r < bMaxRows; r++) for (int c = 0; c < bubbles::simRowLen(r); c++) n += bAt(r, c) != 0;
  return n;
}
static int bLowest() {
  for (int r = bMaxRows - 1; r >= 0; r--) for (int c = 0; c < bubbles::simRowLen(r); c++) if (bAt(r, c)) return r;
  return -1;
}
// bubbles that cannot be reached from the top row through their neighbours
static int bFloating() {
  std::vector<uint8_t> seen(bMaxRows * bCols, 0);
  std::vector<int> q;
  for (int c = 0; c < bubbles::simRowLen(0); c++) if (bAt(0, c)) { seen[c] = 1; q.push_back(c); }
  for (size_t k = 0; k < q.size(); k++) {
    int nr[6], nc[6], n = bNb(q[k] / bCols, q[k] % bCols, nr, nc);
    for (int j = 0; j < n; j++) {
      int i = nr[j] * bCols + nc[j];
      if (!seen[i] && bAt(nr[j], nc[j])) { seen[i] = 1; q.push_back(i); }
    }
  }
  int loose = 0;
  for (int r = 0; r < bMaxRows; r++) for (int c = 0; c < bubbles::simRowLen(r); c++) if (bAt(r, c) && !seen[r * bCols + c]) loose++;
  return loose;
}
static bool bPresent(int color) {
  for (int r = 0; r < bMaxRows; r++) for (int c = 0; c < bubbles::simRowLen(r); c++) if (bAt(r, c) == color) return true;
  return false;
}
static void testBubbles() {
  printf("bubbles rules\n");
  bubbles::simAnimate(false);
  int badStart = 0, floating = 0, spare = 0, lostShot = 0, badScore = 0, badMiss = 0, badRow = 0, badColor = 0,
      badEnd = 0, games = 0, shots = 0, pops = 0, rowsAdded = 0, wins = 0, losses = 0, bounced = 0;
  const int FIELD_ROWS = bubbles::simFieldRows(), LY = bubbles::simCannonY();
  for (int g = 0; g < 40; g++) {
    bubbles::simReset(); bubLoad(); games++;
    if (bTotal() != 5 * bCols - 2 || bubbles::simScore() || bFloating()) badStart++;
    for (int step = 0; step < 400 && !bubbles::simOver(); step++) {
      int before = bTotal(), score0 = bubbles::simScore(), miss0 = bubbles::simMisses(), cur = bubbles::simCur();
      if (!bPresent(cur)) badColor++;
      // every fourth shot is aimed almost flat at a wall, so the bounce is used as well
      bool flat = step % 4 == 3;
      int tx = flat ? (int)(hw::rnd() % 2 ? 12 : 228) : 12 + (int)(hw::rnd() % 216);
      int ty = flat ? LY - 6 - (int)(hw::rnd() % 10) : 40 + (int)(hw::rnd() % (uint32_t)(LY - 50));
      if (flat) bounced++;
      bubbles::simAim(tx, ty);
      bubbles::simShoot();
      shots++;
      int after = bTotal(), gain = bubbles::simScore() - score0, gone = before + 1 - after;
      if (after == before) lostShot++;                        // the shot must stick somewhere
      if (bFloating()) floating++;
      for (int r = 0; r < bMaxRows; r++) if (bShifted(r) && bAt(r, bCols - 1)) spare++;
      if (gain) {
        pops++;
        // gone bubbles popped for 10 or fell for 20, so the gain sits between the two and lands on a ten
        if (gone < 3 || gain < 10 * gone || gain > 20 * gone || gain % 10) badScore++;
        if (bubbles::simMisses() != miss0) badMiss++;   // a pop does not forgive the shots that missed
      } else {
        if (gone != 0 && gone != -(bubbles::simRowLen(0))) badScore++;
        // a shot that pops nothing counts; the fifth brings a row down and the count starts again
        if (miss0 + 1 < 5) { if (bubbles::simMisses() != miss0 + 1) badMiss++; }
        else {
          rowsAdded++;
          if (bubbles::simMisses() != 0) badMiss++;
          if (after != before + 1 + bubbles::simRowLen(0)) badRow++;
          for (int c = 0; c < bubbles::simRowLen(0); c++) if (!bAt(0, c)) badRow++;
        }
      }
    }
    if (!bubbles::simOver()) { badEnd++; continue; }
    if (bubbles::simWon()) { wins++; if (bTotal()) badEnd++; }
    else { losses++; if (bLowest() < FIELD_ROWS) badEnd++; }
  }
  bubbles::simAnimate(true);
  check(!badStart, "a new game starts with five full rows, no score and nothing hanging loose");
  check(!lostShot && bounced > 100, "every shot sticks to the field, flat ones off the walls included");
  check(!floating, "a bubble left without support always falls");
  check(!spare, "a shifted row never holds a bubble in the column it does not have");
  check(!badScore && pops > 20, "popping scores ten a bubble and falling twice that");
  check(!badMiss, "a shot that pops nothing counts towards the next row");
  check(!badRow && rowsAdded > 10, "the fifth such shot brings a full new row down from the top");
  check(!badColor, "the cannon is only loaded with a colour that is still on the field");
  check(!badEnd && wins + losses == games, "the game ends with an empty field or a bubble below the line");
  printf("  %d games, %d shots, %d pops, %d rows added, %d cleared, %d lost\n", games, shots, pops, rowsAdded, wins, losses);
}

/* ---------- texts ---------- */

static void testTexts() {
  printf("texts\n");
  int tooWide = 0;
  static const StrId STATUS[] = {S_M_READY, S_M_PLAY, S_M_PLAY_FLAG, S_M_WON, S_M_WON_BEST, S_M_LOST, S_M_CONFIRM, S_L_PROMPT, S_L_PICKED,
                                 S_L_PICK_FIRST, S_L_BLOCKED, S_L_LINE, S_L_LUCKY, S_L_OVER, S_L_OVER_BEST, S_L_CONFIRM, S_FOOT, S_PICK,
                                 S_PAL_HINT, S_PAL_OWN_HINT,
                                 S_B_PROMPT, S_B_POP, S_B_POP_DROP, S_B_ROW, S_B_WON, S_B_WON_BEST, S_B_LOST, S_B_LOST_BEST};
  struct Fit { StrId id; int width; const Font* font; };
  static const Fit BUTTONS[] = {{S_MENU, 64, &FONT_M}, {S_M_NEW, 62, &FONT_M}, {S_M_DIG, 64, &FONT_M}, {S_M_FLAG, 64, &FONT_M}, {S_L_NEW, 143, &FONT_M},
                                {S_MINES, 154, &FONT_M}, {S_LINES, 154, &FONT_M}, {S_BUBBLES, 154, &FONT_M}, {S_SETTINGS, 206, &FONT_M},
                                {S_TITLE, 236, &FONT_L}, {S_SET_LANG, 86, &FONT_M},
                                {S_SET_SOUND, 86, &FONT_M}, {S_SET_SCREEN, 86, &FONT_M}, {S_SET_COLORS, 86, &FONT_M}, {S_SET_MARKS, 86, &FONT_M}, {S_FLIP, 118, &FONT_M},
                                {S_NORMAL, 118, &FONT_M}, {S_INVERTED, 118, &FONT_M}, {S_SET_CAL, 206, &FONT_M}, {S_SET_RESET, 206, &FONT_M},
                                {S_SET_RESET_SURE, 206, &FONT_M}, {S_SET_RESET_DONE, 206, &FONT_M}, {S_L_SCORE, 56, &FONT_S}, {S_L_NEXT, 52, &FONT_S}, {S_BEST, 56, &FONT_S}, {S_B_ROW_IN, 58, &FONT_S},
                                {S_PAL_TITLE, 206, &FONT_M}, {S_PAL_TITLE, 236, &FONT_L}, {S_PAL_OWN, 236, &FONT_L},
                                {S_PAL_RESET, 94, &FONT_M}, {S_PAL_DONE, 94, &FONT_M}};
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
    tap(120, 100); check(app::simScreen() == app::SCR_MINES, "the Minesweeper button opens Minesweeper");
    tap(40, 305); check(app::simScreen() == app::SCR_MENU, "Menu leads back");
    tap(120, 156); check(app::simScreen() == app::SCR_LINES, "the Five in a Line button opens it");
    tap(40, 305);
    tap(120, 212); check(app::simScreen() == app::SCR_BUBBLES, "the Bubbles button opens Bubbles");
    tap(40, 305);
    tap(120, 266); check(app::simScreen() == app::SCR_SETTINGS, "the Settings button opens settings");
    tap(120, 303); check(app::simScreen() == app::SCR_MENU, "settings lead back");
  }

  // Minesweeper by touch: a tap opens, a long press flags, the mode switch swaps them
  firstBoot(7, 0, 0, false);
  tap(120, 100);
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
  tap(40, 305); tap(120, 100);                                 // out to the menu and back: the game must still be there
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
  tap(40, 305); tap(120, 100);
  sim::shot("08-mines-16");
  // win on 9 x 9 and check the best time is kept
  tap(120, 305); run(200); tap(120, 305);
  check(mines::simCols() == 9, "the second press confirms the size change");
  tap(mines::simCellX(40), mines::simCellY(40));
  run(1500);
  for (int i = 0; i < 81; i++) if (!mines::simOpen()[i] && !mines::simMine()[i]) mines::simPrimary(i);
  tap(40, 305); tap(120, 100);
  check(mines::simStatus() == 2 && mines::bestTenths(0) > 0, "a win is kept as the best time");
  sim::shot("09-mines-won");
  tap(40, 305);
  sim::shot("10-menu-with-best");

  // Five in a Line by touch
  tap(120, 156);
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
  tap(40, 305); tap(120, 156);
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
  tap(40, 305); tap(120, 156);
  tap(lines::simCellX(10), lines::simCellY(10)); tap(lines::simCellX(9), lines::simCellY(9));
  check(lines::simOver(), "the game ends when the board fills up");
  sim::shot("16-lines-over");
  tap(40, 305);

  // Bubbles by touch: the finger aims and lifting it takes the shot
  tap(120, 212);
  bubLoad();
  uint8_t* bg = bubbles::simGrid();
  const int BCOLS = bubbles::simCols(), BCELLS = bubbles::simMaxRows() * BCOLS;
  check(bTotal() == 53 && !bubbles::simScore(), "a new game starts with five full rows and no score");
  sim::shot("17-bubbles-start");
  int c0 = bubbles::simCur(), n0 = bubbles::simNext();
  tap(208, 25);
  check(bubbles::simCur() == n0 && bubbles::simNext() == c0, "a tap on Next swaps the loaded bubble with the waiting one");
  // the picture is taken while the finger is still down, so the dotted guide is in it
  sim::shotAt(300, "18-bubbles-aim");
  sim::tapAt(40, 400, 60, 120);
  finishQueue();
  check(bTotal() != 53, "lifting the finger takes the shot and the bubble sticks");

  // Bubbles is the one screen that draws in pieces: a flying bubble, bubbles that pop and bubbles that fall
  // are all rubbed out by repainting the little box they were in. An erase box one pixel short of the thing
  // it erases leaves a trail across the field, and the only way to see that from here is to compare the field
  // against what a full repaint would have drawn.
  std::vector<uint16_t> drawn;
  for (int y = 36; y < 270; y++) for (int x = 10; x < 230; x++) drawn.push_back(sim::pixel(x, y));
  tap(40, 305); tap(120, 212);                 // a trip to the menu and back repaints the screen from scratch
  size_t at = 0;
  int differ = 0;
  for (int y = 36; y < 270; y++) for (int x = 10; x < 230; x++) if (drawn[at++] != sim::pixel(x, y)) differ++;
  check(!differ, "after a shot the field matches a full repaint, so nothing is left drawn on it");

  // three of a colour by hand: the shot goes straight up between two of its own and all three pop
  bubbles::simReset();
  memset(bg, 0, BCELLS);
  bg[4] = bg[5] = 3; bg[0] = 6;
  bubbles::simLoaded(3, 6);
  tap(40, 305); tap(120, 212);
  sim::shot("19-bubbles-before-pop");
  tap(120, 100);
  check(bubbles::simScore() == 30 && bTotal() == 1, "three of a colour pop for ten points each");

  // and the same shot on a field of nothing else clears it
  memset(bg, 0, BCELLS);
  bg[4] = bg[5] = 3;
  bubbles::simLoaded(3, 3);
  tap(40, 305); tap(120, 212);
  tap(120, 100);
  check(bubbles::simOver() && bubbles::simWon(), "an empty field wins the game");

  // a full field has nowhere left but below the red line
  bubbles::simReset();
  for (int r = 0; r < bubbles::simMaxRows(); r++) for (int c = 0; c < BCOLS; c++)
    bg[r * BCOLS + c] = (uint8_t)(r < bubbles::simFieldRows() && c < bubbles::simRowLen(r) ? 1 : 0);
  bubbles::simLoaded(2, 2);
  tap(40, 305); tap(120, 212);
  tap(120, 100);
  check(bubbles::simOver() && !bubbles::simWon() && bLowest() >= bubbles::simFieldRows(), "a bubble below the line loses the game");
  sim::shot("20-bubbles-over");
  tap(40, 305);

  // settings: language, flip (touch must still land), the marble colours, reset
  tap(120, 266);
  sim::shot("21-settings-ru");
  tap(198, 60);
  check(app::lang == 1, "EN switches the language");
  sim::shot("22-settings-en");
  tap(164, 128);
  check(app::flip, "Turn over flips the screen");
  tap(120, 303);
  check(app::simScreen() == app::SCR_MENU, "after the flip the touch still lands where the picture is");
  sim::shot("23-menu-en");
  tap(120, 100); sim::shot("24-mines-en"); tap(40, 305);
  tap(120, 156); sim::shot("25-lines-en"); tap(40, 305);
  tap(120, 212); sim::shot("26-bubbles-en"); tap(40, 305);
  tap(120, 266); tap(164, 128);
  check(!app::flip, "and flips back");

  // sound: silence and three steps, picked out of a row of four
  tap(114, 94);
  check(app::soundLevel == 0, "the first of the sound buttons asks for silence");
  int quiet = sim::tones();
  tap(198, 60);                                                // a tap that would otherwise tick
  check(sim::tones() == quiet && app::lang == 1, "with the sound off nothing is played");
  tap(147, 94);
  check(app::soundLevel == 1 && sim::tones() > quiet && sim::toneLevel() == 1, "the second button plays again, at the quietest step");
  tap(213, 94);
  check(app::soundLevel == 3 && sim::toneLevel() == 3, "and the fourth asks the board for the loudest");
  tap(180, 94);                                                // left in the middle: the restart below must find it there

  // the sets of marble colours and the marks are picked on their own screen
  tap(120, 201);
  check(app::simScreen() == app::SCR_PALETTE, "Marble colours opens the picking screen");
  check(app::palette == 0 && app::marks, "it starts on the first set with the marks on");
  sim::shot("27-palette");
  tap(120, 118);
  check(app::palette == 1, "a tap picks the second set");
  tap(164, 242);
  check(!app::marks, "Marks turns the signs on the marbles off");
  sim::shot("28-palette-second-no-marks");
  tap(120, 290);
  check(app::simScreen() == app::SCR_SETTINGS, "and the screen leads back to settings");
  tap(120, 303); tap(120, 156); sim::shot("29-lines-set-2"); tap(40, 305);
  tap(120, 266); tap(120, 201); tap(120, 158);
  check(app::palette == 2, "the third set can be picked too");
  tap(120, 290); tap(120, 303); tap(120, 156); sim::shot("30-lines-set-3"); tap(40, 305);

  // the fourth row is the player's own set, and picking it opens the screen that changes it
  int r0, g0, b0, r1, g1, b1;
  lines::customGet(2, r0, g0, b0);
  tap(120, 266); tap(120, 201); tap(120, 198);
  check(app::palette == 3 && app::simScreen() == app::SCR_OWN, "the fourth row is the player's own and opens for changing");
  sim::shot("31-own-colours");
  tap(60, 71);                                                 // the second marble
  tap(22, 110);                                                // the first square of the grid: a vivid red
  lines::customGet(2, r1, g1, b1);
  check(r1 == 255 && !g1 && !b1, "a marble takes the colour of the square that is tapped");
  check(r1 != r0 || g1 != g0 || b1 != b0, "which is not the colour it had");
  tap(190, 230);                                               // and a square of the bottom row
  lines::customGet(2, r1, g1, b1);
  check(r1 == g1 && g1 == b1 && r1 > 200, "the bottom row of the grid holds colours with no hue at all");
  sim::shot("32-own-changed");
  tap(176, 288);
  check(app::simScreen() == app::SCR_PALETTE, "Done leads back to the sets");
  tap(120, 290); tap(120, 303); tap(120, 156);
  sim::shot("33-lines-own");                                   // the board is drawn with the changed set
  tap(40, 305);
  // in again, put the set back and return to the first one
  tap(120, 266); tap(120, 201); tap(120, 198);
  tap(64, 288);
  lines::customGet(2, r1, g1, b1);
  check(r1 == r0 && g1 == g0 && b1 == b0, "Reset puts the first set back");
  tap(176, 288);
  tap(120, 78); tap(164, 242);
  check(app::palette == 0 && app::marks, "the first set and the marks come back");
  tap(120, 290);

  tap(120, 269); sim::shot("34-settings-reset-armed"); tap(120, 269);
  check(mines::bestTenths(0) == 0 && lines::best() == 0 && bubbles::best() == 0, "Reset best results clears them after a second press");
  tap(120, 303);

  // power cycle: nothing is asked again, the language is kept
  int tonesBefore = sim::tones();
  check(tonesBefore > 20, "sounds are played");
  powerOff();
  sim::reset(9, true);
  app::setup(); run(100);
  check(app::simScreen() == app::SCR_MENU && app::lang == 1 && app::soundLevel == 2,
        "after a restart the calibration, the language and the sound step are remembered");
  tap(120, 100);
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
  testBubbles();
  testTexts();
  testScreens();
  printf("%d checks passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
