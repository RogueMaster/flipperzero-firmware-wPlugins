# Changelog

## v1.7.0

- Fixed cancel button in confirmation dialogs returning to main menu instead of the correct sub-menu
- Fixed `clear_pcap_files` not resetting the `HasOpenedFile` flag after closing the file handle, causing silent write failures on active captures
- Fixed app crashing the entire device on SD card mkdir failure, now exits gracefully instead
- Removed dead null check in `settings_storage_init` that could never be reached
- Fixed sync timing counters never resetting between capture sessions
- Fixed IR universals list query failing on slow UART connections due to using a fixed 200ms delay instead of a proper timeout loop
- Fixed app info dialog showing empty text
- Removed unreachable return statement in `settings_custom_event_callback`
- Fixed critical resource leak on text_box allocation failure, early return now properly cleans up UART context, expansion protocol, OTG state and all allocated memory
- Fixed NULL pointer dereference in `uart_storage_rx_callback` when `app` is NULL
- Fixed memory leak in `show_app_info`, confirmation context now tracked in `active_confirm_context`
- Fixed `SETTING_SHOW_INFO` event handler returning `false` instead of `true`
- Fixed `show_confirmation_dialog_ex` overwriting `active_confirm_context` without freeing previous allocation
- Fixed memory leak in `get_latest_log_file` on `realloc` failure
- Fixed race condition in UART cleanup, capture streams now cleaned after thread join
- Fixed missing NULL checks on `storage_file_alloc` in file clearing functions
- Fixed missing NULL check on `context` in `show_app_info`
- Fixed error path in core UI allocation leaking `RECORD_DIALOGS`, expansion protocol and OTG power state
- Replaced all magic number view IDs with named ViewId enum constants across the entire codebase for readability and maintainability
- Added navigate_to_view helper to centralize view switching logic, reducing back event callback from ~240 lines to ~55 lines
- Added Flock Detection, Flock List, and Stop Flock Detection commands to WiFi Scanning menu

## v1.6.3

- Fixed an issue where the text view would be empty
- Fixed a memory leak when showing the confirmation dialog
- Fixed a memory leak when opening the settings menu
- Fixed a potential memory leak on app exit
- Added a warning to the IR Dazzler menu option for the Poltergeist board from rabbit-labs
- Miscellaneous memory optimisations

## v1.6.2

- Close dialogs record during cleanup to avoid leaks
- Added Aerial Detection and Spoofing commands
- Improved IR memory handling to prevent "File too large" errors
- Optimised the text view to hold more data before discarding

## v1.6.1

- Added multiple status display animations from @tototo31
- Added IR Dazzler menu option
- Added new commands

## v1.6.0

- Added IR menu
- Added option in settings to change status display animation
- Text view has been reworked and scrolling will now pause output, you can resume by pressing right arrow and press the center button to send a stop command to the ESP32

## v1.5.1

- Fixed issue where the command label could be wrong when exiting a command

## v1.5.0

- Support for new pcap marker handling by the firmware as of v1.8
- Fixed no wardriving files being saved
- Added new commands
- Removed unused legacy commands

## v1.4.1

- Collapsed things into rotating picker menus in the WiFi section - #9 by @tototo31

## v1.4

- Set Evil Portal HTML from the Flipper's Filesystem
- Added "Scan APs Live" command to WiFi Scanning menu
- Added "Chip Info" command to WiFi Scanning menu
- Added "Disconnect WiFi" command to WiFi Scanning menu
- Added "ARP Scan" command to WiFi Scanning menu
- Added "Scan SSH" command to WiFi Scanning menu
- Fixed RGB Mode setting to work with the correct command in the new firmware

## v1.3.1

- Might fix crash when closing app without a board connected

## v1.3

- Add WiFi and BLE menu subcategories
- Remember menu position after command execution
- Add missing and new commands
- Move settings from WiFi menu to Settings menu

## v1.2.6

- Fix connect command syntax (#2 @tototo31)
- updated Evil Portal entry to require user input

## v1.2.5

- Added new and missing commands to the menu

## v1.2.4

- Fixed race condition and blocking of UART initialization causing freeze on launch
- Fixed cleanup sequence to ensure all resources are freed in the correct order and avoid crashes

## v1.2.3

- Added LED Effects command

## v1.2.2

- fixed race condition and blocking of UART initialization causing freeze on launch
- slightly improved startup time

## v1.2.1

- Added 'apcred' commands to change and reset WebUI SSID & Password

## v1.2.0

- Added Variable Sniff command replacing individual commands
- Added Variable Beacon Spam command replacing individual commands
- Removed individual stop commands in favour of one for each section

## v1.1.9

- Changed text input buffer size to 128 characters
- Added Flipper Runtime Firmware detection check
- Added option to disable ESP Connection Check for debugging
- Revised connection error message

## v1.1.8 Polaris 📡🌍

- Added GPS Info command to view real-time GPS data
- Added Stop GPS Info command
- UART Initialisation tweaks
- Add back EAPOL capture command
- Added BLE Raw Capture command
- Added Stop BLE Raw Capture command
- Added BLE Skimmer Detection command
- Expanded Stop on Back to include all stop commands
- Added wrap-around scrolling in command menus
- Add Scan Local command to scan for devices on connected network
- Respect Momentum settings for ESP UART Channel
- Added PCAP and Wardrive clearing options in Settings menu
- Added BLE Wardriving command
- Added GPS Track (GPX) command for recording GPS tracks in GPX format
- Miscellaneous bug fixes and improvements

## v1.1.7

- added null checks before freeing resources
- remove unused commands from menu.c and cleaned up command details
- initialise uart in esp connection check if needed

## v1.1.6

- sync files more frequently

## v1.1.5

- expanded quick help menu with more context-specific tips
- swapped order of AT and Stop commands in ESP Check
- cleaned up general UI text
- improved handling of corrupted or missing log files
- general code cleanup and improvements

## v1.1.4

- Added confirmation dialog for Cast Video command
- Fixed view locking issues with confirmation dialog

## v1.1.3

**Added specific buttondown icon for help menu button to not be dependent on firmware icons**

## v1.1.2

- improved ESP connection check reliability by trying AT command first with shorter timeouts, while keeping original 'stop' command as fallback

## v1.1.1

- add sniff pwnagotchi command

## v1.1.0 🕸️👻

- Ring buffer implementation for text handling
- New view buffer management
- Added proper locking mechanisms
- **Remove Filtering due to Firmware updates!**
- Made exiting views more consistent for UE
- **Replaced select a utility text with prompt to show NEW Help Menu**
- Refactored and simplified uart_utils
- Made PCAP file handling more robust
- **Add GPS Menu and commands with saving to .csv**
- Miscellaneous bug fixes

## v1.0.9

- Fixed log file corruption when stopping captures
- Added proper bounds checking for oversized messages
- Improved text display buffer management
- **Added automatic prefix tagging for WiFi, BLE and system messages**
- Improved storage init speed

## v1.0.8

### 🔴 CRITICAL FIX - PCAP capture

- **Fixed PCAP file handling and storage system**
- Resolved PCAP file stream corruption issues
- Added proper storage system initialization
- Removed the line buffering logic for PCAP data

### Improvements

- Added error checking for storage operations
- Filtering majorly improved
- Improved stop on back to be much more reliable by added type-specific stop commands with delays between operations

## v1.0.7a

- Disable the expansion interface before trying to use UART

## v1.0.7

- **Increased buffers and stacks: MAX_BUFFER_SIZE to 8KB, INITIAL_BUFFER_SIZE to 4KB, BUFFER_CLEAR_SIZE to 128B, uart/app stacks to 4KB/6KB**
- Added buffer_mutex with proper timeout handling
- **Added Marauder-style data handling**
- Improved ESP connection reliability
- Added view log from start/end configuration setting
- **Added line buffering with overflow detection, boundary protection and pre-flush on mode switches**

## v1.0.6

- Replaced 'Info' command in ESP Check with 'Stop'
- Slightly improved optional UART filtering
- Memory safety improvements.
- Improved Clear Logs to be faster and more efficient
- **Added details view to each command accessible with hold of center button. (Like BLE Spam)**
- **Made ESP Not Connected screen more helpful with prompts to reboot/reflash if issues persist.**
- Renamed CONF menu option to SET to better align with actual Settings menu since its header is "Settings" and there is a configuration submenu
- Replaced textbox for ESP Connection Check with scrollable Confirmation View

## v1.0.5

- **Commands will silently fail if UART isn't working rather than crashing**
- **Fixed double-free memory issue by removing stream buffer cleanup from the worker thread**
- Reorganized initialization order
- **UART initialization happens in background**
- **Serial operations don't block app startup**
- Optimized storage initialization by deferring file operations until needed
- Improved directory creation efficiency in storage handling

## v1.0.4

- Refined confirmation view line breaks for readability
- Improved ESP Connectivity check to decrease false negatives
- **Added optional filtering to UART output to improve readability (BETA)**
- **Added 'App Info' Button in Settings**
- Misc Changes (mostly to UI)

## v1.0.3

- Enhanced confirmation view structure and readability with better text alignment
- **Added confirmation for "Clear Log Files" with a permanent action warning**
- **Enabled back press exit on confirmation views with callback context handling**
- Improved memory management with context cleanup, view state tracking, and transition fixes
- Added NULL checks, fixed memory leaks, and added state tracking for dialogs

## v1.0.2

- **Added confirmation dialogs for WebUI-dependent features in the UI**
- Improved settings menu with actions submenu, NVS clearing, and log clearing
- Enhanced memory management and improved settings storage/loading robustness
- **Added contextual help for WebUI configuration and confirmation dialogs for command safety**
- Improved view navigation, state management, and memory cleanup processes
- **Added safeguards against `furi_check` failures with NULL checks and memory initialization**

## v1.0.1

- **Revamped menu structure with logical grouping (scanning, beacon spam, attacks, etc.)**
- Simplified command addition and cleaned up documentation in `menu.c`
- **Centralized and enum-based settings metadata for improved validation and extensibility**
- **Enhanced settings with Stop-on-Back feature and ESP reboot command**
- **Enabled automatic connectivity check and error recovery for ESP issues**
- **Unified UI with metadata-driven consistency and better type safety**
- Simplified UI view switching and improved error display
- Refined code organization, separating concerns, removing redundancy, and standardizing error handling
