# CYD Game Shelf — notes for Claude

Firmware for the ESP32-2432S028R "Cheap Yellow Display" (one micro-USB, ILI9341 240 × 320, XPT2046 resistive touch): a menu, Minesweeper and Five in a Line, in Russian and English. The whole thing is about 4700 lines and has no framework beyond Arduino and TFT_eSPI.

## Where the games come from

Both games are ports of browser originals by posoxAI, each a single HTML file. When a rule is unclear — what scores, when a game ends, what the first click guarantees — **the original is the reference**, not a guess:

- Minesweeper: [play](https://posoxai.github.io/MinesweeperGame/) · [code](https://github.com/posoxAI/MinesweeperGame)
- Five in a Line: [play](https://posoxai.github.io/FiveInLineGame/) · [code](https://github.com/posoxAI/FiveInLineGame)
- the rest of the shelf: [Игротека](https://posoxai.github.io/posoxAI/)

The port is not a copy: the screen is 240 × 320 and the input is a fingertip, so layouts and cell sizes differ on purpose. Rules match, pixels do not.

## Two targets, and the order to use them

```
sim/run.sh                 # build for this computer and run the tests
pio run                    # build for the board
pio run -t upload          # and flash it; add --upload-port if the board is not found by itself
pio device monitor         # read what the board says, at 115200
```

The CH340 bridge on this board shows up as `/dev/cu.usbserial-*` on macOS.

**Run `sim/run.sh` before every flash.** It compiles all of `src/` plus `sim/` with `g++ -Wall -Wextra -Werror -Wformat=2 -DCYD_SIM`, plays several hundred games of each game to check the rules, walks every screen with a scripted finger, and writes PNGs of the screens to `sim/out`. It ends with `N checks passed, 0 failed` and returns non-zero if anything failed. It needs only `g++` and Python 3 — the PPM→PNG step is hand-rolled on the standard library, so do not reintroduce a Pillow dependency.

The board prints `CYD games: board ready` on the serial port once it is up. On macOS there is no `timeout`; to read the port from a script use `~/.platformio/penv/bin/python` (the system Python has no `pyserial`) and toggle DTR/RTS to reset the board first.

## The one rule about hardware

`src/hw.h` is the complete list of what the games ask of the board. `src/hw_esp32.cpp` answers it with TFT_eSPI and the ESP32 SDK; `sim/hw_host.cpp` answers it with a framebuffer in memory and a queue of pretend touches. Nothing else in `src/` may include `Arduino.h`, `TFT_eSPI.h` or any SDK header — that is what keeps the games testable on a computer. If a screen needs something new from the board, add it to `hw.h` and implement it in both files.

Colours are RGB565, packed by `RGB(r, g, b)` in `src/ui.h`. Text goes through `ui::label`, which composes a line into a buffer and `hw::blit`s it once; drawing glyphs straight onto the panel flickers.

## Adding or changing a text

Three places, in step:

1. a new `S_*` entry in the `StrId` enum in `src/app.h`;
2. the `{Russian, English}` pair at the same position in `src/strings.cpp` — the array is positional, a missed line shifts every text after it;
3. a width entry in `sim/sim_main.cpp`: status lines go in `STATUS[]` (≤ 236 px in `FONT_S`), captions in `BUTTONS[]` with the pixel width of the widget they sit in.

`testTexts()` then checks both languages fit. A string that overflows is a test failure, not a cosmetic issue — the screen is 240 px wide and Russian runs long.

## Screen layouts and the tests

`testScreens()` drives the UI by hard-coded pixel coordinates. **Moving a button means updating its tap in `sim/sim_main.cpp`**, and a tap that lands on nothing usually shows up as a later check failing in a confusing place, so change the two together. Rects live next to the screen that draws them in `src/app.cpp` (`SET_*`, `PAL_*`, `OWN_*`).

## Settings kept in flash

`hw::saveInt`/`hw::loadInt` over NVS; keys are at most 15 characters. In use: `lang`, `sound`, `flip`, `inv`, `marks`, `pal`, `cc1`…`cc7` (the player's own marble colours, one packed RGB each), `calok` and `cal0`…`cal5` (the touch calibration, each ×65536), `msize`, `mbest0`, `mbest1`, `lbest`. Reuse a key and you silently inherit someone else's value.

## Colours on the real panel

The panel washes light colours out and shifts them in ways the simulator does not reproduce — the sim renders RGB565 faithfully, so **a screenshot is never evidence about how something looks on the board**. Two blind palette guesses were both rejected on hardware. Hence the on-device picker: three preset palettes plus a fully editable one (Settings → Marble colours), and Marks — a dot, ring, bar, cross, triangle, square or slash inside each marble — so readability does not depend on hue at all. Do not retune palette constants from screenshots; change the picker, or ask what the board shows.

## Style

Prose in comments, commits and READMEs: plain sentences, lower key, explaining why rather than restating the code. Match what is already there. `README.md` and `README.ru.md` carry the same content in two languages and must be edited together.

Fonts in `src/font_data.cpp` are generated — regenerate with `tools/make_font.py` rather than editing the tables.

## Git

One author, no pull requests, so work lands on `main` directly — no side branches. Commit and push only when asked.
