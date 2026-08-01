# Changelog

## 1.2

- Added PMR446 (Europe): all 16 analogue channels, 446.00625-446.19375 MHz
- New Settings entry to switch between the FRS (US) and PMR446 (EU) channel
  plans; the active band is shown on the Listen screen and the channel list
- Channel list, scanner, and channel up/down now follow the selected band
- Frequencies are displayed to the full 6.25 kHz raster (e.g. 446.00625 MHz)
  instead of being truncated to kHz
- Bands the connected radio cannot tune are detected at startup and skipped
- Settings: OK now acts on the highlighted row instead of toggling mute

## 1.1

- Scanner reliability: wait 75 ms after each retune and require 2 consecutive
  RSSI hits above squelch before pausing — stops false pauses on tuning transients
- Honest audio note: the CC1101 mirror is a digital receive-data stream, not
  demodulated analog voice — documented the app as a channel activity/RSSI scanner
- Updated app description to match

## 1.0

- Initial release
- 22 standard FRS channels with scrollable channel list
- Channel scanner with auto-pause on signal and auto-resume
- Auto-squelch with adjustable sensitivity
- Live RSSI readout and signal-strength bars
- Subchannel (privacy-code) labels 1–38
