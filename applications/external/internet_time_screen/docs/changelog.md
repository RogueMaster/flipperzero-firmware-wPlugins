v0.2:
- Optional DCF77 GPIO sync to correct Flipper RTC (CET/CEST → user UTC offset)
- Auto-sync once per session when last success is older than 12 hours
- Sync screen with polarity invert and demo fixture (Long OK)
- Host unit tests for DCF77 decode, time conversion, and pulse classification

v0.1:
- Swatch Internet Time (@beats) fullscreen clock
- Local time with 12/24h locale
- Configurable UTC offset in 15-minute steps
