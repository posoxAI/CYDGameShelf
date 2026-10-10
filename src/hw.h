// Everything the games need from the board, and nothing else.
// hw_esp32.cpp implements it for the Cheap Yellow Display; sim/hw_host.cpp implements it on a computer for tests.
#pragma once
#include <stdint.h>

namespace hw {

const int W = 240, H = 320;        // the screen is used upright

void begin();
void setFlip(bool flip);           // turn the picture by 180 degrees
void setInvert(bool invert);       // for panels that show colours inverted

void fillRect(int x, int y, int w, int h, uint16_t color);
void fillRoundRect(int x, int y, int w, int h, int r, uint16_t color);
void fillCircle(int x, int y, int r, uint16_t color);
void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color);
void drawLine(int x0, int y0, int x1, int y1, uint16_t color);
void blit(int x, int y, int w, int h, const uint16_t* pixels);   // w*h colours, row by row

// Cuts everything drawn after it to this box, so a list can be scrolled and the row half out of it
// stops at the edge instead of running over the screen above. clearClip() gives the whole screen back.
void setClip(int x, int y, int w, int h);
void clearClip();

// Raw touch panel readings, about 0..4095 each. Returns false while nothing presses the screen.
bool touchRaw(int& rx, int& ry, int& rz);
bool bootButton();                 // the BOOT button on the board is held down

uint32_t ms();
void sleepMs(uint32_t t);

void toneOn(int freq, int level);  // level 1, 2 or 3: how loud the board should play it
void toneOff();

// Small numbers kept in flash under short names (up to 15 characters).
int32_t loadInt(const char* key, int32_t fallback);
void saveInt(const char* key, int32_t value);

uint32_t rnd();
void log(const char* text);        // a line on the USB serial port

}  // namespace hw
