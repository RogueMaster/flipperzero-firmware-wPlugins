# Changelog

## 2.2

- **Fix: `BACK` could look dead when leaving a stealth screen.** Stealth force-darkens the display, and the old exit path only *unlocked* that force — it didn't turn the backlight back on — so a `BACK` press landed you on an unlit menu that looked frozen. Exiting a stealth screen now actively re-lights the display.
- The `BACK` contract is now explicit in every capture view (Sweep/Fingerprint/Survey/Watch): it always bubbles to the scene manager and can't be swallowed by the OK long-press handling.

## 2.1

- **Watch Mode** — an unattended monitor that stands guard between Survey and Logbook: a large mm:ss clock, live contact count, first/last-seen and peak field, plus a blinking "READER PRESENT" band. It wakes the screen the moment a reader appears and auto-logs new contacts (rate-limited). Watch never enters stealth, so it can alert you.
- **Live CSV logging** — the logbook is now written as both a grouped `logbook.txt` and a spreadsheet-friendly `logbook.csv` (`timestamp,type,detail`). Commas and newlines inside a field are scrubbed so columns never shift.
- Sensitivity level (`S:High` etc.) is now shown on the idle Sweep strip, width-measured so the waveform can't overlap it.

## 2.0

- **Fingerprint** — classifies a detected reader's polling cadence (CONTINUOUS / POLLING / INTERMITTENT) with a confidence readout and a logic-analyzer-style pulse train.
- **Site Survey** — sweeps a room over time and returns a CLEAN / TRACE / ACTIVE verdict card.
- **Logbook** — timestamped, RTC-stamped detection history saved to the SD card and browsable on-device.
- **Settings** with persistent config, **Stealth mode** (backlight + LED suppressed, sound/vibe kept), and LEFT-on-Sweep **noise-floor auto-calibration** saved as a Custom sensitivity.
- Hold-OK on Sweep/Fingerprint logs the current reading.

## 1.0

- Initial release. Passive **Sweep** detector for active 13.56 MHz NFC reader/skimmer fields using the onboard NFC chip — analog-style EMF gauge with sweep needle, peak-hold, hot-zone, live waveform, an "ACTIVE READER" alarm, and optional geiger clicks that speed up with field strength. Listen-only; never transmits.
