#include "../src/hw.h"
#include "sim.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <map>
#include <vector>
#include <algorithm>

namespace sim {
std::string outDir = ".";
static uint16_t fb[hw::W * hw::H];
static uint32_t now = 0, rng = 1;
static bool flipped = false, inverted = false;
static int panel = 0, toneCount = 0;
static std::map<std::string, int32_t> store;
static std::string logged;
struct Tap { uint32_t from, to; int x, y; };
struct Shot { uint32_t at; std::string name; };
static std::vector<Tap> taps;
static std::vector<Shot> shots;
static uint32_t lastEnd = 0;

void reset(uint32_t seed, bool keepStorage) {
  now = 0; rng = seed ? seed : 1; flipped = false; inverted = false; toneCount = 0; lastEnd = 0;
  taps.clear(); shots.clear(); logged.clear();
  if (!keepStorage) store.clear();
  for (int i = 0; i < hw::W * hw::H; i++) fb[i] = 0;
}
void tapAt(uint32_t afterMs, uint32_t holdMs, int x, int y) {
  uint32_t from = std::max(lastEnd, now) + afterMs;
  taps.push_back({from, from + holdMs, x, y});
  lastEnd = from + holdMs;
}
void shotAt(uint32_t afterMs, const char* name) { shots.push_back({std::max(lastEnd, now) + afterMs, name}); }
uint32_t queuedUntil() { return lastEnd; }
void setPanel(int kind) { panel = kind; }
int tones() { return toneCount; }
std::string logText() { return logged; }
void shot(const char* name) {
  std::string path = outDir + "/" + name + ".ppm";
  FILE* f = fopen(path.c_str(), "wb");
  if (!f) return;
  fprintf(f, "P6\n%d %d\n255\n", hw::W, hw::H);
  for (int i = 0; i < hw::W * hw::H; i++) {
    uint16_t c = inverted ? (uint16_t)~fb[i] : fb[i];
    unsigned char px[3] = {(unsigned char)((c >> 11) * 255 / 31), (unsigned char)(((c >> 5) & 63) * 255 / 63), (unsigned char)((c & 31) * 255 / 31)};
    fwrite(px, 1, 3, f);
  }
  fclose(f);
}
static void advance(uint32_t t) {
  now += t;
  for (size_t k = 0; k < shots.size();) {
    if (shots[k].at <= now) { shot(shots[k].name.c_str()); shots.erase(shots.begin() + k); } else k++;
  }
}
}  // namespace sim

namespace hw {
using namespace sim;

static void px(int x, int y, uint16_t c) { if (x >= 0 && y >= 0 && x < W && y < H) fb[y * W + x] = c; }

void begin() {}
void setFlip(bool flip) { flipped = flip; }
void setInvert(bool invert) { inverted = invert; }
void fillRect(int x, int y, int w, int h, uint16_t c) { for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) px(x + i, y + j, c); }
void fillCircle(int cx, int cy, int r, uint16_t c) {
  for (int dy = -r; dy <= r; dy++) { int dx = (int)std::floor(std::sqrt((double)(r * r - dy * dy)) + 0.35); for (int i = -dx; i <= dx; i++) px(cx + i, cy + dy, c); }
}
void fillRoundRect(int x, int y, int w, int h, int r, uint16_t c) {
  fillRect(x + r, y, w - 2 * r, h, c); fillRect(x, y + r, w, h - 2 * r, c);
  fillCircle(x + r, y + r, r, c); fillCircle(x + w - r - 1, y + r, r, c); fillCircle(x + r, y + h - r - 1, r, c); fillCircle(x + w - r - 1, y + h - r - 1, r, c);
}
void drawLine(int x0, int y0, int x1, int y1, uint16_t c) {
  int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
  for (;;) { px(x0, y0, c); if (x0 == x1 && y0 == y1) break; int e2 = 2 * err; if (e2 >= dy) { err += dy; x0 += sx; } if (e2 <= dx) { err += dx; y0 += sy; } }
}
void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c) {
  int lo = std::min(y0, std::min(y1, y2)), hi = std::max(y0, std::max(y1, y2));
  const int X[3] = {x0, x1, x2}, Y[3] = {y0, y1, y2};
  for (int y = lo; y <= hi; y++) {
    double a = 1e9, b = -1e9;
    for (int k = 0; k < 3; k++) {
      int xa = X[k], ya = Y[k], xb = X[(k + 1) % 3], yb = Y[(k + 1) % 3];
      if (ya == yb) { if (y == ya) { a = std::min(a, (double)std::min(xa, xb)); b = std::max(b, (double)std::max(xa, xb)); } continue; }
      if ((y >= ya && y <= yb) || (y >= yb && y <= ya)) { double x = xa + (double)(xb - xa) * (y - ya) / (yb - ya); a = std::min(a, x); b = std::max(b, x); }
    }
    for (int x = (int)std::floor(a + 0.5); x <= (int)std::floor(b + 0.5); x++) px(x, y, c);
  }
}
void blit(int x, int y, int w, int h, const uint16_t* p) { for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) px(x + i, y + j, p[j * w + i]); }

// The pretend panel reports odd raw numbers on purpose: axes swapped, mirrored or skewed, depending on `panel`.
bool touchRaw(int& rx, int& ry, int& rz) {
  for (size_t k = 0; k < taps.size(); k++) {
    if (now < taps[k].from || now >= taps[k].to) continue;
    double x = flipped ? W - 1 - taps[k].x : taps[k].x, y = flipped ? H - 1 - taps[k].y : taps[k].y;
    double jitter = ((int)(now * 7919u % 9) - 4) * 1.5;
    if (panel == 0) { rx = (int)(3800 - y * 11.0 + jitter); ry = (int)(300 + x * 14.5 - jitter); }
    else if (panel == 1) { rx = (int)(250 + x * 15.2 + jitter); ry = (int)(3900 - y * 11.3 + jitter); }
    else { rx = (int)(400 + x * 13.0 + y * 0.9 + jitter); ry = (int)(350 + y * 10.5 - x * 0.7 - jitter); }
    rz = 1200;
    return true;
  }
  return false;
}
bool bootButton() { return false; }
uint32_t ms() { return now; }
void sleepMs(uint32_t t) { advance(t ? t : 1); }
void toneOn(int) { toneCount++; }
void toneOff() {}
int32_t loadInt(const char* key, int32_t fallback) { auto it = store.find(key); return it == store.end() ? fallback : it->second; }
void saveInt(const char* key, int32_t value) { store[key] = value; }
uint32_t rnd() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
void log(const char* text) { logged += text; logged += "\n"; }
}  // namespace hw
