# Flipper Zero UHF RFID App v1.1.2

## Overview
This app is designed to work with the YRM100 lineup of readers, M6e Nano UHF RFID Reader and the Flipper Zero. This app supports reading up to 150 tags per second, writing to tags (EPC, TID, Reserved, and User Memory Banks), viewing all tag information, saving tags, and more (with the M6e/M7e)!

## Features
- Read and view up to 150 UHF RFID tags at once!
  - EPC, TID, User, and Reserved Memory Banks
  - Can cycle through multiple tags from a single read!
- Save, rename, delete and view UHF tags on your flipper!
  - EPC, TID, User, and Reserved Memory Banks
- Can lock, and disable tags
- Write UHF tags 
  - EPC Memory Bank
  - TID Memory Bank (not supported if tag is locked)
  - User Memory Bank
  - Reserved Memory Bank 
- UHF RFID Reader Configuration
  - Set RF Power, baud rate, region, etc...
  - Set antenna type (WIP)

## Hardware Requirements

Currently, this Flipper Zero application requires an M6e Nano compatible UHF RFID Reader. I recommend one of the following options:
- SparkFun Simultaneous RFID Reader
- YRM1001 reader or similar
- The M7E Hecto that just came out should work too, however, this I haven't been able to test.

Additionally, a Raspberry Pi Zero is required in order to run the ThingMagic Mercury API (M6e or M7e modules only)
- I hope to eliminate the need for the RPi, however, I thought having it could allow for easier adoption of different UHF RFID readers. 
