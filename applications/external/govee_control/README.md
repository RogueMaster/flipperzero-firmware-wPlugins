# Govee Control for Flipper Zero

Control Govee H6006 Smart LED Bulbs directly from your Flipper Zero!

## Features
- BLE device discovery
- Power on/off control
- RGB color control (0-255 per channel)
- White temperature control (2000K-9000K)
- Brightness control (0-100%)
- Auto keep-alive to maintain connection

## Building

```bash
# Install ufbt if not already installed
pipx install ufbt

# Build the application
ufbt

# The .fap file will be in dist/govee_control.fap
```

## Installation

1. Copy `dist/govee_control.fap` to your Flipper's SD card in `/ext/apps/Bluetooth/`
2. Launch from Flipper's Applications > Bluetooth menu

## Testing

```bash
# Build and launch on connected Flipper
ufbt launch

# Run on emulator
ufbt qflipper

# Debug mode
ufbt debug
```

## Protocol Details

H6006 uses 20-byte BLE packets:
- Service UUID: `000102030405060708090a0b0c0d1910`
- Write Characteristic: `000102030405060708090a0b0c0d2b10`

### Packet Structure
```
[Header][Cmd][Data...][Checksum]
  0x33   0xXX  17 bytes   XOR
```

### Commands
- Power: `0x33 0x01 [0x01=ON/0x00=OFF]`
- Brightness: `0x33 0x04 [0x00-0xFE]`
- RGB Color: `0x33 0x05 0x02 [R] [G] [B]`
- White Mode: `0x33 0x05 0x01 [temp]`
- Keep-alive: `0xAA 0x01 0x00` (every 2s)

## Next Steps
- [ ] Implement proper BLE GAP scanning
- [ ] Add GATT service discovery
- [ ] Multiple device support
- [ ] Scene/effect library
- [ ] Scheduling system