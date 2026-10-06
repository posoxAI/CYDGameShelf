// Entry point on the board. Everything else lives in app.cpp and the game files.
#ifdef ARDUINO
#include <Arduino.h>

namespace app { void setup(); void loop(); }

void setup() { app::setup(); }
void loop() { app::loop(); }
#endif
