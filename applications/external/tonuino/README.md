# TonUINO Writer for Flipper Zero

NFC card writer application for TonUINO audio boxes. Create and manage NFC cards for the TonUINO DIY music player system.

![Version](https://img.shields.io/badge/version-2.0.0-blue)
![Build](https://img.shields.io/badge/build-79-green)
![License](https://img.shields.io/badge/license-MIT-orange)

## Features

- ✅ **Write TonUINO Cards** - Configure NFC cards with folder, mode, and special settings
- ✅ **Read TonUINO Cards** - Read and display existing card configurations
- ✅ **Rapid Write Mode** - Quickly write multiple cards with adjustable settings
- ✅ **Cancelable Operations** - Cancel read/write operations at any time with dedicated button
- ✅ **Card Preview** - View card configuration before writing
- ✅ **Menu Position Memory** - Returns to your last selected menu item
- ✅ **All 11 Play Modes** - Full TonUINO mode support

## Overview

This app allows you to create and write TonUINO-compatible NFC cards directly from your Flipper Zero device. TonUINO cards use a specific 16-byte format to control playback modes and folder selection on TonUINO audio boxes.

## Card Format

Each TonUINO card contains 16 bytes:

- **Bytes 0-3**: Box identifier (always `13 37 B3 47`)
- **Byte 4**: Version (currently `0x02`)
- **Byte 5**: Folder number (1-99, hex `0x01`-`0x63`)
- **Byte 6**: Playback mode (see modes below)
- **Byte 7**: Special value 1 (for von-bis modes)
- **Byte 8**: Special value 2 (for von-bis modes)
- **Bytes 9-15**: Reserved (set to `0x00`)

## Playback Modes

1. **Hoerspiel** (Random) - Plays a random file from the folder
2. **Album** (Complete) - Plays the entire folder in order
3. **Party** (Shuffle) - Plays files in random order
4. **Einzel** (Single) - Plays a specific file from the folder
5. **Hoerbuch** (Progress) - Plays folder and remembers progress
6. **Admin** - Admin functions
7. **Hoerspiel Von-Bis** - Random file from a range (uses Special values)
8. **Album Von-Bis** - All files from a range (uses Special values)
9. **Party Von-Bis** - Shuffled files from a range (uses Special values)
10. **Hoerbuch Einzel** - Single track with progress tracking
11. **Repeat Last** - Repeats the last card/shortcut

## Building

1. Clone the Flipper Zero firmware repository:
   ```bash
   git clone --recursive https://github.com/flipperdevices/flipperzero-firmware.git
   cd flipperzero-firmware
   ```

2. Copy this application to `applications_user/tonuino_writer/`:
   ```bash
   cp -r /path/to/tonuino-flipper-zero/* applications_user/tonuino_writer/
   ```

3. Build the application:
   ```bash
   ./fbt fap_tonuino_writer
   ```

4. Install to your Flipper Zero:
   ```bash
   ./fbt launch_app APPSRC=tonuino_writer
   ```

## Usage

Launch from Apps → NFC → TonUINO Writer. The main menu offers:

- **Set Folder** - Choose folder number (1-99)
- **Set Mode** - Select from 11 playback modes
- **Set Special 1** - First special value (0-255, for von-bis modes)
- **Set Special 2** - Second special value (0-255, for von-bis modes)
- **View Card** - Preview current configuration
- **Read Card** - Read existing TonUINO card
- **Write Card** - Write configuration to NFC card
- **Rapid Write** - Quick multi-card writing mode
- **About** - App version and information

**Writing a Card**

1. Configure your card using Set Folder, Set Mode, and optional Special values
2. Select "View Card" to preview (optional)
3. Select "Write Card"
4. Place MIFARE Classic card on back of Flipper
5. Press "Cancel" button (left) to abort at any time
6. Wait for success confirmation

**Reading a Card**

1. Select "Read Card" from main menu
2. Place TonUINO card on back of Flipper
3. Press "Cancel" button (left) to abort
4. View the card configuration on screen

**Rapid Write Mode**

Perfect for creating multiple cards with the same base configuration:

1. Select "Rapid Write" from main menu
2. **Up/Down** - Toggle between Folder and Mode selection (highlighted in brackets)
3. **Left/Right** - Adjust selected value (wraps around)
4. **OK** - Write current configuration to card
5. **Back** - Return to main menu

Status messages appear at the bottom showing write results.

## Special Values (Von-Bis Modes)

For modes 7-9 (Von-Bis modes), the Special values define the track range:
- **Special 1**: Start track number
- **Special 2**: End track number

Example: To play tracks 5-10 in Album Von-Bis mode, set Special 1 to 5 and Special 2 to 10.

## Technical Details

- Data is written to **block 4** of MIFARE Classic 1K cards
- Automatically searches blocks 4, 1, 5, and 6 when reading
- Uses default MIFARE Classic key (FF FF FF FF FF FF)
- Non-blocking NFC operations with thread-based polling
- SceneManager architecture for stable navigation
- Box ID `13 37 B3 47` is standard for TonUINO devices
- Folder numbers: 1-99, Modes: 1-11, Special values: 0-255

## Compatibility

- **Flipper Zero Firmware**: Official and Unleashed firmware
- **NFC Cards**: MIFARE Classic 1K
- **TonUINO**: Compatible with TonUINO 2.x firmware
- **Build System**: uFBT compatible

## Installation

**From Flipper Lab (Recommended)**

1. Visit [Flipper Lab](https://lab.flipper.net)
2. Search for "TonUINO Writer"
3. Click Install

**Manual Installation**

Copy `tonuino_writer.fap` to `/ext/apps/NFC/` on your Flipper Zero SD card.

## References

- [TonUINO Official Website](https://www.tonuino.de)
- [TonUINO RFID Card Format](https://discourse.voss.earth/t/format-der-rfid-karten/13284)
- [Flipper Zero Developer Documentation](https://developer.flipper.net/)

## License

MIT License - See [LICENSE](LICENSE) file for details.

## Author

Developed for the Flipper Zero and TonUINO communities.

