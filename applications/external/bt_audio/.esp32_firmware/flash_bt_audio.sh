#!/bin/bash
# BT Audio ESP32 Flasher Script
# Flashes BT Audio firmware to ESP32 connected to your computer
#
# Usage: ./flash_bt_audio.sh [OPTIONS]
#
# Options:
#   --port PORT     Serial port (e.g., /dev/ttyUSB0, /dev/ttyACM0)
#   --mode MODE     'source' (default) or 'sink'
#   --board BOARD   esp32dev (default), esp32-c3, esp32-s3, mayhem
#   --help          Show this help

set -e

# Default values
PORT=""
MODE="source"
BOARD="esp32dev"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Print colored message
print_msg() {
    local color=$1
    shift
    echo -e "${color}$@${NC}"
}

# Show help
show_help() {
    cat << EOF
BT Audio ESP32 Flasher

Flashes BT Audio firmware to ESP32 for use with Flipper Zero.

Usage: $0 [OPTIONS]

Options:
  --port PORT     Serial port (e.g., /dev/ttyUSB0, /dev/ttyACM0, COM3)
                  If not specified, will try to auto-detect
  --mode MODE     Firmware mode:
                    'source' - Stream audio TO headphones (default)
                    'sink'   - Receive audio FROM phone/PC
  --board BOARD   Board type: esp32dev (default), esp32-c3, esp32-s3, mayhem
  --help          Show this help

Examples:
  $0                                    # Auto-detect port, source mode
  $0 --port /dev/ttyUSB0               # Specific port, source mode
  $0 --mode sink                       # Sink mode
  $0 --port COM3 --board esp32-c3      # Windows, ESP32-C3 board

Requirements:
  - PlatformIO Core (install: pip install platformio)
  - ESP32 connected via USB
  - Drivers for ESP32 USB-to-Serial chip (CP2102, CH340, etc.)

EOF
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --port)
            PORT="$2"
            shift 2
            ;;
        --mode)
            MODE="$2"
            shift 2
            ;;
        --board)
            BOARD="$2"
            shift 2
            ;;
        --help)
            show_help
            exit 0
            ;;
        *)
            print_msg $RED "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# Validate mode
if [[ "$MODE" != "source" && "$MODE" != "sink" ]]; then
    print_msg $RED "Error: Invalid mode '$MODE'. Must be 'source' or 'sink'."
    exit 1
fi

# Determine environment name
if [[ "$MODE" == "sink" ]]; then
    ENV="${BOARD}_sink"
else
    ENV="${BOARD}"
fi

print_msg $BLUE "╔════════════════════════════════════════╗"
print_msg $BLUE "║   BT Audio ESP32 Flasher for Flipper  ║"
print_msg $BLUE "╚════════════════════════════════════════╝"
echo ""

# Check if PlatformIO is installed
if ! command -v pio &> /dev/null; then
    print_msg $RED "Error: PlatformIO not found!"
    echo ""
    echo "Please install PlatformIO Core:"
    echo "  pip install platformio"
    echo ""
    echo "Or visit: https://platformio.org/install/cli"
    exit 1
fi

print_msg $GREEN "✓ PlatformIO found"

# Auto-detect port if not specified
if [[ -z "$PORT" ]]; then
    print_msg $YELLOW "⚠ No port specified, attempting auto-detection..."
    
    # Try to find ESP32 device
    PORTS=()
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        # Linux - look for ttyUSB or ttyACM
        if ls /dev/ttyUSB* 1> /dev/null 2>&1; then
            PORTS=($(ls /dev/ttyUSB*))
        elif ls /dev/ttyACM* 1> /dev/null 2>&1; then
            PORTS=($(ls /dev/ttyACM*))
        fi
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS - look for cu.usbserial or cu.SLAB_USBtoUART
        PORTS=($(ls /dev/cu.* | grep -E '(usbserial|SLAB|wchusbserial)'))
    fi
    
    if [[ ${#PORTS[@]} -eq 0 ]]; then
        print_msg $RED "Error: Could not auto-detect ESP32 port."
        echo ""
        echo "Please specify port manually with --port option."
        echo "Example: $0 --port /dev/ttyUSB0"
        echo ""
        echo "Available ports:"
        if [[ "$OSTYPE" == "linux-gnu"* ]]; then
            ls /dev/tty* 2>/dev/null | grep -E '(USB|ACM)' || echo "  None found"
        elif [[ "$OSTYPE" == "darwin"* ]]; then
            ls /dev/cu.* 2>/dev/null || echo "  None found"
        fi
        exit 1
    elif [[ ${#PORTS[@]} -gt 1 ]]; then
        print_msg $YELLOW "⚠ WARNING: Multiple serial devices detected:"
        for p in "${PORTS[@]}"; do
            echo "    $p"
        done
        PORT="${PORTS[0]}"
        print_msg $YELLOW "⚠ Using first port: $PORT"
        echo "If this is wrong, specify with --port option."
        echo ""
    else
        PORT="${PORTS[0]}"
        print_msg $GREEN "✓ Auto-detected port: $PORT"
    fi
fi

# Display configuration
echo ""
print_msg $BLUE "Configuration:"
echo "  Port:  $PORT"
echo "  Mode:  $MODE"
echo "  Board: $BOARD"
echo "  Env:   $ENV"
echo ""

# Navigate to firmware directory
cd "$SCRIPT_DIR"

# Safety check
print_msg $YELLOW "⚠ WARNING: This will erase and flash your ESP32!"
echo ""
echo "This will:"
echo "  1. Erase current ESP32 firmware"
echo "  2. Flash BT Audio firmware in $MODE mode"
echo "  3. ESP32 will be ready for Flipper Zero BT Audio app"
echo ""
print_msg $YELLOW "Press Ctrl+C to cancel, or Enter to continue..."
read

# Build firmware
print_msg $BLUE "Building firmware..."
if ! pio run -e $ENV; then
    print_msg $RED "Error: Build failed!"
    exit 1
fi
print_msg $GREEN "✓ Build successful"

# Flash firmware
print_msg $BLUE "Flashing to $PORT..."
if ! pio run -e $ENV -t upload --upload-port $PORT; then
    print_msg $RED "Error: Flash failed!"
    echo ""
    echo "Troubleshooting:"
    echo "  1. Check ESP32 is connected and powered"
    echo "  2. Install USB drivers for your ESP32 board"
    echo "  3. Try holding BOOT button while connecting"
    echo "  4. Check port permissions: sudo chmod 666 $PORT"
    exit 1
fi

print_msg $GREEN "✓ Flash successful"

# Monitor serial output
echo ""
print_msg $BLUE "Opening serial monitor..."
print_msg $YELLOW "(Press Ctrl+C to exit monitor)"
echo ""
sleep 2

pio device monitor --port $PORT --baud 115200 || true

echo ""
print_msg $GREEN "════════════════════════════════════════"
print_msg $GREEN "✓ BT Audio firmware flash complete!"
print_msg $GREEN "════════════════════════════════════════"
echo ""
echo "Next steps:"
echo "  1. Disconnect ESP32 from computer (or flipper)"
echo "  2. (re)Connect ESP32 to Flipper Zero:"
echo "     - Flipper TX (pin 13) → ESP32 RX"
echo "     - Flipper RX (pin 14) → ESP32 TX"
echo "     - Flipper GND → ESP32 GND"
echo "  3. Launch BT Audio app on Flipper Zero"
echo "  4. Scan and connect to Bluetooth headphones"
echo "  5. Play test tone"
echo ""

if [[ "$MODE" == "source" ]]; then
    echo "Mode: A2DP Source - Audio streams TO headphones/speakers"
else
    echo "Mode: A2DP Sink - Receives audio FROM phone/PC"
fi
echo ""
