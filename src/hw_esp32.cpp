// The board: ESP32-2432S028R, the "Cheap Yellow Display" with one micro-USB socket.
// ILI9341 screen on HSPI (driven by TFT_eSPI, set up in platformio.ini), XPT2046 touch on its own SPI pins,
// speaker pin 26, RGB light on pins 4, 16 and 17.
#ifdef ARDUINO
#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <Preferences.h>
#include <esp_system.h>
#include "hw.h"

namespace hw {

static const int TOUCH_CLK = 25, TOUCH_MISO = 39, TOUCH_MOSI = 32, TOUCH_CS_PIN = 33;
static const int SPEAKER = 26, BACKLIGHT = 21, BOOT_BUTTON = 0;
static const int LED_R = 4, LED_G = 16, LED_B = 17;
static const int TOUCH_PRESSURE = 300;      // readings below this are "nobody is touching"
static const int TONE_CHANNEL = 0;

static TFT_eSPI tft = TFT_eSPI();
static SPIClass touchSpi(VSPI);
static Preferences prefs;

void begin() {
  Serial.begin(115200);
  // the RGB light is on when its pins are low; keep it dark
  pinMode(LED_R, OUTPUT); pinMode(LED_G, OUTPUT); pinMode(LED_B, OUTPUT);
  digitalWrite(LED_R, HIGH); digitalWrite(LED_G, HIGH); digitalWrite(LED_B, HIGH);
  pinMode(BOOT_BUTTON, INPUT_PULLUP);

  tft.init();
  tft.setRotation(0);
  tft.setSwapBytes(true);        // blit() sends colours as ordinary 16-bit numbers
  tft.fillScreen(TFT_BLACK);
  pinMode(BACKLIGHT, OUTPUT);
  digitalWrite(BACKLIGHT, HIGH);

  pinMode(TOUCH_CS_PIN, OUTPUT);
  digitalWrite(TOUCH_CS_PIN, HIGH);
  touchSpi.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS_PIN);

  prefs.begin("cydgames", false);

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(SPEAKER, 2000, 8);
  ledcWriteTone(SPEAKER, 0);
#else
  ledcSetup(TONE_CHANNEL, 2000, 8);
  ledcAttachPin(SPEAKER, TONE_CHANNEL);
  ledcWriteTone(TONE_CHANNEL, 0);
#endif
  Serial.println("CYD games: board ready");
}

void setFlip(bool flip) { tft.setRotation(flip ? 2 : 0); }
void setInvert(bool invert) { tft.invertDisplay(invert); }

void fillRect(int x, int y, int w, int h, uint16_t color) { tft.fillRect(x, y, w, h, color); }
void fillRoundRect(int x, int y, int w, int h, int r, uint16_t color) { tft.fillRoundRect(x, y, w, h, r, color); }
void fillCircle(int x, int y, int r, uint16_t color) { tft.fillCircle(x, y, r, color); }
void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color) { tft.fillTriangle(x0, y0, x1, y1, x2, y2, color); }
void drawLine(int x0, int y0, int x1, int y1, uint16_t color) { tft.drawLine(x0, y0, x1, y1, color); }
void blit(int x, int y, int w, int h, const uint16_t* pixels) { tft.pushImage(x, y, w, h, (uint16_t*)pixels); }

// The XPT2046 answers each command one transfer later, 12 bits wide, shifted up by three.
// The command order follows the widely used XPT2046_Touchscreen library.
static int16_t middleOfThree(int16_t a, int16_t b, int16_t c) {
  if ((a <= b && b <= c) || (c <= b && b <= a)) return b;
  if ((b <= a && a <= c) || (c <= a && a <= b)) return a;
  return c;
}
bool touchRaw(int& rx, int& ry, int& rz) {
  int16_t d[6] = {0, 0, 0, 0, 0, 0};
  touchSpi.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
  digitalWrite(TOUCH_CS_PIN, LOW);
  touchSpi.transfer(0xB1);                              // pressure, first half
  int16_t z1 = touchSpi.transfer16(0xC1) >> 3;          // pressure, second half
  int z = z1 + 4095;
  int16_t z2 = touchSpi.transfer16(0x91) >> 3;
  z -= z2;
  if (z >= TOUCH_PRESSURE) {
    touchSpi.transfer16(0x91);                          // the first position reading is always noisy
    d[0] = touchSpi.transfer16(0xD1) >> 3;
    d[1] = touchSpi.transfer16(0x91) >> 3;
    d[2] = touchSpi.transfer16(0xD1) >> 3;
    d[3] = touchSpi.transfer16(0x91) >> 3;
  }
  d[4] = touchSpi.transfer16(0xD0) >> 3;                // last reading, then power down
  d[5] = touchSpi.transfer16(0) >> 3;
  digitalWrite(TOUCH_CS_PIN, HIGH);
  touchSpi.endTransaction();
  if (z < TOUCH_PRESSURE) return false;
  rx = middleOfThree(d[0], d[2], d[4]);
  ry = middleOfThree(d[1], d[3], d[5]);
  rz = z;
  return true;
}
bool bootButton() { return digitalRead(BOOT_BUTTON) == LOW; }

uint32_t ms() { return millis(); }
void sleepMs(uint32_t t) { delay(t); }

void toneOn(int freq) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWriteTone(SPEAKER, freq);
#else
  ledcWriteTone(TONE_CHANNEL, freq);
#endif
}
void toneOff() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWriteTone(SPEAKER, 0);
#else
  ledcWriteTone(TONE_CHANNEL, 0);
#endif
}

int32_t loadInt(const char* key, int32_t fallback) { return prefs.getInt(key, fallback); }
void saveInt(const char* key, int32_t value) { prefs.putInt(key, value); }

uint32_t rnd() { return esp_random(); }
void log(const char* text) { Serial.println(text); }

}  // namespace hw
#endif
