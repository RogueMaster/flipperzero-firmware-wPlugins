<p align="center">
  <img src="media/logo.png" width="560" alt="FlipDeFlock">
</p>

<p align="center"><em>Find the cameras that are watching you.</em></p>

FlipDeFlock is a Flipper Zero app that pairs the Flipper with an ESP32 board to
survey the radio around you for surveillance gear: Flock Safety and other ALPR
hardware, SoundThinking acoustic sensors, body-worn police cameras (Axon, Utility
BodyWorn, Digital Ally), competitor camera vendors (Ubicquia, Motorola Solutions,
Verkada, Genetec, Avigilon), drones, and BLE trackers planted on you. The Flipper is the screen, GPS tagger, and logger; the ESP32
does the Wi-Fi sniffing its BLE-only radio can't. It's for security assessments,
anti-surveillance awareness, and CTF/research.

**It detects cameras that have stopped announcing themselves.** Modern ALPR
hardware randomises its MAC address, which defeats the vendor-prefix matching
every detector is built on — the camera is right there, transmitting, and a
prefix list sees an empty street. FlipDeFlock identifies a probe request by its
*shape*: the order and contents of its information elements, which describe the
radio rather than the address and survive randomisation intact. That is how it
finds a unit no OUI table can see.

It also covers more of the field than a camera-only tool: ALPR, acoustic
gunshot sensors, body-worn and in-car police cameras, five competitor camera
vendors, drones by their federally-mandated Remote ID, and BLE trackers — each
reported as what it actually is, never folded into one "surveillance" bucket.

Drones are found by their **Remote ID** broadcast (ASTM F3411), which every
unmanned aircraft in US airspace is required to transmit. That gives you the
aircraft's serial, its type, its position, and the position of the person flying
it. It also works where nothing else does: of the five drone vendors a US police
department actually buys from, only one holds an IEEE MAC block, so a prefix list
cannot see the rest.

**Passive recon only.** Detection is listen-only — no deauth,
injection, or jamming, ever, and nothing is transmitted at any point. Detections
are indicators, not proof: OUI-only matches are possible, not confirmed, so verify
by eye. Use it only where you are authorized to.

Built for stock OFW, [Unleashed](https://github.com/DarkFlippers/unleashed-firmware),
[Momentum](https://github.com/Next-Flip/Momentum-Firmware) and RogueMaster. Pick the
file that matches your firmware; see [Install](#install).

> [!TIP]
> Want an unreleased fix before it's tagged? Grab a [nightly build](../../releases/tag/nightly)
> instead of the [latest release](../../releases/latest) — untested, rebuilt from `main`. Details
> in [Stable and nightly](#stable-and-nightly).

**Free, and staying that way.** If FlipDeFlock is useful to you, crypto donations fund
development, test hardware, and the legal costs of mapping surveillance infrastructure.
[Donations never gate a feature](SUPPORTERS.md).

[![Bitcoin](https://img.shields.io/badge/Bitcoin-Donate-f7931a?style=for-the-badge&logo=bitcoin&logoColor=white)](https://mempool.space/address/bc1qavy2wdhgpvqturn5he76mxclqr0a3vhg9sj4l8)
[![Ethereum](https://img.shields.io/badge/Ethereum-Donate-3C3C3D?style=for-the-badge&logo=ethereum&logoColor=white)](https://etherscan.io/address/0x481be1838e6B51B1a4013633877Bd967E2484694)
[![Litecoin](https://img.shields.io/badge/Litecoin-Donate-345D9D?style=for-the-badge&logo=litecoin&logoColor=white)](https://blockchair.com/litecoin/address/ltc1qpj3ppsgcdvx5yara9suljeq83t32macr8pe2yt)
[![Bitcoin Cash](https://img.shields.io/badge/Bitcoin_Cash-Donate-0AC18E?style=for-the-badge&logo=bitcoincash&logoColor=white)](https://blockchair.com/bitcoin-cash/address/qzl3a9emduev23nuh6wvk2nzwl5gguc9egmg83ae5a)

| Coin | Address |
|---|---|
| Bitcoin (BTC, native SegWit) | `bc1qavy2wdhgpvqturn5he76mxclqr0a3vhg9sj4l8` |
| Ethereum (ETH) | `0x481be1838e6B51B1a4013633877Bd967E2484694` |
| Litecoin (LTC, native SegWit) | `ltc1qpj3ppsgcdvx5yara9suljeq83t32macr8pe2yt` |
| Bitcoin Cash (BCH) | `bitcoincash:qzl3a9emduev23nuh6wvk2nzwl5gguc9egmg83ae5a` |

Bitcoin is also scannable from the Flipper itself: **FlipDeFlock → Support**.

## Install

Grab the file for your firmware from the [latest release](../../releases/latest),
copy it to `apps/Tools/` on your Flipper's SD card, and launch **FlipDeFlock** from
the Tools menu.

| Your firmware | Download | API |
|---|---|---|
| Official (OFW) | `flipdeflock.fap` | 87.1 |
| Momentum | `flipdeflock-momentum.fap` | 87.1 |
| Unleashed | `flipdeflock-unleashed.fap` | 88.9 |
| RogueMaster | `deflock.fap` | 88.9 |

A `.fap` records the API version it was built against, and the firmware refuses to
load one whose **major** version does not match — the number before the dot. **If
your Flipper says the app is old, you have the wrong file, not an old app.** It is
the API that is old, not the release. Take the matching row above. RogueMaster
tracks Unleashed and reports the same API, but uses its own conventional
`deflock.fap` filename. Use that row's file there.

The minor version — the number after the dot — is **not** checked by the firmware,
so a build made against 88.2 still loads on 88.3. A minor bump only adds symbols.
That is why the table can lag a fresh firmware release by a few days without
anything actually breaking for you.

Every push also builds all four as CI artifacts under the **Actions** tab.

### Stable and nightly

| Channel | Where | What it is |
|---|---|---|
| **Stable** | [latest release](../../releases/latest) | A tagged, released build. This is the one to use. |
| **Nightly** | [`nightly`](../../releases/tag/nightly) | A rolling build of `main`, rebuilt daily when something changes. Untested. |

Stable is the default and the install link above points at it. Nightly is marked
as a prerelease, so it never becomes "latest" and you will not get it by accident.

Take a nightly when a fix you are waiting on has landed but is not tagged yet, or
when a release turns out to have a problem and you want the state of `main`
instead. It carries the same four `.fap` files under the same names, so switching
back is a matter of copying the stable file over it.

Two things to know about nightlies. They have not been through a release check, so
treat a nightly the way you would treat any untested build. Nightlies rebuild the
matching WROOM, S2, and experimental C5 companion images too; install the `.fap`
and board-specific `.bin` from the same nightly. If the app and companion disagree
about the protocol, the app tells you on the scan screen rather than quietly
misreporting.

Releases also carry `SHA256SUMS.txt`, covering every asset. If you got your copy
anywhere other than this repository's releases page, check it:

```sh
sha256sum --ignore-missing -c SHA256SUMS.txt
```

`--ignore-missing` checks the files you actually downloaded. Without it the command
reports `FAILED` for every asset you did not take and exits non-zero, which reads as
"your download is bad" when nothing is wrong. A genuine mismatch still fails. See
[TRADEMARK.md](TRADEMARK.md#verifying-an-official-build).

If your firmware is not listed, or it bumps its API before the next release here,
build it yourself with `ufbt`; see [Build from source](#build-from-source). Other
common problems (UART busy, no detections, GPS no-fix, flasher errors) are covered
in [Troubleshooting](docs/TROUBLESHOOTING.md).

## Hardware

The Flipper's onboard radio is BLE-only and can't do Wi-Fi monitor mode. Flock
cameras are found most reliably by the Wi-Fi probe requests they spray trying to
phone home, so the Wi-Fi work runs on an ESP32. Board support depends on the chip
and firmware image:

| Board/chip | Install path | Coverage status |
|---|---|---|
| Classic ESP32 WROOM/WROVER | Release `...esp32wroom.bin` | Primary companion target |
| Official Wi-Fi Devboard / ESP32-S2 | Release `...esp32s2.bin` | Wi-Fi only; the S2 has no BLE radio |
| ESP32-C5 | Release `...esp32c5_EXPERIMENTAL.bin` | 2.4/5 GHz, compile-verified but not field-verified |
| ESP32-S3/C3 and other boards | Build from source for the exact chip | Compatibility is experimental until hardware-tested |
| ESP32 Marauder boards | Keep their existing firmware and select Marauder mode | Wi-Fi text-scraping fallback; no companion-only features |

Wire the ESP32 (and an optional GPS module) to the Flipper's GPIO:

| Device | Flipper port | Pins |
|--------|--------------|------|
| ESP32  | USART  | 13 (TX) / 14 (RX), 3V3, GND |
| GPS    | LPUART | 15 / 16 |

Both run at once. Ports and bauds are configurable in **Settings** for boards with
nonstandard pinouts. Turn **GPS** on in Settings to geotag detections. Settings
persist.

**GPS on the ESP board instead?** Some carrier boards wire their GPS module to the
ESP32 rather than to the Flipper header, so the Flipper cannot see it on any pin. Set
**GPS From** to `ESP32` and **ESP GPS Pin** to the ESP GPIO the module's TX is on; the
companion firmware then relays each NMEA sentence over the link it already has. Needs
companion firmware v0.52+. Default is `Flipper`, i.e. the pin table above.

**No GPS module at all?** Set **GPS From** to `Phone` and the position comes from a
paired phone instead, over the firmware's RPC location service. Needs **Unleashed
firmware** (the service does not exist on official firmware or Momentum — the badge
says `!FW` there) and the [qUnleashed](https://github.com/DarkFlippers/qUnleashed)
companion app paired over BLE or USB, with location permission granted and the app
open for the whole scan. Fixes coarser than 100 m are rejected rather than used: a
phone answers with a cell-tower estimate when it has no sky view, and a camera pinned
two kilometres from where it actually is is worse than one with no pin at all.

> **If you value anonymity, do not use your phone for GPS.** A GPS module receives
> and never transmits. A phone is a second radio-connected device with its own
> identifiers, tied to a subscriber account and continuously logged by the network —
> so using one to geotag surveillance cameras puts a record of exactly where you went
> in someone else's hands. The `Phone` option exists because a phone is what many
> people already have; it is not the recommended one, and it is not the default.

### Board Mode

Set **Board Mode** in Settings to match your ESP32 firmware:

- **Marauder** — keep the board's existing firmware, no flashing. You get Flock /
  ALPR Detect, GPS, and Reports. The app scrapes MAC/SSID tokens
  from whatever Marauder prints and applies the Flock filter on the Flipper.
- **Companion** — the project firmware in `esp32_companion/`, a clean line
  protocol. Adds Locator and dual-protocol Wi-Fi + BLE detection on chips that
  have both radios. Flash it from the
  app with **ESP32 Firmware** (no computer needed) or with Arduino IDE /
  arduino-cli (see [`esp32_companion/README.md`](esp32_companion/README.md)).

## What it does

Each item is a screen in the app. Screens marked *(companion)* need the companion
firmware; in Marauder mode they explain what's missing.

- **Flock / ALPR Detect** — the main hunt. Finds surveillance hardware over Wi-Fi
  (and BLE, with the companion), geotags it, and lets you mark it for a report.
  Each row carries a confidence tag (see
  [Detection confidence](#detection-confidence)) and shows its source — probe,
  beacon, BLE, or Remote ID — in the detail view. Set **Alert on hit** in Settings
  (Vibrate / Beep / both) to be told about something you aren't watching the
  screen for — it fires once per device, and never for an OUI-only "Possible"
  lead.

  **Five device classes are kept apart rather than lumped together**, because
  calling one of these another is the failure mode the whole confidence system
  exists to prevent:

  | Row tag | Class | What it covers |
  |---|---|---|
  | *(none)* | ALPR camera | Flock Safety and other plate readers |
  | `ST:` | Acoustic sensor | SoundThinking / ShotSpotter — listens, does not read plates |
  | `AX:` | Body-worn camera | Axon, Utility BodyWorn, Digital Ally — moves with a person, says nothing about a pole |
  | `VG:` | Vendor gear, kind unknown | Ubicquia, Motorola Solutions, Verkada, Genetec, Avigilon — one OUI carries plate readers *and* hand-held radios, so the vendor is stated and the product is not |
  | `DR:` | Unmanned aircraft | see **Drones** below |

- **Health / Preflight** — checks the capture stack before a field session. In
  Companion mode it shows the app and ESP builds, wire-protocol and signature
  revisions, production signature self-test, live receive rate and frame count,
  dropped UART lines, companion resets, chip, active band/channel count, and GPS
  state. `Detect` appears only after the link, protocol, signatures, self-test,
  and RF receive counter are healthy. A Marauder/Generic link can report only
  `LIMITED`, because that text protocol has no version or self-test handshake.
  Use this after flashing and before treating a zero-hit drive as meaningful.
- **Drones (Remote ID)** — decodes the ASTM F3411 broadcast that every unmanned
  aircraft in US airspace is required to transmit, over both BLE and Wi-Fi. You
  get the aircraft's serial or registration, its type, its position, and **the
  operator's position**. This is the only method that reaches the fleet: of the
  five drone vendors a US police department realistically buys from (Skydio,
  BRINC, Aerodome, Flock, Paladin), only Skydio holds an IEEE MAC block, so a
  prefix list cannot see the other four. A detection confirms that something is
  flying and announcing itself — never that it is a police drone, which no
  signature could establish.
- **Flock Map** — a live map around your GPS position: you're at center, cameras
  are plotted by bearing and distance, dot size is confidence, with a heading tick
  and a scale bar. Left/Right zoom, OK re-fits. Needs a GPS fix; ungeotagged
  cameras aren't plotted.
- **Locator** *(companion)* — hunt a marked device by live signal strength: a
  hot/cold meter that climbs as you get closer, peak-hold, and a warmer/colder
  trend. Mark a target from any Flock, Wi-Fi or BLE detection. **That includes
  BLE trackers** — AirTag, Tile, SmartTag, Google Find My, and other Flipper
  Zeros — so if something has been planted on your car, this is how you walk it
  down. Works without GPS (a fix only adds a "strongest here" note). There's no
  compass arrow — direction-finding a transmitter needs a directional antenna, so
  you close in by walking.
- **ESP32 Firmware** — backs up the board's current firmware to SD, then flashes a
  `.bin` (companion, Marauder, or a backup) at `0x0`, straight from the Flipper.
  Put the ESP in bootloader/download mode first (hold **BOOT**, tap **RESET**). It
  talks to the bare ROM loader and MD5-verifies the write; flash speed is Safe
  (115200) or Fast (230400) in Settings. You can't brick it — the ROM bootloader
  always allows a re-flash. Built on Espressif's esp-serial-flasher. **Back up
  before you flash.**
- **Reports** — writes to `apps_data/flipdeflock/reports/`: Markdown,
  DeFlock-compatible GeoJSON, and KML. Reports stream row-by-row to SD, so a large
  scan won't run the Flipper out of memory. Pull them with qFlipper or a card
  reader.

  **Redacted by default.** Three items: `Export Marked (Redacted)`, `Export All
  (Redacted)`, `Export All (RAW - private)`. The redacted files keep camera
  coordinates, because that is the point of the report, and drop what describes
  *you*: MACs fall back to their OUI, sighting times and heading are omitted,
  your own labels are left out, and any SSID that is not itself a Flock name is
  replaced by its shape (`AaaaAdd`). That last one matters — a scan sweeps up
  every household network in range, and an SSID is frequently a surname or a
  street address that public wardriving databases can place on a map. The RAW
  item sorts last, names itself, and writes files suffixed `_RAW`.

- **Air Survey** — every wildcard-probe transmitter the board hears, matched or
  not, on the device and ranked. This is what tells an empty street apart from a
  camera running hardware we don't recognise yet, and for a camera that
  randomises its MAC — which current ones do — it is usually the only place it
  appears at all, because there is no vendor prefix for any OUI table to match.

  Rows are ranked on how persistently a device probes measured against
  everything else in the capture, how close it is, and whether one fingerprint is
  turning up on several addresses. `~` marks a randomised address and `g` a
  commodity scan pattern shared with phones, which sinks to the bottom however
  loud it is. Counts are per scan, not since the board booted. Park where you can
  see a camera and the row standing well above its neighbours is that camera.

  It is not a detection list and nothing in it enters the hit table. A high rank
  means the device behaves the way a fixed installation behaves, which a busy
  access point also does.

  Written to the card two ways. `survey.csv` is the last session only, rewritten
  each scan, which is what you want while working one camera. `survey_log.csv`
  appends every session beside it with the scan's start time as the first column,
  so a drive with several stops survives as one file with the stops still
  separable. It rotates to `survey_log.old.csv` past 128 KB.

- **Learning** — `Confirm: I saw it` on a device you physically looked at saves
  its probe fingerprint to `learned.txt`, so the same unit is caught again
  **after it randomises its MAC** — which current Flock cameras do, and which is
  why OUI tables miss them. Available both on a detection and on an Air Survey
  row; the survey is the one that matters for a randomised camera, since it never
  becomes a detection in the first place.

  Learned signatures are capped at `Class?` and can never reach Confirmed, so a
  mis-tap costs a weak lead rather than a false camera. Commodity scan patterns —
  the ones carried by phones and ordinary IoT gear — are refused outright, so the
  easiest mistake to make cannot be made. Un-confirming does not unlearn;
  *Reports → Forget Learned* shows the count and deletes the file. Nothing is ever
  transmitted.
- **Save hits** *(Settings, on by default)* — keeps your detections across app
  restarts in `apps_data/flipdeflock/hits.csv`, so closing the app doesn't throw
  a scan away. Restored hits come back in the list and on the map, showing the age
  of the stored sighting instead of a live signal reading. **Know what it is:** a
  hit log is a durable record of where you have been, so if that matters for your
  situation, switch it off. Turning it off deletes the file, and *Reports → Clear
  Saved Hits* erases it any time.
- **Share to DeFlock** — renders a QR per marked, geotagged camera that opens
  the [DeFlock map](https://maps.deflock.org) zoomed to that location on your
  phone, so you submit through the official app's review flow. The Flipper and ESP never touch a
  network. No Flipper GPS? DeFlock lets you place the pin by hand at
  [deflock.org/report](https://deflock.org/report).

## Screenshots

| | |
|:--:|:--:|
| <img src="media/screenshots/alpr.png" width="330" alt="Flock / ALPR Detect"><br>**Flock / ALPR Detect** | <img src="media/screenshots/menu.png" width="330" alt="Main menu"><br>**Main menu** |
| <img src="media/screenshots/flock-detail.png" width="330" alt="Detection detail"><br>**Why it was flagged** | <img src="media/screenshots/esp32-firmware.png" width="330" alt="ESP32 Firmware"><br>**ESP32 Firmware** |

<sub>Historical screenshots captured on a Flipper Zero running v0.49. The current
menu and status displays have changed since then. The devices shown are fabricated
demo records; no real network or location appears in any screenshot.</sub>

## Asset pack

Releases carry `flipdeflock_asset_pack.zip` — a Flipper asset pack with two
desktop animations. Copy it to `/ext/asset_packs/` and pick it in
**Settings → Desktop → Asset Pack** on a firmware that supports them
(Momentum, Unleashed, RogueMaster).

| | |
|:--:|:--:|
| <img src="media/asset-pack-kick.gif" width="330" alt="Hoodie kicks an ALPR pole"><br>**Kick** | <img src="media/asset-pack-scan.gif" width="330" alt="Scanning animation"><br>**Scan** |

Both are rendered straight from the pack by `tools/make_anim_gif.py`, which reads
the frame order and frame rate out of each animation's own `meta.txt` — so the
picture here cannot drift from what the Flipper actually plays.

## Detection confidence

Flock-associated OUIs are generic vendor prefixes (shared with Espressif and
others), so a prefix match alone is weak evidence. Confidence is scored
accordingly:

| Signal | Confidence |
|--------|------------|
| OUI prefix only | `Possible` |
| OUI + phone-home probe request | `Likely` |
| SSID is `Flock-` + 6 hex, or contains `test_flck` | `CONFIRMED` |
| Unverified user IE fingerprint | `Class?` (candidate device-class, never Confirmed) |

A benign name like `Flock-Guest` or `Flock Freight` does not Confirm — only the
exact provisioning-AP name does; those drop to `Likely`.

You can extend detection without a rebuild: drop `signatures.json` into
`apps_data/flipdeflock/` with extra `ouis`, `ssid_confirmed`, `ssid_likely`, and
`ie_fps` (8-hex probe fingerprints, capped at 32). It is load-only, offline, and
fail-safe — a missing or broken file falls back to the built-ins, and user entries
can only add detections, never override the precision rules (a user IE fingerprint
maxes out at `Class?`). Each detection's fingerprint shows as `IE-fp:` on its
detail screen, so you can read one off a confirmed camera and catch its
MAC-randomized twins. See the [signatures guide](docs/signatures.md).

## Every identifier is verified

Detector tables tend to get inherited. A prefix shows up on one list, gets copied
to the next, and nobody goes back to ask who actually registered it. That is how
a body-cam vendor's prefix ends up pointing at an unrelated networking company
with a similar name, and how somebody's phone ends up flagged as a police camera.

Every prefix in this repo was resolved against the IEEE registry by organisation
name before it was added, and that name is the whole argument for keeping it.

A worked example from September 2026. A Flock camera firmware image was published
showing the hardware runs a Qualcomm radio chip, and the chip's default MAC prefix
started circulating as a Flock identifier. It is registered to the silicon vendor,
not to Flock, and that chip has shipped in a decade of routers, tablets and IoT
gear. Detecting on it would light up a shelf of consumer hardware as surveillance.
The chipset itself is genuinely useful intelligence, because it tells you what the
camera's probe request should look like. The chip vendor's prefix is the one part
of that firmware dump that must never become evidence, so it went on the blocklist
instead of the detection table.

CI enforces this on every push rather than trusting anyone to remember:

- 6 prefixes that were published and later withdrawn cannot come back
- 15 look-alike registrations, right-sounding name and wrong company, are blocked
- 26 chip-vendor and shared IEEE blocks are blocked, including the one above
- the Flipper and companion tables must agree, and the signature table carries a
  revision hash the app checks against the companion it is actually talking to

The same standard applies to this project's own mistakes. Six probe fingerprints
are on a permanent denylist because field evidence later showed them on phones and
consumer hardware rather than cameras. Two of those were ours, including one we
had told an operator to add to his own signature file. A denylisted fingerprint
overrides `signatures.json` as well as the built-ins, so a retraction reaches
cards already in the field without anyone editing anything.

A false positive costs more than a missed detection here. A detector that cries
wolf gets ignored, and an ignored detector protects nobody.

## On-screen legend

RSSI is shown as signal bars (taller = stronger); the highlighted row shows the
exact dB. `-33dB` closer to 0 means physically closer.

**Flock / ALPR Detect** — header `ESP ch6  frames 339  hits 0`
- **ESP** (or `...`) — companion connected / still waiting
- **ch / frames / hits** — channel · 802.11 frames captured · Flock detections, counted this session (reset each time you open the screen)
- **row tag** — `!` CONFIRMED · `F` probe-fingerprint · `L` Likely · `p` Possible · `.` OUI-only · `*` marked
- **`ST:` before the name** — a SoundThinking (ShotSpotter) acoustic sensor, not an ALPR camera. Untagged rows are cameras; the detail screen names the class in full
- **`AX:`** — a body-worn police camera (Axon, Utility BodyWorn, Digital Ally). Not fixed infrastructure: it moves with a person or a vehicle, so it says nothing about a camera on a pole
- **`VG:`** — vendor gear of undetermined kind (Ubicquia, Motorola Solutions, Verkada, Genetec, Avigilon). The vendor is known, the product is not: one OUI carries plate readers and hand-held radios alike
- **`DR:`** — an unmanned aircraft, detected by its Remote ID broadcast
- **GPS badge** - filled `GPS 9` = locked with 9 satellites, filled `GPS` = locked but nothing reported a satellite count (normal on the `Phone` source), hollow `GPS` = on and searching. A fault names what to fix and never says "GPS", because a filled badge starting with those three letters reads as a lock: `!PORT` = GPS and the ESP are on the same UART (put GPS on the other one, LPUART / pins 15-16), `!PIN` = the companion refused that ESP GPS Pin, `!FW` = the companion never answered so reflash it — or, on the `Phone` source, this firmware has no location service (needs Unleashed). Phone-only faults: `!APP` = nothing paired, open qUnleashed · `!PERM` = the phone denied location permission · `!LOC` = the phone's location is off, or the paired device has no receiver · `!ACC` = fixes are arriving but coarser than 100 m, so go outside · `!ERR` = the companion app reported a fault
- Marauder mode shows `rx <n>  hits <n>` instead (serial heartbeat + detection count)

**Locator**
- **mark first** — the report star on any Flock, Wi-Fi or BLE detection adds it to the Locator pool, including BLE trackers (AirTag / Tile / SmartTag / Find My / Flipper)
- **meter / dB** — climbs as you get closer; `WARMER`/`colder` is the trend, the tick above the bar is peak-hold. `out of range` means the target went quiet — walk back to where it was loudest

**Health / Preflight**
- **`READY: capture OK`** — the Companion UART is live, protocol and signature revisions match, the production signature self-test passed, and the radio frame counter advanced
- **`CHECK` / `FAIL`** — wait up to eight seconds; the first row then names the missing or incompatible layer (`no companion`, `proto mismatch`, `sig mismatch`, `sig test failed`, `zero RF`, or `UART busy`)
- **`P1/1 S9627/9627 TPASS`** — actual/expected protocol, abbreviated actual/expected signature revision, and signature-path self-test
- **`RX25/s F815 D0 R0`** — current frame rate, session frames, dropped UART lines, and detected companion counter resets
- **More** — shows the full signature revisions, self-test hash/result, line/drop/reset counters, and GPS state

## Build from source

With [ufbt](https://pypi.org/project/ufbt/) (standalone):

```sh
pip install ufbt
ufbt            # builds flipdeflock.fap in dist/
ufbt launch     # build + install + run on a connected Flipper
```

Inside a Momentum/OFW tree: drop this folder into `applications_user/` and run
`./fbt fap_flipdeflock`. For the ESP32 companion firmware, see
[`esp32_companion/README.md`](esp32_companion/README.md).

## Status

FlipDeFlock is actively developed, not finished. It's useful in the field, but
features are still landing, detection signatures change as surveillance hardware
changes, and not every path is hardware-tested on every board. Expect rough edges
and the occasional breaking change between versions. Treat detections as
indicators and verify by eye; if you rely on it for anything that matters, read
the code and confirm the behavior yourself.

## What's new

**v0.98** - **A zero-hit session can now prove that the detector was actually
working.** The new **Health / Preflight** screen checks the live UART, app and
companion build labels, protocol compatibility, signature-table revision, the
companion's production signature matcher, and an increasing RF frame counter.
It also exposes receive rate, UART drops, companion resets, chip, band/channel
coverage, and GPS state. Companion-backed `Detect` stays unavailable until the
check reaches `READY`; Marauder/Generic can only make the narrower `LIMITED`
claim its text stream supports.

The app and companion now advertise the same content-derived signature revision,
and CI derives that value from the production table instead of trusting two
hand-maintained copies. Session diagnostics move to schema v3 so a support row
records expected/actual signature revisions and the self-test result. The
companion's `sigtest` path is now available from the UI instead of being a hidden
serial-only bench command.

Release plumbing and public instructions were brought back in line with the
artifacts that actually ship: lint is blocking, Unleashed/RogueMaster target API
88.9, S2 is required before release checksums are made, nightlies document all
three companion images, and generated Arduino/Python files can no longer be
swept into a later FAP build. The classic ESP32 pair was flashed and exercised
end to end with live bench traffic; S2 and C5 remain compile-verified only.

**v0.97** - **Telling a camera pole from a two-way radio.** Motorola Solutions
sells ALPR poles and hand-portable radios on the same OUI, so the vendor prefix
alone genuinely cannot say which one is in front of you, and both used to come out
`Possible`. Behaviour can tell them apart: a mains-powered pole phones home with
wildcard probes every ~125 ms forever, and a battery handheld simply cannot. A
vendor-exclusive OUI probing like fixed infrastructure now scores `Likely`. The
class stays `Gear` deliberately, because this separates a pole from a handheld,
not an ALPR from other fixed gear.

That number is also on screen now as **`Probes/8s`**. The companion had been
measuring it since v0.88 and sending it over the wire, and the app parsed it and
threw it away — never stored, never shown. It is the one measurement that settles
the question above, so now you can see it on the row where you are deciding
whether to go and look.

Also: the `ba9fafa0` camera signature is now corroborated against the public
surveillance map rather than only our own reasoning. A mapped Flock ALPR sits 44 m
from one of those devices, and it is the only one of eighteen geotagged detections
within 100 m of a mapped camera. It stays a candidate.

**v0.96** - **A camera on a randomised MAC can now be detected at all.** That gap
was structural, not a matter of tuning: every rung of the companion's ladder
needed an OUI match or a Flock SSID, so a randomised address scored zero and was
thrown away *before* its fingerprint was even computed. A known probe signature
now gets a frame past that on its own, and lands as `Class?` — capped there,
because one contributor's drive is not proof. Verified on the bench against real
randomised addresses with no OUI and no SSID behind them.

The probe fingerprint also got a great deal sharper. The old one hashed each
element's tag and length and threw the contents away, so the fields that describe
a radio counted for nothing — across 120 devices in a field capture it produced
49 distinct values with **74% of devices colliding**, one hash covering 24
separate devices. `survey.csv` now carries a content-aware hash and a *readable*
signature alongside it — an ordered IE tag list you can compare by eye against
another capture, instead of eight opaque hex digits.

**Two new candidate fingerprints, and one retracted.** From a field
drive by @wiilover22, `ba9fafa0` ships as a candidate: four devices carry it, all
on randomised addresses no OUI table can match, and they sit 1.1 km to 6.1 km
apart — so they are four fixed installations, not one device rotating its MAC.
That is the randomised-camera case this project has been chasing since issue #25.
`d0bbec4c` joins it with a vendor anchor: three devices, all on OUIs already in
the built-in Flock table.

**`89c3debf` is retracted and denylisted, and it was our own advice.** We told him
to add it to `signatures.json` as "the camera". His next drive found it on ten
devices spread over 7.9 km, and ten of the nineteen rows in the hit table he sent
back were phones. The denylist outranks your own file, so a card carrying it goes
inert on update with nothing for you to edit.

**Share to DeFlock sent you to the wrong page.** The QR pointed at
`deflock.org`, which is the landing page: it has no map on it and silently drops
the coordinates, so every QR this screen produced landed on "Welcome to DeFlock"
with the location thrown away. It points at `maps.deflock.org` now and carries a
zoom, without which the map opens at a whole-country view even on the right host.
Thanks to @wiilover22 for the report.

**The Locator never locked on, either.** The companion ends Locator mode on any
command it gets, and the app polls its probe survey every ten seconds from a tick
that runs in every screen -- so a hunt was cancelled seconds after it began, or
instantly when opened from a detection on a link that was already up. A target
30 cm away produced nothing across three attempts on the bench; it locks on in
about a second now. The channel a detection is stored with was wrong twice over,
which sent the Locator somewhere the camera never transmits: the companion
stamped frames with wherever its sweep had reached rather than where the frame
arrived, and the app then kept the last channel it heard rather than the
strongest. Since 2.4 GHz channels overlap, a camera on channel 6 really is heard
on 2 and 10 — just far weaker — so the last one heard was often a fringe. On the
bench a target stored as channel 12 read -68 dBm; the same target, stored
correctly as 6, reads -22.

Also: Share to DeFlock no longer labels everything a Flock ALPR camera (a
streetlight, an Axon pole or an unknown MAC was handed over as one, and a passing
drone was offered as a fixed camera), its coordinates are no longer cut in half,
the donation addresses are readable instead of overlapping into a smear, Help &
Warnings is gone with About cut back to name/version/author/contributors, and the
Locator no longer tells you to hold BOOT -- which drops the board into the flash
loader and guarantees it never connects.

**v0.95** - **Pin a whole address.** Randomised is not the same as rotating: the
first camera anyone checked twice kept the identical invented address across
visits days apart. No vendor stands behind it so no OUI table can match it, but
it does not change, so the address itself identifies the unit. `signatures.json`
takes a `macs` key now, and **Air Survey → Flag MAC** does it from the device.
Capped at `Class?` like every other user signature.

Also fixes two things that made evidence read wrong. A fingerprint since
discredited as a commodity scan pattern is dropped when an old hit loads, instead
of being shown as the reason for that detection forever. And `diag.csv` rotates
when its schema changes, because the header was only ever written to an empty
file, so old files ended up with a stale header over rows of a different shape
that nobody could parse correctly.

**v0.94** - Fingerprints you teach the app now actually fire. Learning worked and
matching worked, but they could never meet: the companion scores on OUI and SSID
alone and drops everything else before it even computes the fingerprint, so a
camera on a randomised or unlisted address never crossed the wire and your
`learned.txt` was only ever compared against devices already recognised some
other way. Fingerprints are now matched against the survey feed too, which is not
gated, so a camera you confirmed once is picked up again on the next drive. Still
capped at `Class?`, and no new companion firmware needed.

Also stops a survey being destroyed by the next scan. `survey.csv` still holds
the last session only, which is what you want while hunting one camera, but
`survey_log.csv` now appends every session next to it with the scan time as the
first column, so a drive with several stops is one file and the stops stay
separable.

**v0.93** - **Air Survey**, on the device. Everything probing nearby, matched or
not, ranked so the most camera-shaped behaviour is at the top. A modern Flock
camera randomises its MAC, so it matches no vendor table, scores nothing and is
dropped before it reaches the detection list — standing next to one looked exactly
like standing on an empty street. The survey was already being collected and
written to `survey.csv`; it just wasn't on screen, so the only way to use it was
to pull the card.

"I saw it" now works on a survey row too. Learning was added in v0.91 but hung off
the detection list, which a randomised camera never reaches, so it couldn't be
aimed at the devices it was built for.

Also fixes a way to poison your own detection: confirming a row wrote whatever
fingerprint it carried, with no check, so one mis-tap on a passing phone taught
the app a pattern that then flagged ordinary devices as ALPR candidates. Three
known-generic scan patterns are now refused, at match time as well as when
learning, so a card already carrying one goes inert on upgrade.

**v0.92** - The README and the in-app About screen now list everything the app
actually detects; both were describing three device classes when there are five.
Two real labelling bugs fell out of that audit: a drone rendered untagged in the
list, which by the list's own rule means "ALPR camera", and body-worn cameras
from Utility and Digital Ally were exported with the class "Axon".

**v0.91** - The app can now learn. Confirming a detection you actually looked at
saves its probe fingerprint, so the same camera is caught again after its MAC
randomises -- which every modern Flock unit does. Learned signatures are capped
at "Class?", never Confirmed, and Reports has a Forget Learned option. Nothing
is ever sent anywhere.

**v0.90** - Fixes a bug that left a GPIO companion board unpowered whenever the
Flipper was plugged in. The app stood down from raising the 5V rail whenever USB
was present, on the assumption that the header was fed from VBUS. It isn't, so on
a tethered Flipper the board stayed dead and scans reported no frames at all.

**v0.89** - Same features as v0.88, cut so the release tag is green. Two build
compatibility fixes: the bench emitter now builds on Arduino core 3.x, and the
companion builds for the ESP32-C5. Neither affected a shipped file. The in-app
About screen has also been brought up to date with drones, survey mode and the
redacted exports.

**v0.88** - **Police drones, and exports redacted by default.**

FlipDeFlock now decodes **Remote ID** (ASTM F3411), the broadcast every unmanned
aircraft in US airspace is legally required to transmit, and shows the aircraft's
serial, its type, its position -- and **the operator's position**. Over BLE and
Wi-Fi both. This is the only method that reaches the fleet: of the five drone
vendors a US police department actually buys from (Skydio, BRINC, Aerodome,
Flock, Paladin), **only Skydio holds an IEEE MAC block at all**, so three of the
five can never be found by prefix matching.

Reports now come in three flavours: `Export Marked (Redacted)`, `Export All
(Redacted)` and `Export All (RAW - private)`. The redacted files keep camera
coordinates -- that is the point of the report -- but reduce every MAC to its OUI,
drop the sighting time, your heading and your own labels, and show any SSID that
is not itself a Flock name as a shape rather than a name. That last one matters: a
scan sweeps up every household network in range, and an SSID is often a surname
or a street address and is independently geolocatable. The RAW item names itself,
sorts last, and writes files suffixed `_RAW`.

Also: **survey mode**, which records every transmitter the companion sees rather
than only the ones that matched, so an empty drive can be told apart from a
missed detection; Axon body cameras by their `BWCDEVICE` tag rather than by MAC,
which survives address randomisation; Utility BodyWorn and Digital Ally; four new
field-observed Flock BLE names; and a real export bug fixed -- every exported map
point used to be tagged as a Flock ALPR camera regardless of what it actually
was, including Axon poles, acoustic sensors and unattributed hits.

Seventeen prefixes the community tables carry were checked against the IEEE
registry and **rejected**, including a Samsung block and thirteen Espressif ones
that would have made this app detect its own companion board.

**v0.83** - **Everything a real drive turned up.** A flat battery no longer costs
you the session (hits flush every 30s instead of only on exit). Probe targets are
no longer shown as device names, so a phone looking for "NETGEAR19" stops reading
as a camera called that. Newest hits sort to the top, with the cursor anchored to
the device so a new arrival can't slide a delete onto the wrong one. **Hold OK** on
a hit to Confirm, Rename, Mark or Delete, and a new **Saved Hits** screen to review
a drive afterwards. Renaming never overwrites the observed SSID.

**v0.82** - **Two defaults changed so a drive is worth something out of the box.**
**Save hits is now on** — it was off for privacy, but that meant the common case
was losing a whole drive of detections the moment the app closed, with nothing
written to the card. The toggle is unchanged and turning it off still deletes the
file. **Alert on hit now defaults to Beep+Vibe**, because a camera you drove past
is already behind you by the time a silent buzz gets noticed. Upgrading does not
change settings you already saved. GPS stays off by default.

**v0.81** - **The first real camera fingerprint.** A contributor stood next to a
Flock camera he confirmed by eye and captured its probe fingerprint, and it now
ships as a live detection signal. It is seeded conservatively: a matching probe
lifts a detection from *Likely* to *Class?* (ranked above a bare shared-OUI hit)
but can never auto-confirm on a single source. Only a fingerprint corroborated by
a second independent capture, or a real SSID name, reaches *Confirmed*. No
companion reflash needed.

**v0.80** - **The Support screen's Bitcoin QR never actually worked** -- it fell
back to a "QR n/a" placeholder every time, because the screen never loaded the QR
encoder plugin the way every other QR screen does. Fixed, and while fixing it,
**Ethereum, Litecoin and Bitcoin Cash QRs were added** alongside Bitcoin -- all
four addresses have been in README.md's funding table for a while, only Bitcoin
was ever wired into the app. Support is now a paged list (Up/Down or Left/Right).
Also fixed: the address text shown below a QR for when the scan fails was only
ever showing one line on this screen's 64px height; it now shows the full
address.

**v0.79** - **A camera detector again.** The name says what the app does, so the
app now does only that: find ALPR cameras, police body/in-car equipment and
acoustic sensors, geotag them, and report them. The network-defence and
Bluetooth-device screens have been removed and moved to a separate project.
Camera detection itself is unchanged. Two side effects worth knowing: the app is
now **receive-only with no exceptions** -- the two removed screens held the only
actions that ever transmitted -- and your **companion firmware does not need
reflashing**, because the firmware was not touched.

**v0.78** - **The app powers the companion for you.** The Flipper's GPIO 5V rail
is off at boot, so a board wired to the header stayed dead until you visited
**GPIO -> 5V** by hand first. If the companion has not answered a couple of
seconds into a scan, FlipDeFlock now raises the rail itself, once, and drops it
again on exit -- but only if it was the one that raised it, and never while the
Flipper is on USB (the charger cannot run the boost while a host supplies power).
Toggle at **Settings -> Auto 5V for ESP**. *The power-on itself has not been
watched on hardware yet; see the changelog.*

**v0.77** - **It stops calling everything a Flock camera.** Every ALPR-class
detection used to render as *Flock / ALPR camera*, including competitor hardware
and hits that nothing tied to any vendor. There is a **vendor field** now, shown
on the detail screen and in both report exports; an unattributed hit reads
*ALPR (unattributed)* and only real Flock evidence prints the word Flock. Five
competitor vendors are detected: **Ubicquia, Motorola Solutions, Verkada,
Genetec and Avigilon**, every prefix read out of the IEEE registry one at a time.
Ubicquia matters most, because Axon Lightpost is Ubicquia hardware underneath and
it is a Wi-Fi access point, so it beacons rather than hiding the way a Flock
camera does. **This half needs a companion reflash.**

Also: a **card that says what just beeped**, with OK to jump straight to it; a
nameless row shows the vendor instead of a raw MAC (`Flock 00:00:02`, not
`B4:1E:52:00:00:02`); and Help opens on the row marks rather than on GPS faults.

Older releases (v0.25-v0.74) are in **[changelog.md](changelog.md)** and on the
[Releases page](../../releases). Builds before v0.79 also carried network-defence
and Bluetooth-device screens; those moved to a separate project, and this app is
cameras-only from v0.79 on.

## Layout

```
application.fam          manifest
recon_app.c / _i.h       lifecycle, shared state, settings
scenes/                  start, flock, locator, map, firmware, survey,
                         reports, deflock_handoff, settings, about
views/                   flock list, on-device map, DeFlock QR, locator HUD
helpers/
  flock_db / detect_rules / sig_db   Flock OUIs, SSID/IE signatures, confidence scoring
  esp_link / esp_parser              ESP32 UART link (companion + generic backends)
  esp_flasher                        in-app ESP32 backup/flash (esp-serial-flasher port)
  gps_link / gps_parser              NMEA GPS reader (2nd UART)
  gps_rpc / gps_rpc_convert          phone GPS via the Unleashed RPC location service
  recon_report / report_escape       Markdown + GeoJSON + KML writers
  scan_session / alerts              scan lifecycle, detection alert gating
  survey_rank                        ranks the air survey (not a detection path)
  flock_ble / oui_vendor             BLE Flock signatures, IEEE vendor lookup
lib/esp-serial-flasher/  vendored Espressif flasher (Apache-2.0)
lib/qrcodegen/           vendored Nayuki QR Code generator (MIT)
esp32_companion/         universal ESP32 firmware + flashing guide
```

Prebuilt binaries are published on [Releases](../../releases) and as per-push CI
artifacts, not committed to the repo.

## Credits

The detection method and Flock OUI prefixes build on
[colonelpanichacks/flock-you](https://github.com/colonelpanichacks/flock-you),
[0xXyc/flock-you-wifi-recon](https://github.com/0xXyc/flock-you-wifi-recon), and
the [DeFlock](https://deflock.org) community. The GPS NMEA approach is based on the
Momentum Sub-GHz GPS helper.

Candidate OUI prefixes, the SoundThinking prefix, and the hidden-SSID observation
come from [JakeSwiz/WatchFlock](https://github.com/JakeSwiz/WatchFlock) by Jake /
Swiz Security, itself built on
[justcallmekoko/ESP32Marauder](https://github.com/justcallmekoko/ESP32Marauder).
No code was taken — the signatures and the finding are, with thanks. Which prefixes
were and weren't imported, and why, is recorded in
[docs/signatures.md](docs/signatures.md).

## License

**GPL-3.0-or-later** — see [LICENSE](LICENSE). Copyright (c) 2026 ReconGrunt.

If you distribute a modified version, publish your source under the same terms and
keep the notices intact. Bundled third-party components keep their own compatible
licenses: esp-serial-flasher (Apache-2.0), Nayuki qrcodegen (MIT), jsmn (MIT), and
the ESP MD5 routine (BSD) — see the headers under `lib/`.

**Name & trademark.** The code is free to reuse under the GPL, but the
**"FlipDeFlock"** name and logo are the project's identity, not part of the
licensed code. Don't publish a fork, repackage, or store listing under the
FlipDeFlock name or logo in a way that implies it's official. Rename your
derivative — a "based on FlipDeFlock" credit is welcome. Forking on GitHub keeps
the link and credit intact. Full policy, including how to verify an official
build: [TRADEMARK.md](TRADEMARK.md).

**Commercial licensing.** FlipDeFlock is and stays free under the GPL. If you want
to ship it inside a closed product and can't meet the GPL's source obligations, a
separate commercial licence is available — see [LICENSING.md](LICENSING.md).

## Contributing

This is a community counter-surveillance effort; it improves with more boards and more
field data. The most useful contributions are **field reports and signatures** (new
Flock/ALPR OUIs, SSID/BLE patterns, probe IE fingerprints — and detections that
misfired), **board support** reports, and code.

Ground rules: passive recon only, correctness over features, it builds on every SDK in
the release matrix, and keep it lean.
Full details, the DCO sign-off requirement, and contribution licensing are in
[CONTRIBUTING.md](.github/CONTRIBUTING.md).

Questions, board reports and ideas are welcome in
[Discussions](https://github.com/ReconGrunt/FlipDeFlock/discussions). Bugs and feature
requests belong in [Issues](https://github.com/ReconGrunt/FlipDeFlock/issues/new) —
please open a **new** one even if a closed issue looks related, since closed threads
are not watched.

Found a security issue? Please follow [SECURITY.md](.github/SECURITY.md) rather than opening a
public issue.

## Support

FlipDeFlock is free and stays that way. To help cover development (and the legal
costs of mapping surveillance hardware), crypto (BTC, ETH, LTC, BCH) is accepted —
addresses are near the top of this README and in [SUPPORTERS.md](SUPPORTERS.md).
Donations never gate a feature.

GitHub Sponsors is temporarily unavailable.

What supporters get — recognition and early access to release candidates, never
functionality — is spelled out in [SUPPORTERS.md](SUPPORTERS.md).
