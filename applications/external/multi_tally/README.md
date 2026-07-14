# Multi Tally

Six independent tally counters on one screen — one for each hardware button of your Flipper Zero.

Count several things at once: people entering two doors, score of a board game, reps of different exercises, anything that needs more than one counter in your pocket.

## How it works

The screen is split into six cells matching the buttons: Left, Up, Right on top; OK, Down, Back below. Each cell shows a tiny button pictogram, the group name and its count.

- **Short press** any button: +step to its group
- **Long press**: -step (OK and Back subtract when released)
- **Hold OK** (~2 s): open the menu
- **Hold Back** (~2 s): exit the app

## Menu

- **How to use** — built-in instructions
- **Groups** — per-group settings: rename with the on-screen keyboard, counting step (1/2/5/10/20/50/100), enable/disable, zero the count
- **Settings** — vibro, sound and LED blink on each count, always-on backlight, hold time for menu/exit (1–3 s)
- **Reset all counters** — zero everything, names and settings stay
- **Reset to defaults** — full factory reset

Counts, names and settings are saved to the SD card automatically, so you can continue right where you left off.

## Building

Built with [ufbt](https://github.com/flipperdevices/flipperzero-ufbt): run **python -m ufbt** in the repo root to build, or **python -m ufbt launch** to build, install and run on a connected Flipper.
