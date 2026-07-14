# Air Level

## Overview
This is a simple level that can use the motion-tracking sensor of the Video Game Module.

## Building
- Clone your firmware (`git clone --recursive https://github.com/flipperdevices/flipperzero-firmware.git`)
- Copy the air_level project into your firmware's applications_user folder.
- Connect your Flipper Zero (make sure qFlipper and lab.flipper.net are NOT running)
- Build & deploy your firmware (`fbt COMPACT=1 DEBUG=0 FORCE=1 flash_usb_full`) 

## Credits
- This started with the Air Arkanoid game. Edited by @CodeAllNight.
- Idea by @human_tool
- The [Flipper Zero Game Engine](https://github.com/flipperdevices/flipperzero-game-engine) was copied instead of using as a submodule.

## Support
Contact me on Discord (@CodeAllNight)
