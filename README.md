# Staff Time Clock - Flipper Zero

[![Build & Release](https://github.com/vladpereverzyev/flipper-staff-time-clock/actions/workflows/build.yml/badge.svg)](https://github.com/vladpereverzyev/flipper-staff-time-clock/actions/workflows/build.yml)
[![Latest release](https://img.shields.io/github/v/release/vladpereverzyev/flipper-staff-time-clock?cacheSeconds=300)](https://github.com/vladpereverzyev/flipper-staff-time-clock/releases)
[![Downloads](https://img.shields.io/github/downloads/vladpereverzyev/flipper-staff-time-clock/total?cacheSeconds=300)](https://github.com/vladpereverzyev/flipper-staff-time-clock/releases)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)

[![en](https://img.shields.io/badge/lang-en-red.svg)](./README.md)
[![it](https://img.shields.io/badge/lang-it-green.svg)](./README.it.md)
[![es](https://img.shields.io/badge/lang-es-yellow.svg)](./README.es.md)
[![fr](https://img.shields.io/badge/lang-fr-blue.svg)](./README.fr.md)
[![de](https://img.shields.io/badge/lang-de-lightgrey.svg)](./README.de.md)

A **staff time-clock** app for the [Flipper Zero](https://flipperzero.one/). Use it
to register your collaborators and log their clock in/out times: assign each
person a badge - **NFC**, **RFID** or **iButton** - tap it, and every punch is
timestamped and stored on the microSD card as a CSV timesheet you can open in
Excel. It works fully standalone - no phone or PC required.

**Download**: grab the ready-to-flash `.fap` for your firmware from the
[latest release](https://github.com/vladpereverzyev/flipper-staff-time-clock/releases/latest)
(see [Compatibility](#compatibility) for which file to pick).

The reader supports **NFC**, **RFID** and **iButton**; pick which one is active
with Left/Right on the scan screen (the app remembers your last choice). In one
workplace one person can carry an NFC badge, another an RFID fob and another an
iButton key - each just switches to their technology before tapping.

**Any card the person already has works too.** Because the app only **reads the
UID** and never writes anything to the card, a badge already in use with another
company (an office access card, a gym fob, a transit card, ...) can be registered
and used here without being changed or overwritten in any way - your system simply
remembers its UID alongside the others.

> **Identification only - authorized use only.** The app reads a badge's
> identifier (UID) to tell one collaborator from another. It does **not** write to
> or emulate badges, and it does **not** try to bypass any authentication system;
> registering a card here does not affect wherever else that card is used. Use
> only with people and badges you are authorized to manage. See
> [SECURITY.md](SECURITY.md).

## Screens

Real screenshots from the app (Flipper Zero, 128x64):

<table>
  <tr>
    <td align="center"><img src="docs/img/menu.png" width="240" alt="Main menu"><br><b>Main menu</b><br>Work mode, Overview, Badges, History...</td>
    <td align="center"><img src="docs/img/work-clock.png" width="240" alt="Work mode clock"><br><b>Work mode</b><br>Live clock, tap to punch</td>
  </tr>
  <tr>
    <td align="center"><img src="docs/img/greeting.png" width="240" alt="Greeting"><br><b>Tap a badge</b><br>Welcome / Goodbye by name</td>
    <td align="center"><img src="docs/img/overview.png" width="240" alt="Overview"><br><b>Overview</b><br>Per-person today / week / month + break</td>
  </tr>
  <tr>
    <td align="center"><img src="docs/img/pin.png" width="240" alt="PIN lock"><br><b>PIN lock</b><br>Protects exit from the app</td>
    <td align="center"><img src="docs/img/export.png" width="240" alt="Export"><br><b>Export</b><br>CSV, monthly CSV, JSON</td>
  </tr>
</table>

## Features

- **Clock in/out** by tapping a badge in Work mode - recognized badges are
  matched by UID.
- **Multi-technology reader**: **NFC** (13.56 MHz), **LF RFID** (125 kHz) and
  **iButton** (1-Wire Dallas keys) are all supported. Left/Right on the scan
  screen picks which one is active (remembered across sessions), so NFC, RFID
  and iButton badges all work in the same deployment.
- **Works with existing cards**: since only the UID is read (never written), a
  card already used elsewhere - even one issued by another company - can be
  registered and used without altering it.
- **Register a collaborator** the first time their badge is tapped, with a
  custom name. Each person is bound to that specific chip (its UID): every punch
  references that chip.
- **Manage collaborators (badges)**: rename, **replace the chip** if it is lost
  (keeps the person's name and history, only the chip changes), view per-person
  history, **undo the last punch** (fix a mistake), delete (history is kept).
- **Manual correction**: **Add IN** / **Add OUT** from a person's badge adds a
  missing punch at the current time, to fix a forgotten tap or a wrong state.
- **Automatic IN/OUT**: tapping a badge alternates automatically (first tap IN,
  then OUT, then IN, ...) - no manual choice, the punch is logged instantly.
- **Overnight shifts** are counted correctly: an OUT after midnight closes the
  IN from the previous evening, and the worked time counts on the start day.
- **Daily target hours** (optional, in Settings): the Today summary then shows
  the target and the **overtime** or shortfall.
- **Feedback on punch**: distinct **sound**, **vibration** and **LED** for IN vs
  OUT (ascending tone + 1 buzz + green for IN; descending tone + 2 buzzes + blue
  for OUT), so a tap tells you which one it was. Each is toggleable in Settings
  (all on by default).
- **Overview**: a quick per-collaborator screen (today / week / month worked
  time and today's break), Left/Right to switch between people.
- **History** (one menu button) with the raw log (all / today / this week, or
  per collaborator from Badges), a **Today** summary (first in, last out,
  worked total and break time), a **This week** summary (worked time per day +
  weekly total) and a **This month** summary (worked time per collaborator).
- **Storage on microSD** as plain CSV, plus **JSON export**, **dated CSV
  snapshots**, **monthly CSV export** (`punches-YYYY-MM.csv`), a **Backup**
  (timestamped copy of badges + punches) and **Restore** (reload from a backup).
- **Protected mode (PIN)**: optional 4-step **arrow-sequence** code (Up / Down /
  Left / Right - fast to enter) that gates leaving the app; you are offered to
  set it on first launch, or later in Settings.
- **Languages**: English, Italian, Spanish, French, German - selectable in
  Settings (the official firmware exposes no system language to auto-detect).

See the [Roadmap](#roadmap) for what is planned next.

### One chip per person (and losing a chip)

Each collaborator is identified by their chip's **UID**, so assign one chip per
person and keep it as their reference - all of their punches point to that chip.
If someone **loses their chip**, open **Badges -> (person) -> Replace chip** and
tap a new chip (blank or one they already carry): their name and past punches are
kept, only the reference chip is updated.

## Data files

Everything is stored on the microSD under `/ext/apps_data/timeclock/`:

| File          | Contents                                                        |
|---------------|-----------------------------------------------------------------|
| `badges.csv`  | Registered badges: `uid,name,tech,created,last_used,last_event` (`tech` is `NFC`/`RFID`/`iBTN`) |
| `punches.csv` | Punch history: `date,time,name,uid,type` (`type` is `IN`/`OUT`) |
| `config.txt`  | Settings + PIN **hash** and salt (never the PIN itself)         |
| `export.json` | JSON export of the history (generated by *Export -> Export JSON*) |
| `punches-YYYY-MM-DD.csv` | Dated CSV snapshot (*Export -> Export CSV*)          |
| `punches-YYYY-MM.csv` | Monthly CSV export (*Export -> Export month*)          |
| `backup/`     | Timestamped copies of badges + punches (*Export -> Backup*)     |

`punches.csv` is the internal timesheet: every clock in/out for every
collaborator, by day and time. It opens directly in Excel, LibreOffice, Google
Sheets, etc.

Example `punches.csv`:

```csv
date,time,name,uid,type
2026-09-12,08:02,Mario,04A1B2C3D4,IN
2026-09-12,12:31,Mario,04A1B2C3D4,OUT
```

## Build & install

This is a Flipper **external app (FAP)** for the **official firmware**. Build it
with [`ufbt`](https://github.com/flipperdevices/flipperzero-ufbt):

```bash
python3 -m pip install --upgrade ufbt
```

From the project folder (the one containing `application.fam`):

```bash
ufbt
```

Deploy to a connected Flipper (launches it too):

```bash
ufbt launch
```

The built `.fap` lands in `dist/`. You can also copy it to
`SD Card/apps/Tools/` via qFlipper and run it from **Apps -> Tools -> Staff Time Clock**.

> **Firmware note.** The radio layer lives in `timeclock_reader.c` (NFC via the
> ISO14443-3A poller - MIFARE Classic/Ultralight, NTAG, DESFire - the LF RFID
> worker for 125 kHz, and the iButton worker for 1-Wire Dallas keys, all started
> together). It is the part most sensitive to firmware API changes; if a symbol
> differs on your firmware/version, the fix is localized to that one file.

## Compatibility

Staff Time Clock works on the **official** Flipper Zero firmware and on the popular
custom firmwares. A FAP is compiled against a specific firmware API, so each
GitHub Release ships **one `.fap` per firmware** - just download the one that
matches what you run:

| Firmware    | Release asset               |
|-------------|-----------------------------|
| Official    | `timeclock-official.fap`    |
| Momentum    | `timeclock-momentum.fap`    |
| Unleashed   | `timeclock-unleashed.fap`   |
| RogueMaster | `timeclock-roguemaster.fap` |

RogueMaster is built on the Unleashed SDK (binary-compatible). For any firmware
not listed, build from source with `ufbt` (see above): the code targets standard
APIs and is written to be portable across firmwares.

## Protected mode & PIN - what it can and cannot do

The PIN is a fast **4-step arrow sequence** (e.g. Up, Up, Left, Right). You are
offered to set it on first launch, or any time from *Settings -> Set PIN*. When
set, the app starts locked and **Back no longer leaves the app**; the only
software way out is *Settings -> Exit* (or Work mode -> Back), which asks for
the sequence. It is stored only as a **salted hash**, never in clear text.

**Honest limits (by design):**

- No app can stop a **hardware** power-off or a firmware-level force-quit
  (e.g. holding `Left` + `Back` to reboot, or removing power). Protected mode
  covers only the actions the app/firmware expose to software.
- The PIN hash (FNV-1a) prevents storing the code in clear text and gates the
  on-device UI. It is **not** a strong defense against an attacker with physical
  access to the SD card who brute-forces a short arrow sequence offline.
- There is intentionally **no hidden PIN bypass**. Deleting
  `config.txt` on the SD card resets settings (and the PIN).

## Project layout

```
timeclock/
|-- application.fam            # app manifest
|-- timeclock.h / .c           # app lifecycle, entry point, shared helpers
|-- timeclock_storage.h / .c   # microSD persistence + data model
|-- timeclock_reader.h / .c    # NFC / RFID / iButton reader
|-- timeclock_pin.h / .c       # salted PIN hashing
|-- timeclock_i18n.h / .c      # UI strings and translations
|-- views/                     # custom views: work, scan, overview, PIN
`-- scenes/
    |-- timeclock_scene*.{h,c} # scene manager wiring (X-macro)
    `-- timeclock_scene_*.c    # one file per screen
```

## Roadmap

- **Done**: weekly and monthly summaries, break calculation, history filters,
  backup and restore.
- **Ideas**: Bluetooth sync, companion app, CSV import.

## Contributing

Contributions are welcome - see [CONTRIBUTING.md](CONTRIBUTING.md) and the
[Code of Conduct](CODE_OF_CONDUCT.md).

## Support

If Staff Time Clock is useful to you, you can support development:

[![Sponsor on GitHub](https://img.shields.io/badge/Sponsor-GitHub-ea4aaa?logo=githubsponsors&logoColor=white)](https://github.com/sponsors/vladpereverzyev)
[![Support on Ko-fi](https://img.shields.io/badge/Ko--fi-Buy%20me%20a%20coffee-ff5e5b?logo=ko-fi&logoColor=white)](https://ko-fi.com/vladpereverzyev)

## License

Staff Time Clock is **open source**, licensed under the
[GNU General Public License v3.0 or later](LICENSE).

- **Use, modify and redistribute it freely** - for personal or commercial
  time-clocking, on as many Flippers as you like.
- **If you redistribute a modified version**, it must stay under the same
  license and its source code must be made available.
- Copyright © 2026 Vladyslav Pereverzyev. Source files carry an
  `SPDX-License-Identifier: GPL-3.0-or-later` header.
