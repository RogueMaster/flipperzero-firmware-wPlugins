# Troubleshooting Guide

## COM4 Permission Error

### Problem
`python -m ufbt launch` fails with: `PermissionError(13, 'Access is denied.'` on COM4

### Solutions

#### Option 1: Check for Conflicting Processes

**Run this PowerShell command to check what's using COM4:**
```powershell
$port = new-Object System.IO.Ports.SerialPort COM4
try {
    $port.Open()
    $port.Close()
    Write-Host "COM4 is available"
} catch {
    Write-Host "COM4 is locked: $_"
}
```

**Kill any processes that might be using COM4:**
```powershell
# Kill qFlipper if running
Stop-Process -Name "qFlipper" -Force -ErrorAction SilentlyContinue

# Kill PuTTY if running
Stop-Process -Name "putty" -Force -ErrorAction SilentlyContinue

# Check for Python serial processes
Get-Process python | Stop-Process -Force -ErrorAction SilentlyContinue
```

#### Option 2: Build and Deploy Manually

**Step 1: Build the .fap file only (no USB needed):**
```bash
python -m ufbt fap_dist
```

This creates the .fap file in the `dist/` folder.

**Step 2: Copy manually using qFlipper:**
1. Open qFlipper
2. Go to File Manager
3. Navigate to `/ext/apps/USB/`
4. Drag and drop `switch_controller.fap` from the `dist/` folder

**Step 3: Restart Flipper:**
- Long press BACK button
- Select "Restart"

#### Option 3: Use Different USB Cable/Port

Sometimes USB cables or ports can cause COM port issues:
1. Try a different USB cable (must support data, not just charging)
2. Try a different USB port on your computer
3. Avoid USB hubs - connect directly to your PC

#### Option 4: Check Flipper USB Settings

On your Flipper Zero:
1. Go to: **Settings → System → USB Channel**
2. Make sure it's set to: **USB UART & VCP**
3. If it's in "USB HID" mode, that could cause conflicts

#### Option 5: Windows Device Manager Check

1. Open Device Manager (Win+X → Device Manager)
2. Expand "Ports (COM & LPT)"
3. Look for "USB Serial Device (COM4)"
4. Right-click → Properties
5. Check if there are any errors or warnings
6. Try "Disable" then "Enable" the device

#### Option 6: Reinstall ufbt

If ufbt is corrupted:
```bash
pip uninstall ufbt
pip install --upgrade ufbt
ufbt update
```

## Switch Not Recognizing Controller

### Recent Fixes Applied (Commit 14561c9)

The following critical issues were fixed:

1. **USB HID Report Size**: Fixed from 12 bytes to 11 bytes (was sending garbage data)
2. **VID/PID Changed**: Using HORI licensed controller (0x0f0d/0x00c1) instead of Nintendo official
3. **Continuous USB Updates**: Timer sends reports at 100Hz (every 10ms)

### Testing After Deployment

Once you deploy the latest build, test these:

1. **Connect Flipper to Switch via USB-C**
   - Use a good quality USB-C cable
   - Connect Flipper directly to Switch dock or tablet

2. **Launch the app on Flipper**
   - Apps → USB → Switch Controller
   - Should show "Connected" on screen

3. **Check Switch Controller Settings**
   - Go to: Home → Controllers → Change Grip/Order
   - You should see "Pro Controller" appear
   - Press A (OK button on Flipper) to confirm

4. **Test Inputs**
   - D-Pad: Use arrow keys on Flipper
   - A button: OK button on Flipper
   - B button: BACK button (short press)
   - Other buttons: Long press OK to open button menu

### Control Modes

- **Double-click OK** to cycle through modes:
  - D-Pad mode (arrows control D-Pad)
  - Left Stick mode (arrows control left analog stick)
  - Right Stick mode (arrows control right analog stick)

### Exit the App

- **Long press BACK** → confirmation dialog appears
- **Press LEFT** to confirm exit
- **Press BACK** to cancel

## Debug Steps for Switch Connection

If Switch still doesn't recognize the controller:

### 1. Check USB Cable
- Must be a data cable (not charging-only)
- Try different cable if available
- Connect directly to Switch (not through hub)

### 2. Test with Other Devices
- Try connecting to a PC/Mac
- Open "Game Controller Settings" (Windows: joy.cpl)
- See if "HORI Fighting Commander" appears

### 3. Check Flipper USB Output
On Flipper:
- Settings → System → USB Channel → USB UART & VCP
- Launch the app
- Should show "Connected" on screen

### 4. Try Different Switch USB Port
- If using dock: try both USB ports
- Try connecting to Switch in handheld mode (bottom USB-C port)

### 5. Verify Latest Build
Check the commit hash in the .fap file:
```bash
git log --oneline -1
```
Should show: `14561c9 Fix USB HID report size and use compatible VID/PID for Switch`

## Still Having Issues?

If none of these work, provide the following information:

1. **ufbt version:**
   ```bash
   python -m ufbt --version
   ```

2. **Flipper firmware version:**
   - Settings → System → About
   - Note the version number

3. **Windows COM port status:**
   - Device Manager screenshot of Ports section

4. **Build output:**
   ```bash
   python -m ufbt 2>&1 | tee build.log
   ```

5. **Switch firmware version:**
   - Settings → System → System Update
   - Note current version
