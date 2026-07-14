# Build and Deployment Guide

## Working Commands

### Build
```bash
# Build the application - WORKS
ufbt
# Output: Creates dist/govee_control.fap
```

### Deploy (Requires Connected Flipper Zero)
```bash
# Deploy and launch app - WORKS when Flipper connected
ufbt launch
# Uploads to /ext/apps/Bluetooth/govee_control.fap and launches

# Manual deployment
# Copy dist/govee_control.fap to Flipper SD card via qFlipper app
```

### Commands That Need Device Connection
- `ufbt flash` - Flash firmware (needs Flipper connected)
- `ufbt debug` - Debug session (needs Flipper connected)  
- `ufbt cli` - Serial console (needs Flipper connected)

### Code Quality
```bash
# Format code
clang-format -i govee_control/*.c govee_control/*.h

# Run all checks (has some false positives due to missing SDK headers)
./check.sh
```

## Current Status
- ✅ Builds successfully with `ufbt`
- ✅ Produces `govee_control.fap` in `dist/` folder
- ✅ Can deploy to device with `ufbt launch` when connected
- ⚠️ Code formatting issues exist (run clang-format to fix)
- ⚠️ Static analysis shows warnings (mostly missing SDK headers)

## Testing
1. Connect Flipper Zero via USB
2. Run `ufbt launch` to deploy and test
3. Check logs with `ufbt cli` then type `log`

## File Output
- Debug build: `dist/debug/govee_control_d.elf`
- Release FAP: `dist/govee_control.fap`