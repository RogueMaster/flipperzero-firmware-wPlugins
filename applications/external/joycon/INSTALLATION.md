# Installation Guide - Flipper Zero Switch Controller

This guide provides complete instructions for installing and using the Switch Controller app on your Flipper Zero.

## Table of Contents
1. [Prerequisites](#prerequisites)
2. [Installation Methods](#installation-methods)
   - [Method 1: Using uFBT (Recommended)](#method-1-using-ufbt-recommended)
   - [Method 2: Using Flipper Mobile App](#method-2-using-flipper-mobile-app)
   - [Method 3: Using qFlipper](#method-3-using-qflipper)
3. [First Time Setup](#first-time-setup)
4. [Usage](#usage)
5. [Troubleshooting](#troubleshooting)

---

## Prerequisites

### Hardware Requirements
- **Flipper Zero** device with latest firmware
- **USB-C cable** for connecting Flipper to Switch
- **Nintendo Switch** console
- *Optional:* Computer for building from source

### Firmware Requirements
- **Official Firmware**: 0.98.0 or newer
- **Momentum Firmware**: Any recent version (fully compatible)
- **Unleashed Firmware**: Should work (not officially tested)

Firmware can be updated via:
  - Flipper Mobile App (iOS/Android)
  - qFlipper desktop app (Windows/Mac/Linux)
  - Web updater: https://update.flipperzero.one/
  - Momentum updater: https://momentum-fw.dev/update

---

## Installation Methods

### Method 1: Using uFBT (Recommended)

This method builds the app from source and installs it directly to your Flipper.

**✅ Works with both Official and Momentum Firmware**

#### Step 1: Install uFBT

**On Linux/macOS:**
```bash
python3 -m pip install --upgrade ufbt
```

**On Windows:**
```bash
py -m pip install --upgrade ufbt
```

**Note for Momentum users:** uFBT automatically detects your firmware version and builds accordingly.

#### Step 2: Clone the Repository

```bash
git clone https://github.com/ccyyturralde/Flipper-Zero-Joycon.git
cd Flipper-Zero-Joycon
```

#### Step 3: Build the App

```bash
ufbt
```

This will compile the app and create a `.fap` file (Flipper Application Package).

#### Step 4: Connect Flipper Zero

Connect your Flipper Zero to your computer via USB. Make sure your Flipper is:
- Powered on
- Not in DFU mode (just normal boot)
- USB connection mode set to "USB-UART Bridge" or "USB-HID" (it will auto-switch)

#### Step 5: Install to Flipper

```bash
ufbt launch
```

This will:
- Upload the app to your Flipper
- Place it in `/ext/apps/USB/` directory
- Automatically launch the app

**Alternative:** Install without launching:
```bash
ufbt upload
```

---

### Method 2: Using Flipper Mobile App

If you have a pre-built `.fap` file:

#### iOS/Android:
1. Download the `.fap` file to your phone
2. Open Flipper Mobile App
3. Connect to your Flipper via Bluetooth
4. Go to **Hub** → **Apps**
5. Tap **Install from file**
6. Select the `switch_controller.fap` file
7. App will be installed to `/ext/apps/USB/`

---

### Method 3: Using qFlipper

#### Step 1: Download qFlipper
- Download from: https://flipperzero.one/update
- Available for Windows, macOS, and Linux

#### Step 2: Connect Flipper
1. Connect Flipper Zero via USB
2. Launch qFlipper
3. It should detect your device

#### Step 3: Install the App
1. Click on **File Manager** in qFlipper
2. Navigate to `/ext/apps/USB/`
3. Click **Upload** (top-right)
4. Select your `switch_controller.fap` file
5. Wait for upload to complete

---

## First Time Setup

### 1. Verify Installation

After installation:
1. On Flipper, press **LEFT** to go to main menu
2. Navigate to **Applications**
3. Go to **USB** category
4. You should see **Switch Controller** in the list

### 2. Grant Permissions

The app needs access to:
- USB functionality (automatic)
- SD card storage (automatic)
- No additional permissions required

### 3. Test Connection

1. Launch **Switch Controller** app
2. Connect Flipper to Nintendo Switch via USB-C
3. You should see "Connected" on the screen
4. The Switch should recognize it as a Pro Controller

---

## Usage

### Connecting to Nintendo Switch

1. **Turn on your Nintendo Switch**
2. **Connect Flipper Zero to Switch** using USB-C cable
   - Use the USB-C port on top of Flipper
   - Connect to Switch dock or directly to Switch in handheld mode
3. **Launch Switch Controller app** on Flipper
4. **Select "Manual Controller"** from menu
5. Switch should show "Pro Controller connected"

### Manual Controller Mode

Control your Switch with Flipper buttons:

**Button Mapping:**
- **Up/Down/Left/Right** → D-Pad or Analog Stick (depends on mode)
- **OK button** → A button
- **Back button (short press)** → B button
- **Back button (long press)** → Toggle control mode
- **OK button (long press)** → Button menu for X, Y, L, R, ZL, ZR, +, -, Home

**Control Modes:**
- **Mode: D-Pad** - Directional buttons control D-Pad
- **Mode: Left Stick** - Directional buttons control left analog stick
- **Mode: Right Stick** - Directional buttons control right analog stick

**Toggle between modes:** Long press Back button

### Recording a Macro

1. From main menu, select **Record Macro**
2. Enter a name for your macro
3. **Before recording:** Long press Back to change control mode if needed
4. Press **OK** to start recording
5. Perform your button sequence:
   - Press buttons in the order you want
   - Timing is automatically recorded
   - Switch control modes during recording if needed (long press Back)
6. Press **Back (short press)** to stop and save
7. Macro is saved to SD card automatically

**Example:** Recording a menu navigation macro
```
1. Start recording
2. Press Down (wait) → Press A (wait) → Press Down (wait) → Press A
3. Stop recording
4. Macro saved!
```

### Playing a Macro

1. Connect Flipper to Switch via USB
2. From main menu, select **Play Macro**
3. Choose a saved macro from the list
4. Macro will play automatically
5. **Loop mode:** Macro repeats until you stop it
6. Press **Back** to stop playback

---

## Troubleshooting

### App Not Appearing
- **Check firmware version:** Official 0.98.0+ or any recent Momentum version
- **Check installation path:** Should be in `/ext/apps/USB/`
- **Momentum users:** App may also appear in the custom apps folder
- **Try reinstalling:** Delete old version and install again
- **Restart Flipper:** Hold Back button for 10 seconds

### Switch Not Recognizing Controller
- **Check USB cable:** Must support data transfer (not charge-only)
- **Try different USB port:** If using dock, try different port
- **Check Switch firmware:** Update to latest version
- **Disconnect other controllers:** Remove all other controllers first
- **Restart app:** Exit and relaunch Switch Controller app

### USB Connection Issues
- **Verify cable:** USB-C to USB-C cable required
- **Check Flipper USB mode:** App auto-configures, but try manual reset
- **Power cycle:** Unplug cable, restart app, reconnect
- **Try Switch handheld mode:** Test without dock first

### Macros Not Saving
- **Check SD card:** Must have working SD card inserted
- **Check storage space:** Need at least 1MB free
- **Verify path:** `/ext/apps_data/switch_controller/` should exist
- **Check permissions:** SD card must be readable/writable

### Buttons Not Responding
- **Check connection:** Verify "Connected" status on screen
- **Try button menu:** Long press OK to test other buttons
- **Check control mode:** Verify you're in correct mode (D-Pad/Stick)
- **Restart app:** Exit and relaunch

### Macro Playback Issues
- **Timing off:** Games may vary; re-record with adjusted timing
- **Stuck buttons:** Stop playback and manually press buttons
- **Wrong control mode:** Ensure macro uses correct mode for the game
- **Verify macro file:** Check `/ext/apps_data/switch_controller/`

### Performance Issues
- **Slow response:** Normal USB latency (~10ms)
- **Laggy inputs:** Check USB cable quality
- **App crashes:** Update Flipper firmware
- **Memory errors:** Restart Flipper, clear old macros

---

## Advanced Usage

### Building with Custom Modifications

If you want to modify the source code:

1. Edit the source files in your local repository
2. Rebuild with `ufbt`
3. Test with `ufbt launch_app`
4. Debug with `ufbt cli` to see logs

### Accessing Logs

To view app logs for debugging:
```bash
ufbt cli
```

Then in the CLI:
```
log debug
```

### Uninstalling

**Via qFlipper:**
1. Open File Manager
2. Navigate to `/ext/apps/USB/`
3. Delete `switch_controller.fap`

**Via Mobile App:**
1. Go to Hub → Apps
2. Find Switch Controller
3. Swipe to delete

**Via CLI:**
```bash
ufbt cli
```
Then:
```
storage remove /ext/apps/USB/switch_controller.fap
```

---

## Additional Resources

- **Project Repository:** https://github.com/ccyyturralde/Flipper-Zero-Joycon
- **Macro Guide:** See `MACRO_GUIDE.md` for advanced macro creation
- **Flipper Docs:** https://docs.flipperzero.one/
- **uFBT Docs:** https://github.com/flipperdevices/flipperzero-ufbt

---

## Support

If you encounter issues:

1. Check this troubleshooting guide
2. Review the [project issues](https://github.com/ccyyturralde/Flipper-Zero-Joycon/issues)
3. Create a new issue with:
   - Flipper firmware version
   - Switch firmware version
   - Detailed description of problem
   - Steps to reproduce

---

## Safety Notes

- **Do not** leave macros running unattended
- **Do not** use macros to violate game terms of service
- **Do not** use in online competitive games
- **Always** supervise macro playback
- **Be aware** of USB cable quality to prevent damage

---

## Legal

This software is for educational purposes. Use responsibly and in accordance with Nintendo's terms of service and your local laws.
