# Dual Chip Support - TEA5767 & Si4703

## Overview
FM Radio app now supports **both TEA5767 and Si4703** FM radio chips with automatic detection!

## Features Added

### 1. **Automatic Chip Detection**
- App automatically detects which chip is connected at startup
- Tries Si4703 first (which requires special powerup sequence)
- Falls back to TEA5767 if Si4703 not found
- Works seamlessly with either chip

### 2. **Unified Radio API**
All radio operations now use wrapper functions that work with both chips:
- `radio_init()` - Initialize detected chip
- `radio_set_frequency(freq_khz)` - Set frequency
- `radio_get_frequency(&freq_khz)` - Get current frequency
- `radio_set_mute(bool)` - Mute/unmute
- `radio_seek(bool seek_up)` - Seek up/down
- `radio_set_stereo(bool)` - Set stereo/mono
- `radio_get_info(&info)` - Get radio info (frequency, signal, etc.)
- `radio_sleep()` - Put chip in low power mode

### 3. **Settings Support**
- Audio mode settings work with both chips
  - TEA5767: Stereo, Mono Left, Mono Right
  - Si4703: Stereo or Mono (no individual channel muting)
- Seek strength is automatically mapped to each chip's format
  - TEA5767: 4-level SSL (Search Stop Level)
  - Si4703: 8-bit RSSI threshold

### 4. **UI Updates**
- Signal display now shows chip type: `Signal: 12 (Good) [Si4703]`
- About screen shows detected chip
- Error screen updated: "No FM Chip Detected" with pin info

## Pin Connections

### TEA5767
- **Pin 16 (C0)** - I2C Clock (SCL)
- **Pin 15 (C1)** - I2C Data (SDA)
- **3.3V** - Power
- **GND** - Ground

### Si4703
- **Pin 16 (C0)** - I2C Clock (SCL)
- **Pin 15 (C1)** - I2C Data (SDA)
- **Pin 4 (A4/PA4)** - Reset (**REQUIRED!** Si4703 needs reset pin)
- **3.3V** - Power
- **GND** - Ground
- **SEN pin** - Connect to 3.3V (selects I2C mode)

## How It Works

1. **Startup**: App calls `detect_radio_chip()` which:
   - Tests for Si4703 (performs powerup sequence with RST pin)
   - Tests for TEA5767 (simple I2C check)
   - Sets `detected_chip` global variable

2. **Operations**: All radio functions check `detected_chip` and route to appropriate driver

3. **Registers**: Both register buffers are maintained:
   - `tea5767_registers[5]` (5 x uint8_t)
   - `si4703_registers[16]` (16 x uint16_t)

## Code Changes Summary

### New Global Variables
- `RadioChipType detected_chip` - Tracks which chip is connected
- `uint16_t si4703_registers[16]` - Si4703 register buffer

### New Functions
- `detect_radio_chip()` - Auto-detect chip type
- `get_chip_name()` - Get chip name as string
- `radio_*()` - 8 unified radio control functions

### Modified Functions
- `apply_audio_mode()` - Now works with both chips
- `apply_seek_strength()` - Maps to each chip's format
- All input callbacks - Use unified radio API
- `my_app_alloc()` - Detects and initializes chip
- `my_app_free()` - Properly shuts down radio

## Version History
- **v2.0**: Added Si4703 support with auto-detection
- **v1.1**: Original TEA5767-only version

## Testing
To test:
1. Connect TEA5767 → Should detect and work normally
2. Connect Si4703 → Should detect and work normally
3. Connect neither → Should show "No FM Chip Detected"

## Notes
- Si4703 requires GPIO Pin 4 (PA4) for reset - this is mandatory!
- Si4703 powerup sequence takes ~500ms (oscillator stabilization)
- Both chips share the same I2C bus (pins 15/16)
- Station files and settings work with both chips
