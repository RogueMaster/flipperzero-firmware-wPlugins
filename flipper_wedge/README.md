# Flipper Wedge
### Brought to you by Dangerous Things

A Flipper Zero application that transforms your device into a contactless tag-to-keyboard interface. Read RFID and NFC tags, then automatically type their UIDs and NDEF data as HID keyboard input via USB or Bluetooth.

![Category: Tools](https://img.shields.io/badge/Category-Tools-blue)
![Version](https://img.shields.io/badge/Version-1.1-green)
![License](https://img.shields.io/badge/License-MIT-yellow)

## Features

### Tag Reading
- **RFID (125 kHz)**: EM4100, HID Prox, Indala
- **NFC (13.56 MHz)**: ISO14443A/B, MIFARE Classic, NTAG series, MIFARE Ultralight
- **NDEF Support**: Parse and output text records from NFC tags
  - Type 2 NDEF (MIFARE Ultralight, NTAG)
  - Type 4 NDEF (ISO14443-4A with APDU)
  - Type 5 NDEF (ISO15693)

### Output Methods
- **USB HID Keyboard**: Plug into any computer via USB
- **Bluetooth HID Keyboard**: Pair with phones, tablets, or computers wirelessly
- **Dual Output**: Both USB and Bluetooth simultaneously when connected

### Scanning Modes
1. **NFC Only** - Scan NFC tags, output UID
2. **RFID Only** - Scan RFID tags, output UID
3. **NDEF Mode** - Parse NDEF text records from NFC tags
4. **NFC + RFID** - Scan both in sequence, output combined UIDs
5. **RFID + NFC** - Scan both in sequence (RFID first)

### Configuration
- **Custom Delimiter**: Choose separator between UID bytes (space, colon, dash, or none)
- **Enter Key**: Optionally append Enter key after output
- **Output Mode**: Switch between USB and Bluetooth HID
- **Keyboard Layout**: Support for international keyboards (AZERTY, QWERTZ, Dvorak, etc.)
- **NDEF Max Length**: Limit NDEF text output (250/500/1000 chars)
- **Vibration Level**: Haptic feedback intensity
- **Scan Logging**: Optional logging to SD card

## Installation

### Prerequisites
- Flipper Zero with firmware version **0.105.0 or higher**
- Compatible with:
  - Official firmware
  - Unleashed firmware
  - Xtreme firmware
  - RogueMaster firmware

### From Flipper App Store (Recommended)
1. Open Flipper Mobile App
2. Navigate to **Apps** → **Categories** → **Tools**
3. Find **Flipper Wedge**
4. Tap **Install**

### Manual Installation
1. Download the latest `.fap` file from [Releases](../../releases)
2. Copy to `SD Card/apps/Tools/` on your Flipper Zero
3. Navigate to **Apps** → **Tools** → **Flipper Wedge** on your Flipper

### Build from Source
See [Building from Source](#building-from-source) below.

## Usage

### Quick Start
1. Launch **Flipper Wedge** from Apps → Tools
2. Select your scanning mode from the menu
3. Connect via USB to a computer, or pair via Bluetooth
4. Present a tag to the Flipper Zero
5. The UID (and/or NDEF data) will be typed automatically

### USB Connection
1. Connect Flipper Zero to computer via USB
2. Select **USB HID** mode when prompted on Flipper
3. App will detect connection automatically
4. Status bar shows "USB" when connected

### Bluetooth Pairing
1. In the app, select **Settings** → **Bluetooth** → **Enabled**
2. Navigate to **BT Pair** from main menu
3. Follow on-screen instructions to pair with your device
4. Status bar shows "BT" when connected

### Settings
Access **Settings** from the main menu to configure:
- **Delimiter**: Choose separator between bytes (` `, `:`, `-`, or none)
- **Append Enter**: Toggle Enter key after output
- **Output Mode**: USB or Bluetooth HID (switches dynamically, no restart needed)
- **NDEF Max Length**: Limit for NDEF text output (250, 500, or 1000 chars)
- **Vibration Level**: Haptic feedback intensity (Off, Low, Medium, High)
- **Mode Startup**: Remember last mode or always use a default
- **Scan Logging**: Enable logging scans to SD card

### Keyboard Layouts

By default, Flipper Wedge sends US QWERTY keycodes. If your computer uses a different keyboard layout (French AZERTY, German QWERTZ, Dvorak, etc.), the typed characters may be wrong.

**Built-in layouts:**
- **Default (QWERTY)** - Standard US layout
- **NumPad** - Uses numpad keycodes (layout-independent, requires NumLock)

**Custom layouts:**
1. Download a layout file from [flipper-wedge-keyboard-layouts](https://github.com/dangerous-tac0s/flipper-wedge-keyboard-layouts)
2. Copy to your Flipper SD card: `/ext/apps_data/flipper_wedge/layouts/`
3. Go to **Settings** → **KB Layout** and select your layout

Available layouts include: French AZERTY, German QWERTZ, Hungarian, Czech, Spanish, Italian, Portuguese, Nordic (Swedish, Norwegian, Danish, Finnish), Dvorak, Colemak, and more.

### Scan Modes Explained

#### NFC Only
- Scans for NFC tags (13.56 MHz)
- Outputs UID in hexadecimal
- Example: `04 A1 B2 C3 D4 E5 F6` (7-byte UID)

#### RFID Only
- Scans for RFID tags (125 kHz)
- Outputs UID in hexadecimal
- Example: `1A 2B 3C 4D 5E` (EM4100)

#### NDEF Mode
- Scans NFC tags for NDEF text records
- Outputs **only** the text content (no UID)
- Shows error with red LED if tag has no NDEF data
- Example: Tag contains "Hello World" → Types: `Hello World`

#### NFC + RFID (Combo)
- Scans NFC first, then prompts for RFID
- Outputs both UIDs separated by space
- 5-second timeout if second tag not presented
- Example: `04 A1 B2 C3 1A 2B 3C 4D`

#### RFID + NFC (Combo)
- Scans RFID first, then prompts for NFC
- Outputs both UIDs separated by space
- 5-second timeout if second tag not presented

## Supported Tags

### RFID (125 kHz)
- EM4100 / EM4102
- HID Prox / HID 1326
- Indala

### NFC (13.56 MHz)
- MIFARE Classic 1K / 4K
- MIFARE Ultralight / Ultralight C
- NTAG203 / NTAG213 / NTAG215 / NTAG216
- ISO14443A (any tag type)
- ISO14443B
- ISO15693 (NFC-V)

### NDEF Support
- **Type 2 NDEF**: MIFARE Ultralight, NTAG series
- **Type 4 NDEF**: ISO14443-4A tags
- **Type 5 NDEF**: ISO15693 tags
- **Text Records**: UTF-8 and UTF-16 encoded text

## Building from Source

### Setup Environment
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

### Deploy to Flipper
```bash
# Build and launch on connected Flipper via USB
./fbt launch APPSRC=applications_user/flipper_wedge

# Or manually copy the FAP file
# Output: build/f7-firmware-D/.extapps/flipper_wedge.fap
# Copy to: SD Card/apps/Tools/
```

### Build with Docker (Alternative)
```bash
# If you prefer Docker-based builds
./fbt fap_flipper_wedge COMPACT=1 DEBUG=0
```

## Development

### Project Structure
```
flipper_wedge/
├── application.fam          # App manifest
├── hid_device.c/h          # Main app entry point
├── helpers/                # Core modules
│   ├── hid_device_hid.*    # USB/BT HID output
│   ├── hid_device_nfc.*    # NFC reading & NDEF parsing
│   ├── hid_device_rfid.*   # RFID reading
│   ├── hid_device_format.* # UID formatting
│   ├── hid_device_storage.*# Settings persistence
│   └── ...                 # Haptic, LED, speaker modules
├── scenes/                 # UI scenes
├── views/                  # Custom views
├── icons/                  # App icon assets
└── docs/                   # Documentation
```

### Developer Documentation
- [CLAUDE.md](CLAUDE.md) - Comprehensive development guide
- [TYPE4_NDEF_STATUS.md](TYPE4_NDEF_STATUS.md) - NDEF implementation status
- [docs/changelog.md](docs/changelog.md) - Version history

### Contributing
Contributions are welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly on actual hardware
5. Submit a pull request

## Troubleshooting

### "Connect USB or BT HID" Error
- **USB**: Ensure Flipper is in USB HID mode (not Serial/Storage)
- **Bluetooth**: Complete pairing process in BT Pair menu
- At least one connection method must be active

### Tag Not Detected
- Try a position--wait--move slightly--wait and so on...

### NDEF Mode Shows "No NDEF Found"
- Tag must contain properly formatted NDEF **text** records
- Not all NFC tags have NDEF data
- Use NXP's TagWriter app on phone to write NDEF **text** records
- Switch to NFC mode to read UID instead
- Type 4 records can be finicky to read due to the many message exchanges required--be patient

### NDEF Mode Isn't Sending What I Expect
- Check the record that saved--does it match what you entered?
- **How this works**: Text records are UTF-8 or UTF-16 and must be converted to HID keyboard sequences
- **Non-ASCII characters are stripped**: Only printable ASCII (0x20-0x7E), tab, and newline are supported
- **Length limit**: Check Settings → NDEF Max Length (250/500/1000 chars)
- **What works**: PEM encoded keys, SSH keys, base64, and any ASCII-only text will work perfectly

### Bluetooth Won't Pair
- Ensure Bluetooth is enabled in Settings
- Forget device on target and retry pairing
- Some devices require PIN: try `0000` or `1234`
- Check Flipper Bluetooth settings in main menu

## Credits

### Built With
- **Base Template**: [flipper-zero-fap-boilerplate](https://github.com/leedave/flipper-zero-fap-boilerplate) by [leedave](https://github.com/leedave)
- **Flipper Zero Firmware**: [Official firmware](https://github.com/flipperdevices/flipperzero-firmware)
- **Development**: Created with [Claude Code](https://claude.com/claude-code)

### Special Thanks
- Flipper Devices team for the amazing platform
- leedave for the excellent boilerplate template
- NFC/RFID library contributors
- Community testers and feedback providers

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Support

- **Issues**: [GitHub Issues](../../issues)
- **Discussions**: [GitHub Discussions](../../discussions)
- **Flipper Zero Community**: [Official Forum](https://forum.flipper.net/)

---

**Disclaimer**: This tool is for authorized use only. Always ensure you have permission to read tags and input data on target systems. The developers are not responsible for misuse of this application.