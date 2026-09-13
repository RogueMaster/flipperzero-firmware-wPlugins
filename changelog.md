# Changelog

## 2.7

- The 2.6 debounce fix was not enough: Work mode could still loop IN/OUT
  after a punch even with the badge already pulled away, which meant
  something besides a lingering badge was re-triggering reads. Work mode now
  fully stops the reader (no radio allocated, no poller running) for 5s
  after every read, punch or unknown badge alike, instead of only debouncing
  by time; the scene resumes scanning once the pause elapses. Nothing can
  produce a read event while the reader is stopped, regardless of cause.

## 2.6

- Fixed a runaway IN/OUT loop in Work mode: the same-badge debounce was only
  3.5s, and the reader keeps re-reporting a badge for as long as it sits in
  the field, so a badge left near the Flipper too long kept re-triggering
  punches every 3.5s, alternating IN/OUT indefinitely until pulled away.
  Raised to 60s.
- About screen no longer shows the "GPL-3.0-or-later" line under the
  copyright notice.
- App icon redrawn again with bolder, wider hands so the 10:10 shape reads
  clearly instead of blending into the rim.

## 2.5

- Fixed a hang: Work mode's continuous badge scan rotated NFC/RFID/iButton
  every 500ms forever, tearing down and recreating each radio (including a
  dedicated worker thread for LF RFID and iButton) every time. Left running
  for a while, that churn could wedge the device and force a hard reset. The
  continuous slice is now 4s instead of 500ms; single-shot scans are
  unaffected.
- PIN is now fully optional everywhere: Work mode no longer requires one to
  be set, only at least one registered collaborator. Leaving Work mode or
  the app from the main menu asks for the PIN if one is set, and exits
  immediately if not.
- Settings: the button shown when no PIN is set is now "Enable PIN" instead
  of "Set PIN", to read as the counterpart to "Disable PIN".
- Main menu reordered: Work mode, Badges, Punch, then Overview, History,
  Export, Settings, About.
- App icon redrawn again: the clock hands read closer to 11:05 than 10:10;
  they now spread out more horizontally toward the 10 and 2 positions.

## 2.4

- Fixed a crash: the round-robin badge reader now switches radios on the GUI
  thread instead of the timer service thread, which could hard-fault the
  device on almost any scan or punch.
- Today / This week / This month moved under History, so the main menu has
  fewer buttons.
- New Overview screen: a per-collaborator dashboard (today / week / month
  worked time and today's break); Left/Right switches between people.
- About screen shortened to a quick description, the repo link and the
  copyright notice.
- Settings: disabling the PIN now shows a clear confirmation instead of
  returning silently; "Exit app" renamed to "Exit".
- Onboarding PIN screen text shortened so it always fits on screen.
- Startup warning if the Flipper's clock looks wrong (older than this
  release), so punches are never silently misdated.
- App icon redrawn with the clock hands at 10:10.

## 2.3

- Correct worked-time totals for shifts that cross midnight (an OUT on the next
  day now closes the IN from the evening before, counted on the shift's start
  day).
- Manual correction: add a missing IN or OUT at the current time from
  Badges -> (person) -> Add IN / Add OUT (fixes a forgotten tap).
- Export the current month to its own CSV (Export -> Export month).
- Optional daily target hours (Settings -> Target); Today shows the target and
  the overtime (or shortfall).
- Reader reworked to scan NFC, RFID and iButton in rotation (one radio at a
  time) instead of all at once - lighter on memory, still no manual selection.

## 2.2

- iButton (1-Wire Dallas keys) supported as a third badge technology.
- Reader auto-detects NFC, RFID and iButton with no manual selection.
- Any existing card works (even one issued by another company): only the UID is
  read, never written.

## 2.1

- This month summary (worked time per collaborator).
- Undo last punch per collaborator.
- Warning when the Flipper clock is not set.

## 2.0

- Backup and restore of badges and punch history.

## 1.0

- Register collaborators, automatic IN/OUT punching, Work mode kiosk, history,
  daily and weekly summaries, CSV/JSON export, PIN-protected exit, five UI
  languages.
