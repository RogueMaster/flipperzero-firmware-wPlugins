# BT Audio ESP32 Flashing Guide

Complete step-by-step guide to flash BT Audio firmware to your ESP32 and use it with Flipper Zero.

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Quick Start](#quick-start)
3. [Detailed Flashing Instructions](#detailed-flashing-instructions)
4. [Hardware Connection](#hardware-connection)
5. [First Use](#first-use)
6. [Switching Between Firmwares](#switching-between-firmwares)
7. [Troubleshooting](#troubleshooting)

---

## Prerequisites

### Hardware

- ESP32 board (original ESP32, ESP32-C3, or ESP32-S3)
  - ⚠️ **NOT** ESP32-S2 (no Bluetooth Classic support)
- USB cable to connect ESP32 to computer
- Flipper Zero
- Jumper wires for connecting ESP32 to Flipper (or for putting the ESP32 in firmware flash mode like Mayhem board)
- Bluetooth headphones or speaker (for testing)

### Software

- Python 3 installed
- PlatformIO Core: `pip install platformio`
- USB drivers for your ESP32 board:
  - [CP2102/CP2104](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)
  - [CH340](http://www.wch-ic.com/downloads/CH341SER_ZIP.html)

### Verify Python and PlatformIO

```bash
python --version    # Should show Python 3.x
pio --version      # Should show PlatformIO version
```

If PlatformIO is not found:

```bash
pip install platformio
# or
python -m pip install platformio
```

---

## Quick Start

If you're comfortable with command-line tools:

```bash
# 1. Navigate to firmware directory
cd applications/external/bt_audio/esp32_firmware/

# 2. Flash (Linux/Mac)
./flash_bt_audio.sh

# Or Windows
flash_bt_audio.bat

# 3. Connect ESP32 to Flipper Zero (see Hardware Connection section)
# 4. Launch BT Audio app on Flipper
```

---

## Detailed Flashing Instructions

### Step 1: Download/Clone Repository

If you haven't already:

```bash
git clone https://github.com/FatherDivine/flipperzero-firmware-wPlugins.git
cd flipperzero-firmware-wPlugins/applications/external/bt_audio/esp32_firmware/
```

### Step 2: Connect ESP32 to Computer

1. Take your ESP32 board
2. Connect it to your computer using a USB cable
3. Wait for drivers to install (first time only)

### Step 3: Identify COM/Serial Port

#### Linux

```bash
# Before connecting ESP32
ls /dev/tty*

# After connecting ESP32, run again and look for new device
ls /dev/tty*

# Usually /dev/ttyUSB0 or /dev/ttyACM0
```

#### macOS

```bash
ls /dev/cu.*
# Usually /dev/cu.usbserial-* or /dev/cu.SLAB_USBtoUART
```

#### Windows

1. Open Device Manager (Win+X, then select Device Manager)
2. Expand "Ports (COM & LPT)"
3. Look for "USB-to-UART" or "Silicon Labs" or "CH340"
4. Note the COM port number (e.g., COM3, COM4)

### Step 4: Flash Firmware

#### Automatic (Recommended)

**Linux/Mac:**

```bash
# Auto-detect port, source mode (default)
./flash_bt_audio.sh

# Specific port
./flash_bt_audio.sh --port /dev/ttyUSB0

# Sink mode (optional)
./flash_bt_audio.sh --mode sink
```

**Windows:**

```batch
REM Auto-detect port, source mode (default)
flash_bt_audio.bat

REM Specific port
flash_bt_audio.bat COM3

REM Sink mode (optional)
flash_bt_audio.bat COM3 sink
```

The script will:

1. ✓ Check PlatformIO is installed
2. ✓ Auto-detect your ESP32 (or use specified port)
3. ⚠️ Show a warning and wait for confirmation
4. ✓ Build the firmware
5. ✓ Flash to ESP32
6. ✓ Show serial monitor output

#### Manual Flashing

If the automatic script doesn't work:

```bash
# Build firmware
pio run -e esp32dev

# Flash to specific port
pio run -e esp32dev -t upload --upload-port /dev/ttyUSB0

# Monitor serial output
pio device monitor --port /dev/ttyUSB0 --baud 115200
```

### Step 5: Verify Flash Success

You should see:

```bash
=================================
BT Audio ESP32 Firmware
Mode: A2DP SOURCE (transmit audio)
=================================
A2DP Source started
Ready for commands
Send 'STATUS' to check mode
```

If you see this, flashing was successful! Press Ctrl+C to exit monitor.

---

## Hardware Connection

### Disconnect ESP32 from Computer First

⚠️ **Important**: Disconnect ESP32 from USB before connecting to Flipper to avoid power issues.

### Connection Diagram

```bash
Flipper Zero GPIO          ESP32 Board
┌─────────────────┐       ┌─────────────────┐
│                 │       │                 │
│  Pin 13 (TX) ───┼──────►│ RX (GPIO3)      │
│                 │       │                 │
│  Pin 14 (RX) ◄──┼───────┤ TX (GPIO1)      │
│                 │       │                 │
│  Pin 18 (GND)───┼──────►│ GND             │
│                 │       │                 │
└─────────────────┘       └─────────────────┘
```

### Pin Reference

| Flipper Zero | ESP32 | Purpose                    |
|--------------|-------|----------------------------|
| Pin 13 (TX)  | RX    | Flipper sends commands     |
| Pin 14 (RX)  | TX    | Flipper receives responses |
| Pin 18 (GND) | GND   | Common ground              |

### For Specific Boards

#### Mayhem Board

- Plugs directly into Flipper GPIO header
- No additional wiring needed
- Can be powered from Flipper 3V3 (Pin 9)

#### WiFi Dev Board

- Plugs directly into Flipper GPIO header
- No additional wiring needed
- ⚠️ WiFi Dev Board uses ESP32-S2 which does NOT support A2DP
- You'll need to use a different ESP32 variant (BLE support is WIP, not supported in anyway yet but hopefully one day even if minimal/POC)

#### Generic ESP32-WROOM-32

- TX is usually GPIO1 or labeled "TX"
- RX is usually GPIO3 or labeled "RX"
- May need external power if Flipper 3V3 isn't sufficient

### Power Options

1. **From Flipper** (Most boards):
   - Connect Flipper Pin 9 (3V3) to ESP32 VIN
   - Simple, no extra cables
   - May not work for power-hungry boards

2. **External USB Power** (Recommended for testing):
   - Power ESP32 from USB power bank
   - Connect GND between Flipper and ESP32
   - More reliable for development

---

## First Use

### Step 1: Power On

1. ESP32 should be connected to Flipper (UART + GND)
2. Power on Flipper Zero
3. Wait 2-3 seconds for ESP32 to boot

### Step 2: Launch BT Audio App

1. On Flipper: Apps → GPIO → BT Audio
2. You'll see the main menu

### Step 3: Scan for Devices

1. Put your Bluetooth headphones in **pairing mode**
   - Usually: Hold power button until LED blinks rapidly
2. On Flipper: Select "Scan for devices"
3. Wait for scan to complete
4. You'll see a list of found devices

### Step 4: Connect

1. Select your headphones from the list
2. Wait 2-3 seconds for connection
3. "Connected" message will appear

### Step 5: Play Test Tone

1. Select "Play Test Tone"
2. You should hear a 440Hz (A4 note) tone for 2 seconds
3. ✅ Success! Your setup is working

---

## Switching Between Firmwares

### Restore WiFi Marauder Firmware

#### Download Marauder Firmware
1. Visit: https://github.com/justcallmekoko/ESP32Marauder/releases
2. Download the `.bin` file for your board (e.g., `esp32_marauder_v0_13_10_*.bin`)

#### Flash Marauder

**Linux/Mac:**
```bash
./restore_marauder.sh --port /dev/ttyUSB0 --firmware marauder.bin
```

**Windows:**
```batch
restore_marauder.bat COM3 marauder.bin
```

### Flash BT Audio Again

Just run the flash script again:
```bash
./flash_bt_audio.sh
```

---

## Troubleshooting

### "Port not found" or "Permission denied"

**Linux:**
```bash
# Add user to dialout group
sudo usermod -a -G dialout $USER
# Log out and log back in

# Or use sudo
sudo ./flash_bt_audio.sh --port /dev/ttyUSB0
```

**macOS:**
```bash
# Usually no permission issues
# If problems persist, try different USB cable
```

**Windows:**
- Check Device Manager shows the port
- Install correct USB drivers for your ESP32
- Try a different USB port

### "Failed to connect to ESP32"

1. **Hold BOOT button** on ESP32 while flashing starts
2. Release when "Connecting..." changes to "Writing..."
3. Try a different USB cable (data cable, not charge-only)
4. Try a different USB port
5. Check USB drivers are installed

### "Build failed" or "PlatformIO not found"

```bash
# Install/update PlatformIO
pip install --upgrade platformio

# Or
python -m pip install --upgrade platformio

# Verify
pio --version
```

### Flash succeeds but no serial output

1. Press EN/RST button on ESP32
2. Power cycle the ESP32
3. Check baud rate is 115200
4. Try different serial monitor: `screen /dev/ttyUSB0 115200`

### ESP32-S2 "No Bluetooth" error

ESP32-S2 **does not support** Bluetooth Classic (A2DP).
- Use ESP32 (original), ESP32-C3, or ESP32-S3
- WiFi Dev Board has ESP32-S2, so you need a different board

### App shows "No devices found"

1. Ensure headphones are in pairing mode (LED blinking)
2. Check UART connections (TX to RX, not TX to TX)
3. Verify ESP32 booted (check serial monitor)
4. Try different headphones
5. Test headphones with phone first to verify they work

### Connection succeeds but no audio

1. Verify **source mode** (default firmware)
2. Check headphones volume
3. Try playing test tone multiple times
4. Some headphones need 3-5 seconds after connection
5. Check Bluetooth distance (within 10 meters)

### Script says "esptool not found"

```bash
pip install esptool
# or
python -m pip install esptool
```

---

## Safety Notes

⚠️ **Important Safety Information**

1. **Disconnect from USB before connecting to Flipper**
   - Prevents ground loops and power conflicts
   - Could damage ESP32 or Flipper if both powered differently

2. **Check wiring before powering on**
   - Wrong connections can damage hardware
   - Verify TX→RX crossover, not TX→TX

3. **Don't flash while connected to Flipper unless it's a Mayhem Board or supported**
   - Flash with USB to computer only
   - Disconnect, then connect to Flipper
   - Disregard this note if using a Mayhem V2, as those are meant to be flashed connected to the Flipper using the flipper GPIO UART app.The Mayhem has no ports to easily connect to a PC otherwise. Instructions+video is found here: https://flipper.ried.cl/webinstaller_1.4.4/

4. **Power considerations**
   - ESP32 draws ~160mA during BT streaming
   - Flipper 3V3 pin can supply ~500mA max
   - Use external power for intensive use

5. **Static electricity**
   - Touch grounded metal before handling boards
   - Work on non-static surface

### 💚 Flashing is Safe and Recoverable

**Good news for first-time flashers!** It is generally very safe to flash ESP32 chips (including ESP32, ESP32-S2, ESP32-S3, ESP32-C3, ESP32-CAM, etc.) using PlatformIO or Arduino IDE. Even if something goes wrong with the code or the wrong firmware is flashed, you can almost always recover:

- **The bootloader is protected**: Your code runs separately from the chip's built-in boot ROM, which cannot be overwritten by normal flashing operations.
- **Recovery via serial**: As long as you can physically connect the chip via USB and put it into download mode (using the BOOT/IO0 button), you can always upload new firmware.
- **esptool.py handles everything**: Both PlatformIO and Arduino IDE use the robust esptool.py utility to safely flash firmware.

**To force download mode (recovery):**
1. Connect the board to your computer via USB
2. Press and hold the **BOOT** (or **IO0**) button
3. Press and release the **EN** (Reset) button
4. Release the BOOT button
5. The board is now ready to accept new firmware

**The only things that could permanently damage the chip:**
- Physical damage
- Applying incorrect voltages
- Extreme static discharge

A bad firmware upload or incorrect code will **not** brick your ESP32 - you can always reflash it!

---

## Advanced: Building from Arduino IDE

If you prefer Arduino IDE over PlatformIO:

1. Install ESP32 board support in Arduino IDE
2. Install ESP32-A2DP library (by Phil Schatzmann)
3. Open `bt_audio_source.ino`
4. Select your board and port
5. Click Upload

For sink mode, uncomment `#define BT_AUDIO_SINK_MODE` before uploading.

---

## Getting Help

If you're still having issues:

1. Check the main README.md troubleshooting section
2. Verify hardware compatibility (not ESP32-S2)
3. Test with simple Arduino blink sketch first
4. Check ESP32 drivers are installed
5. Try different USB cable/port
6. Open an issue on GitHub with:
   - Board type (ESP32, ESP32-C3, etc.)
   - Error messages
   - Steps you've tried

---

## Summary Checklist

- [ ] Python and PlatformIO installed
- [ ] ESP32 drivers installed
- [ ] ESP32 connected to computer via USB
- [ ] Firmware flashed successfully
- [ ] Serial monitor shows "Ready for commands"
- [ ] ESP32 disconnected from computer
- [ ] ESP32 connected to Flipper (TX↔RX, GND)
- [ ] BT Audio app launches on Flipper
- [ ] Bluetooth headphones in pairing mode
- [ ] Scan finds devices
- [ ] Connection succeeds
- [ ] Test tone plays through headphones

If all checked: You're all set!

---

**Author**: FatherDivine  
**Version**: 2.0  
**Last Updated**: Jan 8, 2026
