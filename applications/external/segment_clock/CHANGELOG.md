# Changelog

## 1.2

- Add a daily alarm. Press Right to open the setup screen: toggle it On/Off and set a time with a simple hour/minute picker. It fires every day at that time.
- When the alarm goes off the screen flashes a big `ALARM` and a melody plays. Any key stops it, and the alarm stays armed so it goes off again the next day.
- A small dot in the top-left corner shows when the alarm is armed.
- Add screen brightness control: Up/Down adjust the brightness (with a bar along the bottom), and Down at the lowest level turns on a dim red nightlight - handy for overnight use.
- The 12/24-hour format now follows the system locale (matching the rest of the firmware), and the alarm setup shows AM/PM to match. The manual Up-key toggle has been removed.
- Remember the alarm and brightness across restarts, saved under `/ext/apps_data/segment_clock/`.

## 1.1

- Add toggle between 12 and 24 hour mode (by @Tyl3rA)

## 1.0

- Initial release: a 7-segment digital clock with a blinking colon
