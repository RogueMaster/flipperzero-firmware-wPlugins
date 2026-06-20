# flipperzero-osm-logger-gps

[![FAP Build](https://github.com/simongrossi/flipperzero-osm-logger-gps/actions/workflows/fap-build.yml/badge.svg)](https://github.com/simongrossi/flipperzero-osm-logger-gps/actions/workflows/fap-build.yml)

> Log **OSM POIs** in the field with a **Flipper Zero** + **NEO-6M GPS** :
> pick a POI type (bench, bin, drinking water, defibrillator…), save lat/lon/alt/timestamp/OSM tag
> to microSD in **6 native formats** (JSONL, CSV, GPX with OsmAnd extensions, GeoJSON, OSM XML, GPX track).

![osm_logger icon](osm_logger.png)

---

## ✨ Features

- **NMEA 0183** GPS reader (RMC + GGA) over UART, configurable baud rate (4800 / 9600 / 19200 / 38400 / 57600 / 115200)
- Auto-notification (vibration + green LED) when GPS fix is acquired
- **Battery badge** displayed on every live screen
- **GPS Status** screen: fix, lat/lon, altitude, HDOP, satellites, fix age, live NMEA RX counters
- **Settings menu** with persisted prefs on SD: baud rate, track interval, track min distance filter, track HDOP strict toggle, preview-before-save toggle, auto photo ID, duplicate detection threshold, averaging duration, HDOP max gate, **survey mode**
- **Survey mode** — toggle in Settings that auto-appends the canonical OSM tags `source=survey` + `survey:date=YYYY-MM-DD` to every save. These are the OSM community norm to signal "verified on site", appreciated by reviewers.
- **Averaging mode**: collect N seconds of GPS samples (5 / 10 / 30 / 60 s) and save one point with the averaged coordinates and best HDOP observed. Reduces consumer-GPS noise to OSM-grade precision. `OK long` still gives an instant force save.
- **Duplicate detection**: when saving, if a point with the same tag exists within X meters, a confirmation screen appears ("Same tag Ym away — OK=save anyway, Back=cancel"). Threshold configurable (off / 5 / 10 / 25 m).
- **Clear save-error screens** when the SD is absent / full / unwritable.
- **Main menu with icons** (Menu module, one dedicated icon per entry)
- **Quick Log (waypoints)** mode:
  - Navigation in **two levels**: pick a category → pick a preset inside
  - **65 OSM presets** by default grouped in 15 categories (Street furniture, Roads & signs, Parking, Sports & leisure, Waste, Shops, Services, Emergency, Tourism, Nature, Education, Religion, Transport, Address, Other)
  - **SD-loadable presets**: drop a `presets.txt` on the SD to replace the built-in list without recompiling (up to 128 presets). A helper script [`scripts/build_presets.py`](scripts/build_presets.py) generates such a file from the **official OSM [id-tagging-schema](https://github.com/openstreetmap/id-tagging-schema)** (hundreds of community-maintained presets, multi-lingual translations). See [docs/CUSTOM_PRESETS.en.md](docs/CUSTOM_PRESETS.en.md) · [docs/CUSTOM_PRESETS.md](docs/CUSTOM_PRESETS.md).
  - **Preset variants** cyclable with `←` / `→` — alternative values *or* additional secondary tags (e.g. `Bench` → `amenity=bench` → `amenity=bench;material=wood`)
  - **Short note editor** via `Up` (Flipper TextInput), `Down` to clear
  - Live screen: coords, altitude, HDOP, sats, fix age, session + cumulative counters
  - **Short `OK`**: save if `fix OK` and `HDOP ≤ 2.5`, error tone otherwise (or preview screen if enabled)
  - **Long `OK`**: force save regardless of fix quality, bypasses preview
- **Track mode (auto GPX log)**:
  - `<trkpt>` written every N seconds while the view is active and a fix is available
  - Distance filter: skips points that didn't move far enough (configurable threshold)
  - Heading / course over ground displayed live
  - **Live stats on screen**: session duration, points count, cumulative distance, instant speed, max speed
  - Each session starts a new `<trkseg>` (no bogus line between separate sessions)
  - Ideal for mapping a street or path on foot or bike
- **Last points browser** (main menu entry): scrollable list of the 10 most recent saves. Tap any point to see full details (time, coords, altitude, HDOP, sats, tag, note). Actions: **Delete last** (undo across all 4 output files), **Archive session** (moves the 5 output files to a dated subfolder, ready to transfer as a unit via qFlipper) and **Clear all**.
- Saves to `/ext/apps_data/osm_logger/` in **6 native formats** (all valid at all times):
  - `points.jsonl` — one JSON line per point
  - `notes.csv` — spreadsheet-friendly
  - `points.gpx` — GPX 1.1 enriched with **OsmAnd Favorites** extensions (icons + colors + category), direct import into JOSM / iD / QGIS / OsmAnd
  - `points.geojson` — FeatureCollection, direct import into QGIS / geojson.io
  - `points.osm` — **native OSM XML API 0.6**, editable data layer in JOSM (pro workflow: edit + direct OSM upload)
  - `track.gpx` — GPX `<trkseg>` track, import into any GPX viewer
- Works on **stock and Momentum firmware** (the app auto-disables the Expansion service to free the UART)

---

## 🧩 Hardware

- **Flipper Zero** (with microSD)
- **NEO-6M V2 GPS** (u-blox NEO-6) — NMEA output at 9600 bauds
- Ceramic antenna (bundled) or external active antenna

### Wiring (3.3 V, 3 wires)

```
  Flipper Zero GPIO                          NEO-6M GPS
 ┌────┬──────────┐                          ┌──────────┐
 │  9 │ 3V3      ├──────── 🔴 red ────────▶ │ VCC      │
 │ 10 │ SWC      │                          │          │
 │ 11 │ GND      ├──────── ⚫ black ──────▶ │ GND      │
 │ 12 │ SIO      │                          │          │
 │ 13 │ TX       │                          │ RX ◁ ── unused (no command sent)
 │ 14 │ RX       │◀─────── 🟢 green ─────── │ TX       │
 │ 15 │ C1       │                          └──────────┘
 │ 16 │ C0       │
 │ 17 │ 1W       │
 │ 18 │ GND      │
 └────┴──────────┘
```

Only **3 wires** are needed because the app only *reads* NMEA frames from the GPS — never sends commands back.

**⚠️ Use 3.3 V — never 5 V.** See [docs/HARDWARE.md](docs/HARDWARE.md) for details, first-fix tips and troubleshooting.

---

## 🔧 Build & install

This app uses **`ufbt`** (micro Flipper Build Tool) — the official tool for external apps. Python 3.8+ required.

```bash
# 1. Install ufbt
python3 -m pip install --upgrade ufbt    # macOS / Linux
py -m pip install --upgrade ufbt         # Windows

# 2. Clone and build
git clone https://github.com/simongrossi/flipperzero-osm-logger-gps
cd flipperzero-osm-logger-gps
ufbt                    # produces ./dist/osm_logger.fap

# 3. Deploy (Flipper plugged via USB, qFlipper closed)
ufbt launch             # uploads + starts the app
```

> 💡 If `ufbt: command not found`, add `$HOME/Library/Python/3.9/bin` (macOS) or the equivalent to your `PATH`.

Alternatively, drop `dist/osm_logger.fap` into `/ext/apps/GPIO/` via qFlipper.

---

## ▶️ Usage

1. Launch **OSM Logger** from the Apps menu.
2. Main menu → `Quick Log (waypoints)`.
3. Pick a preset (Bench, Pharmacy, Defibrillator, …).
4. **Quick Log screen** — wait for a fix (`fix=Xs` shows the age of the last fix).

| Key         | Action                                                       |
|-------------|--------------------------------------------------------------|
| `OK` short  | Save if `fix OK` and `HDOP ≤ 2.5` (error tone otherwise)     |
| `OK` long   | Force save despite missing fix or high HDOP                  |
| `←` / `→`   | Cycle through variants (if the preset has multiple values)   |
| `Up`        | Open the note editor (Flipper TextInput)                     |
| `Down`      | Clear the current note                                       |
| `Back`      | Back to the preset list                                      |

5. At the end of a session, grab the files via qFlipper (File Manager) or USB mass storage:
   - `/ext/apps_data/osm_logger/points.jsonl`
   - `/ext/apps_data/osm_logger/notes.csv`
   - `/ext/apps_data/osm_logger/points.gpx`
   - `/ext/apps_data/osm_logger/points.geojson`
   - `/ext/apps_data/osm_logger/track.gpx`

The main menu also has `GPS status` (dedicated diagnostic view) and `About`.

### Quick Log screen at a glance

```
     < Cafe 2/4 >           <- preset + current variant (if >1)
     amenity=pub            <- effective OSM tag
    48.43123, -0.09321      <- live coords
    HDOP=1.3 sats=9  #12    <- fix quality + session counter
    alt=45m fix=3s  t=247   <- altitude, fix age, total in file
  OK save  Up:note  <>:var  <- key hints (or "note: ..." when set)
```

---

## 📷 Screenshots

| Main menu | Categories | Presets |
|-----------|------------|---------|
| ![main menu](screenshots/01-menu.png) | ![categories](screenshots/02-categories.png) | ![presets](screenshots/03-presets.png) |

| Quick Log | Note editor | Post-save toast |
|-----------|-------------|-----------------|
| ![quick log](screenshots/04-quicklog.png) | ![note editor](screenshots/05-note-editor.png) | ![quick log saved](screenshots/06-quicklog-saved.png) |

| Track mode | Last points | Settings | About |
|------------|-------------|----------|-------|
| ![track mode](screenshots/07-track.png) | ![last points](screenshots/08-last-points.png) | ![settings](screenshots/09-settings.png) | ![about](screenshots/10-about.png) |

## 📚 Documentation

**📖 Bilingual docs index**: [docs/README.md](docs/README.md) — pick English 🇬🇧 or French 🇫🇷

Direct links:
- Hardware & wiring: 🇬🇧 [HARDWARE.en.md](docs/HARDWARE.en.md) · 🇫🇷 [HARDWARE.md](docs/HARDWARE.md)
- Output formats: 🇬🇧 [FORMATS.en.md](docs/FORMATS.en.md) · 🇫🇷 [FORMATS.md](docs/FORMATS.md)
- OSM beginner tutorial: 🇬🇧 [GETTING_STARTED_OSM.en.md](docs/GETTING_STARTED_OSM.en.md) · 🇫🇷 [GETTING_STARTED_OSM.md](docs/GETTING_STARTED_OSM.md)
- Custom presets (SD override + iD schema importer): 🇬🇧 [CUSTOM_PRESETS.en.md](docs/CUSTOM_PRESETS.en.md) · 🇫🇷 [CUSTOM_PRESETS.md](docs/CUSTOM_PRESETS.md)
- Developer docs (French only for now): [DEVELOPMENT.md](docs/DEVELOPMENT.md), [ROADMAP.md](docs/ROADMAP.md)
- [src/presets.c](src/presets.c) — the 65 default presets (editable by rebuilding, or override via `presets.txt` on SD)
- [presets.txt.sample](presets.txt.sample) — sample presets file (in French, shows the syntax)
- [scripts/build_presets.py](scripts/build_presets.py) — generate a `presets.txt` from the official OSM [id-tagging-schema](https://github.com/openstreetmap/id-tagging-schema) (hundreds of community-maintained presets, multi-lingual). See [docs/CUSTOM_PRESETS.en.md](docs/CUSTOM_PRESETS.en.md).

---

## 🗺️ OSM workflow

- **JOSM / iD**: import `points.gpx` directly (waypoints named with their OSM tag).
- **QGIS / geojson.io**: import `points.geojson` directly.
- **OSM Notes**: `notes.csv` is a text base you can post manually.
- **GPX track**: import `track.gpx` into JOSM, GPX Viewer or Gaia GPS to visualise your route.
- **Alternative**: `python3 scripts/jsonl_to_geojson.py points.jsonl > points.geojson` if you prefer to start from JSONL.

---

## 🚧 Roadmap

See **[docs/ROADMAP.md](docs/ROADMAP.md)** for the full list of planned features with implementation hints.

Short summary:
- Preset variants as additional tags (not just alternative values)
- Sub-categories in the preset menu
- Distance filter in track mode (skip points that didn't move)
- Persistent notes across sessions
- Persistent notes across sessions
- Support for other GPS modules (PA1010D, BN-180, etc.)

---

## 🖊️ License

MIT — see [LICENSE](LICENSE).
