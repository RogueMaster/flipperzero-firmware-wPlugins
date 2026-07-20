# 2.2

- Cleaner "Connect FM Tuner" screen with per-chip pinout (Si4703 / TEA5767)
- Removed stale fallback wiring text that listed the wrong RST pin

# 2.1

- Fixed Si4703 tuning: crystal oscillator now starts reliably (TEST1 literal write, GPIO3 kept high-impedance) so tune/seek complete
- Volume control on Si4703: OK cycles Low / Med / High / Mute, hold OK for instant mute with restore
- New Start Volume setting (Low / Med / High / Last)
- New Resume Station setting: reopens your last frequency on startup
- Last volume and station persist across runs

# 2.0

- Added **Si4703** chip support with automatic detection (TEA5767 still fully supported)
- New Chip Select setting: Auto / TEA5767 / Si4703
- Signal display now shows the active chip
- Fixed and clarified wiring documentation (Si4703 RST is Pin 4 / PA4)

# 1.1

- Settings menu with region selection
- Station lists load from SD card, defaults auto-generated
- Mute On Exit toggle
- Adjustable seek strength (1-15)
- Audio mode selection: Stereo / Mono Left / Mono Right
- Station files viewer

# 1.0

- Initial release with TEA5767 support and station presets
