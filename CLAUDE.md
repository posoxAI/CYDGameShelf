# CYD Game Shelf — notes for Claude

Firmware for the ESP32-2432S028R "Cheap Yellow Display" (one micro-USB, ILI9341 240 × 320, XPT2046 resistive touch): a scrolling menu, Minesweeper, Five in a Line, Bubbles, Bricks and 2048, in Russian and English. The whole thing is about 6900 lines and has no framework beyond Arduino and TFT_eSPI.

## Where the games come from

Every game is a port of a browser original by posoxAI, each a single HTML file. When a rule is unclear — what scores, when a game ends, what the first click guarantees — **the original is the reference**, not a guess:

- Minesweeper: [play](https://posoxai.github.io/MinesweeperGame/) · [code](https://github.com/posoxAI/MinesweeperGame)
- Five in a Line: [play](https://posoxai.github.io/FiveInLineGame/) · [code](https://github.com/posoxAI/FiveInLineGame)
- Bubbles: [play](https://posoxai.github.io/Bubbles/) · [code](https://github.com/posoxAI/Bubbles)
- Bricks: [play](https://posoxai.github.io/Bricks/) · [code](https://github.com/posoxAI/Bricks)
- 2048: [play](https://posoxai.github.io/2048/) · [code](https://github.com/posoxAI/2048)
- the rest of the shelf: [Игротека](https://posoxai.github.io/posoxAI/)

The port is not a copy: the screen is 240 × 320 and the input is a fingertip, so layouts and cell sizes differ on purpose. Rules match, pixels do not. Bubbles is the clearest case — the browser field is 11 × 13 and the device's is 11 × 11, because the cannon needs the bottom of a 320-pixel screen, and the browser's mouse-point-and-click becomes drag-to-aim-and-lift-to-shoot.

Bricks is the one place where the geometry lined up: the browser wall is ten bricks wide and so is a 240-pixel screen at 22 pixels a brick, so all six levels are copied over letter for letter. Everything else there is two thirds of its browser size — paddle, ball, capsules and every speed — because the screen is 240 × 320 against 360 × 480.

2048 needed the least changing of any of them: the rules are the same four directions and the layout is a
square of four by four that a 240-pixel screen takes without argument. Two things are not the browser's. There
is no sliding animation — a tile appears where it landed, because a frame of the whole board costs about forty
milliseconds on this panel and six of those is a slow, flickering move; merged and new tiles grow in instead,
which is cheap because only the cells that changed are redrawn. And the browser's modal after 2048 ("keep
going" or "new game") is gone: the status line says the game is won and play simply continues, which is the
rule the browser's modal was wrapped around.

## The menu scrolls

The shelf is a list of 46-pixel rows in a fixed band between the subtitle and Settings, four at a time, with
`menuScroll` pixels of it above the top. A drag moves it, a press that moved less than eight pixels is the
choice of a game, and Settings is pinned below the band so it never has to be looked for. Adding a game is a
line in `SHELF[]`, a case in `menuSub` and one in `menuIcon` — no re-layout, which is the whole point: the
previous arrangement moved every tap in `testScreens` each time the shelf grew.

The band needs real clipping, because a row halfway out of it would otherwise paint over the title. That is
what `hw::setClip`/`hw::clearClip` are for — TFT_eSPI's viewport on the board, a test in `px()` in the
simulator. They are the only two calls in `hw.h` that change state rather than draw, so clear the clip before
returning from whatever set it.

The one thing scrolling costs is discoverability: with four rows exactly filling the band nothing is cut off,
so the bar down the right is the only sign that the shelf goes on. If that turns out to be too quiet on the
board, shortening `LIST_H` so the fourth row is visibly cut is a one-line change.

## The one game that runs on a clock

Bricks is the only screen that moves without being touched. It steps in fixed 20-millisecond pieces counted off `hw::ms()`, never in whatever time the last frame happened to take, and it catches up at most three steps at once. That matters because the simulator's clock only advances inside `hw::sleepMs`, so the same game plays identically on the board and in the tests — which is what makes the physics as testable as the turn-based rules. Keep the step fixed; a step sized from the elapsed time would make every test depend on how fast the computer is.

It also draws in pieces, like Bubbles: `repaintArea` puts back the background, the mortar, the bricks and the paddle in one small box, and everything that moves is rubbed out that way. `drawThings` is the one place the ball, the capsules and the chips are drawn, so a full repaint and a frame-by-frame one agree pixel for pixel — `testScreens` compares the two and fails on a one-pixel trail. It only agrees just after a step, though: the paddle is drawn on the step, not on the touch that moved it.

The sound is one square-wave pin and a queue of eight notes, not a mixer. Hits use `snd::play`, which drops whatever is queued, so a fast volley cuts its own notes short instead of backing up into a drone several seconds behind the ball.

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

`hw::saveInt`/`hw::loadInt` over NVS; keys are at most 15 characters. In use: `lang`, `vol` (0 silent, 1…3 the loudness steps; `sound` is the older on/off key, still read once so a board that was muted stays muted), `flip`, `inv`, `marks`, `pal`, `cc1`…`cc7` (the player's own ball colours, one packed RGB each), `calok` and `cal0`…`cal5` (the touch calibration, each ×65536), `msize`, `mbest0`, `mbest1`, `lbest`, `bbest`, `kbest`, `gbest`. Reuse a key and you silently inherit someone else's value.

## Colours on the real panel

The panel washes light colours out and shifts them in ways the simulator does not reproduce — the sim renders RGB565 faithfully, so **a screenshot is never evidence about how something looks on the board**. Two blind palette guesses were both rejected on hardware. Hence the on-device picker: three preset palettes plus a fully editable one (Settings → Ball colours), and Marks — a dot, ring, bar, cross, triangle, square or slash inside each ball — so readability does not depend on hue at all. Do not retune palette constants from screenshots; change the picker, or ask what the board shows.

That one set colours three games. Bubbles draws its bubbles with `lines::drawSample` and colours 1…6 of the same palette, and Bricks paints its brick faces with `lines::ballColor` and `lines::ballShade` of the same six, rather than carrying more sets that would have to be settled on hardware all over again. The grey chrome of Bricks — walls, mortar, steel, ball — and the yellow of the paddle are the exception, and they are deliberately the parts where nothing has to be told apart by hue: a strong brick is known by its crack and a steel one by its bolts. 2048 is the other exception and a
clearer one: its heat map is copied straight from the browser's dark theme, because every tile says its own
number and nothing there has to be told apart by hue at all.

## Style

Prose in comments, commits and READMEs: plain sentences, lower key, explaining why rather than restating the code. Match what is already there. `README.md` and `README.ru.md` carry the same content in two languages and must be edited together.

Fonts in `src/font_data.cpp` are generated — regenerate with `tools/make_font.py` rather than editing the tables.

## Git

One author, no pull requests, so work lands on `main` directly — no side branches. Commit and push only when asked.
