# Chameleon Flipper - Chameleon Ultra Controller for Flipper Zero

Complete Flipper Zero application (.fap) to control and manage your Chameleon Ultra device via USB or Bluetooth.

**🎬 Features fun bar animation when devices connect!** Watch Chameleon and Dolphin meet at a bar! 🍺

## Quick Links

- 📖 [Quick Start Guide](docs/QUICK_START.md) - Get started in minutes
- 🎨 [Animation Documentation](docs/ANIMATION.md) - Learn about the connection animation
- 🔧 [Protocol Documentation](docs/PROTOCOL.md) - Detailed protocol specification

## Features

### Connection Methods
- **USB/Serial Connection** - Direct communication via USB CDC ✅ **Fully working**
  - Full bidirectional communication
  - Real-time slot management
  - Tag read/write operations
  - Device diagnostics
  
- **Bluetooth Low Energy (BLE)** - ⚠️ **Not supported** (Flipper Zero hardware limitation)

> **Technical Note**: The Flipper Zero BLE stack is designed as a **peripheral/server only** (HID keyboard, serial device, etc.). It does not support **central/client** mode required to connect to other BLE peripherals like the Chameleon Ultra. The Flipper's BLE APIs (`furi_hal_bt`, `gap`, `ble_app`) only expose peripheral functionality - there are no public APIs for BLE scanning, connecting as central, or GATT client operations.
> 
> **Workaround**: Use USB-C connection for full functionality. The Chameleon Ultra's USB interface provides complete access to all features.

### Functionality
- **Slot Management** - Manage all 8 slots on your Chameleon Ultra
  - View slot information with real-time sync
  - Rename slots (32 characters max, UTF-8)
  - Set active slot
  - Configure tag types
  - **Real device communication** - reads actual slot data from Chameleon Ultra
- **Tag Operations** - Read and write tags
  - Read tags from Chameleon Ultra
  - Transfer Flipper tags to Chameleon Ultra
  - Support for HF (High Frequency) and LF (Low Frequency) tags
- **Device Diagnostics** - View device information
  - Firmware version
  - Device model (Ultra/Lite)
  - Operating mode (Reader/Emulator)
  - Chip ID
  - Connection status
- **Protocol Support** - Full implementation of Chameleon Ultra protocol
  - Device management commands (1000-1037)
  - Slot management (1003-1024)
  - HF operations (2000-2012) - Mifare Classic, NTAG, etc.
  - LF operations (3000-3003) - EM410X, HID Prox
  - Emulator configuration (4000-4030)

## Project Structure

```
Chameleon_Flipper/
├── application.fam                    # App manifest
├── chameleon_app.c                    # Main application
├── chameleon_app_i.h                  # Internal structures and definitions
├── lib/                               # Libraries
│   ├── chameleon_protocol/            # Protocol implementation
│   │   ├── chameleon_protocol.h
│   │   └── chameleon_protocol.c
│   ├── uart_handler/                  # USB/Serial handler
│   │   ├── uart_handler.h
│   │   └── uart_handler.c
│   └── ble_handler/                   # Bluetooth handler
│       ├── ble_handler.h
│       └── ble_handler.c
├── views/                             # Custom views
│   ├── chameleon_animation_view.h     # Bar animation view
│   └── chameleon_animation_view.c
├── scenes/                            # GUI scenes
│   ├── chameleon_scene.h              # Scene headers
│   ├── chameleon_scene.c              # Scene manager
│   ├── chameleon_scene_config.h       # Scene configuration
│   ├── chameleon_scene_start.c        # Start screen
│   ├── chameleon_scene_main_menu.c    # Main menu
│   ├── chameleon_scene_connection_type.c
│   ├── chameleon_scene_usb_connect.c
│   ├── chameleon_scene_ble_scan.c
│   ├── chameleon_scene_ble_connect.c
│   ├── chameleon_scene_slot_list.c
│   ├── chameleon_scene_slot_config.c
│   ├── chameleon_scene_slot_rename.c
│   ├── chameleon_scene_tag_read.c
│   ├── chameleon_scene_tag_write.c
│   ├── chameleon_scene_diagnostic.c
│   └── chameleon_scene_about.c
├── icons/                             # Application icons
│   └── chameleon_10px.png
└── docs/                              # Documentation
    ├── QUICK_START.md                 # Quick start guide
    ├── ANIMATION.md                   # Animation details
    └── PROTOCOL.md                    # Protocol specification
```

## Protocol Implementation

The application implements the official Chameleon Ultra protocol:

### Frame Structure
```
SOF (0x11) | LRC1 (0xEF) | CMD (2 bytes) | STATUS (2 bytes) |
LEN (2 bytes) | LRC2 (1 byte) | DATA (0-512 bytes) | LRC3 (1 byte)
```

### Supported Commands
- **Device Management**: Get version, chip ID, device model, capabilities
- **Slot Operations**: Set active slot, configure slots, rename slots
- **Tag Reading**: Scan HF/LF tags, read blocks, authenticate
- **Tag Writing**: Write to emulator slots, configure emulation
- **Diagnostics**: Get device status, firmware info

## Building

### Prerequisites
- Flipper Zero firmware with FAP support
- uFBT (micro Flipper Build Tool) or full firmware SDK

### Build Instructions

Using uFBT:
```bash
ufbt
```

Using full firmware SDK:
```bash
./fbt fap_chameleon_ultra
```

### Installation

1. Build the .fap file
2. Copy to Flipper Zero SD card: `/ext/apps/Tools/`
3. Launch from Applications > Tools > Chameleon Ultra

## Usage

### Connecting to Chameleon Ultra

#### USB Connection
1. Connect Chameleon Ultra to Flipper Zero via USB-C
2. Open Chameleon Ultra app
3. Select "Connect Device" > "USB Connection"

#### Bluetooth Connection
1. Power on Chameleon Ultra
2. Open Chameleon Ultra app
3. Select "Connect Device" > "Bluetooth Connection"
4. Wait for device scan
5. Select your Chameleon Ultra from the list

### Managing Slots
1. Connect to device
2. Select "Manage Slots"
3. Choose a slot (0-7)
4. Configure slot settings:
   - Activate slot
   - Rename slot
   - Change tag type

### Reading Tags
1. Connect to device
2. Select "Read Tag"
3. Follow on-screen instructions

### Writing to Chameleon
1. Connect to device
2. Select "Write to Chameleon"
3. Choose source tag
4. Select destination slot

### Diagnostics
1. Connect to device
2. Select "Diagnostic"
3. View device information

## Technical Details

### Communication
- **USB**: Uses Flipper's USB CDC interface at 115200 baud
- **BLE**: Connects via Flipper's Bluetooth stack to Chameleon's BLE service

### Memory
- Stack size: 2KB
- Supports up to 512-byte protocol payloads
- Caches device and slot information locally

### Compatibility
- Flipper Zero firmware: Latest official/unleashed
- Chameleon Ultra firmware: All versions supporting standard protocol
- Chameleon Lite: Supported (limited features)

## Development Status

### Implemented
- ✅ Complete protocol implementation
- ✅ USB/Serial communication
- ✅ Bluetooth communication framework
- ✅ GUI with scene management
- ✅ Slot management
- ✅ Device diagnostics
- ✅ Connection handling

### In Progress
- 🔄 Tag reading from Chameleon
- 🔄 Tag writing to Chameleon
- 🔄 Full BLE GATT implementation
- 🔄 Response parsing and error handling

### Planned
- 📋 Mifare Classic key management
- 📋 NTAG configuration
- 📋 EM410X and HID Prox emulation setup
- 📋 Batch operations
- 📋 Settings persistence

## Contributing

Contributions are welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly on hardware
5. Submit a pull request

## License

This project is open source. See LICENSE file for details.

## Credits

- **Chameleon Ultra Protocol**: [RfidResearchGroup](https://github.com/RfidResearchGroup/ChameleonUltra)
- **Flipper Zero**: [Flipper Devices](https://flipperzero.one/)

## Disclaimer

This tool is for educational and authorized security research purposes only. Users are responsible for complying with local laws and regulations regarding RFID/NFC devices.

## Support

For issues, questions, or feature requests, please open an issue on GitHub.

---

**Version**: 1.0
**Author**: Chameleon Flipper Team
**Platform**: Flipper Zero
