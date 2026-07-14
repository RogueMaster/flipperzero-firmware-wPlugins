# BT Audio ESP32 Firmware

This directory contains the **BT Audio Firmware** for ESP32, which implements the BT Audio UART protocol for controlling Bluetooth A2DP audio connections from Flipper Zero.

## Firmware Name

This firmware is called **"BT Audio Firmware"** (or `bt_audio` for short). It's specifically designed for Bluetooth A2DP audio streaming with Flipper Zero.

## Overview

The firmware runs on ESP32-based boards and provides:
- **A2DP Sink Mode**: Acts as a Bluetooth audio receiver (like headphones/speakers)
- **UART Protocol**: Simple line-based commands at 115200 baud
- **Audio Output**: I2S interface for connecting to external DACs or amplifiers
- **Test Tone Generation**: Built-in 440Hz test tone for verification
- **WiFi Streaming (DISABLED)**: Code present but disabled due to ESP32 hardware limitation

### ⚠️ WiFi Streaming Limitation

WiFi streaming is **DISABLED** in the main application because the ESP32 (original) cannot reliably run WiFi and Bluetooth A2DP simultaneously. Both radios share the same 2.4GHz hardware, causing WiFi initialization to fail when Bluetooth audio is active. This is a hardware limitation, not a software bug.

For audio streaming, use SD card playback instead. Future ESP32-S3 support may enable WiFi streaming.

## Multi-Board Compatibility

If you're using a multi-board like **Mayhem Board** that has ESP32 + nRF24 + CC1101:

| Module | Status with BT Audio Firmware |
|--------|------------------------------|
| **ESP32** | Runs BT Audio firmware for Bluetooth audio |
| **nRF24L01** | ✅ **Unaffected** - Works normally with Flipper apps |
| **CC1101** | ✅ **Unaffected** - Works normally with Flipper apps |

**The nRF24 and CC1101 modules are separate chips** with their own connections to Flipper Zero. Flashing the ESP32 with BT Audio firmware does not affect them in any way. You can continue using Sub-GHz and NRF-based Flipper apps while the ESP32 runs BT Audio.

**ESP32-CAM Camera**: The camera module on ESP32-CAM boards is not used by BT Audio firmware. If you need camera functionality, you would need to flash different ESP32 firmware. The camera module hardware remains intact - it's just not utilized by this particular firmware.

## Hardware Requirements

### Supported Boards
- **ESP32 (original)**: Full support with Bluetooth Classic A2DP
- **ESP32-C3**: Full support with Bluetooth Classic A2DP
- **ESP32-S2**: Limited - BLE only, no A2DP support (not recommended)

### Connections
| ESP32 Pin | Flipper Zero Pin | Purpose |
|-----------|------------------|---------|
| TX (GPIO1 or 17) | RX (Pin 14) | UART from ESP32 to Flipper |
| RX (GPIO3 or 16) | TX (Pin 13) | UART from Flipper to ESP32 |
| GND | GND | Ground |
| VIN (optional) | 3V3 | Power from Flipper |

### I2S Audio Output (Optional)
For actual audio output to speakers/headphones:
| ESP32 Pin | I2S DAC | Purpose |
|-----------|---------|---------|
| GPIO 14 | BCK | Bit Clock |
| GPIO 15 | WS/LRC | Word Select (Left/Right Clock) |
| GPIO 22 | DATA | Audio Data |
| GND | GND | Ground |

## Software Requirements

### Build Environment
- [PlatformIO](https://platformio.org/) - Recommended IDE and build system
- OR [Arduino IDE](https://www.arduino.cc/) with ESP32 board support

### Dependencies
The following libraries are automatically installed by PlatformIO:
- **ESP32-A2DP** (vendored in `../lib/ESP32-A2DP/`)
- **AudioTools** (https://github.com/pschatzmann/arduino-audio-tools)

## Building the Firmware

### Using PlatformIO (Recommended)

1. **Install PlatformIO**:
   ```bash
   # Install via pip
   pip install platformio
   
   # Or install PlatformIO IDE (VS Code extension)
   ```

2. **Navigate to firmware directory**:
   ```bash
   cd applications/external/bt_audio/esp32_firmware
   ```

3. **Build for your board**:
   ```bash
   # For ESP32 (original)
   pio run -e esp32dev
   
   # For ESP32-C3
   pio run -e esp32-c3
   
   # For ESP32-S2 (not recommended - no A2DP)
   pio run -e esp32-s2
   ```

4. **Flash to board**:
   ```bash
   # Connect ESP32 via USB and flash
   pio run -e esp32dev -t upload
   
   # Monitor serial output
   pio device monitor -b 115200
   ```

### Using Arduino IDE

1. **Install Arduino IDE** and add ESP32 board support:
   - Open Arduino IDE
   - Go to File → Preferences
   - Add to "Additional Board Manager URLs":
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - Tools → Board → Boards Manager → Search "esp32" → Install

2. **Install Libraries**:
   - Tools → Manage Libraries
   - Search and install: "AudioTools" by Phil Schatzmann

3. **Add vendored library**:
   - Copy `applications/external/bt_audio/lib/ESP32-A2DP` to Arduino's libraries folder
   - Usually: `~/Arduino/libraries/ESP32-A2DP`

4. **Open and compile**:
   - Open `src/main.cpp` in Arduino IDE
   - Select board: Tools → Board → ESP32 → ESP32 Dev Module
   - Select port: Tools → Port → (your port)
   - Click Upload

## Flashing the Firmware

### Via USB
Most ESP32 boards can be flashed directly via USB:
```bash
pio run -e esp32dev -t upload
```

> Note: All PlatformIO environments in this project use the built-in `huge_app` partition scheme to accommodate the firmware size.

### Via Serial Adapter
If your board doesn't have USB:
1. Connect a USB-to-Serial adapter (TX→RX, RX→TX, GND→GND)
2. Hold BOOT button while connecting power
3. Run the upload command
4. Release BOOT button after flashing starts

### Via Flipper Zero USB-UART Bridge (boards without USB)
1. On the Flipper, open **USB-UART Bridge** → **GPIO** → **USB-UART Bridge**.
2. Enter boot/flash mode by connecting IO0 to GND (pull IO0 low) while powering/resetting the ESP32.
3. Wire ESP32 TX/RX/GND to the Flipper GPIO pins (TX↔RX, RX↔TX, GND↔GND) and connect the Flipper to the PC via USB-C.
4. Run the flashing script or `pio run -e esp32dev -t upload` using the detected serial port.

### Troubleshooting Flashing
- **"Failed to connect"**: Hold BOOT button during connection
- **Wrong port**: Check with `pio device list`
- **Permission denied**: Add user to dialout group (Linux): `sudo usermod -a -G dialout $USER`

## Testing the Firmware

### Serial Monitor Test
1. Connect ESP32 via USB
2. Open serial monitor at 115200 baud:
   ```bash
   pio device monitor -b 115200
   ```
3. You should see:
   ```
   ESP32 BT Audio Firmware Ready
   Version: 1.0
   Mode: A2DP Sink (Bluetooth Headphones/Speakers)
   A2DP Sink Started
   READY
   ```

### UART Command Test
Send commands via serial monitor:
```
SCAN          → Should respond with SCAN_START and SCAN_COMPLETE
PLAY          → Should respond with PLAYING and PLAY_COMPLETE (if connected)
PLAY_JINGLE   → Should respond with PLAYING and PLAY_COMPLETE (if connected) - plays "Mary Had a Little Lamb"
PLAY_BEEPS    → Should respond with PLAYING and PLAY_COMPLETE (if connected) - plays 3-tone beep
DISCONNECT    → Should respond with DISCONNECTED (if connected)
```

### With Flipper Zero
1. Flash ESP32 firmware
2. Connect ESP32 to Flipper Zero GPIO
3. Launch BT Audio app on Flipper
4. Try scanning and connecting to Bluetooth devices

## UART Protocol Reference

### Commands (Flipper → ESP32)
All commands are newline-terminated strings.

| Command | Format | Description |
|---------|--------|-------------|
| SCAN | `SCAN\n` | Start Bluetooth device scan |
| CONNECT | `CONNECT <MAC>\n` | Connect to device with MAC address |
| PLAY | `PLAY\n` | Play test tone (440Hz, 2 seconds) through connected device |
| PLAY_JINGLE | `PLAY_JINGLE\n` | Play "Mary Had a Little Lamb" jingle (~3.5 seconds) |
| PLAY_BEEPS | `PLAY_BEEPS\n` | Play simple 3-tone beep sequence (~1 second) |
| PLAY_MP3 | `PLAY_MP3\n` | Play all MP3 files from SD card /music folder (base files first, then subfolders) |
| PLAY_PLAYLIST | `PLAY_PLAYLIST\n` | Play only files listed in /music/playlist1.m3u |
| PLAY_PLAYLIST2 | `PLAY_PLAYLIST2\n` | Play only files listed in /music/playlist2.m3u |
| PLAY_FAVORITES | `PLAY_FAVORITES:<count>\n` | Start receiving favorites list (followed by FAV: commands) |
| FAV | `FAV:<filename>\n` | Add a favorite file to the playlist (during PLAY_FAVORITES) |
| FAV_END | `FAV_END\n` | End favorites list and start playing |
| VOL_UP | `VOL_UP\n` | Increase volume by one step (~6%) |
| VOL_DOWN | `VOL_DOWN\n` | Decrease volume by one step (~6%) |
| SET_INIT_VOL | `SET_INIT_VOL:<0-127>\n` | Set initial volume for first playback |
| SET_BT_NAME | `SET_BT_NAME:<name>\n` | Set Bluetooth device name (1-31 chars, saved to NVS, requires restart) |
| EQ_BASS | `EQ_BASS:<-10 to +10>\n` | Set bass EQ gain in dB |
| EQ_MID | `EQ_MID:<-10 to +10>\n` | Set mid EQ gain in dB |
| EQ_TREBLE | `EQ_TREBLE:<-10 to +10>\n` | Set treble EQ gain in dB |
| PAUSE | `PAUSE\n` | Pause current playback |
| RESUME | `RESUME\n` | Resume paused playback |
| NEXT | `NEXT\n` | Skip to next track (debounced to prevent BT disconnection from rapid presses) |
| PREV | `PREV\n` | Go to previous track (debounced to prevent BT disconnection from rapid presses) |
| RESTART | `RESTART\n` | Restart current track from beginning |
| STOP | `STOP\n` | Stop current playback |
| TX_POWER | `TX_POWER:<LEVEL>\n` | Set BT TX power (LOW, MEDIUM, HIGH, MAX) |
| RECONNECT | `RECONNECT\n` | Force reconnection to last connected device (useful when auto-reconnect fails) |
| DISCONNECT | `DISCONNECT\n` | Disconnect from current device |
| PLAY_MP3_FLIPPER | `PLAY_MP3_FLIPPER\n` | Start Flipper SD streaming mode (stream MP3 from Flipper's SD) |
| FLIPPER_FILE | `FLIPPER_FILE:<filename>\n` | (Optional) Set filename being streamed for display purposes |
| MP3_CHUNK | `MP3_CHUNK:<base64_data>\n` | Send base64-encoded MP3 chunk (512 bytes recommended) |
| STREAM_END | `STREAM_END\n` | Signal end of Flipper stream (playback finishes after buffer drains) |
| FLIPPER_STOP | `FLIPPER_STOP\n` | Immediately stop Flipper streaming |
| LIST_FILES | `LIST_FILES\n` | List MP3 files in /music folder for remote SD browsing |
| LIST_FILES | `LIST_FILES:<path>\n` | List files in specific directory for remote SD browsing |
| PLAY_FILE | `PLAY_FILE:<path>\n` | Play a specific file by full path from ESP32 SD |

### Responses (ESP32 → Flipper)
| Response | Format | Description |
|----------|--------|-------------|
| DEVICE | `DEVICE:<MAC>,<Name>\n` | Discovered device during scan |
| SCAN_COMPLETE | `SCAN_COMPLETE\n` | Scanning finished |
| SCAN_START | `SCAN_START\n` | Scanning started |
| CONNECTED | `CONNECTED\n` | Successfully connected to device |
| CONNECTING | `CONNECTING\n` | Connection in progress |
| RECONNECTING | `RECONNECTING\n` | Reconnection attempt in progress |
| RECONNECT_ATTEMPT | `RECONNECT_ATTEMPT\n` | Active reconnection attempt initiated |
| RECONNECT_TIMEOUT | `RECONNECT_TIMEOUT\n` | Reconnection attempts exhausted |
| BT_DISCONNECTED | `BT_DISCONNECTED\n` | Bluetooth connection dropped (may auto-reconnect) |
| BT_DISCONNECTED_RECONNECTING | `BT_DISCONNECTED_RECONNECTING\n` | Bluetooth disconnected, active reconnection in progress |
| BT_RECONNECTED | `BT_RECONNECTED\n` | Bluetooth auto-reconnected successfully |
| PLAYBACK_RESUMED | `PLAYBACK_RESUMED\n` | Playback resumed after reconnection |
| DISCONNECTED | `DISCONNECTED\n` | Fully disconnected from device |
| FAILED | `FAILED\n` | Operation failed |
| ERROR | `ERROR: <message>\n` | Error occurred |
| STATUS | `STATUS: <message>\n` | Status update |
| PLAYING | `PLAYING\n` | Started playing audio |
| PLAYING_FILE | `PLAYING_FILE:<filename>\n` | Currently playing specific file |
| PLAY_COMPLETE | `PLAY_COMPLETE\n` | Finished playing |
| NO_PLAYLIST | `NO_PLAYLIST\n` | playlist1.m3u not found |
| NO_PLAYLIST2 | `NO_PLAYLIST2\n` | playlist2.m3u not found |
| VOLUME | `VOLUME:<level>\n` | Current volume level (0-127) |
| INIT_VOLUME_APPLIED | `INIT_VOLUME_APPLIED:<level>\n` | Initial volume was set |
| BT_NAME_SET | `BT_NAME_SET:<name>\n` | BT device name was set |
| BT_NAME_SAVED | `BT_NAME_SAVED\n` | BT device name was saved to NVS |
| FILE_NOT_FOUND | `FILE_NOT_FOUND:<filename>\n` | File was not found (skipped) |
| FAVORITES_SKIPPED | `FAVORITES_SKIPPED:<count>\n` | Number of favorites that were skipped |
| PREV_TRACK | `PREV_TRACK\n` | Moved to previous track |
| RESTARTED | `RESTARTED\n` | Current track was restarted |
| NAV_QUEUED | `NAV_QUEUED: offset=N, total=N\n` | Navigation command queued (rapid press debouncing) |
| NAV_START | `NAV_START: offset=N\n` | New navigation sequence started |
| NAV_EXECUTE | `NAV_EXECUTE: offset=N, target=N\n` | Queued navigation executed |
| READY | `READY\n` | Firmware ready |
| FLIPPER_STREAM_READY | `FLIPPER_STREAM_READY\n` | ESP32 ready to receive Flipper SD stream data |
| FLIPPER_FILE_ACK | `FLIPPER_FILE_ACK:<filename>\n` | Filename for Flipper streaming was set |
| CHUNK_ACK | `CHUNK_ACK\n` | MP3 chunk was received and decoded |
| STREAM_END_ACK | `STREAM_END_ACK\n` | Stream end marker received |
| FLIPPER_STOPPED | `FLIPPER_STOPPED\n` | Flipper streaming stopped immediately |
| LIST_FILES_PATH | `LIST_FILES_PATH:<path>\n` | Current path being listed for remote SD browsing |
| FILE | `FILE:<full_path>\|<filename>\|<type>\n` | File entry (type: f=file, d=directory) |
| LIST_FILES_COUNT | `LIST_FILES_COUNT:<dirs>,<files>\n` | Count of directories and files found |
| LIST_FILES_END | `LIST_FILES_END\n` | End of file listing |
| LIST_FILES_ERROR | `LIST_FILES_ERROR:<reason>\n` | Error during file listing |

## Configuration

### Changing Bluetooth Device Name
The default BT device name can be changed in `main.cpp`:
```cpp
#define BT_DEVICE_NAME_DEFAULT "FlipperAudio"
```

To change the name at runtime, use the SET_BT_NAME command from Flipper. The name is saved to ESP32's NVS storage and will persist across reboots. Note that a restart is required for the new name to take effect.

### Changing I2S Pins
Edit the pin definitions in `main.cpp`:
```cpp
#define I2S_BCK_PIN 14
#define I2S_WS_PIN 15
#define I2S_DATA_PIN 22
```

### Changing Test Tone
Edit the tone parameters in `main.cpp`:
```cpp
#define TEST_TONE_FREQ 440.0  // Frequency in Hz
#define TEST_TONE_DURATION_MS 2000  // Duration in milliseconds
```

## Architecture Notes

### A2DP Sink Mode
The firmware operates as an A2DP **sink** (receiver), meaning:
- It appears as a Bluetooth audio receiver (like headphones)
- Source devices (phones, computers) connect TO it
- Audio is received and output via I2S

This is the **correct mode for Bluetooth headphones/speakers**.

### A2DP Source Mode (Not Used)
A2DP source mode would:
- Act as an audio transmitter (like a phone)
- Connect TO sink devices (headphones/speakers)
- Require the ESP32 to send audio to the headphones

**Important**: The current implementation uses **sink mode**, which is appropriate for receiving audio from Bluetooth devices, but the scanning/connection logic would need enhancement for production use.

## Known Limitations

1. **Scanning**: The current implementation has a simplified scan function. Full BT Classic device discovery requires additional GAP API integration.

2. **Outbound Connections**: A2DP Sink mode typically waits for connections. For true outbound connections to headphones/speakers, the code would need to be adapted.

3. **ESP32-S2**: Does not support Bluetooth Classic (A2DP). Only BLE is available on S2.

4. **Pairing**: Currently uses simple pairing. PIN/passkey pairing is not implemented.

## Troubleshooting

### Build Errors
- **"AudioTools.h not found"**: Library not installed. Run `pio lib install`
- **"BluetoothA2DPSink.h not found"**: Check that `lib_extra_dirs = ../lib` is in platformio.ini
- **Platform errors**: Update PlatformIO: `pio upgrade`

### Runtime Errors
- **No serial output**: Check baud rate is 115200
- **Bluetooth not starting**: ESP32-S2 doesn't support BT Classic
- **Connection fails**: Ensure correct BT mode and device compatibility

### Flipper Communication Issues
- **No response**: Check UART wiring (TX↔RX crossed)
- **Garbled data**: Verify both sides use 115200 baud
- **Commands ignored**: Check for proper newline termination

## Further Development

### Enhancements
- Implement full BT GAP device discovery for scanning
- Add A2DP Source mode for connecting to headphones
- Support PIN/passkey pairing
- Add device name filtering
- Implement connection retry logic
- Add RSSI reporting
- Support multiple audio profiles (AVRCP metadata)

### Alternative Approaches
- Use ESP-IDF directly for more control
- Implement BLE Audio (LC3 codec) for ESP32-S2 compatibility
- Add WiFi-based audio streaming

## License

This firmware is part of the BT Audio app for Flipper Zero and is licensed under the MIT License.

**ESP32-A2DP Library**: GNU GPL v3 (https://github.com/pschatzmann/ESP32-A2DP)
**AudioTools Library**: GNU GPL v3 (https://github.com/pschatzmann/arduino-audio-tools)

## Author

FatherDivine

## Version

2.3 - Fixed WiFi Streaming Crash (ESP32 Guru Meditation Error)
- Fixed: ESP32 crash during WiFi streaming due to thread-safety issue
  - Audio callback was calling http_client.end() which is not thread-safe
  - This caused "LoadProhibited" crash at address 0x00000030 (null pointer)
  - Now uses flag-based signaling for main loop to clean up HTTP client
- Improved: Significantly larger buffers for more stable internet radio streaming
  - Increased MP3 buffer from 8KB to 32KB (~1-2 seconds of buffer at typical bitrates)
  - Increased pre-buffer from 6KB to 24KB (~75% of buffer filled before playback starts)
  - Increased low-water mark from 2KB to 8KB (trigger refill earlier)
  - Increased chunk size from 1KB to 4KB per WiFi read (more efficient)
  - Increased minimum threshold from 2KB to 8KB for playback attempt
- Improved: More aggressive buffer refilling
  - Reduced refill interval from 5ms to 2ms
  - Reduced main loop delay to 1ms during active streaming
  - Multi-chunk reads when buffer is low (up to 8 chunks in one call when urgent)
  - Faster response to buffer underrun conditions
- Improved: Extended underrun timeout from 5s to 10s for slow streams
- Power: 5V power (via Mayhem board or similar) is recommended for stable WiFi+BT streaming

2.2 - Fixed WiFi Streaming Watchdog Timeout
- Fixed: WiFi streaming causing ESP32 watchdog reset (WDT timeout)
  - Moved WiFi data fetching from audio callback to main loop
  - Audio callback now only consumes pre-buffered data (non-blocking)
  - Added proper yield() calls during pre-buffering
  - Prevents BTC_TASK from blocking and triggering watchdog
- Improved: Better task scheduling during WiFi + Bluetooth operation
  - Using vTaskDelay() for better RTOS integration
  - Chunked WiFi reads to avoid long blocking periods
- Power: 5V power (via Mayhem board or similar) is recommended for stable WiFi+BT streaming

2.1 - Improved Saved Device Connection Reliability
- Fixed: Saved devices now connect reliably like "last connection" feature
  - Device is now set as "trusted" before connection attempt using set_auto_reconnect()
  - This establishes proper bonding relationship for simpler BT devices (like "Jabba")
  - All 10 saved devices can now reconnect without needing pairing mode
- Added: Authentication status feedback during connection
  - STATUS: Authentication successful/failed messages
  - STATUS: Device set as trusted connection target
  - Better debugging info for connection issues
- Increased: Connection timeout from 10s to 15s for simpler BT devices
- Improved: GAP event handling with authentication complete reporting

2.0 - Improved Reconnection & Navigation Rate Limiting
- Added: BT_DISCONNECTED and BT_RECONNECTED notifications to Flipper
  - Allows Flipper app to track connection state during temporary disconnections
  - Provides vibration/sound feedback on connection events
- Added: RECONNECT command for forcing reconnection when auto-reconnect fails
  - Useful when BT connection becomes stale or headphones reconnect without audio
- Added: Navigation rate limiting/debouncing to prevent BT disconnection
  - Rapid NEXT/PREV presses are now queued and executed as single track change
  - Prevents overwhelming the A2DP stream which caused BT disconnection
  - 400ms debounce window collects rapid presses before execution
- Added: PLAYBACK_RESUMED notification after auto-reconnection
- Improved: Auto-reconnect now preserves playback state and resumes automatically
- Improved: Navigation status messages (NAV_QUEUED, NAV_START, NAV_EXECUTE)

1.9 - BT Name Persistence & Simplified Track Navigation
- Added: NVS (Preferences) storage for persistent BT device name
  - Name is saved to ESP32's NVS when SET_BT_NAME command is received
  - Name is loaded from NVS at boot before A2DP initialization
  - Custom BT name now survives power cycles and reboots
- Changed: PREV command simplified to directly go to previous track
  - Removed double-tap "smart restart" behavior
  - Use explicit RESTART command if track restart is desired
- Added: SET_BT_NAME command now returns BT_NAME_SAVED confirmation
- Improved: Error handling for NVS operations

1.8 - EQ Implementation, Auto-Reconnect & Smart Navigation
- Added: Working 3-band EQ with real-time biquad IIR filters
  - Bass: Low-shelf filter at 200Hz
  - Mid: Peaking filter at 1kHz (Q=1.0)
  - Treble: High-shelf filter at 4kHz
- Added: Auto-reconnect support using ESP32-A2DP library
- Changed: PREV command now implements smart restart behavior
  - First press: Restarts current track
  - Second press within 2 seconds: Goes to previous track
- Added: PREV_TRACK response to distinguish actual previous track from restart

1.7 - Audio Improvements, Favorites & Playlist Support
- Added: Configurable initial volume (default 75%) - prevents ear-blasting first play
- Fixed: Volume down no longer jumps to 0 on first press (proper step decrement)
- Added: PLAY_FAVORITES command with file validation (skips missing files)
- Added: PLAY_PLAYLIST command (plays only from playlist1.m3u)
- Added: SET_INIT_VOL command for configuring initial volume
- Added: EQ_BASS, EQ_MID, EQ_TREBLE commands for equalizer adjustment
- Added: FILE_NOT_FOUND response for graceful handling of missing files
- Improved: Robust file validation - missing files are skipped instead of crashing

1.6 - Volume control improvements
- Added: VOL_UP and VOL_DOWN commands for AVRCP volume control
- Added: TX_POWER command for adjusting Bluetooth transmit power

1.5 - Improved SD card initialization for MP3 playback
- Added: SD_MMC support for ESP32-CAM (1-bit mode)
- Added: SPI SD fallback for generic ESP32 boards
- Added: Automatic retry logic for SD card initialization
- Added: Late initialization - will retry SD init when PLAY_MP3 is called
- Changed: SD card initialization now happens AFTER Bluetooth to avoid SPI conflicts
- Improved: Better error messages for SD card troubleshooting

1.4 - Added melody playback options
- Added: "Mary Had a Little Lamb" jingle (PLAY_JINGLE command)
- Added: Simple 3-tone beep sequence (PLAY_BEEPS command)
- Changed: PLAY_TREK command now maps to PLAY_JINGLE for compatibility
- Improved: MP3 playback stability
