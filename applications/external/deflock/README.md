<p align="center">
  <img src="assets/logo.png" width="560" alt="FlipDeFlock">
</p>

A Flipper Zero app that pairs your Flipper with **any ESP32 board** for
counter-surveillance site surveys:

- 🛰️ **Flock / ALPR Detect** — finds Flock Safety / ALPR cameras over Wi-Fi and
  BLE (the companion firmware interleaves a 2.4 GHz sniff with a BLE scan),
  geotags them with GPS, and marks them for a report.
  
- 🗺️ **On-device Map** — a live Flipper-screen map plotting detected cameras by
  bearing and distance around your GPS position (auto-fit, heading, scale bar).
  Lets you see what's nearby in the field without exporting.
  
- 📶 **WiFi Audit** — scans nearby networks and grades each one's security
  (Open/WEP/WPA1/WPA2/WPA3, WPS, TKIP, hidden), spelling out what's weak, and
  flags **evil-twin** SSIDs (same name, multiple BSSIDs). Also does passive
  deauth/disassoc-flood detection that alerts live on the Detect screen.
  *Requires the companion firmware* (Marauder doesn't emit encryption over serial).
  
- 📡 **BLE / Tracker Scan** — detects Flock/Raven BLE beacons and AirTag / Tile /
  SmartTag trackers, and flags any tracker that follows you across GPS waypoints
  (anti-stalking). Positively labels a **Raven (audio sensor)** when it sees the
  Raven's own Bluetooth services. Companion firmware.
  
- 🛡️ **Net Guardian** — a leave-it-on-the-desk "personal net guardian." Keeps the
  ESP running and **rotates** it across Wi-Fi + BLE so the fused
  **CLEAR / WATCHFUL / ELEVATED** "am I being watched?" score stays live, with a
  pwnagotchi-style face and a discreet haptic the moment two independent radios
  agree something's watching. Counts **Flock cameras**, **Flipper Zeros** nearby,
  and **active attacks** — deauth/disassoc floods, evil-twin APs, plus attack-tool
  signatures (BLE-spam, Pineapple/Marauder beacon-spam, probe floods). An opt-in
  **Anomaly flag** adds unidentified-device flagging. (A purely *passive* sniffer
  emits nothing and can't be detected by anything — only active transmitters can.)
  Press **OK** to open a **Suspicious list** (Flippers, anomaly devices, rogue APs)
  and send one straight to the Locator. Companion firmware.
  
- 🎯 **Locator** — **mark** any device (the report star on a Flock / BLE / WiFi
  detection doubles as the Locator pool) and hunt it down by **live signal
  strength**: a hot/cold meter that climbs as you physically get closer, peak-hold,
  and a warmer/colder trend. Works **without GPS** (a fix only adds a "strongest
  here" note). Honest by design — there's no compass arrow (that needs a
  directional antenna); you close in by walking. Companion firmware.
  
- 💳 **NFC / RFID Audit** — identifies a presented card's protocol and grades its
  security posture for access-control reviews. On a MIFARE Classic, a **Deep**
  check captures the UID and tries the Flipper's on-SD key dictionary to report
  how many sectors open with factory/default keys (trivially cloneable).
  
- ⚡ **ESP32 Firmware** — backs up the board's current firmware to SD and flashes
  a `.bin` (companion / Marauder / a backup) straight from the Flipper, no
  computer. Lets you switch between Marauder and the FlipDeFlock companion. (Built
  on Espressif's esp-serial-flasher; put the ESP in bootloader mode first.)
  
- 🗺️ **Reports** — exports to Markdown, DeFlock-compatible GeoJSON (ready to
  contribute to [deflock.org](https://deflock.org)), CSV, and WiGLE CSV (the
  wardriving format) on the SD card.
  
- 📲 **Share to DeFlock** — renders a QR per camera that opens DeFlock on your
  phone so you contribute through the official app. The
  Flipper and ESP never touch a network; submission stays an off-device,
  phone-side action, which keeps the passive-only promise intact. No Flipper GPS
  is needed to contribute — DeFlock lets you place the pin by hand.

> **Passive recon only.** No deauth, no injection, no jamming. For lawful,
> authorized use: your own security assessments, anti-surveillance awareness,
> CTF/research. OUI-only matches are *possible*, not confirmed. Verify by eye.

Built for the [Momentum firmware](https://github.com/Next-Flip/Momentum-Firmware)
(works on stock OFW too).

## Screenshots

<p align="center">
  <img src="assets/screenshots/net-guardian.png" width="420" alt="Net Guardian">
  <br><em>Net Guardian — the always-on "personal net guardian" watch face</em>
</p>

| | |
|:--:|:--:|
| <img src="assets/screenshots/alpr.png" width="330" alt="Flock / ALPR Detect"><br>**Flock / ALPR Detect** | <img src="assets/screenshots/menu.png" width="330" alt="Main menu"><br>**Main menu** |
| <img src="assets/screenshots/ble-scan.png" width="330" alt="BLE / Tracker scan"><br>**BLE / Tracker scan** | <img src="assets/screenshots/ble-scan-results.png" width="330" alt="BLE / Tracker results"><br>**BLE / Tracker results** |
| <img src="assets/screenshots/wifi-audit.png" width="330" alt="WiFi Audit"><br>**WiFi Audit** | <img src="assets/screenshots/esp32-firmware.png" width="330" alt="ESP32 Firmware"><br>**ESP32 Firmware** (backup &amp; flash) |

## On-screen legend

What the numbers/words on each scan screen mean. RSSI is shown as **signal bars**
(taller = stronger) rather than a raw `-33dB` number — the selected/highlighted
row still shows the exact dB.

**Net Guardian** — e.g. `(-_-) CLEAR` · `Flock 0  Atk 0  Flip 0` · `WiFi+BLE ch6  OK=sus` · `0:00:07`
- **face / word** — fused "am I being watched?" state: `(-_-)` CLEAR → `(o_o)` WATCHFUL → `(>_<)` ELEVATED
- **threat meter** — the fused 0–100 surveillance score as a bar; fills as independent signals correlate
- **Flock** — Flock/ALPR camera detections this session (was `hits`)
- **Atk** — active attacks this session: distinct APs under a deauth/disassoc **flood**, plus a live attack-tool signature (BLE-spam, beacon-spam from a Pineapple/Marauder, or a probe flood). Flood-gated, so a lone benign disassoc never counts
- **Flip** — Flipper Zeros advertising nearby (BLE name `Flipper …`)
- **bottom line** — sweep radio (`WiFi+BLE` / `BLE` / `WiFi`) + channel + **`OK=sus`** (press OK for the Suspicious list). On an alert it becomes the live breakdown naming the cause — e.g. `Flipper Zero`, `BLE-spam`, `beacon flood`, `deauth flood`, `evil-twin AP`, `unknown device on you`
- **0:00:07** — guardian uptime (top-right)
- *Detection notes:* a passive sniffer that only **listens** transmits nothing and can't be detected by anything — only active transmitters are. A Flipper alone raises **WATCHFUL**; reaching **ELEVATED** still needs two independent radios to agree. "Unknown device on you" is the opt-in **Anomaly flag** (Settings, default off): an unnamed, unidentified device transmitting strongly and repeatedly right next to you.

**Locator** — hunt a marked device by signal. `Flip 0` becomes a live `-42dB` reading with a hot/cold meter.
- **mark first** — the report star (`*`/Mark/Tag) on any Flock / BLE / WiFi detection adds it to the Locator's target list; or pick one from the Guardian's **OK** Suspicious list. Open **Locator** from the menu to choose a target.
- **meter / dB** — live signal strength; the bar climbs and the number rises as you get **closer**. `WARMER` / `colder` shows the trend; the small tick above the bar is **peak-hold** (the strongest you've seen)
- **out of range** — the target has gone quiet (stopped transmitting or moved off); walk back toward where it was loudest
- *No compass arrow* — direction-finding a transmitter needs a directional antenna; you close in by walking while watching the meter. With GPS, the footer notes when you're at the strongest spot

**Flock / ALPR Detect** — header `ESP ch6  frames 339  hits 0`
- **ESP** (or `...`) — companion connected / still waiting
- **ch / frames / hits** — current channel / 802.11 frames captured / Flock detections. `frames` and `hits` count *this session* (they reset to 0 each time you open the screen), not the ESP's lifetime totals
- **row tag** — confidence: `!` CONFIRMED · `F` probe-fingerprint · `L` Likely · `p` Possible · `.` OUI-only. `*` = marked for the report
- **`!DEAUTH ch<n> <bssid>`** — a deauth/disassoc **flood** is active on that channel/AP
- *(Marauder mode shows `rx <n>  hits <n>` instead — serial-line heartbeat + detection count)*

**BLE / Tracker Scan** — header `BLE 33  trk 9  follow 0`
- **BLE** — total BLE devices seen · **trk** — known trackers (AirTag/Tile/SmartTag/FindMy/Flock) · **follow** — trackers flagged **following you**
- **row** — `<type> <name|MAC-tail> <rssi>dB`; type = `FLOCK` / `AirTag` / `Tile` / `Tag` / `FindMy` / `Flipper` / `BLE` (`Flipper` = a Flipper Zero; it's a recon device, not a tracker, so it isn't in `trk`)
- **prefix** — `!` following (anti-stalking) · `*` tagged for the report

**WiFi Audit** — header `10 AP  2crit 1weak 3twin`
- **AP** — total APs seen · **crit** — critical-grade · **weak** — weak-grade · **twin** — evil-twin/duplicate-SSID counts
- **row grade** — `CRIT` / `WEAK` / `OK` / `STRONG` / `INFO`
- **marker** — `!` rogue/evil-twin (same SSID, mismatched security) · `~` duplicate SSID (mesh?) · `*` tagged. A row shown as `[ABCDEF]` is a hidden SSID (last 3 BSSID bytes)

**Everywhere** — `-33dB` is signal strength (RSSI); closer to 0 = physically closer.

## Status — a work in progress

FlipDeFlock is actively developed, not a finished product. It's already useful in
the field, but features are still landing, the detection signatures change as
surveillance hardware changes, and not every path is hardware-tested on every
board. Expect rough edges and the occasional breaking change between versions.
Treat detections as indicators, not proof, and verify by eye. If you're relying on
it for anything that matters, read the code and confirm the behavior yourself.
Feedback and field data are what move it forward; see [Contributing](#contributing).

## What's new

**v0.42**
- **Detect MAC-randomizing cameras, and update signatures without a rebuild.** Fielded
  Flock cameras now phone home as plain Wi-Fi **probe requests** and rotate their MAC to
  evade OUI matching — so the durable signal is the probe's **IE fingerprint** (the shape
  of its 802.11 info-elements, independent of the MAC). You can now drop an **`"ie_fps"`**
  list into `apps_data/flipdeflock/signatures.json` (next to `ouis` / `ssid_*`), and each
  detection's fingerprint is shown as **`IE-fp:`** on its detail screen so you can harvest
  it from a *confirmed* camera and catch its randomized twins. Still load-only, offline,
  and fail-safe — and an unverified user fingerprint only ever scores a candidate
  **"Class?"**, never "Confirmed" (precision over recall). Plus an internal performance
  pass (unlocked list rendering, single-pass WiFi grading, fewer Guardian re-locks).

**v0.41**
- **Locator — find a marked device by signal, and a Suspicious list on the Guardian.**
  Mark any detection (the report star on Flock / BLE / WiFi is now also the Locator
  pool) and open **Locator** to hunt it down: a live **hot/cold signal meter** that
  climbs as you get closer, peak-hold, and a warmer/colder trend. Works with no GPS.
  The **Net Guardian** now answers "which device?": press **OK** for a **Suspicious
  list** (Flippers, anomaly/unknown devices, rogue APs) and send one straight into
  the Locator. Honest by design — no compass arrow (that needs a directional
  antenna), and attacker MACs are spoofable so the list sticks to what's actually
  locatable. Needs the updated companion firmware for the live-signal homing.

**v0.39**
- **Net Guardian now spots Flippers, attack tools, and (opt-in) anomalies.** The
  watch face gained two more of its three counters — **`Flock`** (cameras),
  **`Atk`** (active attacks), **`Flip`** (Flipper Zeros nearby, detected by their
  `Flipper …` BLE name). On top of deauth floods and evil-twin APs, the fused score
  now reacts to **active attack-tool signatures**: BLE-spam advert floods,
  Pineapple/Marauder **beacon-spam**, and **probe floods** (these need the updated
  companion firmware — reflash via *ESP32 Firmware*). A new **Anomaly flag**
  setting (default off) flags an unnamed, unidentified device transmitting strongly
  right on you. Honest by design: a purely *passive* sniffer emits nothing and
  can't be detected — only active transmitters can, and ELEVATED still needs two
  independent radios to agree.

**v0.34**
- **Net Guardian — an always-on "personal net guardian."** A dedicated
  leave-it-on-the-desk mode (top menu). It keeps the ESP running and **rotates** it
  through the detection modes — dual-band Flock + deauth, BLE trackers, evil-twin
  Wi-Fi — so every input to the fused "am I being watched?" score stays live (and
  can actually reach **ELEVATED**, which needs two independent radios to agree).
  Shows a pwnagotchi-style face (**CLEAR `(-_-)` → WATCHFUL `(o_o)` →
  ELEVATED `(>_<)`**) with the per-signal breakdown, live sweep mode, frame/hit
  counters and an uptime clock, and fires a discreet haptic (+ wakes the screen) on
  the edge into ELEVATED. Companion firmware; Marauder mode points you to
  Flock/ALPR Detect.

**v0.30–v0.33**
- **Reliability pass.** The in-app ESP32 flasher now flashes/verifies cleanly over
  the bare ROM loader (fixes "Finalize failed", "VERIFY FAILED", and the stuck-stub
  "software loader is resident" errors); the lower **Fast = 230400** baud is far
  more reliable over Flipper↔ESP wiring. Saving a report after a large scan no
  longer runs the Flipper out of memory — reports stream row-by-row to the SD card.
  A multi-agent code audit also hardened report escaping (CSV/GeoJSON/KML/WiGLE stay
  valid for any SSID/name) and the NFC deep check on a tight heap. No change to
  detection logic.

**v0.25**
- **Tells a Raven (audio) from a Falcon (camera).** A Flock pole carries either an
  ALPR camera (Falcon) or an acoustic/gunshot sensor (Raven) — both share the same
  Bluetooth battery, so the battery alone can't tell them apart. When the companion
  firmware sees a Raven's own Bluetooth services, the app now labels it
  **"Flock Raven (audio)"** outright. It only says "Raven" on that positive signal
  — it never *guesses* "camera" just because the Raven services are missing, so it
  won't mislabel a pole. (Reflash the companion to pick up the new signal; older
  firmware just shows "Flock device.")
- **Update detection signatures without a new release.** Drop a
  `signatures.json` on the SD card at `apps_data/flipdeflock/` to add your own
  Flock OUI prefixes, SSID patterns, and probe IE fingerprints on top of the
  built-in list. It's **read-only and offline** — the app never writes the file or
  phones home — and **fail-safe**: a missing or broken file just falls back to the
  built-ins. Your additions can only *add* detections, never override the precision
  rules. See the [signatures guide](docs/signatures.md).

**v0.22**
- **"Am I being watched right now?" indicator.** A single home-screen status
  (**CLEAR / WATCHFUL / ELEVATED**) that fuses the app's alerts — a Flock camera
  nearby, a Bluetooth tracker following you, a deauth attack, an evil-twin WiFi —
  into one light instead of four separate beeps. It only goes **ELEVATED** when
  two different radios agree, so it doesn't cry wolf. *Coverage note:* the
  Bluetooth and deauth signals only exist with the companion firmware. In
  **Marauder mode** it can only watch the WiFi/Flock side, so it shows
  **"watch: WiFi only"**; a "clear" there means "nothing on WiFi," not "no one's
  watching." Flash the companion for full coverage.
- **Flock unit serial readout.** Reads a Flock camera's serial from its always-on
  Bluetooth battery beacon (shown on the BLE detail screen; logging to reports is
  off by default for privacy).
- **MAC-randomization defense (groundwork).** The companion now fingerprints the
  shape of a camera's WiFi probe, not just its hardware address, so detection can
  survive Flock scrambling MACs. Ships dormant until validated against a confirmed
  unit, so it can't raise false alarms before it's proven.

**v0.21** — On-device map of detected cameras, NFC default-key audit, and
Share-to-DeFlock QR phone-handoff.
**v0.20** — Tighter anti-stalking: a tracker is only flagged as "following" after
a real multi-waypoint track, not a single drive-by.

Full history in [changelog.md](changelog.md).

## Why an ESP32?

The Flipper's onboard radio is BLE-only and can't do Wi-Fi monitor mode. Flock
cameras are most reliably found by the Wi-Fi probe requests they constantly spray
trying to phone home, so the Wi-Fi work runs on an ESP32 and the Flipper acts as
the UI, GPS-tagger and logger.

Works with any ESP32 Flipper board (Wi-Fi Dev Board, ESP32 Marauder boards,
ReksLab Tri-Board, bare WROOM/WROVER, Xiao ESP32-S3, and so on):

1. **Companion FW** (`esp32_companion/`): flash the sketch to any ESP32 for a
   clean, low-noise line protocol. Recommended.
2. **Marauder / Generic**: keep your existing firmware (e.g. ESP32 Marauder). The
   app scrapes MAC/SSID tokens from whatever it prints and applies the Flock
   filter on the Flipper. Set *Board Mode = Marauder* in Settings.

## Wiring

| Device | Flipper port | Pins |
|--------|--------------|------|
| ESP32  | USART  | 13 (TX) / 14 (RX), 3V3, GND |
| GPS    | LPUART | 15 / 16 |

Both run at the same time. Ports/bauds are configurable in Settings for boards
with nonstandard pinouts.

## Install (prebuilt)

Download `flipdeflock.fap` from the
[latest release](../../releases/latest), copy it to `apps/Tools/` on your
Flipper's SD card, and launch **FlipDeFlock** from the Tools menu.

> The release `.fap` targets Flipper API **87.1**, shared by current stock OFW
> and Momentum. If a future firmware bumps the API and it refuses to load
> ("API mismatch"), just rebuild for your firmware with `ufbt` (below). Every
> push also builds a fresh `.fap` as a CI artifact under the **Actions** tab.

## Using the app

Wire up your ESP32 (and optional GPS) per [Wiring](#wiring), launch **FlipDeFlock**
from `Apps → Tools`, and start with **Settings**.

### 1. Pick your Board Mode (Settings)

Open **Settings** and set **Board Mode** to match your ESP32:

- **Marauder** — keep your board's existing firmware, no flashing. You get
  **Flock/ALPR Detect + NFC + GPS + Reports**.
- **Companion** — the project firmware (flash it with **ESP32 Firmware**, below).
  Adds **WiFi Audit, BLE/Tracker Scan, Net Guardian, deauth detection, and
  dual-band Flock**.

While here, check **ESP Port/Baud** and **GPS** if your wiring differs from the
defaults, and turn **GPS** on if you want detections geotagged. Settings persist.

### 2. Flock / ALPR Detect

The main camera hunt. It shows a live list as the ESP32 sniffs:

- Each row is a detection with a **confidence** tag (`Possible` / `Likely` /
  `CONFIRMED`, see [confidence](#how-detection-confidence-works)) plus the
  detection source in the detail view (`probe` / `beacon` / `BLE`).
- A `!DEAUTH ch<n> <bssid>` banner appears if a real deauth **flood** is detected,
  and clears when the flood stops.
- Press **OK** on a row to open its detail; press **OK** again (the **Mark**
  button) to flag it for the report. Press **Back** to return; the ESP goes idle
  when you leave.

### 3. Net Guardian *(Companion only)*

Leave it running on a desk like a pwnagotchi. It keeps the ESP alive and
**rotates** it across the detection modes — dual-band Flock + deauth, BLE
trackers, then evil-twin Wi-Fi — so every input to the fused score stays fresh.
The screen shows a calm face that shifts **CLEAR `(-_-)` → WATCHFUL `(o_o)` →
ELEVATED `(>_<)`** with the live breakdown, the current sweep mode, frame/hit
counters and an uptime clock. It only reaches **ELEVATED** when two independent
radios agree (e.g. a BLE Flock beacon *and* a Wi-Fi signal), and fires a discreet
haptic — and wakes the backlight — on that edge. **Back** exits and idles the ESP.
*(In Marauder mode this screen explains it needs the companion firmware.)*

### 4. Flock Map

A live map around your GPS position: you're at center, detected cameras are
plotted by bearing and distance (dot size = confidence), with a heading tick and
a scale bar. **Left/Right** zoom, **OK** re-fits. Needs a GPS fix; cameras without
a geotag aren't plotted.

### 5. WiFi Audit *(Companion only)*

Scans nearby networks and grades each one's security worst-first. Markers:
`!` = rogue/evil-twin (same SSID, mismatched security), `~` = duplicate SSID,
`*` = tagged. Open a row for the full breakdown (auth/cipher, WPS, vendor, and
exactly what's weak). Use **Save Report** to write it out. *(In Marauder mode
this screen explains it needs the companion firmware.)*

### 6. BLE / Tracker Scan *(Companion only)*

Continuously scans for **AirTag / Tile / SmartTag / Google Find My** trackers and
Flock/Raven BLE. With **GPS on**, a tracker that stays with you across several
waypoints is flagged **`!FOLLOWING`** (anti-stalking); open it to see the track
(distance / waypoints / time). **Tag** suspicious devices for the report.

### 7. NFC / RFID Audit

Present a card to the Flipper; the app identifies its protocol and grades the
security posture (e.g. UID-only / cloneable vs. authenticated) for access-control
reviews. If it's a **MIFARE Classic**, a **Deep** softkey appears: it captures
the UID and tries the Flipper's on-SD key dictionary against every sector, then
reports how many open with **default keys** (`N/total` = trivially cloneable).
Audit only cards you own or are authorized to test.

### 8. ESP32 Firmware — backup & flash

Manage your board's firmware from the Flipper, no computer:

1. **Backup current FW → SD** *(do this first!)* — dumps the whole ESP32 flash to
   `apps_data/flipdeflock/firmware/` so you can restore Marauder later. It's
   read-only and safe.
2. **Flash a .bin** — pick the companion `flock_companion-merged.bin` (from a
   release, copied to SD), a backup, or any merged image; it writes at `0x0`.

When prompted, put the ESP32 into **bootloader/download mode** (hold **BOOT**, tap
**RESET**), then it connects. Flash speed is **Safe (115200)** or **Fast (230400)**
in Settings; the flasher talks to the bare ROM loader and MD5-verifies the write.
You can't brick it: the ROM bootloader always allows a re-flash.

### 9. Reports

Saved reports land on the SD under **`apps_data/flipdeflock/reports/`**:
Markdown (human-readable), DeFlock-compatible GeoJSON (ready for
[deflock.org](https://deflock.org)), KML, plain CSV, and WiGLE CSV (WiFi and BLE)
for wardriving uploads. Pull them with qFlipper or a card reader.

### 10. Share to DeFlock (phone handoff)

For each camera you **marked** (and that has a GPS geotag), this screen shows a
QR code. Scan it with your phone to open DeFlock at that location and submit
through the official app's review flow. **Left/Right** pages between cameras; the
lat/lon and OSM tags are shown on-screen too. The Flipper and ESP never connect
to a network, so the submission happens entirely on your phone.

**No GPS module on your Flipper?** You can still contribute. DeFlock does not
require GPS — its app and web editor let you place and drag the camera's pin on
the map by hand. Stand at the camera, open
[deflock.org/report](https://deflock.org/report) (or the DeFlock app), and drop
the pin on it. The QR handoff only lists cameras the Flipper geotagged, so without
a fix you add those directly on DeFlock.

## Build from source

**With [ufbt](https://pypi.org/project/ufbt/) (standalone, recommended):**

```sh
pip install ufbt
ufbt            # builds flipdeflock.fap in dist/
ufbt launch     # build + install + run on a connected Flipper
```

**Inside a Momentum/OFW tree:** drop this folder into `applications_user/` and
run `./fbt fap_flipdeflock`.

**ESP32 companion:** see [`esp32_companion/README.md`](esp32_companion/README.md)
for Arduino IDE / arduino-cli flashing.

## How detection confidence works

Flock-associated OUIs are generic vendor prefixes, so a prefix match alone is weak
evidence. Confidence is scored accordingly:

| Signal | Confidence |
|--------|------------|
| OUI prefix only | `Possible` |
| OUI + phone-home probe request | `Likely` |
| SSID matches `Flock-XXXXXX` / `test_flck` | `CONFIRMED` |

## Layout

```
application.fam          manifest
recon_app.c / _i.h       lifecycle, shared state, settings
scenes/                  start, flock, guardian, map, wifi, ble, nfc, firmware,
                         reports, deflock_handoff, settings, about
views/
  flock_view.*           custom live-detection list view
  flock_map_view.*       on-device map (operator-centered camera plot)
  deflock_qr_view.*      QR render for the DeFlock phone-handoff
  guardian_view.*        pwnagotchi-style Net Guardian status face
helpers/
  flock_db.*             Flock OUIs + SSID patterns + confidence scoring
  esp_link.*             ESP32 UART link (companion + generic backends)
  esp_flasher.*          in-app ESP32 backup/flash (esp-serial-flasher port)
  gps_link.*             NMEA GPS reader (2nd UART)
  recon_nfc.*            NFC scanner + grading + default-key deep check
  recon_report.*         Markdown + GeoJSON + KML + CSV/WiGLE writers
lib/esp-serial-flasher/  vendored Espressif flasher (Apache-2.0)
lib/qrcodegen/           vendored Nayuki QR Code generator (MIT)
esp32_companion/         universal ESP32 firmware + flashing guide
```

Prebuilt binaries are published on the [Releases](../../releases) page and as
per-push CI artifacts, not committed to the repo.

## Credits & data

The detection method and Flock OUI prefixes build on the open
counter-surveillance work of
[colonelpanichacks/flock-you](https://github.com/colonelpanichacks/flock-you),
[0xXyc/flock-you-wifi-recon](https://github.com/0xXyc/flock-you-wifi-recon), and
the [DeFlock](https://deflock.org) community. Thanks to the researchers who mapped
these signatures. The GPS NMEA approach is based on the Momentum Sub-GHz GPS
helper.

## License

**GPL-3.0-or-later** — see [LICENSE](LICENSE).

```
Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version. It is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.
```

FlipDeFlock is copyleft: you're free to use, study, share, and modify it, but if
you distribute a modified version you must publish your source under the same
GPL-3.0-or-later terms and keep the copyright and license notices intact. Bundled
third-party components keep their own (compatible) licenses:
esp-serial-flasher (Apache-2.0), Nayuki qrcodegen (MIT), jsmn (MIT), and the ESP
MD5 routine (BSD) — see their headers under `lib/`.

## Name & trademark

The code is free to reuse under the GPL, but **"FlipDeFlock"** — the name and
logo — is the project's identity, not part of the licensed code. Please don't
publish a fork, repackage, or app-store listing under the FlipDeFlock name or logo
in a way that implies it's the official project. Rename your derivative and a clear
"based on FlipDeFlock" credit is welcome. The friendly move when building on this
is to **fork on GitHub** (keeps the link and credit intact) rather than re-uploading
a detached copy.

## Support

FlipDeFlock is free and open source (GPL-3.0-or-later), and it stays that way. If
it's useful and
you want to help cover development (and any legal costs that come with mapping
surveillance hardware), there's a **Sponsor** button at the top of the repo, or
you can donate directly via [Ko-fi](https://ko-fi.com/recongrunt).
Donations are optional and never gate a feature. Spreading the word, filing good
detections, and sending patches help just as much — see [Contributing](#contributing).

## Contributing

Contributions are welcome. This is a community counter-surveillance effort, and it
improves with more eyes, more boards, and more field data. The most useful things
you can do:

- **Field reports & signatures** — new Flock/ALPR OUIs, SSID/BLE patterns, or
  false positives and misses you hit in the wild. Precision feedback is the most
  valuable. You can test a candidate signature on your own device first by adding
  it to `apps_data/flipdeflock/signatures.json` (see the
  [signatures guide](docs/signatures.md)), then send the
  ones that hold up.
- **Board support** — try it on your ESP32 hardware and report wiring/quirks.
- **Code** — bug fixes, new report formats, or any of the deferred roadmap items.

Open an issue or a pull request. A few ground rules keep the project coherent:

- **Passive recon only.** No deauth, injection, or jamming, ever.
- **Correctness over features.** A false positive is worse than a missed
  detection; don't trade precision for recall without good reason.
- Target **API 87.1**, and it must build with both `ufbt` and `fbt`.
- Keep it lean. The `.fap` loads entirely into the Flipper's 256 KB of RAM.

By contributing you agree to license your work under this repo's GPL-3.0-or-later
license.

## Roadmap

Planned work, deferred items (with reasons), and the community/funding track are
in [ROADMAP.md](ROADMAP.md). Shipped history is in [changelog.md](changelog.md).
