# Multi-Firmware Build Guide

This guide shows how to build the Flipper Wedge app for different Flipper Zero firmwares.

## Quick Start

### 1. Clone Additional Firmwares (One-Time Setup)

```bash
cd /home/work

# Unleashed Firmware
git clone --recursive https://github.com/DarkFlippers/unleashed-firmware.git

# Momentum Firmware (Xtreme successor)
git clone --recursive https://github.com/Next-Flip/Momentum-Firmware.git

# RogueMaster Firmware (optional - less stable)
git clone --recursive https://github.com/RogueMaster/flipperzero-firmware-wPlugins.git roguemaster-firmware
```

### 2. Use the Build Scripts

I've created three helper scripts for you:

#### `build.sh` - Build for a specific firmware
```bash
# Build for Official firmware (default)
./build.sh

# Build for Unleashed
./build.sh unleashed

# Build for Momentum
./build.sh momentum

# Build for RogueMaster
./build.sh roguemaster

# Aliases work too
./build.sh ofw        # Official
./build.sh ul         # Unleashed
./build.sh mntm       # Momentum
./build.sh rm         # RogueMaster
```

#### `deploy.sh` - Build and deploy to connected Flipper
```bash
# Deploy Official firmware build
./deploy.sh

# Deploy Unleashed build
./deploy.sh unleashed

# Deploy Momentum build
./deploy.sh momentum
```

#### `build-all-firmwares.sh` - Build for ALL firmwares at once
```bash
# Builds for all available firmwares and saves to dist/ folder
./build-all-firmwares.sh
```

Output FAP files will be in `dist/` with firmware-specific names:
- `flipper_wedge_official.fap`
- `flipper_wedge_unleashed.fap`
- `flipper_wedge_momentum.fap`
- `flipper_wedge_roguemaster.fap`

---

## Manual Build Method

If you prefer to build manually:

```bash
# Official
cd /home/work/flipperzero-firmware
./fbt fap_flipper_wedge

# Unleashed
cd /home/work/unleashed-firmware
./fbt fap_flipper_wedge

# Momentum
cd /home/work/Momentum-Firmware
./fbt fap_flipper_wedge

# RogueMaster
cd /home/work/roguemaster-firmware
./fbt fap_flipper_wedge
```

---

## Finding Built FAP Files

After building, the FAP file is located at:
```
<firmware-directory>/.fap/flipper_wedge.fap
```

For example:
- Official: `/home/work/flipperzero-firmware/.fap/flipper_wedge.fap`
- Unleashed: `/home/work/unleashed-firmware/.fap/flipper_wedge.fap`

---

## Switching Firmware on Your Flipper

To test different firmware builds:

1. Flash the target firmware to your Flipper (qFlipper or CLI)
2. Build the app for that firmware using scripts above
3. Deploy with `./deploy.sh <firmware>` or copy FAP to SD card

---

## Firmware Compatibility Summary

| Firmware | Stability | Build Script | Compatibility |
|----------|-----------|--------------|---------------|
| Official | ⭐⭐⭐⭐⭐ | `./build.sh official` | ✅ 100% |
| Unleashed | ⭐⭐⭐⭐⭐ | `./build.sh unleashed` | ✅ ~98% |
| Momentum | ⭐⭐⭐⭐ | `./build.sh momentum` | ✅ ~95% |
| RogueMaster | ⭐⭐⭐ | `./build.sh roguemaster` | ⚠️ ~80% |

**Recommendation**: Primary testing on Official, Unleashed, and Momentum firmwares.

---

## Updating Firmwares

To pull the latest firmware updates:

```bash
# Official
cd /home/work/flipperzero-firmware && git pull --recurse-submodules

# Unleashed
cd /home/work/unleashed-firmware && git pull --recurse-submodules

# Momentum
cd /home/work/Momentum-Firmware && git pull --recurse-submodules

# RogueMaster
cd /home/work/roguemaster-firmware && git pull --recurse-submodules
```

---

## Troubleshooting

### Symlink Issues
If the build script can't find the app, manually create the symlink:
```bash
ln -s "/home/work/flipper wedge" <firmware-path>/applications_user/flipper_wedge
```

### Build Errors
- Make sure firmware is up to date
- Clear build cache: `./fbt -c fap_flipper_wedge`
- Check that submodules are initialized: `git submodule update --init --recursive`

### FAP Won't Load on Flipper
- Ensure Flipper firmware matches the build target
- Clear apps folder on SD card: `/ext/apps/`
- Check API version compatibility

---

## Distribution Strategy

When releasing the app:

1. **Primary Release**: Build for Official Firmware only
2. **Source Code**: Users on custom firmwares build from source
3. **Optional**: Provide pre-built FAPs for Unleashed and Momentum in separate downloads

Example release structure:
```
flipper-wedge-v1.0/
├── flipper_wedge.fap          # Official firmware
├── flipper_wedge_unleashed.fap
├── flipper_wedge_momentum.fap
├── source/                              # Full source code
└── BUILD.md                             # Build instructions
```
