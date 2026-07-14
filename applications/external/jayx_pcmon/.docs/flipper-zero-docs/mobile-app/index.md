<!-- Source: https://docs.flipper.net/zero/mobile-app -->
<!-- Mirrored: 2026-07-14 -->

# Flipper Mobile App

## Overview

The Flipper Mobile App enables remote control and management of your Flipper Zero device. According to the documentation, you can "remotely control and update your flipper zero, share saved keys, manage and edit data, and more." The application is available on both iOS and Android platforms.

## Connecting to Flipper Zero

To establish a connection:

1. Activate Bluetooth on both your Flipper Zero (Main Menu > Settings > Bluetooth) and smartphone
2. Open the Flipper Mobile App and tap Connect
3. Select your detected Flipper Zero device
4. Enter the pairing code displayed on the device screen
5. Tap Pair to complete the connection

**Troubleshooting connection issues:**
- Verify Bluetooth is enabled on both devices
- Check for interference from other paired devices
- Update Flipper Zero to the latest firmware
- Ensure you have the latest version of the mobile app installed
- Disable sleep mode on your Flipper Zero (Settings > System)

## Key Features

**Main Menu Tab:** Update firmware via Bluetooth, access device information, manually synchronize, and play sounds on your device.

**Archive Tab:** Access all saved remotes, keys, and cards. You can search, mark favorites, and restore deleted items.

**Apps Tab:** Install community-developed applications via Bluetooth LE.

**Tools Tab:** Includes utilities like mfkey32 for calculating MIFARE Classic keys and a remotes library for infrared control.

## Firmware Updates

Three update channels are available:
- **Development (Dev):** Cutting-edge builds with latest features but potential instability
- **Release Candidate (RC):** Tested versions awaiting final validation
- **Release:** Stable, thoroughly tested firmware (recommended)

Updates via the mobile app may take approximately 10 minutes.

## Data Backup

You can back up keys, cards, and remotes by navigating to Main Menu > Options > Backup Keys, then selecting your preferred save or share location. Backed-up data can be shared via messenger or uploaded to a PC.
