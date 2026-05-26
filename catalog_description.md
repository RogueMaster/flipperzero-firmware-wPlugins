# flipnCGM

A Flipper Zero application that reads Abbott FreeStyle Libre continuous glucose monitor (CGM) sensor serial numbers via NFC and optionally logs them to the SD card.

## Features

- Read FreeStyle Libre 1, 2, and 3 sensor serial numbers over NFC (ISO 15693)
- Displays the decoded 9-character ASCII serial and raw 8-byte UID
- Audio feedback on scan (short chime on success, low beep on unrecognised tag)
- Configurable SD card logging with ISO 8601 timestamps and UTC offset
- Four log levels: **Off**, **Error**, **Info**, **Debug**
- UTC offset persists across app restarts

## Usage

1. Open **flipnCGM** from the NFC apps menu.
2. Hold a FreeStyle Libre sensor to the **back** of the Flipper Zero.
3. The decoded serial number and raw UID are displayed on screen.
4. Press **OK** to dismiss the result and scan another sensor.
5. Press **Back** to exit.

## Logging Controls

- **UP** — Cycle log level: Off → Error → Info → Debug → Off
- **LEFT / RIGHT** — Adjust UTC offset in 30-minute steps

Log file is written to /ext/apps_data/flipncgm/flipncgm.log. Log level and UTC offset are shown on the scanning screen and the UTC offset is saved automatically.

## Compatibility

- FreeStyle Libre 1 — tested
- FreeStyle Libre 2 — tested
- FreeStyle Libre 3 — tested (UID format identical)
