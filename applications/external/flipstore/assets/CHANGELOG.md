## v1.3.5
- Updated the keyboard class input handling to support long presses for capitalization.
- Updated the keyboard class to show a blinking text cursor 
- Updated the keyboard class to allow users to change the text cursor position (scroll up until you reach the text box, then use left/right buttons to move the cursor)
- Added support for downloading the FlipperHTTP firmware for the PicoCalc, ESP32-C3, ESP32-C5, and more ESP32 variants.
- Optimized the queue handling for downloading multiple files

## v1.3.4
- Updated the save/load functions to use a specific folder ("flipper_http") for storing Wi-Fi and user credentials. This simplifies management and avoids potential conflicts with other applications.

## v1.3.3
- Merged keyboard class from FlipSocial
- Added auto updating

## v1.3
- Fixed the ESP32 firmware download functionality to ensure it downloads each file one-by-one (as intended)
- Switched from "regular" text input to MasterX's (for saving WiFi credentials)
- Added a new option to download GitHub repositories 

## v1.2
- Changed from C to C++ (saved about 50k bytes)
- Created a new downloading screen

## v1.1
- Updated download links to the latest versions

## v1.0
- Changed app name to FlipDownloader (for official firmware)
- Updated Marauder to 1.4
- Added all firmwares from VGM-Library: https://github.com/jblanked/VGM-Library
- Improved memory allocation

## v0.8.1
- Updated GitHub repository downloads

## v0.8
- Updated FlipperHTTP to the latest library.
- Switched to use Flipper catalog API.
- Added an option to download Video Game Module firmware (FlipperHTTP)
- Added an option to download Github repositories.
- Updated Marauder to the version 1.2

## v0.7.2
- Final memory allocation improvements

## v0.7.1
- Improved memory allocation
- Fixed a crash when re-entering the same app list  

## v0.7
- Improved memory allocation
- Added updates from Derek Jamison
- Updated Marauder to the latest version

## v0.6
- Updated app layout
- Added an option to download Developer Board firmware (Black Magic, FlipperHTTP, and Marauder)

## v0.5
- Added app descriptions and versioning

## v0.4
- Added an option to delete apps
- Edits by WillyJL

## v0.3
- Edits by WillyJL
- Improved memory allocation
- Stability patch

## v0.2
- Added categories: Users can now navigate through categories, and when FlipDownloader installs a selected app, it downloads directly to the corresponding category folder in the apps directory
- Improved memory allocation to prevent "Out of Memory" warnings
- Updated installation messages

## v0.1
- Initial release