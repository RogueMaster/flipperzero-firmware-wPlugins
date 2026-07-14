# Quick Start Guide

## Building the Application

### Prerequisites

- Flipper Zero with latest firmware
- uFBT (micro Flipper Build Tool) or full firmware SDK

### Build Commands

**Using uFBT (recommended):**
```bash
cd Chameleon_Flipper
ufbt
```

**Using full SDK:**
```bash
./fbt fap_chameleon_ultra
```

### Installation

1. Build the `.fap` file
2. Copy to SD card: `/ext/apps/Tools/chameleon_ultra.fap`
3. Launch from: `Applications → Tools → Chameleon Ultra`

## First Connection

### USB Connection

1. Connect Chameleon Ultra to Flipper Zero via USB-C cable
2. Open Chameleon Ultra app on Flipper
3. Select `Connect Device`
4. Choose `USB Connection`
5. Enjoy the bar animation! 🍺
6. You're connected!

### Bluetooth Connection

1. Power on your Chameleon Ultra
2. Open Chameleon Ultra app on Flipper
3. Select `Connect Device`
4. Choose `Bluetooth Connection`
5. Wait for device scan (~3 seconds)
6. Select your Chameleon from the list
7. Watch the connection animation! 🎉

## Main Features

### Slot Management

```
Main Menu → Manage Slots → Select Slot (0-7)
```

**Available actions:**
- Activate slot (make it the active one)
- Rename slot (up to 32 characters)
- Change tag type (coming soon)

### Device Diagnostics

```
Main Menu → Diagnostic
```

**Information shown:**
- Firmware version
- Device model (Ultra/Lite)
- Operating mode (Reader/Emulator)
- Chip ID
- Connection type

### Tag Operations (In Development)

```
Main Menu → Read Tag          # Read from Chameleon
Main Menu → Write to Chameleon  # Transfer from Flipper
```

## Common Operations

### Activate a Different Slot

1. Connect to device
2. `Manage Slots`
3. Select desired slot
4. `Activate Slot`
5. Confirmation appears

### Rename a Slot

1. Connect to device
2. `Manage Slots`
3. Select slot to rename
4. `Rename Slot`
5. Enter new name (max 32 chars)
6. Confirm

### Check Device Info

1. Connect to device
2. Select `Diagnostic`
3. Scroll to view all information
4. Press Back to return

## Troubleshooting

### USB Connection Failed

- Check cable connection
- Ensure Chameleon Ultra is powered on
- Try reconnecting the cable
- Restart both devices

### Bluetooth Not Finding Device

- Make sure Chameleon Ultra is on
- Bluetooth is enabled on Chameleon
- Try scanning again
- Move devices closer together

### Application Crashes

- Update Flipper firmware to latest version
- Rebuild the .fap file
- Check SD card for errors
- Report issue on GitHub

## Tips

- The bar animation can be skipped by pressing any button
- Both USB and BLE can be used, choose what's convenient
- Always check connection status before operations
- Slot nicknames help organize your tags

## Next Steps

- Explore all 8 available slots
- Configure your preferred tags
- Try different connection methods
- Check out the animated connection scene! 🎬

For detailed documentation, see:
- `README.md` - Full project documentation
- `docs/ANIMATION.md` - Animation details
- `docs/PROTOCOL.md` - Protocol specification
