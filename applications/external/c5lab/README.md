# Flipper Companion App

The Flipper Zero companion app (`Lab_C5.fap`) mirrors the ESP32-C5 CLI workflows (scan, results, attacks, wardrive, Sniffer Dog, portal control) so you can steer the board from the handheld UI.

## Build with compatible SDKs

If you need an alternate firmware SDK that keeps the app compatible with the Unleashed or Momentum releases, refresh the toolchain and build with `ufbt`:

```bash
ufbt update --hw f7 --url="https://github.com/DarkFlippers/unleashed-firmware/releases/download/unlshd-083/flipper-z-f7-sdk-unlshd-083.zip"
ufbt launch

ufbt update --hw f7 --url="https://github.com/Next-Flip/Momentum-Firmware/releases/download/mntm-011/flipper-z-f7-sdk-mntm-011.zip"
ufbt launch
```

Each `ufbt update` pulls the SDK that matches the specific firmware track (Unleashed or Momentum) before running `ufbt launch` to build and drop the `.fap` into `dist/`. Future firmware drops may move to different SDK names, so be prepared to pick the correct SDK zip manually from the firmware release page to keep builds in sync.

## Install the `.fap` (Windows + qFlipper)

1. Build or grab the packaged binary from `FLIPPER/dist/Lab_C5.fap`.
2. Launch **qFlipper**, connect your Flipper Zero, and wait until it shows as `Connected`.
3. Open **File manager** in qFlipper, then navigate to your SD card (`SD Card` tab) and browse to `/apps`.
4. Drag-drop or copy `Lab_C5.fap` from your PC (the repo `FLIPPER/dist` folder) into `/apps` on the SD card.
5. Safely eject the device or close qFlipper—the app now appears under **Applications → External** on the Flipper.

A short walkthrough video is also available: https://youtu.be/Jf8JnbvrvnI

Need help? Ping the team on Discord: https://discord.gg/57wmJzzR8C
