# CYD Game Shelf

[Русская версия](README.ru.md)

Five games and a menu for the "Cheap Yellow Display", the ESP32 board with a 2.8-inch touch screen: Minesweeper, Five in a Line, Bubbles, Bricks and 2048. The interface is in English and Russian.

<p>
  <img src="screenshots/menu-en.png" width="180" alt="The menu: Minesweeper, Five in a Line, Bubbles, Bricks and, below the fold, 2048">
  <img src="screenshots/minesweeper-ru.png" width="180" alt="Minesweeper mid-game on the 9 by 9 field, Russian interface">
  <img src="screenshots/lines-ru.png" width="180" alt="Five in a Line after a line was cleared, Russian interface">
  <img src="screenshots/bubbles-ru.png" width="180" alt="Bubbles at the start of a game, the cannon aimed up the middle, Russian interface">
  <img src="screenshots/bricks-ru.png" width="180" alt="Bricks in play: the ball in flight under the wall, a capsule falling towards the hazard-striped paddle, Russian interface">
  <img src="screenshots/2048-ru.png" width="180" alt="2048 with a full board, tiles from 2 to 128 in heat-map colours, Russian interface">
</p>

The pictures come from the computer simulator in `sim/`, which runs the same game code as the board.

## Status

The game rules and every screen are tested on a computer, and the firmware builds, flashes and comes up on the board. Boards differ, so expect to adjust something on a first run; the Troubleshooting section lists the likely things. Bricks is the newest game and the only one that moves without being touched, so how its paddle answers a finger on a resistive panel is worth judging on the board rather than from a picture.

## The board

ESP32-2432S028R with one micro-USB socket: an ESP-WROOM-32 module, a 240 × 320 ILI9341 screen and an XPT2046 resistive touch panel. The version with two USB sockets has a different screen driver and needs other settings in `platformio.ini`.

## Build and flash

The project is built with [PlatformIO](https://platformio.org/).

```
pio run -t upload
pio device monitor
```

PlatformIO downloads the ESP32 toolchain and the screen library, TFT_eSPI, by itself. The library is configured by the flags in `platformio.ini`, so none of its files need editing.

## First start

1. **Touch calibration.** Four crosses appear one after another. Press each one and hold until it disappears. A stylus or a fingernail is more precise than a fingertip.
2. **Language.** Choose Russian or English.

Both are remembered. They can be changed later in Settings.

## The games

**Minesweeper.** Two fields: 9 × 9 with 10 mines and 16 × 16 with 40 mines. A tap opens a cell, a long press plants a flag. The Dig / Flag button swaps the two. A tap on an open number opens its neighbours once that many flags surround it. The first opened cell and its neighbours never hold a mine. The best time is kept for each field. The 16 × 16 field has small cells and wants a stylus.

**Five in a Line.** Tap a marble, then a free cell: the marble moves there if a path is free. Five or more of one colour in a row disappear and score. After a move without a line three new marbles arrive. The game ends when the board is full.

**Bubbles.** Drag a finger over the field to aim the cannon and lift it to shoot; lifting below the cannon takes the shot back. A dotted line shows where the bubble will go, bouncing off the side walls on the way. Three or more of one colour touching pop for 10 each, and whatever is left hanging without support falls for 20. Every fifth shot that pops nothing brings a new row down from the top. Tapping the waiting bubble under Next swaps it with the loaded one. The game is won when the field is empty and lost when a bubble passes the red line.

**Bricks.** Drag a finger anywhere on the field and the paddle follows the movement, so the finger never hides it; a tap launches the ball. The further from the middle of the paddle the ball lands, the steeper it flies off, and every hit on the paddle makes it a little faster. A coloured brick breaks at one hit for 10 points, a dark one takes two and cracks after the first for 20, and a steel one with bolts never breaks and is not needed to clear the level. A cleared level adds 100 points times its number. Broken bricks sometimes drop a capsule to catch with the paddle: a wider paddle, a slower ball, three balls at once or an extra ball. You have three balls; a ball that falls past the paddle is lost. There are six walls, and after the sixth they come round again, faster.

**2048.** Swipe across the board the way you want the tiles to go. Every tile slides as far as it can, two equal tiles that meet merge into one with their sum, and the sum is added to the score; a tile merges only once in a move, so 2 2 2 2 becomes 4 4 rather than 8. After every move a new tile appears, a 2 nine times out of ten and otherwise a 4. Making 2048 wins, and you may keep playing. The game ends when the board is full with no two neighbours equal. Undo takes back the last move, one move only, and it works after the game has ended too. The small number in a tile's corner is its power of two, and the colours run cold to hot as the tiles double.

A game stays in memory while you are in the menu or in another game. It is lost when the power goes off. Leaving Bricks for the menu pauses it, and a tap brings it back.

## The menu

The shelf is longer than the screen, so it scrolls: drag a finger over the rows and the list follows, and a press that hardly moved opens the game under it. The bar down the right-hand side shows how much of the shelf is in view. Settings stays below the list and never scrolls away.

## Settings

- Language: RU or EN.
- Sound: simple tones on the speaker connector, in four steps — off, then quiet, middle and loud, shown as a cross and the numbers 1 to 3. The chosen step is heard straight away in the note that confirms the tap. Nothing is heard unless a speaker is plugged in.
- Screen: turns the picture by 180 degrees.
- Colours: for boards that show the colours inverted.
- Ball colours: four rows of colours for the marbles of Five in a Line, the bubbles and the brick faces, shown one above the other at the size they have in the game. Panels differ from board to board, so the readable set is the one that looks readable on yours — pick it there. Three rows are ready-made; the fourth is yours, and tapping it opens a screen where each marble is given a colour out of a grid of squares. Reset puts the first set back. The picking screen also holds Marks: a little sign inside every marble, a dot, a ring, a bar and so on, one for each colour. Marks start on, because these panels render some colours close to each other; turn them off for plain marbles.
- Touch calibration.
- Reset best results.

## Troubleshooting

- **Touches land in the wrong place and the menu cannot be used.** Hold a finger on the screen, or hold the BOOT button, while the board powers on. Calibration starts again.
- **Calibration keeps saying it did not work.** The numbers at the bottom of the calibration screen are the raw readings of the panel. If they do not change when you press in different places, the touch wiring differs from this board. The same readings go to the serial monitor.
- **The picture is upside down.** Settings, Screen.
- **Colours look like a photo negative.** Settings, Colours.
- **Red and blue are swapped.** Add `-DTFT_RGB_ORDER=TFT_RGB` to `build_flags` in `platformio.ini`. If that makes it worse, use `TFT_BGR`.
- **The screen stays white or black.** This is usually the two-USB version of the board. It needs `-DST7789_DRIVER=1` in place of `-DILI9341_2_DRIVER=1`.

## Case

`case/` holds a 3D-printed case for a resin printer: it runs the board from two AA cells, with a power switch, a speaker and a pocket for the stylus. Print files, the parts list and the assembly order are in [case/README.md](case/README.md).

## Tests on a computer

```
sim/run.sh
```

The script builds the games with `g++`, plays several hundred games of each to check the rules, drives every screen with a scripted finger and saves pictures of the screens to `sim/out`. It needs `g++` and Python, nothing else.

## Files

- `src/app.cpp`: touch, sound, settings, calibration, menu.
- `src/game_mines.cpp`, `src/game_lines.cpp`, `src/game_bubbles.cpp`, `src/game_bricks.cpp`, `src/game_2048.cpp`: the games.
- `src/ui.cpp`, `src/font_data.cpp`: text drawing and the fonts. The fonts are generated by `tools/make_font.py`.
- `src/strings.cpp`: every text in both languages.
- `src/hw.h`: what the games need from the board. `src/hw_esp32.cpp` provides it on the board, `sim/hw_host.cpp` on a computer.
- `case/`: the case, its model and print files.

## Credits

The firmware was written by Claude, the AI assistant made by Anthropic: the game logic, the screens, the touch handling and the simulator.

The idea and the choice of games came from posoxAI. All five are ports of browser originals, which can be played in a browser right now: Minesweeper ([play](https://posoxai.github.io/MinesweeperGame/), [code](https://github.com/posoxAI/MinesweeperGame)), Five in a Line ([play](https://posoxai.github.io/FiveInLineGame/), [code](https://github.com/posoxAI/FiveInLineGame)), Bubbles ([play](https://posoxai.github.io/Bubbles/), [code](https://github.com/posoxAI/Bubbles)), Bricks ([play](https://posoxai.github.io/Bricks/), [code](https://github.com/posoxAI/Bricks)) and 2048 ([play](https://posoxai.github.io/2048/), [code](https://github.com/posoxAI/2048)). The rules of 2048 are Gabriele Cirulli's, from 2014. The other games on the shelf are at [Игротека](https://posoxai.github.io/posoxAI/).

The case in `case/` was designed by Claude too, from posoxAI's measurements and test prints.

The letters are drawn from the DejaVu fonts, which are free to use and to embed.

## License

MIT. The full text is in [LICENSE](LICENSE).
