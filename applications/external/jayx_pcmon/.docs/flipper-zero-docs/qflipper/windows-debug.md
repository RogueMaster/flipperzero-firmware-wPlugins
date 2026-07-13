<!-- Source: https://docs.flipper.net/zero/qflipper/windows-debug -->
<!-- Mirrored: 2026-07-14 -->

# Troubleshooting Drivers on Windows - Flipper Zero Documentation

## Overview

This guide addresses driver issues when updating a Flipper Zero device via qFlipper on Windows. The document emphasizes installing qFlipper from official sources, as "the required drivers are installed along with it."

## Key Setup Steps

**Update qFlipper**
- Ensure qFlipper is current by accessing the advanced controls tab and checking for app updates
- Connect your Flipper Zero via USB-C cable

## Driver Verification

The guide covers two driver scenarios based on connection mode:

### Serial Device Driver (Normal Mode)

When the device runs its operating system, Windows recognizes it as a serial device. Verification involves:
- Opening Device Manager and navigating to Ports (COM & LPT)
- Confirming the Vendor ID (VID) is `0483` and Product ID (PID) is `5740`
- Ensuring the default Microsoft driver is installed

**COM Port Conflicts**

A Windows bug can cause multiple serial devices to bind to the same port number. Resolution requires assigning each device a unique COM port through Device Manager's port settings.

### DFU Device Driver (Recovery Mode)

When the device enters recovery mode, it appears as a DFU device. To verify:
- Reboot to recovery mode (holding specific buttons until the blue LED activates)
- Check Device Manager under Universal Serial Bus Devices
- Confirm the device name contains "DFU in FS Mode"
- Verify VID `0483` and PID `df11` with STMicroelectronics driver

**Missing or Incorrect DFU Driver**

If the DFU device doesn't appear, the solution involves uninstalling qFlipper, reinstalling from official sources, and potentially manually selecting the WinUSB device driver through Device Manager's "Other Devices" section.
