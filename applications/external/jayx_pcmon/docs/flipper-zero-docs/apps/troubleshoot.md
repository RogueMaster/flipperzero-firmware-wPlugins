<!-- Source: https://docs.flipper.net/zero/apps/troubleshoot -->
<!-- Mirrored: 2026-07-14 -->

# Troubleshooting Apps

## Overview

This documentation page provides solutions for common error messages users encounter when running apps on Flipper Zero devices.

## Common Errors and Solutions

**App Not Found**
- Install the official Flipper Mobile App from the App Store or Google Play
- Reinstall the app or update Flipper Zero firmware to the latest official version
- If using custom firmware, reinstall official Flipper Zero firmware from the release channel
- Ensure you're using the official mobile app, not a custom application

**Invalid File**
Potential causes include:
- Corrupted ELF file header
- Corrupted or incomplete file content
- ELF file sections that couldn't be loaded

*Solution:* Update or reinstall the app file

**Manifest Invalid**
This error relates to a special ELF file section containing API version information.
- If firmware is older than the ELF file, update your Flipper Zero firmware
- If ELF file is older than firmware, update the app
- If ELF file is corrupted, update the app

**Missing Imports**
- App was created for newer firmware and is incompatible with older versions
*Solution:* Update Flipper Zero firmware to the latest official version

**Hardware Target Mismatch**
This error indicates the app was created for a different Flipper device model.

**Outdated App**
- Update via Flipper Lab (Apps section) or the Flipper Mobile App (Apps tab)
- Ensure the app version matches your current firmware

**Outdated Firmware**
The app requires newer firmware than currently installed.
*Solution:* Update to the latest official firmware version
