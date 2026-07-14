<!-- Source: https://docs.flipper.net/zero/basics/firmware-update -->
<!-- Mirrored: 2026-07-14 -->

# Firmware Update - Flipper Zero Documentation

## Overview

Flipper Zero firmware updates are regularly released to deliver new features and improvements. Users can update their devices through either the Flipper Mobile App or qFlipper desktop application.

## Firmware Update Channels

The documentation describes three firmware versions available:

**Development (Dev)** - Built with each new commit, often multiple times daily. Includes the latest features but may be unstable and could cause data corruption or freezing.

**Release Candidate (RC)** - Submitted for QA validation testing. If bugs are found, revisions are issued. Upon passing all tests, it becomes the release version.

**Release** - The stable version, extensively tested for reliability and recommended for general use.

Users can find the complete changelog on the official GitHub releases page.

## Updating via Flipper Mobile App

### Connecting to Flipper Zero

1. Enable Bluetooth on both the phone and Flipper Zero (Main Menu > Settings > Bluetooth)
2. In the mobile app, tap "connect" next to the detected device name
3. Enter the pairing code displayed on the Flipper Zero screen
4. Tap "pair" to finalize

### Performing the Update

1. In the Main tab, select "update channel" and choose desired firmware (Release recommended)
2. Tap the "update" button and confirm
3. The process takes approximately 10 minutes

### Troubleshooting

Check Bluetooth connectivity, ensure the device is powered on, and reboot if unresponsive by holding the left and back buttons for 5 seconds.

## Updating via qFlipper

qFlipper is a desktop application available for Windows 10/11, macOS 10.14+, and Linux.

### Installation

- Download the appropriate installation file for your operating system
- For Linux (AppImage format), make the file executable via file manager properties or terminal using `chmod +x`
- Set up udev rules after installation

### Performing the Update

1. Connect Flipper Zero via USB cable
2. Open qFlipper on your computer
3. Go to the Advanced Controls tab
4. Click "update channel" and select firmware (Release recommended)
5. Click "update" to begin
