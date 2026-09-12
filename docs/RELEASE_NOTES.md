## Time Clock

Standalone staff time-clock for Flipper Zero. Register collaborators on **blank**
NFC/RFID badges and log clock IN/OUT to a CSV timesheet. Fully offline and
PIN-protected. Identification only - no badge emulation, no authentication bypass.

### Which file do I download?

Pick the `.fap` that matches your firmware:

| Firmware    | Asset                       |
|-------------|-----------------------------|
| Official    | `timeclock-official.fap`    |
| Momentum    | `timeclock-momentum.fap`    |
| Unleashed   | `timeclock-unleashed.fap`   |
| RogueMaster | `timeclock-roguemaster.fap` |

### Install

Copy the matching `.fap` to your microSD under `apps/Tools/` (via qFlipper),
then open **Apps -> Tools -> Time Clock**.

### Highlights

- **Work mode**: a locked kiosk clock; tap a badge for Welcome / Goodbye with
  automatic IN/OUT.
- **Sound / vibration / LED feedback** on each punch, distinct for IN vs OUT
  (toggle each in Settings; on by default).
- **Full NFC coverage**: any 13.56 MHz card (ISO14443-A/B, FeliCa, NFC-V) is
  recognized via NfcScanner; LF RFID (125 kHz) via the RFID worker.
- **History** and **daily totals**; **CSV** and **JSON** export.
- **PIN-protected** exit so collaborators cannot leave or tamper with the app.
- Each collaborator is bound to their chip; if a chip is lost, reassign a new one
  from **Badges -> (person) -> Replace chip** (name and history are kept).
- Data is saved to the microSD on every punch - nothing is lost.
