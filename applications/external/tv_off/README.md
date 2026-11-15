# TV Off

One-tap TV power-off for Flipper Zero.

Open the app and it immediately sends every TV power code it knows, one after another. Point the Flipper at the TV and wait until it turns off.

## Controls

- **OK**: pause / resume. After a full pass, OK starts again.
- **Back**: exit.

## Code list

The app uses the TV universal remote dictionary already on your SD card (`SD Card/infrared/assets/tv.ir`), so it stays current whenever you update the firmware databases.

To use your own list, save an `.ir` file as `SD Card/apps_data/tv_off/codes.ir`. Every signal named `Power` in that file is sent, in file order. Put the codes you need most at the top.

## Notes

- Many TVs use the same code for power on and power off, so a TV that is already off may turn on.
- Range depends on the built-in IR LEDs. Aim directly at the TV.

## Building

Build tooling is managed with [uv](https://docs.astral.sh/uv/). It installs Python and uFBT into a local `.venv`; nothing goes into your system Python.

```
brew install uv         # once
uv run ufbt             # build -> dist/tv_off.fap
uv run ufbt launch      # build, install over USB and start the app
```
