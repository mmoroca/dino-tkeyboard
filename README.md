# Dino Run for T-Keyboard

🇬🇧 English · [🇪🇸 Español](README.es.md)

A Chrome-style dinosaur runner for the LilyGO **T-Keyboard** (ESP32-C3 with a 0.99" GC9D01N screen, 40×160 px). It has a black-and-white mode and a colour mode. There is only one key to play with: **space**, and **ALT** to switch the colours.

> by **@mmoroca & Leo.ai** · 2026 · [MIT License](LICENSE)

## Features

- Waiting screen: the dinosaur stands under the **GAME OVER** title. Press space to start.
- Cacti (two types, and a double one), and birds: **high birds** (you must duck) and **low birds** (you must jump).
- Two display modes, switched with **ALT**:
  - **Black and white** (default): black graphics on a white background.
  - **Colour**: blue sky, brown ground, dark green dinosaur, light green cacti, white birds and white text.
- The speed increases slowly as the distance grows.
- After each run: your **score** and the **best score**. The device saves the top 5 scores in flash memory.
- Screen turns off after **5 s** without any key press, to save battery. Any key turns it back on.

## Controls

| Key | Action |
|---|---|
| **Space**, quick tap (less than about 230 ms) | Jump |
| **Space**, held on the ground | Duck. Release to stand up |
| **Space**, held in the air | Fall faster |
| **Space** on the waiting screen, or 0.7 s after a game over | Start / restart |
| **ALT** | Switch between black-and-white and colour mode |
| **Any key** while the screen is off | Turns the screen on (this press does nothing else) |

Duck under the high birds. Jump over the cacti and the low birds.

If the screen turns off during a run (5 s without pressing any key), the run ends and the score is saved.

The display mode is not saved: the device starts in black-and-white mode after a power cycle.

## Hardware

- LilyGO **T-Keyboard** (ESP32-C3, 0.99" GC9D01N screen 40×160). Product page: <https://lilygo.cc/products/t-keyboard>
- USB-C cable.

## Build and upload (VSCode + PlatformIO)

1. Install [Visual Studio Code](https://code.visualstudio.com/) and the **PlatformIO IDE** extension.
2. Open this folder with **File → Open Folder**.
3. Wait for PlatformIO to install the platform (first time only).
4. Click **Build** (✓ in the bottom bar), then **Upload** (→).
5. If the upload fails, put the T-Keyboard in download mode: hold **BOOT**, press **RESET** (or connect USB), and release **BOOT**. See the [T-Keyboard README](https://github.com/Xinyuan-LilyGO/T-keyboard).

From the terminal (with PlatformIO CLI installed):

```bash
pio run -e T-Keyboard-dino                 # build
pio run -e T-Keyboard-dino -t upload       # upload
pio device monitor -b 115200               # serial monitor
```

## Project layout

- `src/main.cpp`: hardware glue (screen, keyboard, colour mode, saved scores, main loop).
- `src/dino_game.h`: game logic, palettes and drawing into a frame buffer. It has no Arduino dependencies.
- `src/bitmaps.h`: 1-bit sprites (dinosaur, cacti, birds, floor, GAME OVER title).
- `src/dino_font.h`: the 8×16 font of the display driver, copied so the game can draw text into the frame buffer.
- `lib/TFT_GC9D01N-Patched`: display driver (MIT, Xinyuan-LilyGO).

## Notes

- The best scores are stored in the `dino` namespace of `Preferences` (keys `h0`…`h4`).
- The game has no sound: the T-Keyboard has no documented buzzer pin.
- Texts on the screen avoid accented letters, because the on-screen font has a pending accent issue.

## Credits and licenses

- **Author:** @mmoroca & Leo.ai (2026).
- **License:** [MIT](LICENSE).
- **Sprites:** MIT, as provided by the author of the reference Dino code.
- **Display driver:** `lib/TFT_GC9D01N-Patched` (MIT, Copyright (c) 2022 Xinyuan-LilyGO).
- **Inspiration:** the Chrome dinosaur game. This project does not use any code or artwork from Google and is not affiliated with it.
