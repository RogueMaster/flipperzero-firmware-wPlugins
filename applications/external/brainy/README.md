# Brainy

![Brainy](brainy.png)

A Simon Says memory game for the Flipper Zero, featuring Brainy — a pixel-art brain mascot who challenges your memory one step at a time.

## How to play

Watch the sequence of flashing d-pad buttons, then repeat it in the same order. Each round adds one more step. How far can you go?

- **OK** — start / retry
- **Up / Down / Left / Right** — press the matching button during your turn
- **Back** — return to the title screen

![Screenshot](screenshot_1.png)
![Screenshot](screenshot_2.png)

## Features

- 100-step sequence cap — beat it and you win
- Playback speed increases with each level (up to 4× faster)
- Ambient background music with slowly shifting pitch
- Particle effects that drift across the screen in all directions
- Vibration and RGB LED flash on a wrong answer
- High score saved to the SD card between sessions

## Building

Requires the [Flipper Zero firmware](https://github.com/flipperdevices/flipperzero-firmware) build environment (`fbt`).

```bash
git clone --recursive https://github.com/flipperdevices/flipperzero-firmware
cp -r brainy flipperzero-firmware/applications_user/
cd flipperzero-firmware
./fbt fap_brainy
```

The compiled `.fap` will be at `build/f7-firmware-D/.extapps/brainy.fap`. Copy it to `SD:/apps/Games/` on your Flipper.

## Author

deya-eldeen — [github.com/deya-eldeen](https://github.com/deya-eldeen)
