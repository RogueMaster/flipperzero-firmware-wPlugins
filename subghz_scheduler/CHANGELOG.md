# v2.3 (2026-01-11)

## New Features
- Added new interval times (1, 2, and 5 second).
- Added external CC1101 radio support.
## Bug-fix
- Fixed missing initialization setters on scene enter.
- Fixed TX delay & radio initializer bug.
- General code updates and fixes.

# v2.2 (2025-02-11)

## New Features
- Added 24-hour interval.
- Added option to select interval timing:
  - 'Precise' mode: Interval is length of time from START of one transmission to START of next.
  - 'Relative' mode: Interval is length of time from END of one transmission to START of next.
## Bug-fix
- Fixed TX Delay initial value index.
- Fixed off-by-one-second timing discovered while working on 'precise' mode.
- Fixed long filename display overlap on run screen.

# v2.1 (2025-02-07)

## Bug-fix
- Sending NULL to file browser extension filtering caused intermittent problems on OFW, but completely breaks on Momentum 009 with NULL dereference error. Fixed & tested on Original and M009 firmwares.

# v2.0 (2025-02-05)

## New Features
- Updated running view.
- Transmission now sends in thread instead of blocking.
- Mode Selection:
  - Normal ("Normal"): Transmits after interval triggers, continuing until exited with back button.
  - Immediate ("Immed"): Same as Normal mode, only this transmits immediately when entering the run state.
  - One-Shot ("1-Shot"): Waits the specified interval, transmits, then exits the run loop.
- Menu changes, including moving 'Single' and 'Playlist' markers from "Start" to "Select File"
- If playlist selected, number of list items are displayed in main menu.
- Inter-transmission delay is now selectable from 100, 250, 500, and 1000 milliseconds. It also works for repeated transmissions now instead of just 'Playlist' files.
## Bug-fixes
- Timing bug where 'previous_time' would be set before transmission completes. Very noticeable on longer playlists.
- Repeats caused lockup in v1.0. TX repeats now functional.

# Initial release - v1.0 (2025-01-29)

## Features:
- Can be used with single SUB files (beacon) and TXT playlists.
- Each transmitted signal can individually be sent once or repeated up to 6 times.
- Discrete interval values range from 10 seconds to 12 hours.
- Display shows countdown to next scheduled transmission.