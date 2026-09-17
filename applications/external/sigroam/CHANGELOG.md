# Changelog

All notable changes to SigRoam are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.3] — 2026-09-04

Scan control, where the GPS reading comes from, the notification switches, and
one file to download instead of two.

### Added

- **The Sound, Vibro, Backlight and Stealth switches now do something.** They
  have been in Settings since 0.1, stored and displayed but never read by
  anything. The app now alerts you when the GPS fix changes state during a
  survey: a rising two-note beep and a green blink when the fix is acquired, a
  falling beep and a red blink when it is lost, and a short vibrate on either.
  Each channel is gated on its own switch — Stealth suppresses the LED — and
  alerts are throttled to at most one every 3 seconds, with the first one after
  a restart always allowed through. Backlight, when on, holds the display lit
  for as long as you are on the dashboard instead of letting it time out
  mid-drive.

### Changed

- **A release now ships one `.fap` instead of one per firmware.** Built against
  the Official SDK. Loading is gated on the API major. This build targets API 87
  (Official firmware 1.4.3). CFW images that report the same API major can load
  the same file; images on a different API major are not covered. Building this
  source against both SDKs produced files that differ in five bytes — a
  debug-link checksum and one byte of section size — with every code, data,
  relocation and symbol section byte-identical. `sigroam-0.3.fap` is the whole
  download; there is nothing to pick.
- **The GPS reading now comes from the survey data itself.** Until now the GPS
  tab was filled by a separate `gpsdata` query, which the scanner answers only
  when it is not scanning — so the one moment you actually want a position was
  the one moment the reading went stale. Each scan row already carries its own
  latitude, longitude and timestamp, and the tab now reads those, marked
  `(live)` to distinguish them from a sampled reading. Sampling still works on
  the GPS tab while no scan is running.

### Fixed

- **Pressing OK during a scan start or stop no longer stacks up commands.** The
  key was mapped from two states only. While the app was waiting for the scanner
  to confirm a stop, or while it was busy, another press sent another command on
  top of the one still in flight; and after a stop had failed, OK sent a start
  rather than retrying the stop. All seven states are now mapped explicitly:
  transitional states send nothing, and a failed stop retries the stop.
- **A command that failed to reach the scanner no longer looked like it had been
  sent.** The pending marker was cleared regardless of whether the write
  succeeded, so a failed send left the dashboard waiting for a reply to a
  command that never left the Flipper.

## [0.2] — 2026-09-03

Power handling. Both fixes come from one wrong assumption about where the 5 V on
pin 1 comes from, showing up from two directions.

### Fixed

- **The serial port would not open while USB was plugged in.** 0.1 treated the
  OTG boost converter as the only source of 5 V on pin 1 and refused to open the
  port whenever the boost was off. The firmware deliberately keeps the boost off
  while USB VBUS is above 4.5 V — the charger refuses it — and pin 1 is fed
  straight from VBUS in that case, so the port was being closed on a board that
  had power the whole time. The app now checks both sources: VBUS above the
  threshold opens the port, and only below it does the boost state decide.
  Running on a car charger now works the same as running on battery.
- **Unplugging USB stopped the survey without saying so.** Power handover to the
  battery succeeds, but the scanner board browns out and restarts into its
  non-scanning state. 0.1 did not notice: the byte counter simply stopped rising
  while the session timer kept going, so collection could stop with nothing on
  screen to show it. The app now detects the stall and re-issues the scan command
  until the board answers again. On the reference board that takes 67 to 77
  seconds — the time the scanner needs to boot and accept commands — and the
  retries are bounded at 24 attempts, 5 seconds apart, capped at 2 minutes.
- **The README presented the old behaviour as a hardware limitation.** It told
  you the scanner had no power while USB was connected and that you had to unplug
  USB and restart the app. That was a consequence of the power gate above, not of
  the hardware. Corrected, along with the FAQ entry built on it.

### Added

- **Resync status on the Dash tab** — `Resyncing...` while the app is bringing
  the scanner back, a distinct message if the retries run out, and a diagnostic
  row showing the state of the resync itself.

### Changed

- The OTG request is now left standing while USB feeds pin 1, so the power
  service raises the boost on its own the moment USB is unplugged. Handover from
  a car charger to the battery no longer requires restarting the app.
- The power line in the serial log is now prefixed `pwr:` rather than `otg:` and
  reports the measured VBUS voltage next to the OTG state.

## [0.1] — 2026-09-02

First public release. Wi-Fi survey front-end for the Flipper Zero driving an
external ESP32 Marauder scanner over the GPIO serial port.

### Added

- **Dashboard** with four tabs (left/right to switch): live counters, parsed
  record stream, GPS status, and session diagnostics. OK on the Dash tab starts
  and stops the scan; the scan keeps running when you leave the page with Back.
- **Firmware probe** — sends `info` and reports what answered, distinguishing
  "nothing connected" from "connected but not Marauder".
- **Raw log** — unparsed serial lines, including anything the parser did not
  recognise, so a malformed feed is inspectable rather than invisible.
- **Settings** — baud rate (six choices, default 115200), source codec, sound,
  vibro, backlight, stealth, and debug rows. Persisted across restarts.
- **Unique-BSSID estimate** from a 4 KB Bloom filter (32768 bits, 4 hashes),
  session-scoped and RAM-only.
- **GPIO serial handling** with the OTG power gate, physical-state verification
  before opening the port, and detection of the Log Device conflict on pins
  13/14 — each reported on screen instead of failing silently.
- **QR code on the About page** linking to this repository.

### Notes

- Receive-only by design. No deauthentication, handshake capture, evil twin,
  karma, beacon/BLE spam, or password cracking — permanently out of scope.
- Survey data is not written to the Flipper's SD card. Logging is the scanner's
  job; the Flipper provides control and live visibility.
- Built against the official SDK (API 87) using only the common Flipper API.
