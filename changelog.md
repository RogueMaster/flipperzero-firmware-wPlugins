# Changelog

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
