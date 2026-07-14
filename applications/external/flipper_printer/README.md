# Flipper Printer

A Flipper Zero external application that combines a coin flip game with thermal printer functionality using the T7-US thermal printer module.

## Demo

![Demo](assets/CleanShot%202025-06-22%20at%2017.15.16@2x.png)

*The T7-US thermal printer in action with the Flipper Zero coin flip game and custom text printing.*

## Features

- **Coin Flip Game**: Interactive coin flipping with statistics tracking
- **Custom Text Printing**: Print any text via on-screen keyboard input
- **Thermal Printer Integration**: Full ESC/POS command support
- **Statistics Tracking**: Track your coin flip history and streaks
- **Haptic & Sound Feedback**: Customizable user experience

## Hardware Requirements

- Flipper Zero device
- T7-US Thermal Printer Module (or compatible ESC/POS printer)
- Jumper wires for connection

## Connecting the T7-US Thermal Printer

### Wiring Diagram

Connect the T7-US thermal printer module to your Flipper Zero using one of the following methods:

#### Method 1 (Default/Current): RX/TX Pins
| T7-US Pin | Flipper Zero Pin | Description |
|-----------|------------------|-------------|
| VCC       | 3V3              | Power (3.3V) |
| GND       | GND              | Ground |
| RX        | RX (Pin 13)      | Data (Flipper TX → Printer RX) |
| TX        | TX (Pin 14)      | Data (Printer TX → Flipper RX) |

#### Method 2 (Alternative): C0/C1 Pins  
| T7-US Pin | Flipper Zero Pin | Description |
|-----------|------------------|-------------|
| VCC       | 3V3              | Power (3.3V) |
| GND       | GND              | Ground |
| RX        | C0 (Pin 15)      | Data (Flipper TX → Printer RX) |
| TX        | C1 (Pin 16)      | Data (Printer TX → Flipper RX) |

> **Note**: The app currently uses **Method 1 (RX/TX pins)**. To use Method 2, you need to modify the code in `printer.c` by commenting/uncommenting the appropriate sections.

### Connection Steps

1. **Power off** your Flipper Zero before making connections
2. Connect the wires according to the pinout table above
3. Ensure all connections are secure
4. Power on your Flipper Zero
5. The printer should initialize when you launch the app

### Important Notes

- The T7-US module operates at **9600 baud** (configured automatically)
- Uses **GPIO C0/C1 (pins 15/16)** for UART communication
- Maximum current draw: ~1.5A during printing (Flipper can provide sufficient power)
- Thermal paper width: 58mm standard

## Installation

### Using uFBT (Recommended)

```bash
# Clone the repository
git clone https://github.com/yourusername/flipper-printer.git
cd flipper-printer

# Build the application
make build

# Build and upload to Flipper
make upload
```

### Manual Installation

1. Build the `.fap` file using the Makefile
2. Copy `flipper_printer.fap` to your Flipper's SD card: `/ext/apps/Tools/`
3. Launch from Flipper Zero: `Applications → Tools → Flipper Printer`

## Usage

### Main Menu Options

1. **Coin Flip Game**
   - Press OK to flip the coin
   - View statistics (total flips, heads/tails count, streaks)
   - Press OK again to print results
   - Press BACK to return to menu

2. **Print Custom Text**
   - Use on-screen keyboard to enter text (up to 256 characters)
   - Press SAVE to print
   - Text is printed with timestamp header

3. **Printer Setup Info**
   - View connection instructions
   - Troubleshooting tips
   - Hardware requirements

### Controls

- **UP/DOWN**: Navigate menu
- **OK**: Select menu item / Flip coin / Print
- **BACK**: Return to previous screen / Exit

## Troubleshooting

### Printer Not Responding

1. Check all wire connections
2. Ensure printer has paper loaded
3. Verify printer power (LED indicator)
4. Try power cycling both devices

### Garbled Text

1. Check baud rate (should be 9600)
2. Ensure TX/RX are not swapped
3. Verify ground connection

### Paper Feed Issues

1. Check paper alignment
2. Ensure paper roll is not too tight
3. Clean printer mechanism if needed

## Building from Source

### Requirements

- Flipper Zero SDK (via uFBT or FBT)
- Make (for using Makefile)
- Git

### Build Commands

```bash
make build      # Build the application
make upload     # Build and upload to Flipper
make clean      # Clean build artifacts
make debug      # Build debug version
make format     # Format source code
make lint       # Run linter
```

## Technical Details

- **UART Configuration**: 9600 baud, 8N1
- **Protocol**: ESC/POS command set
- **Print Features**: Bold, text size, alignment, paper cut
- **Memory**: 2KB stack allocation
- **App Category**: Tools

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- Flipper Zero community for development resources
- ESC/POS documentation and examples
- Contributors and testers