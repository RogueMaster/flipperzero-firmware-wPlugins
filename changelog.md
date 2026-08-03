## v0.2

- Threat tiering: Info (silent, logged), Warn (sustained carrier), Alarm
- Configurable alerts: vibration and sound patterns, tone, volume, per-tier
  thresholds, silent mode
- Vibration is now the primary alert channel and engages one tier earlier
  than sound
- Event log with timestamps, protocol, and reader command decode; persisted
  to SD as CSV
- Session summary screen
- History strip on the status screen: last ~5 minutes at a glance
- First-run screen explaining that tags do not trigger detection, only readers
- Settings persist across reboots
- Diagnostics screen retained for testing

## v0.1

- Initial release: passive 13.56 MHz reader detection via the ST25R3916
  hardware External Field Detector
- Alarm tone, red LED, and on-screen banner while a reader field is present
- Diagnostics screen with live EFD bit, raw edge counter, and alarm self-test
