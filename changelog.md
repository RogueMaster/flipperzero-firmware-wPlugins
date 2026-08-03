## v0.2

- Renamed from "NFC Alerter" to **NFC Canary** — a canary warns you of danger
  you cannot sense yourself, which is what this does. The app id changed with
  it, so if you installed the earlier build, delete the old
  nfc_alerter.fap from /ext/apps/NFC/ on the SD card; the new app stores its
  settings and event log separately.

- Fixed: alert pattern selection had no effect. Both channels were driven from
  a single pattern, so the sound setting was ignored and every alert played the
  vibration pattern (default Double). Vibration and sound now play their own
  patterns independently.
- Fixed: pattern tables counted a trailing terminator as a real step, so
  one-shots ended with a zero-length step and Constant stuttered once a second
  instead of holding.
- Fixed: the settings alert test blocked the UI thread. It now runs
  asynchronously, and lasts long enough (2.6s) to hear a full SOS cycle.
- Clearer navigation hints; the previous abbreviated footer was unreadable.
- Mode selector hidden until Decoy mode is implemented, so it cannot be set to
  something that does nothing.

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
