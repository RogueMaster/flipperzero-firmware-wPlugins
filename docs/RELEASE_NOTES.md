## Staff Time Clock

Standalone staff time-clock for Flipper Zero. Register collaborators on **NFC**,
**RFID** or **iButton** badges and log clock IN/OUT to a CSV timesheet. Fully
offline, with an optional PIN lock. Identification only - no badge emulation,
no authentication bypass.

**New in v2.14**

- **Overview page weights fixed**: only the collaborator name is bold now.
  The today/week/month/break stat lines below it are back to regular weight
  - the font was never reset after the bold name, so every line was bold.

**Earlier (v2.13)**

- **Work mode cooldown lowered** from 60s to 2s: a real OUT tap a few
  seconds after IN now registers instead of being silently ignored. The
  long cooldown was a leftover from before 2.12's reader-level flood fix.
- **Work mode text centered**: the clock/greeting was off-center relative
  to the date and technology label; all three now line up on the same
  center.
- **Overview arrows** now match Work mode's chevron style and size.

**Earlier (v2.12)**

- **Work mode freeze fixed, right after the first IN**: the NFC poller in
  continuous mode never stopped itself on a read, so a badge still in the
  field kept re-triggering it as fast as the radio could re-detect it,
  flooding the event queue faster than the GUI thread could drain it. The
  poller now always stops itself on a read; continuous mode re-arms it
  explicitly afterward, paced by actual reads instead of the radio's raw
  poll rate.
- **Chevrons moved** to the same row as the NFC/RFID/iBTN label at the
  bottom, on both the Scan and Work mode screens.

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
