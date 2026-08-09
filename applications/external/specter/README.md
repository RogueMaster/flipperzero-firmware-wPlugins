<!-- banner -->
<p align="center">
  <img src="images/banner.png" alt="Specter — NFC reader & skimmer bug-sweep for Flipper Zero" width="100%">
</p>

<h1 align="center">Specter 👻</h1>
<p align="center"><i>Sweep for the readers you can't see.</i></p>

<p align="center">
  <img src="https://img.shields.io/badge/platform-Flipper%20Zero-FF8200?style=for-the-badge&logo=flipper&logoColor=white" alt="Flipper Zero">
  <img src="https://img.shields.io/badge/version-2.3-FF3DAE?style=for-the-badge" alt="Version 2.3">
  <img src="https://img.shields.io/badge/radio-13.56%20MHz%20NFC%20(onboard)-26E8CA?style=for-the-badge" alt="Onboard NFC">
  <img src="https://img.shields.io/badge/hardware-none%20required-9B5DE5?style=for-the-badge" alt="No extra hardware">
  <img src="https://img.shields.io/badge/build-ufbt-2da0ff?style=for-the-badge" alt="ufbt">
  <img src="https://img.shields.io/badge/license-MIT-3ad17a?style=for-the-badge" alt="MIT">
</p>

<p align="center">
  <b>Specter</b> turns your Flipper Zero into a pocket <b>counter-surveillance bug-sweep</b> for
  <b>active 13.56 MHz NFC readers</b>. It <i>passively listens</i> for the RF field that a powered-on
  reader is constantly emitting — a hidden card skimmer slipped into a payment terminal, a covert
  reader behind a door panel, a rogue logger taped under a desk — then tells you <b>where it is</b>,
  <b>what kind of thing it is</b>, <b>whether the room is clean</b>, and — left on watch — <b>the
  moment one appears while you're away</b>. It never transmits.
</p>

<p align="center"><sub>The readers are invisible. Specter makes them visible.</sub></p>

---

## 📟 On the Flipper

<p align="center">
  <img src="images/screens_all.png" width="100%" alt="Specter screens">
</p>
<p align="center">
  <sub>
    <b>Sweep</b> (quiet) &nbsp;·&nbsp; <b>Sweep</b> (reader locked) &nbsp;·&nbsp; <b>Fingerprint</b> &nbsp;·&nbsp; <b>Survey</b> (running) &nbsp;·&nbsp; <b>Survey</b> (verdict)<br>
    <b>Watch</b> (clear) &nbsp;·&nbsp; <b>Watch</b> (reader!) &nbsp;·&nbsp; <b>Logbook</b> &nbsp;·&nbsp; <b>Calibrate</b> &nbsp;·&nbsp; <b>Settings</b>
  </sub>
</p>

---

## ✨ What it does

Specter answers four different questions — *where is it, what is it, is the room clean,* and *did one
show up while I was away* — and each has its own screen.

### 🔍 Sweep — *where is it?*

<img src="images/screen_reader.png" width="42%" align="right" alt="Sweep — active reader found">

The showpiece: an **analog-style EMF gauge**. The needle rides the **FIELD %** in real time, a
peak-hold marker remembers the strongest hit, and the top of the dial is a **hot zone**. A live
waveform traces the noise floor while it's quiet.

The instant a reader's carrier is sensed the screen frames itself in an **alarm border**, a throb
ring pulses around the dial, and the strip flips to **`● ACTIVE READER`** with a proximity readout
(`FAINT → NEAR → CLOSE → STRONG`). Optional **geiger clicks speed up as the field gets stronger**, so
you can sweep a surface with the Flipper in your pocket and *hear* yourself getting warmer.

`OK` resets peak/contacts · `hold OK` logs the reading · `LEFT` calibrates to the room you're in.

<br clear="right">

### 🔬 Fingerprint — *what is it?* &nbsp;<sub>`NEW in 2.0`</sub>

<img src="images/screen_fingerprint.png" width="42%" align="right" alt="Fingerprint — polling reader identified">

Found something? Hold still on it. Specter stops chasing proximity and starts **timing the carrier's
on/off edges**, then tells you what kind of emitter you're looking at:

| Class | What it means |
|---|---|
| **CONTINUOUS** | Carrier held permanently up |
| **POLLING** | Fixed poll cycle — the period is shown in ms |
| **INTERMITTENT** | Bursty but irregular |

You get the **polling period**, **burst width**, **jitter**, **duty-cycle** and a **confidence bar** —
plus a **logic-analyser pulse train** of the raw carrier along the bottom, so the verdict is never
asked to be believed on its own. A reader's polling loop is crystal-timed, so a genuine poll shows up
as an unmistakable square wave.

`OK` saves the finding to the logbook · `hold OK` restarts the measurement.

<br clear="right">

### 🗺️ Site Survey — *is this room clean?* &nbsp;<sub>`NEW in 2.0`</sub>

<img src="images/screen_survey_done.png" width="42%" align="right" alt="Site Survey — verdict">

A **timed sweep** of a whole space. Start it, walk the room, and get **one verdict** at the end
instead of having to watch a needle the entire time:

- **`CLEAN`** — nothing crossed the noise floor
- **`TRACE`** — brief or faint hits, worth a second pass
- **`ACTIVE READER`** — something was up and emitting for real

…with max/average field, contact count, and **how much of the survey a carrier was actually up**.
Runs for 30 s, 60 s or 2 min, and auto-logs the result.

<br clear="right">

### 🛰️ Watch Mode — *tell me if one shows up* &nbsp;<sub>`NEW in 2.1`</sub>

<img src="images/screen_watch_hit.png" width="42%" align="right" alt="Watch Mode — reader present">

Where Sweep is you hunting and Survey is a bounded verdict, **Watch stands guard indefinitely**. Arm
it, set the Flipper down, and walk away. It keeps a running clock and a **detection count**, remembers
**when the last contact was**, and — the whole point — **wakes the screen and sounds off the instant a
reader appears**. Each new contact is auto-logged with its timestamp.

It deliberately **ignores stealth**: a silent, dark guard that never tells you it saw something would
be worse than useless. `OK` re-arms it (clears the count and clock).

<br clear="right">

### 📖 Logbook + live CSV &nbsp;<sub>`CSV NEW in 2.1`</sub>

Every finding is written **twice, from one action** — so it's readable on the device *and* already a
spreadsheet, with no export step to remember:

<table>
<tr><th><code>logbook.txt</code> — grouped, for the on-device viewer</th><th><code>logbook.csv</code> — one flat row per entry</th></tr>
<tr><td>

```
2026-07-18 14:35:11
  SURVEY 60s ACTIVE max74…
2026-07-18 14:32:07
  READER POLLING 204ms…
```

</td><td>

```
timestamp,type,detail
2026-07-18 14:35:11,SURVEY,60s ACTIVE …
2026-07-18 14:32:07,READER,POLLING 204ms …
```

</td></tr>
</table>

Both live in `apps_data/specter/` on the SD card. Nothing leaves the device.

### 🎯 Auto-calibration &nbsp;<sub>`NEW in 2.0`</sub>

Press **LEFT** on the Sweep screen and Specter listens to the **ambient noise floor for 3 seconds**,
then sets the detection threshold just above whatever it measured — saved as your **Custom**
sensitivity. Every room has a different RF floor; this tunes to the one you're standing in rather
than a number baked in at build time.

### 🕶️ Stealth mode &nbsp;<sub>`NEW in 2.0`</sub>

Keeps the **screen and LED dark** for the whole sweep, so the Flipper doesn't glow while you're the
one doing the looking. Sound and vibration keep working — the point isn't to disable the feedback
you're sweeping by. Exiting a stealth screen **re-lights the display**, so `BACK` always lands you on
a lit menu <sub>(fixed in 2.2 — it used to leave the screen dark, which made `BACK` look dead)</sub>.

### 📏 Reading the meter &nbsp;<sub>`FIXED in 2.3`</sub>

**Why a reader you're touching doesn't emit 100% of the time.** The detector measures one physical
thing: what fraction of the time a 13.56 MHz carrier is actually up. But readers **poll** — a short
burst, a sleep, another burst. A typical terminal or access reader is only radiating **20–35% of the
time**, so raw duty-cycle *saturates* around 30% no matter how close you get.

Specter used to print that raw number on the gauge, which made a perfect detection look like a third
of one — and left `CLOSE`/`STRONG` and the survey's peak test permanently out of reach. The meter is
now scaled against that real polling band:

| Raw carrier duty | Meter | Proximity |
|---|---|---|
| 3% (room noise) | 9% | `FAINT` |
| 12% | 34% | `NEAR` |
| 20% | 57% | `CLOSE` |
| 31% *(on top of a reader)* | **89%** | `STRONG` |
| ≥35% / continuous wave | 100% | `MAX` |

`MAX` means **pegged** — you're as close as this measurement can resolve, and moving nearer won't
change it. That's stated rather than hidden, so a stuck needle never looks like a broken one.

The **raw duty is never lost**: the noise floor, auto-calibration and the Fingerprint screen's `DUTY`
all still work in true duty-cycle, because those describe the *signal*, not your distance from it.
Prefer the literal number? **Settings → Meter → Raw**.

### 💾 Everything persists &nbsp;<sub>`NEW in 2.0`</sub>

Sensitivity, survey length, sound, vibe, LED, stealth, logging and meter mode are **saved to the SD
card** the moment you change them. A sweep kit that forgets its setup is worse than no persistence at
all.

---

## 🧠 How it works

Every powered-on NFC reader continuously **pings the air with a 13.56 MHz carrier**, waiting for a card
to wake up. You can't see it, but the Flipper's NFC chip can: the **ST25R3916** has a hardware
**external-field detector** (the same circuit that lets the Flipper emulate a card and know when a reader
is talking to it). Specter parks the chip in detect-only mode and **samples that "field present?" bit
~500 times a second** — without ever switching on its own carrier.

That single bit, sampled fast enough, carries two independent signals:

```mermaid
flowchart LR
    R["🔍 Hidden 13.56 MHz reader / skimmer<br/>(constantly polling its field)"] -- "RF carrier" --> A

    subgraph FLIP["Flipper Zero — Specter.fap"]
      A["ST25R3916 NFC chip<br/>external-field detector"] --> S["Sampler thread<br/>~500 samples/s"]
      S --> M["<b>How much?</b><br/>carrier duty-cycle<br/>peak · average · contacts"]
      S --> C["<b>What rhythm?</b><br/>burst / gap edge timing<br/>period · jitter"]
      M --> G["EMF gauge · waveform<br/>geiger clicks · LED · vibe"]
      M --> V["Survey verdict<br/>CLEAN / TRACE / ACTIVE"]
      C --> K["Emitter class<br/>CONTINUOUS / POLLING /<br/>INTERMITTENT + pulse train"]
      V --> L["📖 SD logbook"]
      K --> L
    end
```

**How much → strength.** Over a short window Specter measures *what fraction of the time* a carrier is
present and smooths it into the **FIELD %**. A reader sitting right on top of the Flipper pegs the meter;
a weaker or further one nudges it. That drives the needle, the proximity word and the click rate.

**What rhythm → identity.** Separately, every transition of that bit is timed. A reader that wakes for
20 ms every 200 ms produces a burst/gap pattern with almost no jitter, because its polling loop is
driven by a crystal-timed state machine. Hand movement and RF noise are not that steady. That
difference is what separates **POLLING** from **INTERMITTENT**.

### The decision layers are tested on a real computer

The places where Specter turns numbers into a *claim* — "this is a polling reader", "this room is
clean", "this is what the needle should read" — are pure C with no hardware dependencies, and they're
pinned down by host tests rather than discovered on the device:

```bash
make -C test     # 284 checks: classifier, survey verdict, meter scaling
```

---

## 🚀 Install

No devboard, no firmware to flash — it's a single `.fap`.

**Option A — prebuilt `.fap` (easiest)**

1. Grab `specter.fap` from the [**Releases**](https://github.com/at0m-b0mb/Specter-FlipperZero/releases) page.
2. Open **qFlipper**, drag the file onto `SD Card / apps / NFC /`.
3. On the Flipper: **Apps → NFC → Specter**.

**Option B — build it yourself with [`ufbt`](https://github.com/flipperdevices/flipperzero-ufbt)**

```bash
# one-time
python3 -m pip install --upgrade ufbt

# from the repo root, with your Flipper plugged in over USB:
ufbt            # build specter.fap into ./dist
ufbt launch     # build, upload to the Flipper and open it
make -C test    # run the host tests for the pure logic
```

The `.fap` lands in `dist/specter.fap`; `ufbt launch` copies it to `apps/NFC/` and starts it for you.

> Icons in `icons/`, screenshots and the banner in `images/` are generated — regenerate with
> `python3 tools_gen_icons.py`, `python3 tools_gen_mockups.py` and `python3 tools_gen_banner.py` (needs `pillow`).

---

## 🎮 Using it

**A sweep, end to end:**

1. Launch **Specter**. On the **Sweep** screen, press **LEFT** and hold still for 3 seconds to
   calibrate to the room's noise floor.
2. Hold the Flipper flat and move it **slowly** across the thing you're checking — a card terminal, a
   door reader, the underside of an ATM lip, a parcel, a desk.
   - **Quiet / flat waveform** → no active reader in range.
   - **Needle climbing, clicks speeding up** → you're approaching an emitter. Keep going toward the peak.
   - **`● ACTIVE READER` + alarm border** → an active reader is right here.
3. Found something? Back out and open **Fingerprint**. Hold the Flipper still against it and let the
   cadence settle — a few seconds is usually enough. Press **OK** to save the finding.
4. Clearing a whole room instead? Open **Site Survey**, walk the space until the bar fills, and read
   the verdict.
5. Leaving the area? Drop the Flipper in **Watch Mode** and it'll stand guard, waking and sounding off
   if a reader turns up while you're gone.
6. Check **Logbook** for everything you've saved, timestamped — or pull `logbook.csv` off the card
   into a spreadsheet.

> 💡 Sweep a known-good reader first (your own phone doing NFC, or a contactless terminal you trust) to
> see what a strong, legitimate field looks like on the meter — and what its fingerprint reads as. Then
> go hunting.

### Controls

| Screen | Key | Action |
|---|---|---|
| **Sweep** | `OK` | Reset peak / contacts |
| | `hold OK` | Log the current reading |
| | `LEFT` | Calibrate the noise floor (3 s) |
| **Fingerprint** | `OK` | Save the finding to the logbook |
| | `hold OK` | Restart the measurement |
| **Site Survey** | `OK` | Run the survey again |
| **Watch Mode** | `OK` | Re-arm (clear count and clock) |
| *anywhere* | `BACK` | Up a level |

---

## 🔬 Honest limitations

- **13.56 MHz (HF) only.** Specter senses the **NFC band** — the one used by contactless payment skimmers,
  most modern access readers, transit and hotel readers. It **cannot** see **125 kHz (LF)** readers
  (older HID Prox / EM4100 door panels); the Flipper's LF path has no equivalent field-detect bit.
- **It senses a reader's *carrier*, not what it reads.** Specter tells you *an active reader is here,
  roughly how close, and what rhythm it polls on* — it does not decode, identify, or capture anything
  the reader does.
- **Strength is relative, not calibrated.** `FIELD %` and the proximity words are a comparative "warmer /
  colder" guide for sweeping, not a measured distance in cm. It's a *scaled* reading of carrier
  duty-cycle against a typical polling band (see [Reading the meter](#-reading-the-meter--fixed-in-23));
  a reader that polls unusually sparsely will read lower at the same distance, and one in continuous-wave
  mode will peg from further away.
- **The meter tops out.** `MAX` means the carrier is up as much as this reader ever keeps it up — past
  that point, closing in genuinely cannot produce a bigger number. Use the `PK` peak-hold to compare
  positions instead.
- **Cadence resolves to ~2 ms.** That's the sampling period. Timings anywhere near it are shown with a
  **`~`** and the confidence is discounted accordingly — Specter would rather flag its own resolution
  floor than quote a precise-looking number it can't stand behind.
- **`CLEAN` means clean at the sensitivity you chose.** A dormant skimmer that only wakes on a real tap,
  or one that's shielded, stays invisible at *any* threshold. Absence of a reading isn't a guarantee of
  absence.
- **One radio at a time.** Specter takes over the NFC chip while sweeping, so close any other NFC app first
  (it'll say *NFC unavailable* if something else holds the radio).

---

## ⚖️ Legal & ethical

Specter is a **defensive, listen-only** tool — it **never transmits**, never powers a field, never touches
the reader. Use it to sweep **your own** POS area, door, desk or belongings, or hardware you're **explicitly
authorised** to assess. You are responsible for how you use it. Know your local laws.

---

## 🗺️ Roadmap

- [x] Persist sensitivity / sound / vibe / LED across reboots
- [x] "Logbook" of detections with timestamps to the SD card
- [x] Background sweep with the screen off — *stealth mode*
- [x] Adjustable threshold for finer range control — *auto-calibration + Custom sensitivity*
- [x] Fingerprint an emitter by its polling cadence
- [x] Timed site survey with a room verdict
- [x] Export the logbook as CSV for reporting — *written live alongside the .txt*
- [x] Long-run unattended watch mode with a wake-on-detection alarm
- [x] Make the meter use its full range against real polling readers
- [ ] Investigate an LF (125 kHz) coil-based reader sense as a separate mode
- [ ] On-device logbook filtering by type

---

## 🗂️ Project layout

```
Specter-FlipperZero/
├── application.fam               # Flipper app manifest (category: NFC)
├── specter.c / specter_i.h       # app entry, wiring, alert feedback, stealth
├── helpers/
│   ├── field_detector.{c,h}      # worker thread: samples the field-present bit,
│   │                             #   duty-cycle + edge timing + calibration
│   ├── emitter_classify.{c,h}    # pure: cadence -> CONTINUOUS/POLLING/INTERMITTENT
│   ├── survey_verdict.{c,h}      # pure: survey stats -> CLEAN/TRACE/ACTIVE
│   ├── field_scale.{c,h}         # pure: raw carrier duty -> full-scale meter
│   ├── specter_settings.{c,h}    # persisted settings (saved_struct)
│   └── specter_log.{c,h}         # SD logbook, RTC-stamped .txt + live .csv
├── views/
│   ├── sweep_view.{c,h}          # the EMF gauge / waveform / alarm screen
│   ├── fingerprint_view.{c,h}    # classification card + pulse-train trace
│   ├── survey_view.{c,h}         # progress + verdict card
│   └── watch_view.{c,h}          # unattended guard: clock + count + alarm
├── scenes/                       # scene-manager navigation
├── test/                         # host tests for the pure decision layers
├── icons/                        # 1-bit Flipper icons (generated)
├── images/                       # banner + screen mockups (generated)
└── tools_gen_*.py                # regenerate icons / mockups / banner
```

---

## 🙏 Credits

- Built by **[at0m-b0mb](https://github.com/at0m-b0mb)**.
- Part of a Flipper security-tool family: **[Argus](https://github.com/at0m-b0mb/Argus-FlipperZero)** (Wi-Fi deauth / evil-twin watchdog), **[GhostTag](https://github.com/at0m-b0mb/GhostTag-FlipperZero)** (BLE anti-stalking hunter) and **[Cerberus](https://github.com/at0m-b0mb/flipper-cerberus)** (Sub-GHz RF watchdog).
- Powered by the [Flipper Zero firmware](https://github.com/flipperdevices/flipperzero-firmware) NFC HAL (`furi_hal_nfc` field detection) + [ufbt](https://github.com/flipperdevices/flipperzero-ufbt).

## 📄 License

[MIT](LICENSE) © 2026 at0m-b0mb
