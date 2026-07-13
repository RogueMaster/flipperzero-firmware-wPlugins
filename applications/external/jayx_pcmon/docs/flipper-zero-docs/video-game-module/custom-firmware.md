<!-- Source: https://docs.flipper.net/zero/video-game-module/custom-firmware -->
<!-- Mirrored: 2026-07-14 -->

# Custom Firmware - Flipper Zero Documentation

## Сustom firmware

The Video Game Module can function as a standalone device for applications developed for the RP2040 microcontroller, similar to the Raspberry Pi Pico. This page explains how to install custom firmware on the Video Game Module through either your Flipper Zero or a personal computer.

### Installing Custom Firmware via Flipper Zero

This method allows you to store multiple firmware versions on your Flipper Zero and install them on the module as needed without requiring a computer.

**Step 1: Transfer the firmware file to Flipper Zero**

You can transfer custom firmware files to your Flipper Zero using qFlipper:

1. Connect your Flipper Zero to your computer via USB-C cable
2. Open the qFlipper application
3. Go to the file manager tab, then double-click SD card
4. Right-click the empty space and create a new folder for module firmware
5. Go to the created folder, right-click the empty space, and select "upload here" or drag and drop the file

**Step 2: Install the firmware on the module**

Once the module firmware file is on your Flipper Zero:

1. If you don't have the Video Game Module tool on your Flipper Zero, install it
2. Connect the Video Game Module to your Flipper Zero
3. On your Flipper Zero, go to Main Menu > Apps > Tools > Video Game Module Tool
4. In the app, select "Install firmware from file"
5. Select the file with custom firmware
6. Confirm installation by pressing Install

Once the module firmware is installed, you'll see the message "video game module updated."

### Installing Custom Firmware via PC

Alternatively, you can install UF2 format firmware directly from your computer:

1. Disconnect the module from Flipper Zero and any USB-C cable
2. On the module, press and hold the boot button with a paperclip
3. Connect the module to your computer via USB-C cable
4. The module will switch to USB mass storage mode and mount as "RPI RP2"
5. Release the boot button
6. In a file manager, drag and drop the UF2 firmware file onto the RPI RP2 device
7. The module will automatically reboot and unmount itself
