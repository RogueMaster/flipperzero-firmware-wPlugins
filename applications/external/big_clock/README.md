# Big Clock v1.3

> Full-screen digital bedside/tableside clock with adjustable brightness

![Big Clock Screenshot](screenshots/screenshot1.png)

[![Flipper Zero](https://img.shields.io/badge/Flipper%20Zero-FF6600?style=flat&logo=flipper&logoColor=white)](https://flipperzero.one/)
[![Version](https://img.shields.io/badge/version-1.3-blue)](../../releases)
[![Release](https://img.shields.io/github/v/release/Eris-Margeta/flipper-apps?filter=big-clock-v*&label=release)](https://github.com/Eris-Margeta/flipper-apps/releases?q=big-clock)
[![CI](https://github.com/Eris-Margeta/flipper-apps/actions/workflows/ci.yml/badge.svg)](https://github.com/Eris-Margeta/flipper-apps/actions/workflows/ci.yml)

## Download

**[⬇️ Download Latest Release](https://github.com/Eris-Margeta/flipper-apps/releases?q=big-clock)** - Get the `.fap` file and copy to `/ext/apps/Tools/` on your Flipper.

## Use Case

Turn your Flipper Zero into a **bedside or desk clock** with:
- Large, easy-to-read digits visible from across the room
- Adjustable brightness from completely off (0%) to your system maximum
- Perfect for nightstands, desks, or anywhere you need a simple clock

## Features

- **Large Display**: Full-screen time display with custom 24x48 pixel digits
- **Adjustable Brightness**: 21 levels from 0% (off) to 100% in 5% increments
- **Visual Feedback**: Shows brightness bar and percentage when adjusting
- **Always On**: Backlight stays on while the app is running (no timeout)
- **Flicker-Free**: Smooth brightness transitions without screen flashing
- **Power Efficient**: Updates only once per minute (since we display HH:MM only)

## Controls

| Button | Action |
|--------|--------|
| UP | Increase brightness (+5%) |
| DOWN | Decrease brightness (-5%, down to 0% = off) |
| BACK | Exit application |

## Brightness Control

> **Important Note**: This app controls brightness as a **percentage of your system's maximum backlight setting**.
>
> The maximum brightness is determined by:
> **Settings > LCD and Notifications > LCD Backlight**
>
> For example:
> - If your system backlight is set to 50%, then 100% in this app = 50% actual brightness
> - If your system backlight is set to 100%, then 100% in this app = full brightness
>
> **0% brightness** turns the backlight completely off (screen blank but clock still running).

## Screenshots

![Big Clock Screenshot 2](screenshots/screenshot2.png)

alternate style (thinking about it)
```
    ██  ██████    ██████  ██████
    ██      ██  ●     ██  ██
    ██  ██████  ●  █████  ██████
    ██  ██          ██        ██
    ██  ██████    █████  ██████
```

## Building

**Prerequisites:** [ufbt](https://github.com/flipperdevices/flipperzero-ufbt) (micro Flipper Build Tool)

```bash
# Using Poetry (recommended)
poetry run python -m ufbt           # Build only
poetry run python -m ufbt launch    # Build + install + run

# Using pip
pip install ufbt
python -m ufbt launch
```

The compiled `.fap` file will be in the `dist/` directory.

## Installation

**Via ufbt (recommended):**
```bash
poetry run python -m ufbt launch
```

**Manual:** Build the app with `ufbt`, then copy `dist/big_clock.fap` to your Flipper Zero's SD card at `/ext/apps/Tools/`

## Technical Details

| Property | Value |
|----------|-------|
| Target | Flipper Zero |
| App Type | External (.fap) |
| Category | Tools |
| Stack Size | 2KB |
| Version | 1.3 |

**Implementation:** Uses direct notification settings modification for flicker-free brightness control. Screen updates every 60 seconds (power efficient for HH:MM display). Brightness is reapplied each update cycle to prevent firmware timeout reversion.

## Version History

See [changelog.md](changelog.md) for full version history.

Current version: see [VERSION](VERSION)

## Known Issues

**Brief flicker on button press/release:** When adjusting brightness, a brief flicker may occur at the moment of button press and release. This appears to be a hardware/firmware limitation related to how the Flipper Zero's backlight system handles brightness changes. The brightness change itself works correctly.

## Tested Firmware

| Firmware | Version | Status |
|----------|---------|--------|
| Official Flipper | 1.43 | ✅ Works |
| Momentum | mntm-012 | ✅ Works |
| Unleashed | - | ❓ Not tested |
| RogueMaster | - | ⛔ Not supported (firmware too unstable) |

## License

MIT License - see [LICENSE](../../LICENSE)

## Author

**Eris Margeta** ([@Eris-Margeta](https://github.com/Eris-Margeta))

---

Part of [flipper-apps](https://github.com/Eris-Margeta/flipper-apps) monorepo.
