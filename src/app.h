// The shell around the games: touch, sound, settings, the menu and switching between screens.
#pragma once
#include <stdint.h>
#include "hw.h"
#include "ui.h"

// Every text on the device, in Russian and in English.
enum StrId {
  S_TITLE, S_PICK, S_MINES, S_LINES, S_SETTINGS, S_FOOT,
  S_MENU, S_BEST, S_NO_BEST,
  // minesweeper
  S_M_NEW, S_M_DIG, S_M_FLAG, S_M_READY, S_M_PLAY, S_M_PLAY_FLAG, S_M_WON, S_M_WON_BEST, S_M_LOST, S_M_CONFIRM,
  // five in a line
  S_L_SCORE, S_L_NEXT, S_L_NEW, S_L_PROMPT, S_L_PICKED, S_L_PICK_FIRST, S_L_BLOCKED, S_L_LINE, S_L_LUCKY,
  S_L_OVER, S_L_OVER_BEST, S_L_CONFIRM,
  // settings
  S_SET_LANG, S_SET_SOUND, S_SET_SCREEN, S_SET_COLORS, S_ON, S_OFF, S_FLIP, S_NORMAL, S_INVERTED,
  S_SET_CAL, S_SET_RESET, S_SET_RESET_SURE, S_SET_RESET_DONE,
  S_COUNT
};
const char* T(StrId id);

namespace app {

enum Screen { SCR_MENU, SCR_MINES, SCR_LINES, SCR_SETTINGS };

extern int lang;             // 0 Russian, 1 English
extern bool soundOn, flip, invert;

// What the finger is doing. `pressed`, `released` and `longPress` are true for one pass of the loop only.
struct Touch {
  bool down, pressed, released, longPress;
  bool wasLong;              // the press that has just ended had already fired longPress
  int x, y;                  // where, in screen pixels
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
}
