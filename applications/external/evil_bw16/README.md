# Evil BW16 Controller - Flipper Zero App

A Flipper Zero application for controlling the Evil-BW16 WiFi deauther module via UART.

**Author:** dag nazty  
**Version:** 1.0  
**Category:** GPIO/UART Applications

## Description

The Evil BW16 Controller allows you to control an Evil-BW16 WiFi deauther module directly from your Flipper Zero device. The app provides a user-friendly interface for WiFi scanning, target selection, deauthentication attacks, and device configuration with real-time UART monitoring.

## Features

- **WiFi Network Scanner**: Discover and list available WiFi networks with real-time UART display
- **Target Selection**: Choose specific networks for attacks with multi-select interface
- **Attack Mode**: Perform WiFi deauthentication attacks (deauth, disassoc, random)
- **Editable Configuration**: Adjust attack parameters with interactive menus
- **UART Terminal**: Direct command-line interface with live monitoring
- **Dual-band Support**: Works with both 2.4GHz and 5GHz networks
- **Real-time Monitoring**: Live display of attack status and network responses

## Software Rquirements

[Evil-BW16](https://github.com/Evil-Project-Team/Evil-BW16-WebUI/blob/main/Evil-BW16/Evil-BW16.ino)

## Hardware Requirements

### BW16 Module (Evil-BW16)
- Realtek RTL8720DN-based module
- Evil-BW16 firmware by 7h30th3r0n3
- Dual-band WiFi capability (2.4GHz & 5GHz)

### Flipper Zero
- Any Flipper Zero device
- GPIO expansion connector

## Wiring Diagram

```
┌─────────────────┐    ┌─────────────────┐
│   Flipper Zero  │    │   BW16 Module   │
├─────────────────┤    ├─────────────────┤
│ Pin 1  (5V)     │────│ 5V              │
│ Pin 13/15 (TX)  │────│ PB1 (RX)        │
│ Pin 14/16 (RX)  │────│ PB2 (TX)        │
│ Pin 18 (GND)    │────│ GND             │
└─────────────────┘    └─────────────────┘
```

**Communication:** 115200 baud, 8N1  
**Power:** 5V from Flipper Zero expansion connector

## Installation

### Method 1: Using ufbt (Recommended)
1. Install ufbt (micro Flipper Build Tool):
   ```bash
   pip install --upgrade ufbt
   ```

2. Navigate to the Evil-Fap directory and build:
   ```bash
   cd Evil-Fap
   ufbt
   ```

3. Install to connected Flipper Zero:
   ```bash
   ufbt launch
   ```

   Or copy the built `.fap` file to SD card: `SD:/apps/GPIO/`


## Usage

### Main Menu Structure

1. **WiFi Scanner** - Scan for networks and view results in UART terminal
2. **Attack Mode** - Launch deauth attacks on selected targets
3. **Set Targets** - Select which networks to attack (multi-select)
4. **Configuration** - Edit device settings (cycle delay, scan time, etc.)
5. **Help** - Hardware setup guide and usage instructions
6. **UART Terminal** - Direct serial communication interface

### Basic Operations

#### 1. WiFi Scanning
1. Select "WiFi Scanner" from the main menu
2. Command is automatically sent and UART terminal opens
3. View real-time scan results showing:
   - SSID (including those with spaces like "first home")
   - BSSID (MAC address)
   - Channel and signal strength
   - Frequency band (2.4GHz/5GHz)

#### 2. Target Selection
1. After scanning, go to "Set Targets"
2. See all discovered networks with selection indicators:
   - `[*]` = Selected for attack
   - `[ ]` = Not selected
3. Tap networks to toggle selection
4. Use bulk operations:
   - "Clear All Targets" - Deselect everything
   - "Select All Targets" - Select all networks
   - "Confirm Targets" - Send selections to BW16

#### 3. Attack Mode
1. Select "Attack Mode" from main menu
2. Choose attack type:
   - **Deauth Attack** - Standard deauthentication frames
   - **Disassoc Attack** - Disassociation frames  
   - **Random Attack** - Attack randomly selected networks
   - **Stop Attack** - Stop all ongoing attacks (always available)
3. Attack launches in UART terminal with real-time monitoring

#### 4. Configuration
Interactive settings menu with live editing:
- **Cycle Delay** (100-60000ms) - Delay between attack cycles
- **Scan Time** (1000-30000ms) - Duration of WiFi scans
- **Frames per AP** (1-20) - Number of frames sent per target
- **Start Channel** (1-14) - Starting channel for scans
- **LED Enabled** (Yes/No) - Toggle LED indicators
- **Debug Mode** (Yes/No) - Enable debug output
- **GPIO Pins** (13/14 or 15/16) - Select UART pin configuration (applies immediately)
- **Send Config to Device** - Apply all settings to BW16

#### 5. UART Terminal
- Immediate live monitoring of BW16 communication
- Send custom commands directly to the module
- View all responses, errors, and status messages
- Automatic logging of TX/RX data

## Supported Commands

The app communicates with the BW16 module using these commands:

- `scan` - Perform WiFi network scan
- `start deauther` - Begin deauth attack on selected targets
- `stop deauther` - Stop all attacks
- `disassoc` - Start disassociation attack
- `random_attack` - Execute random attack
- `set target X,Y,Z` - Set target networks by index
- `set cycle_delay <ms>` - Configure cycle delay
- `set scan_time <ms>` - Configure scan duration
- `set frames <count>` - Set frames per AP
- `set start_channel <num>` - Set starting channel
- `set led on/off` - Control LED indicators
- `set debug on/off` - Toggle debug mode
- `info` - Display device information

## App Features

### Robust UART Communication
- Hardware UART at 115200 baud on GPIO pins 13/14 or 15/16 (configurable)
- Real-time parsing of all BW16 response types
- Handles SSIDs with spaces and special characters
- Automatic network data extraction and storage

### Smart Target Management
- Parse and store up to 50 networks from scans
- Multi-select interface for choosing attack targets
- Automatic target index management
- Support for both 2.4GHz and 5GHz networks

### Live Monitoring
- Real-time attack progress tracking
- Immediate UART terminal access
- Continuous packet count updates
- Live status and error reporting

## Troubleshooting

### Connection Issues
- Verify wiring: Pin 1→5V, Pin 13/15→PB1, Pin 14/16→PB2, Pin 18→GND
- Check 115200 baud rate communication
- Ensure BW16 module has Evil-BW16 firmware flashed
- Try UART terminal to test basic communication

### No Networks Found
- Check BW16 module power and connectivity
- Verify antenna connections on BW16
- Check debug log at `/ext/apps_data/evil_bw16_debug.txt`
- Try manual scan command in UART terminal

### App Issues
- If UI freezes, check for ViewPort warnings in logs
- Restart app if UART communication fails
- Clear targets and rescan if selections don't work
- Check SD card space for debug logging

## Legal Disclaimer

**IMPORTANT:** This tool is designed for educational and authorized security testing purposes only.

- Only test networks you own or have explicit permission to test
- Comply with all local laws and regulations regarding WiFi security testing
- Deauthentication attacks may disrupt network services
- Use responsibly and ethically
- The creators assume no liability for misuse

## Contributing

Contributions are welcome! Please feel free to submit pull requests, report bugs, or suggest new features.

## Credits

- **Original Evil-BW16 Firmware**: 7h30th3r0n3
- **Flipper Zero App Development**: dag nazty
- **BW16 Hardware**: Realtek RTL8720DN module

Special thanks to the Flipper Zero community and WiFi security researchers.

## License

This project is licensed under the MIT License. See the original Evil-BW16 project for firmware licensing details.

## Version History

- **v1.0**: Initial release
  - Real-time WiFi scanning with UART terminal
  - Multi-target selection system
  - Deauth, disassoc, and random attack modes
  - Interactive configuration menu
  - Hardware setup help and usage guide
  - Robust UART communication with packet parsing

---

**Hardware Requirements**: Flipper Zero + BW16 module with Evil-BW16 firmware  
**Original Firmware**: [Evil-BW16 by 7h30th3r0n3](https://github.com/7h30th3r0n3/Evil-BW16) 
