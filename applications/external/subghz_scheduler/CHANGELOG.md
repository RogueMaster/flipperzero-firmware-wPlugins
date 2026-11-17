# v3.5 (2026-11-17)

- Add valid frequency and region checks before playlist & file loading.

# v3.4 (2026-10-21)

## Bug-fix
- Improve countdown LED blink timing synchronization.
- Update external device power detection to accept both USB-C power as well as OTG.

# v3.3 (unreleased)

## New Features
- Time schedule now uses HH:MM:SS instead of HH:MM.
- 'Start' setting for the Time schedule: start with line 1, or with the line whose time comes up first ('Nearest').
- One-Shot with the Time schedule also sends following lines that share the same time.
- Time editor: Up/Down change the selected block right away and Left/Right switch blocks, no OK needed first. OK saves and closes, Back closes without saving.
- Schedule files are now version 3 (line times in seconds). Version 1 and 2 files still load.
- Red idle LED only blinks during the last minute before a send, instead of every second while waiting.
## Bug-fix
- Playlist lines with the same time now fire one after the other, instead of the second one waiting a day.
- Line times that pass during a long transmission fire right after it, instead of the next day.

# v3.2 (unreleased)

## New Features
- Playlist 'Step' setting: send the whole playlist per trigger ('All') or one line per trigger ('One'), wrapping around.
- 'Schedule' setting when stepping one line at a time:
  - 'Uniform' -- one interval for every line.
  - 'Per Line' -- each line has its own wait before it is sent. A wait of 00:00:00 chains a line to the one before it.
  - 'Time' -- each line is sent at a time of day (HH:MM), in playlist order, repeating daily.
- 'Edit Lines...' menu to set the wait or time for each playlist line.
- One-Shot with a stepped playlist sends a single line (plus any chained zero-wait lines).
- Run view shows the next line and its wait or time, and 'Sending' while transmitting.
- Schedule files are now version 2 and store step mode, schedule type and per-line settings. Version 1 files still load.
## Bug-fix
- Scheduling uses the RTC instead of counting GUI ticks, so long intervals no longer drift.
- Back on the run screen aborts a transmission in progress instead of waiting for the playlist to finish.
- TX thread is no longer freed from its own state callback, and scenes are only switched from the GUI thread.
- Charging is only suppressed while transmitting, not for the whole time the schedule runs.
- Radio is put back to sleep when a file fails to load.
- Empty playlists and playlists over 127 lines are rejected when selected.
- Cancelling file selection keeps the previously selected file.

# v3.1 (unreleased)

## Bug-fix
- Fix external module detection and usage PR #10, by @Lechnio

# v3.0 (2026-02-12)

## New Features
- New top level menu:
  - 'New Schedule' -- Opens schedule edit window.
  - 'Load Schedule' -- Loads schedule settings from a custom file.
  - 'About' -- General info.
- Creates 'subghz_scheduler' app folder for saving files.
- Edit menu now has 'Save Schedule' option to save settings.
- Add custom filetypes for settings and future enhancements.
- Interval is now H:M:S customizable instead of preset, from 00:00:01 to 99:59:59.
- TX Delay is now customizable in 50ms increments from 0-1000ms.
## Bug-fix
- Code cleanup and stability improvements.

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
