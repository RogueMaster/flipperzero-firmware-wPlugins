# Changelog

All notable changes to Cerberus are documented here.

## [1.1] — 2026-06-24

### Added
- **Scope view** — a scrolling RSSI oscilloscope on the Watch screen. Toggle with
  ◄/►, pick the band with ▲/▼. Draws the live noise-floor and detection-threshold
  reference lines so you can watch an attack build over time.
- **Stats dashboard** — a new menu screen with total alerts, per-threat
  (jam/flood/replay) and per-band breakdowns, live noise floors and uptime, plus
  a one-press **Reset** that clears the counters, alert log and CSV.
- **SD card logging** — optional `Log to SD` setting appends every alert to
  `/ext/apps_data/cerberus/alerts.csv` (`uptime_s,threat,mhz,rssi_dbm`).
- **Arm / Silent** — long-press OK on the Watch screen to mute the LED/buzzer/tone
  while detection and logging keep running. Status shown in the title bar.
- **Keep Screen On** — optional setting that holds the backlight on while watching,
  so Cerberus works as a desk sentry.
- Live alert counter in the Watch-screen status bar.

### Changed
- Snapshot now publishes the per-band detection threshold (used by the Scope).
- Config format bumped to v2 (old config is discarded and rebuilt with defaults).

## [1.0] — 2026-06-24

### Added
- Initial release: passive Sub-GHz RF watchdog for 433 / 868 / 915 MHz.
- Jamming, flood and replay detection driven by CC1101 RSSI sampling.
- Three-band live meter UI, adaptive per-band noise floor, alert log,
  configurable detectors and alert channels, and persisted settings.
