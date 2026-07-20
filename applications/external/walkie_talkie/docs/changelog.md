# Changelog

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
