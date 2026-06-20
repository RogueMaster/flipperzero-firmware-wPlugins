# Installation

## Quick Install (pre-built)

1. Copy `gps_track.fap` to `/ext/apps/GPIO/` on your Flipper SD card
2. Copy contents of `demo_data/` to `/ext/apps_data/gps_track/` (optional, for demo POI)

## Build from Source

**Requirements:** Python 3, ufbt

```bash
# Install ufbt
pip install ufbt

# Update to Unleashed SDK
py -m ufbt update --index-url=https://up.unleashedflip.com/directory.json

# Clone and build
git clone https://github.com/your-username/flipperzero-gps-track
cd flipperzero-gps-track
git submodule update --init
py -m ufbt
```

Built FAP will be at `.ufbt/build/gps_track.fap`

## Demo Data

Copy `demo_data/*.csv` to `/ext/apps_data/gps_track/` to get:
- 30 major Russian cities as POI
- Sample Moscow walking track
- Moscow timezone (UTC+3) pre-configured

## First Run

1. Connect GPS module (see README for pinout)
2. Launch app from GPIO category
3. If no data: Settings → Baud → try 38400 or 115200
4. Use NMEA dump in Settings to diagnose connection issues
