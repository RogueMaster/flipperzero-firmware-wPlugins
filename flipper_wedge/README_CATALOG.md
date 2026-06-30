# Flipper Wedge

**Brought to you by Dangerous Things**

A Flipper Zero application that transforms your device into a contactless tag-to-keyboard interface. Read RFID and NFC tags, then automatically type their UIDs and NDEF data as HID keyboard input via USB or Bluetooth.

## Features

**Tag Reading**
- RFID (125 kHz): EM4100, HID Prox, Indala
- NFC (13.56 MHz): ISO14443A/B, MIFARE Classic, NTAG series, MIFARE Ultralight
- NDEF Support: Parse and output text records from NFC tags (Type 2, Type 4, Type 5)

**Output Methods**
- USB HID Keyboard: Plug into any computer via USB
- Bluetooth HID Keyboard: Pair with phones, tablets, or computers wirelessly
- Dual Output: Both USB and Bluetooth simultaneously when connected

**Scanning Modes**
1. NFC Only - Scan NFC tags, output UID
2. RFID Only - Scan RFID tags, output UID
3. NDEF Mode - Parse NDEF text records from NFC tags
4. NFC + RFID - Scan both in sequence, output combined UIDs
5. RFID + NFC - Scan both in sequence (RFID first)

**Keyboard Layouts** (New in v1.1)
- Built-in: QWERTY (default) and NumPad
- Custom layouts loaded from SD card
- International keyboard support: AZERTY, QWERTZ, Dvorak, Colemak, and more

**Configuration**
- Custom Delimiter: Choose separator between UID bytes (space, colon, dash, or none)
- Enter Key: Optionally append Enter key after output
- Output Mode: Switch between USB and Bluetooth HID
- NDEF Max Length: Limit NDEF text output (250/500/1000 chars)
- Vibration Level: Haptic feedback intensity
- Scan Logging: Optional logging to SD card

## Installation

**Prerequisites:**
- Flipper Zero with firmware version 0.105.0 or higher
- Compatible with Official, Unleashed, Momentum, and RogueMaster firmwares

**From Flipper App Store (Recommended):**
1. Open Flipper Mobile App
2. Navigate to Apps > Categories > Tools
3. Find Flipper Wedge
4. Tap Install

**Manual Installation:**
1. Download the latest .fap file from Releases
2. Copy to SD Card/apps/Tools/ on your Flipper Zero
3. Navigate to Apps > Tools > Flipper Wedge on your Flipper

## Usage

**Quick Start:**
1. Launch Flipper Wedge from Apps > Tools
2. Select your scanning mode from the menu
3. Connect via USB to a computer, or pair via Bluetooth
4. Present a tag to the Flipper Zero
5. The UID (and/or NDEF data) will be typed automatically

**USB Connection:**
1. Connect Flipper Zero to computer via USB
2. App will detect connection automatically
3. Status bar shows "USB" when connected

**Bluetooth Pairing:**
1. Set output mode to Bluetooth in Settings
2. Navigate to BT Pair from main menu
3. Follow on-screen instructions to pair with your device
4. Status bar shows "BT" when connected

## Supported Tags

**RFID (125 kHz):**
- EM4100 / EM4102
- HID Prox / HID 1326
- Indala

**NFC (13.56 MHz):**
- MIFARE Classic 1K / 4K
- MIFARE Ultralight / Ultralight C
- NTAG203 / NTAG213 / NTAG215 / NTAG216
- ISO14443A (any tag type)
- ISO14443B
- ISO15693 (NFC-V)

**NDEF Support:**
- Type 2 NDEF: MIFARE Ultralight, NTAG series
- Type 4 NDEF: ISO14443-4A tags
- Type 5 NDEF: ISO15693 tags
- Text Records: UTF-8 and UTF-16 encoded text

## Building from Source

```bash
# Clone official Flipper Zero firmware
cd ~
git clone --recursive https://github.com/flipperdevices/flipperzero-firmware.git
cd flipperzero-firmware

# Clone this app into applications_user
git clone https://github.com/DangerousThings/flipper-wedge.git applications_user/flipper_wedge

# Build the app
./fbt fap_flipper_wedge
```

## Troubleshooting

**"Connect USB or BT HID" Error:**
- USB: Ensure Flipper is connected via USB cable
- Bluetooth: Complete pairing process in BT Pair menu
- At least one connection method must be active

**Tag Not Detected:**
- Try repositioning the tag on the reader
- Some tags may require specific placement
- Check that the tag type is supported

**NDEF Mode Shows "No NDEF Found":**
- Tag must contain properly formatted NDEF text records
- Not all NFC tags have NDEF data
- Switch to NFC mode to read UID instead

**Bluetooth Won't Pair:**
- Ensure output mode is set to Bluetooth in Settings
- Forget device on target and retry pairing
- Some devices require PIN: try 0000 or 1234

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Support

- **Issues:** [GitHub Issues](https://github.com/DangerousThings/flipper-wedge/issues)
- **Source:** [GitHub Repository](https://github.com/DangerousThings/flipper-wedge)
