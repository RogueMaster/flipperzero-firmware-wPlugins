# Build Instructions

This guide covers how to build and install **flipper_tv_remote** from source.

## Prerequisites

| Requirement | Notes |
|---|---|
| [Flipper Zero](https://flipperzero.one/) | With a microSD card inserted |
| Python 3.8 or newer | Required by uFBT |
| [uFBT](https://github.com/flipperdevices/flipperzero-ufbt) | Flipper build tool for external apps |
| USB-C cable | Required for deploying directly from your computer |

## 1 — Install uFBT

uFBT (micro Flipper Build Tool) is the official toolchain for building external Flipper apps.

```bash
# Linux / macOS
python3 -m pip install --upgrade ufbt

# Windows
py -m pip install --upgrade ufbt
```

On first run, uFBT automatically downloads the Flipper SDK that matches your device's firmware version.

## 2 — Clone this repository

```bash
git clone https://github.com/jmanion0139/flipper_tv_remote.git
cd flipper_tv_remote
```

## 3 — Build the app

```bash
ufbt
```

This produces `dist/flipper_tv_remote.fap` — the compiled Flipper Application Package.

## 4 — Deploy to your Flipper Zero

### Option A — Deploy and launch automatically (recommended)

Connect your Flipper via USB, unlock the device, and run:

```bash
ufbt launch
```

This builds, copies the `.fap` to the correct location on the SD card, and launches the app automatically.

### Option B — Copy the file manually

Copy `dist/flipper_tv_remote.fap` to `apps/Infrared/` on your Flipper's microSD card, then open it from **Applications → Infrared → TV Remote**.

## Updating the SDK

If you see an API version mismatch warning after a Flipper firmware update, refresh the SDK to match:

```bash
ufbt update
```

Then rebuild with `ufbt`.

## Troubleshooting

| Symptom | Fix |
|---|---|
| `ufbt launch` cannot find the device | Ensure the Flipper is connected via USB, unlocked, and not running an app that blocks the USB serial port |
| API version mismatch at launch | Run `ufbt update` to download the matching SDK, then rebuild |
| Build fails with missing headers | Ensure you are using a supported uFBT version (`pip install --upgrade ufbt`) |
| App does not appear in the Infrared menu | Confirm the `.fap` was copied to `apps/Infrared/` on the SD card (not the root) |
