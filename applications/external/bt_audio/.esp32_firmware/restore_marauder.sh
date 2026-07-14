#!/bin/bash
# Restore Marauder/WiFi Firmware Script
# Flashes WiFi Marauder or other firmware back to ESP32
#
# Usage: ./restore_marauder.sh [OPTIONS]
#
# Options:
#   --port PORT     Serial port (e.g., /dev/ttyUSB0)
#   --firmware FILE Path to firmware binary (.bin file)
#   --help          Show this help

set -e

PORT=""
FIRMWARE=""
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_msg() {
    local color=$1
    shift
    echo -e "${color}$@${NC}"
}

show_help() {
    cat << EOF
Restore Marauder/WiFi Firmware to ESP32

This script helps you restore WiFi Marauder or other firmware to your ESP32
after using BT Audio firmware.

Usage: $0 [OPTIONS]

Options:
  --port PORT       Serial port (e.g., /dev/ttyUSB0, /dev/ttyACM0, COM3)
                    If not specified, will try to auto-detect
  --firmware FILE   Path to firmware .bin file
                    (Download from WiFi Marauder releases)
  --help            Show this help

Examples:
  $0 --port /dev/ttyUSB0 --firmware marauder.bin
  $0 --firmware esp32_marauder_v0_13_10_20240626.bin

Where to get firmware:
  - WiFi Marauder: https://github.com/justcallmekoko/ESP32Marauder/releases
  - Choose the .bin file matching your board (e.g., esp32_marauder_v*.bin)

Alternative: Use esptool directly:
  esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 \\
    write_flash -z 0x1000 firmware.bin

EOF
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --port)
            PORT="$2"
            shift 2
            ;;
        --firmware)
            FIRMWARE="$2"
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

print_msg $BLUE "╔════════════════════════════════════════╗"
print_msg $BLUE "║  Restore Marauder/WiFi Firmware       ║"
print_msg $BLUE "╚════════════════════════════════════════╝"
echo ""

# Check if esptool is installed
if ! command -v esptool.py &> /dev/null; then
    print_msg $YELLOW "⚠ esptool not found. Installing..."
    pip install esptool || {
        print_msg $RED "Error: Failed to install esptool"
        echo "Please install manually: pip install esptool"
        exit 1
    }
fi

print_msg $GREEN "✓ esptool found"

# Check firmware file
if [[ -z "$FIRMWARE" ]]; then
    print_msg $RED "Error: No firmware file specified!"
    echo ""
    echo "Please provide a firmware .bin file with --firmware option."
    echo ""
    echo "Download WiFi Marauder firmware from:"
    echo "  https://github.com/justcallmekoko/ESP32Marauder/releases"
    echo ""
    show_help
    exit 1
fi

if [[ ! -f "$FIRMWARE" ]]; then
    print_msg $RED "Error: Firmware file not found: $FIRMWARE"
    exit 1
fi

print_msg $GREEN "✓ Firmware file: $FIRMWARE"

# Auto-detect port
if [[ -z "$PORT" ]]; then
    print_msg $YELLOW "⚠ No port specified, attempting auto-detection..."
    
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        if ls /dev/ttyUSB* 1> /dev/null 2>&1; then
            PORT=$(ls /dev/ttyUSB* | head -n 1)
        elif ls /dev/ttyACM* 1> /dev/null 2>&1; then
            PORT=$(ls /dev/ttyACM* | head -n 1)
        fi
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        PORT=$(ls /dev/cu.* | grep -E '(usbserial|SLAB|wchusbserial)' | head -n 1)
    fi
    
    if [[ -z "$PORT" ]]; then
        print_msg $RED "Error: Could not auto-detect port."
        echo "Please specify with --port option."
        exit 1
    fi
    
    print_msg $GREEN "✓ Auto-detected port: $PORT"
fi

# Display configuration
echo ""
print_msg $BLUE "Configuration:"
echo "  Port:     $PORT"
echo "  Firmware: $FIRMWARE"
echo ""

# Safety check
print_msg $YELLOW "⚠ WARNING: This will erase BT Audio firmware and flash new firmware!"
echo ""
echo "Press Ctrl+C to cancel, or Enter to continue..."
read

# Detect chip type
print_msg $BLUE "Detecting chip type..."
CHIP_TYPE=$(esptool.py --port $PORT chip_id 2>&1 | grep "Detecting chip type" | awk '{print $NF}' || echo "esp32")
print_msg $GREEN "✓ Detected chip: $CHIP_TYPE"

# Erase flash
print_msg $BLUE "Erasing ESP32 flash..."
if ! esptool.py --chip $CHIP_TYPE --port $PORT erase_flash; then
    print_msg $RED "Error: Erase failed!"
    exit 1
fi
print_msg $GREEN "✓ Erase successful"

# Flash firmware
print_msg $BLUE "Flashing firmware..."
if ! esptool.py --chip $CHIP_TYPE --port $PORT --baud 921600 \
    write_flash -z 0x1000 "$FIRMWARE"; then
    print_msg $RED "Error: Flash failed!"
    exit 1
fi
print_msg $GREEN "✓ Flash successful"

echo ""
print_msg $GREEN "════════════════════════════════════════"
print_msg $GREEN "✓ Firmware restore complete!"
print_msg $GREEN "════════════════════════════════════════"
echo ""
echo "Your ESP32 has been restored with the provided firmware."
echo ""
echo "To flash BT Audio again, run:"
echo "  ./flash_bt_audio.sh"
echo ""
