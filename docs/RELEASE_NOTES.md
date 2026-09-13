## Staff Time Clock

Standalone staff time-clock for Flipper Zero. Register collaborators on **NFC**,
**RFID** or **iButton** badges and log clock IN/OUT to a CSV timesheet. Fully
offline, with an optional PIN lock. Identification only - no badge emulation,
no authentication bypass.

**New in v2.8**

- **Work mode freeze fixed**: 2.7 tried to stop the IN/OUT loop by stopping
  and restarting the reader after every badge read, but that froze the
  device solid instead (no input worked at all, needed a hard reset).
  Reverted - Work mode no longer touches the reader after a read; the 60s
  same-badge cooldown from 2.6 already blocks a repeat punch on its own.
- **Icon fix**: the hands no longer touch the outer ring, and the two hands
  are now clearly different lengths (short hour, long minute).
- **Registering a badge**: a sound/vibro/LED cue (honoring the Settings
  toggles) now plays the moment a new chip is read, instead of staying
  silent until it's saved with a name.

**Earlier (v2.6)**

- **Work mode punch loop fixed**: the same-badge debounce was only 3.5s,
  allowing a badge left near the Flipper too long to re-trigger punches
  repeatedly. Raised to 60s.
- **About screen**: no longer shows the "GPL-3.0-or-later" line under the
  copyright notice.

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
