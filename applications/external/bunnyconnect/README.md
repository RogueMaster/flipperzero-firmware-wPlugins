# BunnyConnect

A Flipper Zero application named after the desire to connct to a BashBunny, that bridges serial communication with USB HID keyboard functionality, enabling seamless text input and command execution across connected devices.

## Overview

BunnyConnect transforms your Flipper Zero into a versatile communication hub that can:
- Establish serial connections with external devices
- Capture and display terminal output in real-time
- Send keyboard input via USB HID to connected computers
- Provide a custom on-screen keyboard for text input
- Configure serial communication parameters

## Features

### 🔗 Serial Communication
- **Multiple Baud Rates**: Support for standard baud rates including 115200, 9600, 38400, and more
- **Real-time Display**: Live terminal output with scrollable text buffer

### ⌨️ Keyboard
- **Custom Keyboard Layout**: Intuitive on-screen keyboard with QWERTY and symbol layouts
- **Instant Text Transmission**: Send typed text directly to connected computers via USB HID
- **Special Characters**: Support for symbols, numbers, and special key combinations

### 🛠️ Configuration Options
- **Connection Settings**: Flexible serial port configuration
- **Auto-connect**: Optional automatic connection on startup
- **Session Management**: Save and restore connection preferences

### 🎯 User Interface
- **Intuitive Menu System**: Easy-to-navigate interface with clear options
- **Terminal View**: Full-screen terminal output display
- **Status Indicators**: Real-time connection and data transfer status
- **Error Handling**: Comprehensive error messages and recovery options

## Installation

### Prerequisites
- Flipper Zero device with updated firmware
- ufbt (Unofficial Flipper Build Tool) installed
- USB cable or dongle for connection to target devices

### Building from Source
```bash
# Clone the repository
git clone https://github.com/yourusername/flipper-bunnyconnect.git
cd flipper-bunnyconnect

# Build the application
ufbt

# Install to Flipper Zero
ufbt launch
```

### Getting Started

1. **Launch BunnyConnect** from your Flipper Zero's Applications menu
2. **Configure Connection** - Set up serial parameters in the Config menu
3. **Connect Device** - Use the Connect option to establish serial communication
4. **Monitor Output** - View real-time data in the Terminal view
5. **Send Input** - Use the custom keyboard to send text via USB HID

### Contributing
1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

**Disclaimer**: This software is provided for educational and legitimate testing purposes only. Users are responsible for ensuring compliance with applicable laws and regulations in their jurisdiction.
