0.1
Added Changelog and published app
## v2.0.0 - Major Improvements

### Faster Lights
- MagicBand+ now uses continuous BLE advertising at 20ms interval (no stop/start gaps)
- Bands hear packets immediately → lights trigger noticeably faster
- E9-05 timing byte changed to always-on (0x8F) so lights don't time out mid-session

### Better UI
- Header shows "20ms" for MB attacks (always locked at fastest interval)
- Attack counter shows "LIVE" indicator when advertising is active
- Single Color page shows live color name + vibration status (e.g. "Cyan + Vibe")
- OK button label changed to "Send" (more accurate than "Start")

### Single Color overhaul
- 32 individual color entries consolidated into ONE "Single Color" entry
- Hold OK to configure color (0-31 with names) and vibration pattern (Off/Pulse/Short/Long/Fast)
- Defaults: Cyan + 1s Pulse vibration

### More vibration options for E9-05
- Previously: ON/OFF only
- Now: Off, Pulse (1s), Short (0.5s), Long (2s), Fast patterns

### Reduced clutter
- Attack list: ~72 entries → 38 entries
- Better descriptive names throughout (no more "00 cyan", "E9-0C Taste the Rainbow", etc.)
- CC codes labeled with their known park context

### Code quality
- MB_ATTACK() macro removes ~400 lines of boilerplate attack definitions
- DISPATCH() macro cleans up the make_packet switch
- Vibration as uint8_t palette index instead of bool
