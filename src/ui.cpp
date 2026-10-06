#include "ui.h"
#include "hw.h"

namespace ui {

static const Glyph* findGlyph(const Font& font, uint32_t code) {
  int lo = 0, hi = font.count - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    if (font.glyphs[mid].code == code) return &font.glyphs[mid];
    if (font.glyphs[mid].code < code) lo = mid + 1; else hi = mid - 1;
  }
  return 0;
}

// Reads one character of UTF-8 text and moves the pointer past it.
static uint32_t nextCode(const char*& s) {
  uint8_t c = (uint8_t)*s++;
  if (c < 0x80) return c;
  if ((c & 0xE0) == 0xC0 && (s[0] & 0xC0) == 0x80) { uint32_t v = ((c & 0x1F) << 6) | (s[0] & 0x3F); s += 1; return v; }
  if ((c & 0xF0) == 0xE0 && (s[0] & 0xC0) == 0x80 && (s[1] & 0xC0) == 0x80) {
    uint32_t v = ((c & 0x0F) << 12) | ((s[0] & 0x3F) << 6) | (s[1] & 0x3F); s += 2; return v;
  }
  return '?';
}

int textWidth(const char* utf8, const Font& font) {
  int w = 0;
  while (*utf8) {
    const Glyph* g = findGlyph(font, nextCode(utf8));
    if (g) w += g->adv;
  }
  return w;
}

// Sixteen steps from the background colour to the text colour, for smooth letter edges.
static void makeRamp(uint16_t fg, uint16_t bg, uint16_t* ramp) {
  int fr = fg >> 11, fg6 = (fg >> 5) & 63, fb = fg & 31;
  int br = bg >> 11, bg6 = (bg >> 5) & 63, bb = bg & 31;
  for (int a = 0; a < 16; a++) {
    int r = br + (fr - br) * a / 15, g = bg6 + (fg6 - bg6) * a / 15, b = bb + (fb - bb) * a / 15;
    ramp[a] = (uint16_t)((r << 11) | (g << 5) | b);
  }
}

// The text is drawn into this strip first and sent to the screen in one piece, so it never flickers.
static const int STRIP_MAX = 240 * 34;
static uint16_t strip[STRIP_MAX];

static void drawRun(int w, int h, int penX, int top, const char* utf8, const Font& font, const uint16_t* ramp) {
  while (*utf8) {
    const Glyph* g = findGlyph(font, nextCode(utf8));
    if (!g) continue;
    const uint8_t* src = font.pixels + g->off;
    for (int gy = 0; gy < g->h; gy++) {
      int y = top + g->yo + gy;
      for (int gx = 0; gx < g->w; gx++) {
        int n = gy * g->w + gx;
        int a = (n & 1) ? (src[n >> 1] & 15) : (src[n >> 1] >> 4);
        int x = penX + g->xo + gx;
        if (a && x >= 0 && x < w && y >= 0 && y < h) strip[y * w + x] = ramp[a];
      }
    }
    penX += g->adv;
  }
}

void label(int x, int y, int w, int h, const char* utf8, const Font& font, uint16_t fg, uint16_t bg, Align align) {
  if (w <= 0 || h <= 0) return;
  uint16_t ramp[16];
  makeRamp(fg, bg, ramp);
  int tw = textWidth(utf8, font);
  int penX = align == LEFT ? 0 : (align == RIGHT ? w - tw : (w - tw) / 2);
  if (penX < 0) penX = 0;
  int top = (h - font.height) / 2;
  if (w * h <= STRIP_MAX) {
    for (int k = 0; k < w * h; k++) strip[k] = bg;
    drawRun(w, h, penX, top, utf8, font, ramp);
    hw::blit(x, y, w, h, strip);
    return;
  }
  // a tall box: paint it, then send only the line of text
  hw::fillRect(x, y, w, h, bg);
  int lh = font.height;
  if (w * lh > STRIP_MAX) return;
  for (int k = 0; k < w * lh; k++) strip[k] = bg;
  drawRun(w, lh, penX, 0, utf8, font, ramp);
  hw::blit(x, y + top, w, lh, strip);
}

void button(int x, int y, int w, int h, const char* utf8, const Font& font, uint16_t fg, uint16_t bg) {
  hw::fillRoundRect(x, y, w, h, 5, bg);
  label(x + 5, y + 2, w - 10, h - 4, utf8, font, fg, bg, CENTER);
}

}  // namespace ui
