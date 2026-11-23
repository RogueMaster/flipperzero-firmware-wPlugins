# Clock'o'Dial

**Clock out before burnout!**

A Flipper Zero app that turns the device into a work-shift and overtime timer for remote workers.
Built for Flipper Zero to reduce distraction and not to draw attention to your phone.

![](https://raw.githubusercontent.com/wiki/ManeFunction/clock-o-dial--fz/screenshot-1.png)    ![](https://raw.githubusercontent.com/wiki/ManeFunction/clock-o-dial--fz/screenshot-2.png)

![](https://raw.githubusercontent.com/wiki/ManeFunction/clock-o-dial--fz/screenshot-3.png)    ![](https://raw.githubusercontent.com/wiki/ManeFunction/clock-o-dial--fz/screenshot-4.png)

## Features

12-hour analog dial showing actual time of day, tracking elapsed work time, breaks, and a prediction of how much of your shift is left.
Hourly chimes, vibro, always-on backlight (everything is optional), **highly optimized** as a full-time running app.

## Installation

### From the official Flipper Apps catalog (recommended)

1. Open the [Clock'o'Dial page](https://lab.flipper.net/apps/clock_o_dial) on Flipper Lab, or find **Clock'o'Dial** in the **Tools** category of the Apps tab in the [Flipper mobile app](https://flipperzero.one/downloads).
2. Connect your Flipper Zero (USB for Flipper Lab, Bluetooth for the mobile app) and press **Install**.
3. On the Flipper, launch it from **Apps → Tools → Clock'o'Dial**.

Installing from the catalog keeps the app up to date through the same Flipper Lab / mobile app.

### Manually from a GitHub release

1. Download `clock_o_dial.fap` from the [latest release](https://github.com/ManeFunction/clock-o-dial--fz/releases/latest).
2. Copy it to `apps/Tools/` on the Flipper's SD card, using [qFlipper](https://flipperzero.one/downloads) (File manager), Flipper Lab, or a card reader.
3. On the Flipper, launch it from **Apps → Tools → Clock'o'Dial**.

Release builds target the **Release** firmware channel. If the Flipper says the app is outdated or
incompatible, update the firmware to the latest release, or [build the app from source](#building-from-source)
against your firmware.

## Controls

**Setting up a shift** (before you start):
- **Left / Right** - adjust the configured shift length (1-12 hours)
- **Up / Down** - switch the time format between `H:MM` and `H:MM:SS`
- **OK** (tap) - start the shift
- **OK** (hold) - open the info and options
- **Back** (hold) - close the app

**While working or on a break:**
- **OK** (tap) - pause / resume
- **OK** (hold) - reset back to the shift-setup screen
- **Up** - toggle mute
- **Down** - toggle backlight
- **Left** - toggle vibration
- **Right** - toggle eco mode
- **Back** (hold) - close the app

**Info and options screen:**
- **Left / Right** - cycle between pages
- **Back** - return to the shift-setup screen

## About the optimization

As an app made to run at least a third of the day, it is highly optimized, buffering every heavy calculation needed for rendering.
Rendering runs at one screen refresh per second when animation is active, or two refreshes per minute in eco mode.

My personal tests show it runs on my Flipper Zero with very little overhead.
Here are some battery drain stats for a **9-hour** time period:
- Nothing is running: **-2%** (for reference)
- In eco mode without backlight: **-3%**
- In normal mode without backlight: **-4%**
- With backlight active: **-11%**

So, basically, the app itself adds about **1–2%** drain for a full working day.

## Building from source

This is a Flipper Zero external app (FAP), built with [ufbt](https://github.com/flipperdevices/flipperzero-ufbt):

```bash
pip install ufbt
ufbt
```

To build and immediately launch it on a connected Flipper:

```bash
ufbt launch
```

Or simply connect the Flipper Zero to your machine and run `launch_app.sh`.

## License

The source code is licensed under the [Apache License 2.0](LICENSE). The "Clock'o'Dial" name and
icon are trademarks and are **not** covered by that license - see [TRADEMARKS.md](TRADEMARKS.md)
before publishing a fork under the same name.

## Repository info

This repo follows the [Conventional Commits](https://www.conventionalcommits.org/) specification.

[![GitHub Sponsors](https://img.shields.io/github/sponsors/ManeFunction?label=Sponsor&logo=GitHubSponsors&style=flat)](https://github.com/sponsors/ManeFunction)
