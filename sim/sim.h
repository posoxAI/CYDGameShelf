// The board, pretended on a computer: a picture in memory instead of the screen, a script instead of a finger.
#pragma once
#include <stdint.h>
#include <string>

namespace sim {
void reset(uint32_t seed, bool keepStorage);          // a fresh power-on
void tapAt(uint32_t afterMs, uint32_t holdMs, int x, int y);   // queue a touch at a screen position, after the previous one ends
void shotAt(uint32_t afterMs, const char* name);      // queue a screenshot, counted from the end of the last queued touch
void shot(const char* name);                          // a screenshot now
uint16_t pixel(int x, int y);                         // what is on the screen at that spot
uint32_t queuedUntil();                               // when the last queued touch ends
void setPanel(int kind);                              // how the pretend touch panel is glued on: 0, 1 or 2
int tones();                                          // how many notes have been started
int toneLevel();                                      // how loud the last of them was asked to be
std::string logText();
extern std::string outDir;
}

// test views exported by the game files when CYD_SIM is defined
namespace app { int simScreen(); }
namespace mines {
const uint8_t* simMine(); const uint8_t* simAdj(); const uint8_t* simOpen(); const uint8_t* simFlag();
int simCols(); int simRows(); int simMines(); int simStatus(); int simFlags(); int simCellX(int i); int simCellY(int i);
void simNew(int idx); void simPrimary(int i); void simSecondary(int i); void simPowerOff();
}
namespace lines {
uint8_t* simBoard(); int simScore(); bool simOver(); int simSel(); int simPoints(int n); void simReset(); void simTap(int i);
int simCellX(int i); int simCellY(int i); void simPowerOff();
}
namespace bubbles {
uint8_t* simGrid(); int simCols(); int simMaxRows(); int simFieldRows(); int simRowLen(int r);
int simScore(); int simMisses(); int simCur(); int simNext(); bool simOver(); bool simWon();
void simReset(); void simLoaded(int a, int b); void simAnimate(bool on); void simAim(int x, int y); void simShoot(); void simPowerOff();
int simCannonY();
}
namespace bricks {
void simReset(); void simAnimate(bool on); void simNoDrops(bool on); void simStep(int ticks); void simLaunch();
void simPaddleTo(int x); void simDrop(int kind, int x, int y); void simPowerOff();
int simPaddleX(); int simPaddleW(); int simBalls(); int simBallX(int k); int simBallY(int k); bool simStuck(int k);
int simSpeed(); int simScore(); int simLives(); int simLevel(); int simState(); int simCapsules(); int simBricks(int kind);
int simLeft(); int simRight(); int simTop(); int simFloor(); int simPaddleY(); int simBallR(); int simFieldY();
}
