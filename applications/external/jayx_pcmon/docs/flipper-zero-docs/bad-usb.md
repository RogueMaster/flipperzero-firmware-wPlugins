<!-- Source: https://docs.flipper.net/zero/bad-usb -->
<!-- Mirrored: 2026-07-14 -->

# Bad USB - Flipper Zero Documentation

## Overview

Flipper Zero can function as a "bad USB" device—a human interface device (HID) recognized by computers as a keyboard or mouse. According to the documentation, such devices can "change system settings, open backdoors, retrieve data, initiate reverse shells, or do basically anything that can be achieved with physical access."

The device executes commands written in DuckyScript (Rubber Ducky scripting language), with Flipper Zero supporting "extended rubber ducky script syntax" that includes additional commands beyond the classic 1.0 version.

## Flipper Zero Scripting Language

Payloads must be created as `.txt` files using ASCII text editors. The scripting language is "compatible with the classic rubber ducky scripting language 1.0" while providing extended features such as the Alt+NumPad input method and SysRq commands.

## Uploading Payloads

Once created, payloads can be uploaded via qFlipper or the Flipper Mobile App to the `SD card/badusb/` folder, where they become accessible in the Bad USB application.

## USB Connection Method

1. Close qFlipper if running
2. Navigate to Main Menu > Bad USB
3. Select your payload
4. Verify USB mode (USB logo displayed)
5. Adjust keyboard layout if needed (default: US English)
6. Connect via USB cable
7. Press **OK** to execute

## Bluetooth Low Energy (BLE) Connection Method

1. Activate Bluetooth in Main Menu > Settings
2. Close qFlipper if running
3. Navigate to Main Menu > Bad USB
4. Select payload and switch to BLE mode
5. Connect via computer's Bluetooth settings
6. Confirm connection on both devices
7. Press **Run** to execute

To unpair, navigate to config and select remove pairing.
