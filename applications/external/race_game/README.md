# 🏎️ Race Game for Flipper Zero

A fast-paced 3-lane vertical scrolling car racing game for Flipper Zero.

## Features

- **3 Lane Racing** — Dodge incoming vehicles on a scrolling road
- **Multiple Obstacle Types** — Motorcycles (fast), sedans (normal), boss trucks (2-lane wide!)
- **Power-Ups** — Collect shields (invincibility), magnets (auto-collect coins), and fuel (+1 life)
- **Coin Collection & Combo System** — Collect coins for bonus points with increasing multiplier
- **3 Lives System** — Don't worry about one crash, you have 3 chances
- **High Score** — Your best score is saved to SD card
- **Night Mode** — Toggle dark theme from the menu
- **Sound Effects** — Lane change beeps, crash sounds, level up chimes (toggleable)
- **Vibration Feedback** — Feel the crash!
- **Difficulty Selection** — Easy, Normal, or Hard
- **Crash Animation** — Pixel particle explosion on collision
- **Roadside Scenery** — Trees and poles for speed immersion
- **Pause** — Press Back during gameplay to return to menu

## Controls

| Button | Action |
|--------|--------|
| **◀ / ▶** | Change lane (gameplay) / Navigate menu |
| **▲ / ▼** | Navigate menu options |
| **OK** | Select menu option / Restart |
| **Back** | Pause (gameplay) / Exit (menu) |

## Menu Options

- **START** — Begin the race
- **SOUND: ON/OFF** — Toggle sound effects
- **NIGHT: ON/OFF** — Toggle night mode (inverted colors)
- **EASY / NORMAL / HARD** — Select difficulty

## Building

Requires ufbt (https://pypi.org/project/ufbt/):

ufbt build

## Installation

Copy race_game.fap to your Flipper Zero SD card:

SD Card/apps/Games/race_game.fap

Or install from the **Flipper Apps Catalog** (coming soon).

## Author

**Mehmet Miraç ARIK** ([@mrc19056](https://github.com/mrc19056))

## License

MIT License
