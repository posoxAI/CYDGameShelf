// The shell around the games: touch, sound, settings, the menu and switching between screens.
#pragma once
#include <stdint.h>
#include "hw.h"
#include "ui.h"

// Every text on the device, in Russian and in English.
enum StrId {
  S_TITLE, S_PICK, S_MINES, S_LINES, S_BUBBLES, S_BRICKS, S_G2048, S_SETTINGS, S_FOOT,
  S_MENU, S_BEST, S_NO_BEST,
  // minesweeper
  S_M_NEW, S_M_DIG, S_M_FLAG, S_M_READY, S_M_PLAY, S_M_PLAY_FLAG, S_M_WON, S_M_WON_BEST, S_M_LOST, S_M_CONFIRM,
  // five in a line
  S_L_SCORE, S_L_NEXT, S_L_NEW, S_L_PROMPT, S_L_PICKED, S_L_PICK_FIRST, S_L_BLOCKED, S_L_LINE, S_L_LUCKY,
  S_L_OVER, S_L_OVER_BEST, S_L_CONFIRM,
  // bubbles
  S_B_ROW_IN, S_B_PROMPT, S_B_POP, S_B_POP_DROP, S_B_ROW, S_B_WON, S_B_WON_BEST, S_B_LOST, S_B_LOST_BEST,
  // bricks
  S_K_LEVEL, S_K_BALLS, S_K_READY, S_K_PLAY, S_K_PAUSE, S_K_LOST, S_K_CLEAR,
  S_K_WIDE, S_K_SLOW, S_K_MULTI, S_K_LIFE, S_K_OVER, S_K_OVER_BEST,
  // 2048
  S_G_UNDO, S_G_START, S_G_MOVED, S_G_STUCK, S_G_UNDONE, S_G_NOUNDO, S_G_WIN, S_G_OVER, S_G_OVER_BEST,
  // settings
  S_SET_LANG, S_SET_SOUND, S_SET_SCREEN, S_SET_COLORS, S_SET_MARKS, S_ON, S_OFF, S_FLIP, S_NORMAL, S_INVERTED,
  S_SET_CAL, S_SET_RESET, S_SET_RESET_SURE, S_SET_RESET_DONE,
  // picking the marble colours
  S_PAL_TITLE, S_PAL_HINT, S_PAL_OWN, S_PAL_OWN_HINT, S_PAL_RESET, S_PAL_DONE,
  S_COUNT
};
const char* T(StrId id);

namespace app {

enum Screen { SCR_MENU, SCR_MINES, SCR_LINES, SCR_BUBBLES, SCR_BRICKS, SCR_2048, SCR_SETTINGS, SCR_PALETTE, SCR_OWN };

extern int lang;             // 0 Russian, 1 English
extern int soundLevel;       // 0 silent, then 1 quiet, 2 and 3 loudest
extern bool flip, invert;
extern bool marks;           // a little sign inside every marble, for telling the colours apart
extern int palette;          // which set of marble colours, picked on the device by the Marble colours screen

// What the finger is doing. `pressed`, `released` and `longPress` are true for one pass of the loop only.
struct Touch {
  bool down, pressed, released, longPress;
  bool wasLong;              // the press that has just ended had already fired longPress
  int x, y;                  // where, in screen pixels
  int liveX, liveY;          // where the finger is now: unlike x and y these follow it while it is down
  uint32_t downAt;
};
extern Touch touch;

void setup();
void loop();
void go(Screen s);
void wait(uint32_t ms);      // a pause that keeps the sound going
void pollTouch();
void calibrate();            // the four-cross touch calibration; returns when it has succeeded
// Copies `pattern` to `out`, putting `word` in place of %s and the numbers in place of the first and second %d.
void fillText(char* out, int size, const char* pattern, const char* word, int a, int b);

}  // namespace app

namespace snd {
void note(int freq, int ms);         // adds a note to the queue
void play(int freq, int ms);         // drops what is queued and plays this
void tick();
}

namespace mines {
void enter();
void update();
void leave();
int bestTenths(int size);            // 0 when there is no best time yet
void resetBest();
void formatTime(int tenths, char* out, int outSize);
}

namespace lines {
void enter();
void update();
int best();
void resetBest();
uint16_t ballColor(int c);           // colour 1..7 of a marble, so the menu icon matches the game
uint16_t ballShade(int c, int percent);   // the same colour darker or lighter: 100 is itself
int paletteCount();                  // sets to choose from; the last one is the player's own
int presetCount();                   // of those, the ready-made ones
void drawSample(int cx, int cy, int r, int c, int pal);   // one marble of any set, for the picking screen
void loadCustom();                   // reads the player's own set out of flash
void customGet(int c, int& r, int& g, int& b);
void customSet(int c, int r, int g, int b);               // keeps it in flash straight away
void customFromPreset(int pal);
}

namespace bubbles {
void enter();
void update();
int best();
void resetBest();
}

namespace bricks {
void enter();
void update();
int best();
void resetBest();
void drawIcon(int x, int y);         // the little wall with a ball, for the menu
}

namespace g2048 {
void enter();
void update();
int best();
void resetBest();
void drawIcon(int x, int y);         // four little tiles, for the menu
}
