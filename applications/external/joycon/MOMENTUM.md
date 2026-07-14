# Momentum Firmware Compatibility

This app is **fully compatible** with Momentum Firmware!

## Why It Works

Momentum Firmware is designed to be "backwards- and inter-compatible" with official Flipper firmware. Since our Switch Controller app uses standard Flipper APIs (USB HID, GUI, Storage), it works seamlessly on Momentum without any modifications.

## Installation on Momentum

You can use the exact same installation methods:

### Method 1: uFBT (Recommended)
```bash
# uFBT automatically detects Momentum firmware
python3 -m pip install --upgrade ufbt
git clone https://github.com/ccyyturralde/Flipper-Zero-Joycon.git
cd Flipper-Zero-Joycon
ufbt
ufbt launch
```

### Method 2: Flipper Mobile App
- Works identically to official firmware
- Install `.fap` file through the app

### Method 3: qFlipper
- Same process as official firmware
- Upload to `/ext/apps/USB/`

## Features on Momentum

All features work identically:
- ✅ Manual controller mode with 3 control modes
- ✅ Macro recording
- ✅ Macro playback
- ✅ USB HID emulation
- ✅ SD card storage

## Momentum-Specific Benefits

Since Momentum has enhanced customization:
- You may have better USB compatibility depending on your settings
- Asset packs won't affect the app (it uses standard UI)
- Any Momentum-specific security features won't interfere

## Differences from Official Firmware

**None for this app!** The USB HID APIs, file system, and GUI system are the same.

## Building from Source

If you want to build specifically for Momentum using their build system:

```bash
# Clone Momentum firmware
git clone https://github.com/Next-Flip/Momentum-Firmware.git
cd Momentum-Firmware

# Copy app to applications_user folder
# (Not recommended - uFBT is easier)
```

**Recommendation:** Just use uFBT - it's simpler and works for both firmwares.

## Troubleshooting

### App appears in different location
- On Momentum, apps may be organized differently in the menu
- Check both USB category and any custom app categories

### USB not working
- Momentum has advanced USB settings
- Check Settings → System → USB settings
- Ensure USB is not locked down by Momentum security features

### Build errors with uFBT
- Make sure uFBT is updated: `python3 -m pip install --upgrade ufbt`
- Update SDK: `ufbt update`
- uFBT auto-detects firmware, so this should be rare

## Verified Compatible

This app has been confirmed compatible with:
- ✅ **Official Firmware** 0.98.0+
- ✅ **Momentum Firmware** (latest versions)
- ⚠️ **Unleashed Firmware** (should work, not officially tested)

## Questions?

If you encounter any Momentum-specific issues, please open an issue on GitHub with:
- Your Momentum firmware version
- The specific error message
- Steps to reproduce

---

**Bottom line:** If you're on Momentum, install it the same way as you would on official firmware. It just works! 🎉
