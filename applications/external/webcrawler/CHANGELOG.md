## 1.0.1
- Updated the settings so they save as intended.
- Added additional error handling.
- Fixed a bug in the text input that prevented users from typing.

## 1.0
- Updated the HTTP Method toggle to work as intended.
- Updated FlipperHTTP to the latest version.
- Improved memory allocation
- Fixed loading display messages.
- Added a BROWSE method, which fetches HTML data from the specified path, parses the HTML, then displays the data as a "webpage". 

## 0.8
Updates from Derek Jamison:
- Improved progress display.
- Added connectivity check on startup.

## 0.7
- Improved memory allocation
- App restructured

## 0.6
- Added a DOWNLOAD method, which downloads the file at the specified path. This is best used for downloading images, binary files, and other non-text files.
- Increased buffer size to handle larger responses.
- Fixed the variable item lists to display as intended.

## 0.5 (Cleanup)
- Refactored using the easy flipper library
- New python library for the FlipperHTTP flash
- Removed unnecessary GPIO initialization
- Removed unnecessary logging

## 0.4 (New Keyboard and HTTP Methods)
- Updated the text input to match the text input from the UART Terminal app (big thanks to xMasterx)
- Added POST, DELETE, and PUT requests (with payloads)
- Added headers
- Added more error handling

## 0.3 (New Features)
- Updated the progress messages displayed after sending a HTTP request.
- Renamed the "Config" main submenu option to "Settings."
- Added a submenu (WiFi, File, Request) that appears when users click the "Settings" option.

Clicking **WiFi** displays a variable item list for SSID and Password. If you click:
- **SSID**: A text input screen will appear with the current saved SSID pre-filled. You can change the SSID by editing the text and clicking "Save."
- **Password**: A text input screen will appear with the current saved password pre-filled. You can change the password by editing the text and clicking "Save."

Clicking **File** displays a variable item list with options to Read File, Set File Type, Rename File, and Delete File. If you click:
- **Read File**: The contents of the saved file will be displayed.
- **Set File Type**: A text input screen will appear with the current saved file type pre-filled. You can change the file type by editing the text and clicking "Save."
- **Rename File**: A text input screen will appear with the current saved file name pre-filled. You can change the file name by editing the text and clicking "Save."
- **Delete File**: The saved file will be deleted.

Clicking **Request** displays a variable item list with a single text input option called **Path**. If you click **Path**, you can change the URL path that the GET request will be sent to.

## 0.2 (Stability Patch)
- Changed serial functions from synchronous to asynchronous.
- Added error handling for HTTP requests and WiFi Dev Board connections.
- Updated the WiFi Dev Board firmware (FlipperHTTP) to blink the green LED three times when the board is first connected, remain solid green when the board receives a serial command, and stay solid green while processing.
- Added auto-connect and auto-disconnect for WiFi when a user enters or exits the app.
- Added an option to view saved data.

## 0.1
- Initial release