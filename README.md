# Flo Period Tracker for Flipper Zero

A menstrual cycle tracking application for Cat on her Flipper, following a standard 14-day luteal phase model.

## Author

Harim Woo

## Features

- **Status Screen** — Shows current cycle day, days until next period, fertile window status, and average cycle length
- **Calendar View** — Monthly calendar with period days highlighted (filled blocks). Navigate months with Left/Right
- **Log Period** — Record a new period with date and duration using the D-pad
- **Settings** — Configure average cycle length (20–45 days) and default period duration (1–14 days)
- **Predictions** — Automatic prediction of next period, ovulation, and fertile window based on logged data
- **Persistent Storage** — All data is saved to the Flipper's SD card and persists across reboots

## How to Build

### Prerequisites

1. Clone the [Flipper Zero firmware](https://github.com/flipperdevices/flipperzero-firmware)
2. Set up the build environment following the [official docs](https://github.com/flipperdevices/flipperzero-firmware/blob/dev/documentation/fbt.md)

### Build Steps

1. Copy the `flipper-flo-tracker` folder into the firmware's `applications_user/` directory:
   ```
   cp -r flipper-flo-tracker /path/to/flipperzero-firmware/applications_user/flo_tracker
   ```

2. Build the app using `fbt` (Flipper Build Tool):
   ```
   cd /path/to/flipperzero-firmware
   ./fbt fap_flo_tracker
   ```

3. The compiled `.fap` file will be in `build/f7-firmware-D/.extapps/flo_tracker.fap`

4. Copy it to your Flipper's SD card under `apps/Tools/` or use:
   ```
   ./fbt launch_app APPSRC=applications_user/flo_tracker
   ```

### Using ufbt (Standalone)

Alternatively, use `ufbt` for standalone builds without cloning the full firmware:

```bash
# Install ufbt
pip install ufbt

# Navigate to the app directory
cd flipper-flo-tracker

# Build
ufbt

# Flash to Flipper (connected via USB)
ufbt launch
```

## Usage

1. **First launch** — Go to "Log Period" and enter the start date and duration of your most recent period
2. **Status** — The main status screen shows predictions based on your logged data
3. **Calendar** — Visual overview of your cycle with period days marked
4. **Settings** — Adjust cycle length and period duration defaults (auto-calculated after 2+ logged periods)

## File Structure

```
flo_tracker/
├── application.fam      # Flipper app manifest
├── flo_tracker.c         # Main app entry, menu, settings, lifecycle
├── flo_app.h             # App struct and view model definitions
├── flo_data.h            # Data types, date utils, prediction API
├── flo_data.c            # Date math, storage, prediction engine
├── flo_views.h           # View allocation function declarations
├── flo_views.c           # Status, Calendar, and Log Period views
├── icons/                # App icon (10x10 PNG for launcher)
└── README.md
```

## Notes

- The app icon (`icons/flo_10x10.png`) needs to be a 10x10 pixel 1-bit PNG. You can create one with any image editor — a simple droplet or flower shape works well.
- Data is stored at `apps_data/flo_tracker/flo_data.bin` on the SD card.
