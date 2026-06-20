# GPS Track for Flipper Zero

GPS navigation app for Flipper Zero with breadcrumb trail recording, POI navigation, LED guidance, and area measurement. Built for [Unleashed firmware](https://github.com/DarkFlippers/unleashed-firmware).

## Versions

| Version | Use case |
|---------|----------|
| **v1.0** | First-time setup, NMEA diagnostics, testing new GPS modules |
| **v2.0** | Daily use with a working GPS module |

**v1.0** includes the **NMEA dump tool** — captures raw serial data and diagnoses baud rate issues, malformed output, or missing fix. Start here if you just got a new module.

**v2.0** replaces the dump tool with **area measurement** (Shoelace formula, up to 30 adaptive vertices), cleans up the UI, and fixes several navigation bugs.

→ Download both from the [Releases](../../releases) page.

---

## Hardware

Connect any NMEA 0183 GPS module to Flipper Zero GPIO:

| GPS pin | Flipper Zero |
|---------|-------------|
| VCC | Pin 9 (3.3V) |
| GND | Pin 11 |
| TX | Pin 14 (RX) |
| RX | Pin 13 (TX) — optional |

**Tested modules:**
- Quescan G10A F30 (AG3335M) — GPS/GLONASS/Galileo/BeiDou/QZSS, default 38400 baud
- GY-NEO6MV2 (u-blox NEO-6M) — GPS only, default 9600 baud

If the module doesn't respond, use **v1.0 NMEA dump** to check the baud rate and signal quality before switching to v2.0.

---

## Features

### Main screen
Latitude, longitude, speed, course, altitude, satellite count, local time (UTC + timezone offset).

**LED status indicators:**

| Color | Pattern | Meaning |
|-------|---------|---------|
| Red | 1× / sec | No GPS signal |
| Blue | 2× / sec | Signal received, no fix yet |
| Green | 1× / 3 sec | Fix acquired |

### Breadcrumbs (CRUMBS)
Records your path automatically: first point 1 min after fix, then every 10 min, minimum 50 m between points. Follow the trail back with a direction arrow, distance markers, and timestamps.

**LED navigation:**

| Color | Meaning |
|-------|---------|
| Green | On course (< 15° error) |
| Yellow | Slight left (15–45°) |
| Cyan | Slight right (15–45°) |
| Magenta | Far left (45–90°) |
| Blue | Far right (45–90°) |
| Red | Wrong direction (> 90°) |
| White | Waypoint switched |

LED is off when stopped, charging, or outside navigation screens.

### POI Navigation (NAV)
Save named waypoints, navigate to them with bearing, distance, and ETA. Sorts by distance on entry.

### Area Measurement (v2.0)
Settings → Start area calc → walk the perimeter → Settings → OK.

- Averages GPS coordinates over 5-second windows to reduce jitter
- Adds a vertex every 5 ticks if moved > 2 m
- Maintains up to 30 adaptive vertices (discards the least significant point when limit reached)
- Displays `S=XXXX m2` in real time
- Saves polygon to `area.csv` on exit or reset

### Settings
Speed units, baud rate, deep sleep, timezone offset (UTC±).

---

## Navigation

```
Long ← / →    Ring:  NORMAL ↔ NAV ↔ CRUMBS
Back           Return to NORMAL from any screen
```

| Screen | Input | Action |
|--------|-------|--------|
| NORMAL | OK | Settings |
| NORMAL | Long ↑ | Save HOME POI |
| NORMAL | Long ↓ | Save new POI |
| CRUMBS | Long OK | Crumbs menu |
| CRUMBS | ↑ / ↓ | Navigate waypoints |
| CRUMBS | OK | Toggle total distance |
| NAV | Long ↑ | Select POI from list |
| NAV | Long ↓ | Enter coordinates manually |
| NAV | OK | Toggle speed / ETA |
| NAV list | Long ← | Delete POI |
| NAV list | Long → | Rename POI |

---

## Building from source

```bash
pip install ufbt
py -m ufbt update --index-url=https://up.unleashedflip.com/directory.json
git clone https://github.com/xumbax/flipperzero-gps-track
cd flipperzero-gps-track
py -m ufbt
```

---

## Data files

All stored at `/ext/apps_data/gps_track/`:

| File | Format | Description |
|------|--------|-------------|
| `track.csv` | `lat,lon,HH:MM` | Breadcrumb trail |
| `poi.csv` | `icon,lat,lon,name` | Points of interest |
| `state.txt` | `crumbs_target,poi_target` | Navigation state |
| `settings.csv` | `speed_unit,tz,baud_idx,deepsleep` | Settings |
| `area.csv` | `lat,lon` per line | Last measured polygon |

See `demo_data/` for example files including 30 major Russian cities as POI.

---

## Credits

NMEA parsing: [minmea](https://github.com/kosma/minmea)  
Based on [flipperzero-gps](https://github.com/ezod/flipperzero-gps) by ezod
