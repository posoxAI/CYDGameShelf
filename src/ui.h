// Colours, text and buttons shared by every screen.
#pragma once
#include <stdint.h>

#define RGB(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

// One dark palette for the whole device.
const uint16_t C_BG = RGB(16, 22, 29);
const uint16_t C_PANEL = RGB(30, 41, 54);
const uint16_t C_LINE = RGB(50, 66, 82);
const uint16_t C_INK = RGB(232, 238, 243);
const uint16_t C_MUTED = RGB(146, 160, 174);
const uint16_t C_ACCENT = RGB(79, 176, 240);
const uint16_t C_ON_ACCENT = RGB(6, 26, 40);
const uint16_t C_DANGER = RGB(255, 107, 94);
const uint16_t C_GOOD = RGB(95, 208, 138);

struct Glyph {
  uint16_t code;      // Unicode code point
  uint8_t w, h;       // size of the picture
  int8_t xo, yo;      // where the picture sits: right of the pen, below the top of the line
  uint8_t adv;        // how far the pen moves
  uint16_t off;       // where the picture starts in the pixel data
};
struct Font {
  const Glyph* glyphs;
  const uint8_t* pixels;   // 4 bits per pixel, two pixels per byte
  uint16_t count;
  uint8_t height;          // line height
  uint8_t ascent;
};
extern const Font FONT_S;  // 12 px, status lines and notes
extern const Font FONT_M;  // 15 px bold, buttons and numbers
extern const Font FONT_L;  // 26 px bold, titles
extern const Font FONT_D;  // 11 px bold, digits only, for small cells

namespace ui {

enum Align { LEFT, CENTER, RIGHT };

int textWidth(const char* utf8, const Font& font);

// Fills the box w x h with `bg` and writes the text into it on one line. Text that is too long is cut at the box edge.
void label(int x, int y, int w, int h, const char* utf8, const Font& font, uint16_t fg, uint16_t bg, Align align = CENTER);

// A rounded button with a caption.
void button(int x, int y, int w, int h, const char* utf8, const Font& font, uint16_t fg, uint16_t bg);

struct Rect {
  int x, y, w, h;
  bool has(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; }
};

}  // namespace ui
