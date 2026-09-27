# Changelog

All notable changes to GhostTag are documented here.
This project adheres to [Semantic Versioning](https://semver.org/).

## [2.0.0] — 2026-09-27

A correctness and usability release. Every screen was rebuilt, two deadlock
classes were removed, and the app now does something useful with no extra
hardware at all.

### The big one: the official devboard never worked

The README, the app description and the ESP32 sketch all told people to use the
official Flipper WiFi Devboard. That board is an **ESP32-S2, which has no
Bluetooth radio of any kind** — so GhostTag's documented primary hardware could
never have found a single tracker. Anyone who followed the instructions got a
board that flashed perfectly and then sat there forever.

- The sketch now **refuses to compile** for an S2 rather than producing a
  useless binary.
- The README, the About screen and the app description name the boards that
  actually work (ESP32-S3, ESP32-C3, ESP32-C6, classic ESP32), with real
  products.

### Added

- **Air Check** — a mode that runs on the **Flipper alone**. The onboard
  STM32WB55 cannot scan BLE, but its RF test mode can measure raw energy, so
  Air Check hops the three advertising channels and reports how busy the band
  is. It is **energy only** and says so on every frame: it cannot read an
  address, a vendor or a payload, and it will never claim a tracker is present.
- **Demo mode** — a scripted stalking scenario played through the real UI,
  needing no hardware. An ambient phone and earbuds that are never graded
  threats, a Tile that walks past and leaves without tripping, an AirTag that
  stays and trips at ~20 s, and a SmartTag that joins later. Every demo screen
  is stamped `DEMO`, and a demo session is never written to the log.
- **Settings now persist.** They were RAM-only, so the app forgot everything on
  close — including somebody turning Sound off because they did not want a
  stalker to hear the alert.
- **Session log** to `apps_data/ghosttag/sessions.csv`, capped at 64 KB, with
  *full* reported distinctly from *failed*. Demo sessions are never logged.
- **Boot intro**, and a **Help & About** screen that answers the question a
  first-run user actually has ("why does it say NO BOARD?").
- **Left / Right on the detail screen** step through the detections, and a
  `<3/12>` counter says so. The screen previously had no working key at all.
- **`Clear detections`** in the main menu, and a count beside `Detections`.
- Settings gained **Screen on** and **Log to SD**, plus a **Restore defaults**
  row — which also gives `OK` something to do on a screen where it previously
  did nothing at all.
- `tools_check_layout.py`, `tools_strict_build.py`, `tools_screenshot.py`,
  `tools_gen_banner.py`, `tools_brand_data.py`. The first two run in CI.

### Fixed

- **Two-thread deadlock on teardown.** The radio worker called into the view
  dispatcher, which blocks when its queue is full; the thread that drains that
  queue is the GUI thread, which was simultaneously blocked in
  `furi_thread_join`. Detections now only touch the database and the GUI polls
  for alerts on its tick, so nothing crosses a thread boundary.
- **Dead buttons.** Every view handled `InputTypeShort` only. The firmware
  sends `InputTypeLong` — never Short — once a key is held past the long-press
  threshold, so a slightly firm press was silently dropped on the radar, the
  list and the alert screen. All views now accept Short **or** Long.
- **Lost alerts.** The pending alert was a single slot, so a second tracker
  tripping before the UI drained its queue overwrote the first and that warning
  was gone for good. Promotions are now queued in the records themselves.
- **Alerts were dropped entirely** on the detail, settings and about screens.
- **The alert re-opened itself.** `OK` pushed the detail screen on top of the
  alert, so backing out re-armed a strobing alarm the user had already dealt
  with. The alert is popped first.
- **A second alert stacked another scene**, leaving the user pressing Back
  through a pile of alarms. It now re-points the alert already on screen.
- **The list selection jumped under your finger.** The snapshot is re-sorted on
  every refresh, so a one-dB wobble swapped two rows and `OK` opened the wrong
  device. Selection is anchored to the device address.
- **The link indicator lied.** Liveness was inferred from detections, so a
  working board in a quiet room reported `NO ESP`. The ESP32 now heartbeats and
  any line counts as proof of life — and "never heard a board" is now worded
  differently from "the board went quiet".
- **`STOP` was truncated** by an immediate `furi_hal_serial_deinit`, so the
  board kept scanning after the app closed.
- **Restarting a hunt silently wiped the session.** It is now an explicit
  `Clear detections`; only switching between real and simulated wipes.
- **A tracker seen twice, minutes apart, could be graded a follower.** The
  dwell clock now restarts after 30 s of silence, which removes the app's worst
  false positive.
- **Stale records sat on the dial forever.** The radar shows what is around you
  *now*; the list keeps the history.
- **2.1 KB of stack** on the GUI thread in each of three scene handlers, from a
  local `TrackerRecord[48]`. Now one shared heap buffer.
- **The fourth list row was drawn off-screen** (4 rows × 13 px from y=13 ends
  at 65 on a 64-row display), and three help lines overflowed 128 px.
- **The alert strobed at 10 Hz**, full-width — unpleasant, and squarely in the
  flicker band photosensitive people are warned about. Now 2.5 Hz.
- **Samsung classification matched company ID `0x0075`**, which is on every
  Samsung phone, watch, TV and pair of earbuds — reporting half a train
  carriage as SmartTags. Only the SmartThings Find service UUID is used now.
- **Apple Find My is now split by its length byte**, so a *separated* tag (the
  stalking case) is graded differently from one whose owner is standing there.
- `application.fam` set explicit `sources=`; the recursive glob pulled a stray
  `__pycache__` into the link step.
- CI builds **both** firmware lines and verifies the API version baked into
  each artifact, instead of shipping one build that warns on half of all
  installs.

### Changed

- **The radar no longer draws a crosshair.** A cross through the middle reads
  as N/E/S/W, and GhostTag has no compass and no antenna array. The rings stay,
  because range is real; the angle is a stable hash of the address and the help
  screen says exactly that.
- **"following you" → "travelling with you".** Time in range is the only thing
  measured — there is no GPS and no route matching — so the screen reports what
  it saw rather than what it suspects.
- The detections list shows **dwell time**, which is the number that decides
  the verdict, and the detail screen shows **peak RSSI**, which was recorded
  and never displayed.
- New banner, social card, marks and README.

## [1.0.0] — 2026-06-24

- Initial release: radar view, detections list, per-device detail, alert
  screen, settings, the "following you" heuristic, and the ESP32 companion
  firmware with its plain-ASCII UART protocol.
