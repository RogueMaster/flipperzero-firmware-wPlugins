<p align="center">
  <img src="media/logo.png" width="560" alt="FlipDeFlock">
</p>

<p align="center"><em>Find the cameras that are watching you.</em></p>

FlipDeFlock is a Flipper Zero app that pairs the Flipper with an ESP32 board to
survey the radio around you for surveillance cameras: Flock Safety and other ALPR
hardware, SoundThinking acoustic sensors, and Axon body-worn / in-car police
equipment. The Flipper is the screen, GPS tagger, and logger; the ESP32 does the
Wi-Fi sniffing its BLE-only radio can't. It's for security assessments,
anti-surveillance awareness, and CTF/research.

**Passive recon, and cameras only.** Detection is listen-only — no deauth,
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
| Unleashed | `flipdeflock-unleashed.fap` | 88.3 |
| RogueMaster | `deflock.fap` | 88.3 |

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
instead. It carries the same three `.fap` files under the same names, so switching
back is a matter of copying the stable file over it.

Two things to know about nightlies. They have not been through a release check, so
treat a nightly the way you would treat any untested build. And the **companion
firmware is not rebuilt for them** — keep the `.bin` from the newest tagged
release. If the app and the companion ever disagree about the protocol, the app
tells you on the scan screen rather than quietly misreporting.

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
phone home, so the Wi-Fi work runs on an ESP32. Any ESP32 Flipper board works —
Wi-Fi Dev Board, ESP32 Marauder boards, ReksLab Tri-Board, bare WROOM/WROVER,
Xiao ESP32-S3.

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
  protocol. Adds Locator and dual-band (Wi-Fi + BLE) Flock detection. Flash it from the
  app with **ESP32 Firmware** (no computer needed) or with Arduino IDE /
  arduino-cli (see [`esp32_companion/README.md`](esp32_companion/README.md)).

## What it does

Each item is a screen in the app. Screens marked *(companion)* need the companion
firmware; in Marauder mode they explain what's missing.

- **Flock / ALPR Detect** — the main camera hunt. Finds Flock Safety / ALPR cameras
  over Wi-Fi (and BLE, with the companion), geotags them, and lets you mark them
  for a report. Each row carries a confidence tag (see
  [Detection confidence](#detection-confidence)) and shows its source — probe,
  beacon, or BLE — in the detail view. Set **Alert on
  hit** in Settings (Vibrate / Beep / both) to be told about a camera you aren't
  watching the screen for — it fires once per device, and never for an OUI-only
  "Possible" lead. Three device classes are distinguished rather than lumped
  together: Flock/ALPR cameras, SoundThinking acoustic sensors (`ST`), and Axon
  body-worn / in-car police equipment (`AX`).
- **Flock Map** — a live map around your GPS position: you're at center, cameras
  are plotted by bearing and distance, dot size is confidence, with a heading tick
  and a scale bar. Left/Right zoom, OK re-fits. Needs a GPS fix; ungeotagged
  cameras aren't plotted.
- **Locator** *(companion)* — hunt a marked device by live signal strength: a
  hot/cold meter that climbs as you get closer, peak-hold, and a warmer/colder
  trend. Mark a target from any Flock detection. Works without GPS (a fix only adds a "strongest here" note).
  There's no compass arrow — direction-finding a transmitter needs a directional
  antenna, so you close in by walking.
- **ESP32 Firmware** — backs up the board's current firmware to SD, then flashes a
  `.bin` (companion, Marauder, or a backup) at `0x0`, straight from the Flipper.
  Put the ESP in bootloader/download mode first (hold **BOOT**, tap **RESET**). It
  talks to the bare ROM loader and MD5-verifies the write; flash speed is Safe
  (115200) or Fast (230400) in Settings. You can't brick it — the ROM bootloader
  always allows a re-flash. Built on Espressif's esp-serial-flasher. **Back up
  before you flash.**
- **Reports** — writes to `apps_data/flipdeflock/reports/`: Markdown,
  DeFlock-compatible GeoJSON, and KML. Reports stream row-by-row to SD, so a large scan won't run
  the Flipper out of memory. Pull them with qFlipper or a card reader.
- **Save hits** *(Settings, off by default)* — keeps your detections across app
  restarts in `apps_data/flipdeflock/hits.csv`, so closing the app doesn't throw
  a scan away. Restored hits come back in the list and on the map, showing the age
  of the stored sighting instead of a live signal reading. It is **off by default
  on purpose**: a hit log is a durable record of where you have been. Turning it
  back off deletes the file, and *Reports → Clear Saved Hits* erases it any time.
- **Share to DeFlock** — renders a QR per marked, geotagged camera that opens
  [DeFlock](https://deflock.org) at that location on your phone, so you submit
  through the official app's review flow. The Flipper and ESP never touch a
  network. No Flipper GPS? DeFlock lets you place the pin by hand at
  [deflock.org/report](https://deflock.org/report).

## Screenshots

| | |
|:--:|:--:|
| <img src="media/screenshots/alpr.png" width="330" alt="Flock / ALPR Detect"><br>**Flock / ALPR Detect** | <img src="media/screenshots/menu.png" width="330" alt="Main menu"><br>**Main menu** |
| <img src="media/screenshots/flock-detail.png" width="330" alt="Detection detail"><br>**Why it was flagged** | <img src="media/screenshots/esp32-firmware.png" width="330" alt="ESP32 Firmware"><br>**ESP32 Firmware** |

<sub>Captured on a Flipper Zero running v0.49. The devices shown are fabricated
demo records — no real network or location appears in any screenshot.
The row tags for SoundThinking (`ST`) and Axon (`AX`) arrived later and are not
pictured here yet.</sub>

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

## On-screen legend

RSSI is shown as signal bars (taller = stronger); the highlighted row shows the
exact dB. `-33dB` closer to 0 means physically closer.

**Flock / ALPR Detect** — header `ESP ch6  frames 339  hits 0`
- **ESP** (or `...`) — companion connected / still waiting
- **ch / frames / hits** — channel · 802.11 frames captured · Flock detections, counted this session (reset each time you open the screen)
- **row tag** — `!` CONFIRMED · `F` probe-fingerprint · `L` Likely · `p` Possible · `.` OUI-only · `*` marked
- **`ST` after the tag** — a SoundThinking (ShotSpotter) acoustic sensor, not an ALPR camera. Untagged rows are cameras; the detail screen names the class in full
- **`AX` after the tag** — Axon body-worn or in-car police equipment. Not fixed infrastructure: it moves with a person or a vehicle, so it says nothing about a camera on a pole
- **GPS badge** - filled `GPS 9` = locked with 9 satellites, filled `GPS` = locked but nothing reported a satellite count (normal on the `Phone` source), hollow `GPS` = on and searching. A fault names what to fix and never says "GPS", because a filled badge starting with those three letters reads as a lock: `!PORT` = GPS and the ESP are on the same UART (put GPS on the other one, LPUART / pins 15-16), `!PIN` = the companion refused that ESP GPS Pin, `!FW` = the companion never answered so reflash it — or, on the `Phone` source, this firmware has no location service (needs Unleashed). Phone-only faults: `!APP` = nothing paired, open qUnleashed · `!PERM` = the phone denied location permission · `!LOC` = the phone's location is off, or the paired device has no receiver · `!ACC` = fixes are arriving but coarser than 100 m, so go outside · `!ERR` = the companion app reported a fault
- Marauder mode shows `rx <n>  hits <n>` instead (serial heartbeat + detection count)

**Locator**
- **mark first** — the report star on any Flock detection adds it to the Locator pool
- **meter / dB** — climbs as you get closer; `WARMER`/`colder` is the trend, the tick above the bar is peak-hold. `out of range` means the target went quiet — walk back to where it was loudest

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
scenes/                  start, flock, locator, map, firmware,
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
