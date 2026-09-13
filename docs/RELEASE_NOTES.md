## Staff Time Clock

Standalone staff time-clock for Flipper Zero. Register collaborators on **NFC**,
**RFID** or **iButton** badges and log clock IN/OUT to a CSV timesheet. Fully
offline and PIN-protected. Identification only - no badge emulation, no
authentication bypass.

**New in v2.3**

- **Overnight shifts** are now counted correctly: an OUT after midnight closes
  the IN from the evening before (the time counts on the shift's start day).
- **Manual correction**: add a missing IN or OUT at the current time from
  **Badges -> (person) -> Add IN / Add OUT**, to fix a forgotten tap.
- **Export month**: save the current month's punches to their own CSV.
- **Daily target hours** (Settings): the Today screen shows the target and the
  overtime (or shortfall).
- **Reader reworked** to scan NFC, RFID and iButton **in rotation** (one radio at
  a time) - lighter on memory, still no manual selection.

**Earlier (v2.2)**

- **iButton** support: 1-Wire Dallas keys work as badges alongside NFC and RFID.
- **Use existing cards**: only the UID is read (never written), so a card already
  used with another company can be registered and used without being changed.

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
then open **Apps -> Tools -> Staff Time Clock**.

### Highlights

- **Work mode**: a locked kiosk clock; tap a badge for Welcome / Goodbye with
  automatic IN/OUT.
- **Sound / vibration / LED feedback** on each punch, distinct for IN vs OUT
  (toggle each in Settings; on by default).
- **NFC** (ISO14443-A: MIFARE/NTAG/DESFire), **LF RFID** (125 kHz) and **iButton**
  (1-Wire Dallas keys) - scanned in rotation, no manual selection. The chip's UID
  is read only (no writing, no emulation).
- **Automatic** punching (tap = IN, then OUT, ...); unknown chips show
  "Not registered". Register people in Badges; lost chip -> Replace chip.
- **Manual correction**: add a missing IN/OUT from Badges, and **Undo last punch**
  per collaborator, to fix a mistaken or forgotten tap.
- **This week** summary: worked time per day of the current week plus the total.
- **This month** summary: worked time per collaborator for the current month.
- **Daily target** hours with overtime shown on the Today screen (optional).
- **History filters**: view all punches, only today, only this week, or a single
  collaborator's history.
- **History** and **daily totals**; **CSV** and **JSON** export.
- **PIN-protected** exit so collaborators cannot leave or tamper with the app.
  The PIN is a fast **4-step arrow sequence**, offered on first launch.
- **Languages**: English, Italian, Spanish, French, German (Settings).
- **Dated CSV export** (`punches-YYYY-MM-DD.csv`), **monthly CSV export**
  (`punches-YYYY-MM.csv`), **Backup** (timestamped copies of badges + punches
  under `backup/`) and **Restore** (reload badges + punches from a saved backup).
- Each collaborator is bound to their chip; if a chip is lost, reassign a new one
  from **Badges -> (person) -> Replace chip** (name and history are kept).
- Data is saved to the microSD on every punch - nothing is lost.
