# Getting started with OSM Logger — first contribution guide

🇫🇷 French version: [GETTING_STARTED_OSM.md](GETTING_STARTED_OSM.md)

This guide walks you **from zero** to your first POI added to OpenStreetMap using OSM Logger. No prior OSM knowledge required.

---

## 🗺️ Why contribute to OSM?

**OpenStreetMap** is a free world map built by millions of volunteer contributors. When you add a bench, a fountain or a defibrillator, your data is **immediately usable** by:
- Mapping apps (Organic Maps, OsmAnd, MapComplete…)
- Delivery, logistics, emergency services (lives have been saved thanks to mapped defibrillators)
- Researchers, city planners, travellers

OSM Logger lets you enrich that map **without taking out your phone** and **without a network connection** — you record in the field, you import later at your desk.

---

## 🧩 What you need

- A Flipper Zero
- An NMEA GPS module (NEO-6M V2 recommended, ~€5 on AliExpress)
- 3 female-to-female dupont wires
- 5 min to wire
- 5 min to install the app
- A microSD in the Flipper (usually already there)

**Not needed**: an OSM account to log in the field. You'll only need one **when uploading** your contributions.

---

## 🔧 Step 1 — Connect the GPS

3 wires, safe if you respect 3.3 V:

| NEO-6M | Wire     | Flipper          |
|--------|----------|------------------|
| VCC    | 🔴 red   | pin 9 (3V3)      |
| GND    | ⚫ black  | pin 11           |
| TX     | 🟢 green | **pin 14** (RX)  |

⚠️ **Do not connect** the NEO-6M at 5 V. Its TX would output 5 V and could fry the Flipper UART.

Full schema: [HARDWARE.en.md](HARDWARE.en.md).

---

## 📲 Step 2 — Install the app

**Method 1 (recommended)**: via the [Flipper Apps Catalog](https://lab.flipper.net) → search "OSM Logger" → Install.

**Method 2 (developer)**: compile from source:

```bash
python3 -m pip install --upgrade ufbt
git clone https://github.com/simongrossi/flipperzero-osm-logger-gps
cd flipperzero-osm-logger-gps
ufbt launch
```

---

## 🛰️ Step 3 — First GPS fix

1. Launch the app: **Apps → GPIO → OSM Logger**
2. Go **outside**, clear sky (not under a tree, not in a dense city)
3. Wait 30 s to 2 min on first boot (the GPS downloads its ephemeris data)
4. In the menu, open **GPS status** to monitor the fix in real time
5. When the green LED blinks + vibration → **fix acquired** 🎉

Some on-screen indicators:
- `FIX OK`: the GPS has a valid position
- `sats=N`: number of satellites used (4+ = good, 7+ = great)
- `HDOP`: horizontal precision (1-2 = excellent, 2-5 = OK, 5+ = rough)
- `~Xm`: estimated precision in meters (HDOP × 3)
- `NMEA: X B / Y lines`: raw incoming data (handy if the GPS captures nothing)

---

## 🎯 Step 4 — Log a POI

1. Main menu → **Quick Log (waypoints)**
2. Pick a category (Street furniture / Roads / Emergency / …) then a preset (Bench, Waste basket, Defibrillator…)
3. You land on the Quick Log screen:
   - Top: the preset name (e.g. `Bench 2/4`) and its OSM tag (`amenity=bench;material=wood`)
   - Bottom: live coordinates + save counter
4. Keys to remember:

| Key         | Action                                                              |
|-------------|---------------------------------------------------------------------|
| `←` / `→`   | Cycle through variants (wood / metal / concrete…)                   |
| `Up`        | Edit a short note (e.g. "vandalised", "wooden")                     |
| `Down`      | Clear the note                                                      |
| `OK` short  | Save (if fix OK, HDOP ≤ configured threshold, default 5.0)          |
| `OK` long   | Force save even with a degraded fix                                 |

**Golden rule**: **stand next to the POI** (within 2 m) before pressing OK. The GPS has ~2-5 m precision.

### 🎯 Precision mode (Averaging) — recommended for serious OSM

Consumer GPS is noisy (±3-5 m per sample). For an important POI, enable **averaging**:

1. Menu → Settings → **Averaging** → cycle up to `10s` (or `30s` for higher precision)
2. Back to Quick Log. Now **OK short** starts a capture instead of a direct save.
3. An `Averaging... N/10 s` screen appears. Stand still for the duration.
4. At the end, the app saves ONE point with the **averaged coordinates** of the samples + the **best HDOP observed** during capture.
5. The note automatically contains `avg` so you can distinguish these points at post-processing.

Typical gain: 30 s averaging → ~1 m precision vs 5 m. Perfect for well-located POIs (notable trees, markers, defibrillators).

`OK long` always bypasses averaging and does an instant save (useful when you don't have time to wait).

### 🚫 Duplicate detection

By default, if you try to save a tag another point already has within 10 m, a **`Duplicate?`** screen appears with the exact distance. You can:
- **OK** = save anyway (force)
- **Back** = cancel

Configurable via Settings → `Dup check` (off / 5m / 10m / 25m). Avoids duplicates when you pass by the same spot twice.

---

## 🏠 Step 5 — Logging street numbers

The app includes a special preset **House number** (Address category). It saves a placeholder tag `addr:housenumber=TBD`. Workflow:

1. Stand in front of house **#23** Victor Hugo Street
2. Quick Log → **Address** category → **House number**
3. Press `Up` to open the note editor, type **`23`**, OK
4. Back to Quick Log → OK to save → you get a point with `addr:housenumber=TBD` and `note=23`
5. At post-processing (JOSM), replace `TBD` with `23` on each point, delete the note

Even better: in **JOSM**, use the `utilsplugin2` plugin to batch-generate numbers from a field.

**Building** and **Building entry** presets are also available to enrich addresses.

---

## 📸 Step 6 — Photo workflow (optional)

If you also take photos to document, enable **Auto photo ID** in Settings. On each save, the app automatically adds `photo:<N>` in the note (N auto-incremented). You take photos **in the same order** and at import time you know which photo matches which point.

**Example**:
- Point 1: `photo:1` → your photo 1 = bench #1
- Point 2: `photo:2` → your photo 2 = the waste basket
- ...

Your photos keep their `IMG_1234.jpg` names, but you can align them on the Flipper numbers via a script later.

---

## 💾 Step 7 — Retrieve the data

At end of session:

**Via qFlipper** (official desktop app):
1. Plug the Flipper via USB
2. Launch qFlipper → **File Manager** → navigate to `/ext/apps_data/osm_logger/`
3. Download these files to your computer:
   - `points.gpx` — for JOSM / iD
   - `points.geojson` — for QGIS / geojson.io
   - `points.jsonl` — rich format with note / full timestamps
   - `notes.csv` — for Excel
   - `track.gpx` — your path (if you used Track mode)

**Without a computer, USB mode**: from the Flipper, `Apps → Settings → Storage → Mass Storage mode` → your Flipper appears as a USB drive.

**Archive session before transfer** (optional): Menu → Last points → Archive session. Moves all current output files to a timestamped subfolder `session_YYYYMMDDTHHMMSS/`. Resets counters, so next session starts fresh. Handy for separating outings.

---

## ✏️ Step 8 — Importing into JOSM

[JOSM](https://josm.openstreetmap.de/) is the reference OSM editor — free, powerful.

1. Install JOSM (Java 8+ required)
2. **File → Open** → choose `points.gpx`
3. Your waypoints appear on the map with their tags (`amenity=bench` etc.)
4. For each waypoint:
   - Click it
   - Check you're at the right position (zoom in, fix with the mouse if needed)
   - Open the tag editor (`P` key or **Edit → Preferences → Tag**)
   - Verify / enrich tags: add e.g. `check_date=2026-04-19`, `source=survey`
5. Once everything's good, **File → Upload** → authenticate with your OSM account → describe your changeset (e.g. "Added benches and waste baskets in port area")

Congrats, your data is on OSM in minutes! ⭐

---

## 🎨 Simpler alternative: MapComplete / iD

If JOSM feels intimidating, use **[iD](https://www.openstreetmap.org/edit)** (the official OSM web editor, simpler).

Workflow:
1. On openstreetmap.org, log in
2. Zoom to the area of your walk
3. Click **Edit**
4. Open `points.geojson` in [geojson.io](https://geojson.io) in parallel to see where your points are
5. In iD, manually add each POI at the matching coordinates, with tags

More manual but zero setup.

---

## 💡 OSM best practices

- **Don't duplicate**: before logging, check if the POI already exists on OSM (use https://www.openstreetmap.org/#map=19/YOUR_LAT/YOUR_LON)
- **Be precise**: less than 3 m between your position and the actual POI
- **source=survey**: add this tag at import — it tells the community you verified on the ground (highly valued)
- **photo=yes / photo=no**: some contributors map from guesses. Indicate if your tag comes from a verified photo or an assumption
- **Respect the terrain**: don't log inside private buildings, private property nobody asked you to map, etc.
- **Start small**: 5 well-placed points beat 50 sloppy ones

---

## 🆘 Troubleshooting

- **GPS catches nothing**: check the TX/RX wiring (GPS TX → Flipper pin **14**); verify on GPS status that `NMEA: X B / Y lines` is growing
- **Fix never acquired**: really go outside, wait 5 min, check that `sats` goes up
- **Weird coordinates (0,0 or far away)**: fix not yet established, wait
- **App crashes**: reboot (hold Back), try again. If it persists, open an issue at https://github.com/simongrossi/flipperzero-osm-logger-gps/issues

For more debug, the Flipper CLI:

```bash
ufbt cli
> log debug
```

Then reproduce the bug — internal errors show up.

---

## 🔗 Going further

- [OpenStreetMap Wiki](https://wiki.openstreetmap.org/) — the OSM encyclopedia, finds the right tags
- [taginfo.openstreetmap.org](https://taginfo.openstreetmap.org/) — which tags are actually in use
- [OSM MapComplete](https://mapcomplete.org) — web interface to add simple POIs
- [Learn OSM](https://learnosm.org) — official tutorials for beginners

**Happy mapping! 🗺️**
