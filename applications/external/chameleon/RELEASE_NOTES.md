# 🎉 Chameleon Flipper v1.0.0 - Release Notes

**Release Date:** November 7, 2025  
**Build:** `chameleon_ultra.fap` (27,052 bytes)  
**Compatibility:** Flipper Zero firmware 1.3.4+ (API 86.0)  
**Target:** Flipper Zero f7

---

## 🚀 Major Features

### 🎬 Contextual Animation System
The highlight of this release is the **9 unique animated interactions** between the Flipper Zero dolphin and Chameleon Ultra:

1. **🍺 Bar Scene** - Social meetup at "Chameleon Bar" with drinks and conversation
2. **🤝 Handshake** - Professional greeting animation for connections
3. **🔧 Workshop** - Technical collaboration with animated sparks
4. **💃 Dance Party** - Success celebration with floating music notes
5. **❌ Error Display** - Sympathetic error handling with sad expressions
6. **📡 Bluetooth Scan** - Radar-style device discovery with concentric circles
7. **📊 Data Transfer** - Visual progress bars with moving data packets
8. **👋 Disconnect** - Friendly farewell wave animation
9. **🎉 Success** - Achievement celebration sequence

### 📱 Core Functionality

#### Dual Connectivity
- **USB-C Connection** - Direct serial communication via USB CDC
- **Bluetooth LE** - Wireless control and monitoring
- **Auto-detection** - Seamless switching between connection types
- **Connection animations** - Visual feedback for connection status

#### Slot Management
- **8-Slot Support** - Full management of all Chameleon Ultra slots
- **Custom Naming** - Up to 32-character slot nicknames
- **Quick Activation** - One-tap slot switching
- **Visual Feedback** - Clear slot status indicators

#### Device Diagnostics
- **Firmware Version** - Real-time version checking
- **Device Model** - Ultra/Lite detection
- **Operating Mode** - Reader/Emulator status display
- **Chip Information** - Hardware ID and specifications
- **Connection Status** - Live connection monitoring

#### Tag Operations (Beta)
- **Tag Reading** - Import tags from Chameleon to Flipper
- **Tag Writing** - Transfer Flipper tags to Chameleon slots
- **Format Support** - Multiple RFID/NFC tag types

---

## 🎨 Visual Experience

### Interactive Characters
- **Flipper Dolphin**: 
  - Blinking eyes (every 4 frames)
  - Waving fin animations
  - Characteristic smile
  - Expressive movements

- **Chameleon**:
  - Moving eye (chameleon characteristic)
  - Extending tongue animation
  - Animated crest
  - Curled tail
  - Color-changing effects

### Rich Environments
- **Bar Setting**: Complete bar with counter, bottles, stools, and bubbling drinks
- **Tech Workshop**: Tools, workbench, and animated sparks
- **Dance Floor**: Musical notes and synchronized movements
- **Radar Display**: Concentric scanning circles and beams

### Performance
- **8 FPS** smooth animations
- **32 frames** per sequence (~4 seconds)
- **Skip Option** - Any button press skips animation
- **Memory Efficient** - Optimized for Flipper Zero constraints

---

## 🔧 Technical Details

### Protocol Implementation
- **Complete Chameleon Ultra Protocol** - All command sets implemented
- **Frame Structure** - SOF (0x11) | LRC1 (0xEF) | CMD | STATUS | LEN | DATA | CRC
- **Error Handling** - Robust connection recovery and validation
- **Data Validation** - Checksums and integrity verification
- **Timeout Management** - Configurable operation timeouts

### Architecture
- **Modular Design** - Clean separation of concerns
  - `chameleon_protocol` - Protocol handling
  - `uart_handler` - USB serial communication
  - `ble_handler` - Bluetooth communication
  - `chameleon_animation_view` - Animation system
- **Memory Management** - Proper allocation and cleanup
- **Event Handling** - Responsive user interactions

### Code Quality
- **2KB Stack Size** - Optimized memory usage
- **512-byte Payloads** - Full protocol support
- **Documentation** - Comprehensive inline comments
- **Examples** - Usage examples in docs

---

## 📋 Installation

### Requirements
- Flipper Zero with firmware 1.3.4 or later
- SD card with free space (~30KB)
- Chameleon Ultra device (optional for testing)

### Quick Install
```bash
# Method 1: Manual SD Card
1. Copy `dist/chameleon_ultra.fap` to `/ext/apps/Tools/` on SD card
2. Restart Flipper Zero
3. Navigate: Applications → Tools → Chameleon Ultra

# Method 2: qFlipper
1. Connect Flipper Zero via USB
2. Open qFlipper File Manager
3. Navigate to /ext/apps/Tools/
4. Drag and drop chameleon_ultra.fap

# Method 3: uFBT (for developers)
cd Chameleon_Flipper
ufbt launch
```

### Building from Source
```bash
# Install uFBT
pip install ufbt

# Configure SDK
ufbt update

# Build
cd Chameleon_Flipper
ufbt

# Output: dist/chameleon_ultra.fap
```

---

## 🎮 Quick Start Guide

### First Connection

#### USB Connection
1. Connect Chameleon Ultra to Flipper Zero via USB-C cable
2. Open Chameleon Ultra app
3. Select `Connect Device` → `USB Connection`
4. Enjoy the handshake animation! 🤝
5. You're connected!

#### Bluetooth Connection
1. Power on your Chameleon Ultra
2. Open Chameleon Ultra app
3. Select `Connect Device` → `Bluetooth Connection`
4. Watch the scanning animation 📡
5. Select your device from the list
6. Connected!

### Basic Operations

#### Managing Slots
```
Main Menu → Manage Slots → Select Slot (0-7)
```
Available actions:
- Activate slot
- Rename slot (32 characters max)
- View slot info

#### Device Diagnostics
```
Main Menu → Diagnostic
```
View:
- Firmware version
- Device model
- Operating mode
- Chip ID
- Connection type

---

## 🐛 Known Issues

### Minor Issues
- **BLE Scanning**: May require 2-3 scan attempts in crowded BLE environments
- **Animation Skip**: Very rapid button presses may cause brief UI lag
- **Memory**: Large operations may show loading screen on firmware < 1.3.0

### Workarounds
- **BLE Issues**: Move closer to Chameleon Ultra, retry scan
- **Connection Timeout**: Restart both devices if connection hangs
- **Animation Lag**: Press Back to skip and retry operation

---

## 🔮 Roadmap (v1.1.0 - Coming Soon)

### Planned Features
- **Extended Tag Support** - More RFID/NFC formats (NTAG, Ultralight, DESFire)
- **Mifare Key Management** - Advanced key configuration and storage
- **Custom Animations** - User-configurable animation sequences
- **Sound Effects** - Audio feedback for actions
- **Batch Operations** - Multiple slot operations at once
- **Statistics** - Usage tracking and analytics

### Performance Improvements
- **Faster BLE** - Improved connection speed
- **Reduced Latency** - Optimized protocol handling
- **Memory Optimization** - Better resource management

---

## 📚 Documentation

- **[README.md](README.md)** - Project overview and features
- **[QUICK_START.md](docs/QUICK_START.md)** - Getting started guide
- **[ANIMATION.md](docs/ANIMATION.md)** - Animation system details
- **[ANIMATION_EXAMPLES.md](docs/ANIMATION_EXAMPLES.md)** - Code examples
- **[PROTOCOL.md](docs/PROTOCOL.md)** - Protocol specification
- **[BUILD.md](BUILD.md)** - Build instructions and troubleshooting

---

## 🙏 Acknowledgments

### Credits
- **Flipper Zero Team** - For the amazing development platform
- **Chameleon Ultra Team** - For the excellent RFID/NFC hardware
- **uFBT Developers** - For the build toolchain
- **Community Contributors** - For testing and feedback

### Special Thanks
- Everyone who tested the animations and provided feedback
- The Flipper Zero Discord community
- Open source contributors

---

## 📞 Support & Community

### Getting Help
- **Documentation**: Check the `docs/` folder first
- **GitHub Issues**: Report bugs and request features
- **Discord**: Join Flipper Zero community for discussions
- **Reddit**: r/flipperzero for general questions

### Contributing
Contributions welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Test thoroughly on hardware
4. Submit a pull request

---

## 📊 Statistics

### Code Metrics
- **Total Lines**: ~5,000 lines of C code
- **Files**: 30+ source files
- **Scenes**: 13 UI scenes
- **Animations**: 9 unique animations
- **Icons**: 5 custom icons

### Build Info
- **Binary Size**: 27,052 bytes (.fap file)
- **Compression**: FastFAP optimization enabled
- **API Level**: 86.0
- **Stack Size**: 2KB per instance

---

## 📄 License

This project is open source. See LICENSE file for details.

---

## 🎯 Conclusion

**Chameleon Flipper v1.0.0** represents a major milestone in bringing fun, interactive, and functional Chameleon Ultra control to the Flipper Zero. The contextual animation system creates a unique user experience while maintaining robust functionality.

**Where the dolphin meets the chameleon** 🐬🦎

**Happy Hacking!** 🎉

---

*For the latest updates, visit the [GitHub repository](https://github.com/muylder/Chameleon_Flipper)*
