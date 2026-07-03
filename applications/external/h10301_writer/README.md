H10301 Writer for Flipper Zero

A specialized utility for the Flipper Zero designed for batch programming and sequential writing of HID Prox H10301 (26-bit) RFID credentials onto T5577 tags.

Overview

While the Flipper Zero has native RFID writing capabilities, the H10301 Writer introduces a streamlined workflow for facility managers and testers who need to program multiple cards in a row. It eliminates the need to manually edit and save separate files for every new card.

Key Features

Sequential Workflow: Automatically increments the Card Number (CN) after every successful write.

Precision UI: Features a unique blinking digit-highlighting system. Edit exactly the digits you need without scrolling through long numbers.

Adjustable Step Sizes: Quickly change values by increments of 1, 10, 100, 1,000, or 10,000 using the Left/Right D-pad buttons.

Real-time Feedback: Integrated 10-second write/verify countdown with haptic and notification support.

Persistence: Automatically saves your last used Facility Code (FC) and Card Number (CN) to the SD card.

Usage

Select Field: Press OK (Short Press) to toggle between editing the Facility Code (FC) and the Card Number (CN).

Adjust Step: Use Left/Right to change the decimal place you are editing (Step size).

Change Value: Use Up/Down to increase or decrease the value.

Write Tag: Hold the OK button (Long Press) to start the writing process.

Burn: Hold a blank T5577 tag to the back of the Flipper. The app will attempt to write and verify the credential for 10 seconds.

Success: Upon a successful write, the Card Number automatically increments by 1, and the app saves your progress.

Installation

Using ufbt (Recommended)

If you have the flipperzero-ufbt toolchain installed:

Clone this repository:

git clone [https://github.com/Zacvr/h10301_writer.git](https://github.com/Zacvr/h10301_writer.git)


Navigate to the folder:

cd h10301_writer


Connect your Flipper and run:

python -m ufbt launch


Requirements

Hardware: Flipper Zero

Tags: Rewritable 125kHz tags (T5577 / ATA5577)

Firmware: Compatible with Official, Momentum, and Unleashed firmware sets.

Credits

Developed by ZacVR.

Based on the standard H10301 26-bit Wiegand protocol.

License

This project is licensed under the MIT License - see the LICENSE file for details.