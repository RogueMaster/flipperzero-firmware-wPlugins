<!-- Source: https://docs.flipper.net/zero/qflipper -->
<!-- Mirrored: 2026-07-14 -->

# qFlipper - Flipper Zero Documentation

## qFlipper

qFlipper is a desktop application for updating Flipper Zero firmware and databases, managing files on the microSD card, and repairing corrupted firmware. It's available on Windows, macOS, and Linux.

### Installing qFlipper

**System Requirements:**
- Windows 10 and 11
- macOS version 10.14 or later (optimized for M1 Apple Silicon)
- Linux (AppImage format)

**Linux Setup:**
1. Right-click the downloaded AppImage file
2. Go to Properties > Permissions
3. Check "Allow executing file as program" (or equivalent for your file manager)
4. Double-click to run, or use terminal: `chmod +x filename.appimage` then `./filename.appimage`
5. Set up udev rules after installation

### qFlipper Overview

After installing qFlipper and connecting your Flipper Zero via USB:

**Device Information Tab:**
- View hardware and firmware information
- Update firmware
- Control Flipper Zero remotely

**Advanced Control Tab:**
- Back up, restore, and reset your device
- Choose firmware to install

**File Manager Tab:**
- Delete, rename, and upload files
- Navigate using keyboard arrow keys

### Keyboard Shortcuts

| Action | Windows/Linux | macOS |
|--------|---------------|-------|
| Upload | Ctrl+L | Command+L |
| Download | Ctrl+D | Command+D |
| Delete | Delete | Delete |
| New Folder | Ctrl+N | Command+N |
| Rename | Ctrl+E | Command+E |
| Refresh | Ctrl+G | Command+G |

### Firmware Update Channels

- **Development (Dev):** Built with every commit; includes latest features but may be unstable
- **Release Candidate (RC):** Version undergoing QA testing
- **Release:** Stable, extensively tested version (recommended)

**To update:**
1. Connect Flipper Zero via USB
2. Open qFlipper
3. Go to Advanced Controls tab
4. Click "Update channel" and select from dropdown
5. Click "Update" to start

### Remote Control

In the Device Information tab, click the Flipper Zero image to control it remotely using on-screen buttons or keyboard. Use the info button to learn keyboard controls. You can also capture and save screenshots.

### Reporting Issues

If errors occur during updates, report them on the forum at https://forum.flipperzero.one/ in the Firmware Update/qFlipper section. Include detailed steps and attach qFlipper logs in .txt format.

**To retrieve logs:**
1. Click "Logs" in qFlipper
2. Click "Open full log" and save to your computer
3. For previous sessions, right-click logs and select "Browse all logs"
