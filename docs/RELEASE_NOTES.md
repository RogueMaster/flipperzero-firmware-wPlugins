## Staff Time Clock

Standalone staff time-clock for Flipper Zero. Register collaborators on **NFC**,
**RFID** or **iButton** badges and log clock IN/OUT to a CSV timesheet. Fully
offline, with an optional PIN lock. Identification only - no badge emulation,
no authentication bypass.

**New in v2.11**

- **Freeze on tap fixed**: the NFC poller was never told its own worker
  thread should stop on a read (a regression from the reader rewrite in
  2.9/2.10), so the GUI thread's teardown raced an actively running poller
  thread and froze the device solid the moment an NFC badge was read, in
  Scan and Work mode alike. RFID and iButton were unaffected. Restored the
  exact v1.3.0 shutdown signaling.
- **Chevron hints**: the "<"/">" button labels and "< NFC >" bracket text
  are now a minimal V-shaped chevron at each screen edge, with the active
  technology name in the same spot (bottom center) on both the Scan and
  Work mode screens.

**Earlier (v2.10)**

- **Manual technology selection everywhere**: the round-robin rotation
  between NFC/RFID/iButton is gone from the whole app - Punch, register,
  replace-chip and Work mode all show the active technology with
  Left/Right to change it, shared and remembered across restarts.

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
