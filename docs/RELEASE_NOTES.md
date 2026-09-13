## Staff Time Clock

Standalone staff time-clock for Flipper Zero. Register collaborators on **NFC**,
**RFID** or **iButton** badges and log clock IN/OUT to a CSV timesheet. Fully
offline, with an optional PIN lock. Identification only - no badge emulation,
no authentication bypass.

**New in v2.6**

- **Work mode punch loop fixed**: the same-badge debounce was only 3.5s, and
  the reader keeps re-reporting a badge for as long as it sits in the field,
  so a badge left near the Flipper too long kept re-triggering punches every
  3.5s, alternating IN/OUT indefinitely until pulled away. Raised to 60s.
- **About screen**: no longer shows the "GPL-3.0-or-later" line under the
  copyright notice.
- **Icon fix**: bolder, wider hands so the 10:10 shape reads clearly instead
  of blending into the rim.

**Earlier (v2.5)**

- **Work mode hang fixed**: the continuous badge scan used to rotate
  NFC/RFID/iButton every 500ms for as long as Work mode stayed open, tearing
  down and recreating each radio (including a dedicated worker thread for LF
  RFID and iButton) every time. Left running a while, that churn could wedge
  the device and force a hard reset. The continuous slice is now 4s instead
  of 500ms; single-shot scans (Punch, register, replace chip) are unaffected.
- **PIN is now fully optional**: Work mode no longer requires one to be set,
  only at least one registered collaborator. Leaving Work mode or the app
  from the main menu asks for the PIN if one is set, and exits immediately
  if not.
- **Settings**: the button shown when no PIN is set is now "Enable PIN"
  (was "Set PIN"), to read as the counterpart to "Disable PIN".
- **Main menu reordered**: Work mode, Badges, Punch, then Overview, History,
  Export, Settings, About.
- **Icon fix**: the clock hands read closer to 11:05 than 10:10; they now
  spread out more horizontally toward the 10 and 2 positions.

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
- **Overview**: a per-collaborator dashboard (today / week / month worked time
  and today's break); Left/Right switches between people.
- **History**: all punches, today, this week or this month, all reachable from
  one menu button.
- **This week** summary: worked time per day of the current week plus the total.
- **This month** summary: worked time per collaborator for the current month.
- **Daily target** hours with overtime shown on the Today screen (optional).
- **CSV** and **JSON** export.
- **PIN-protected** exit so collaborators cannot leave or tamper with the app.
  The PIN is a fast **4-step arrow sequence**, offered on first launch.
- **Languages**: English, Italian, Spanish, French, German (Settings).
- **Dated CSV export** (`punches-YYYY-MM-DD.csv`), **monthly CSV export**
  (`punches-YYYY-MM.csv`), **Backup** (timestamped copies of badges + punches
  under `backup/`) and **Restore** (reload badges + punches from a saved backup).
- Each collaborator is bound to their chip; if a chip is lost, reassign a new one
  from **Badges -> (person) -> Replace chip** (name and history are kept).
- Data is saved to the microSD on every punch - nothing is lost.
