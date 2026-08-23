v1.2:
- Added a settings menu with editable ping IP and internet check timer
- Added reset behavior that restores the default IP and timer and clears remembered Wi-Fi networks
- Added return from the monitoring screen back to Wi-Fi scanning with the Back button
- Updated LED behavior: steady green when internet is online, 1-second red blinking while offline
- Polished the settings screen header and selection visuals
- Added validation for saved ping IP settings with fallback to the default target

v1.1:
- Added persistent storage for up to 16 Wi-Fi credentials
- Added one-button connection to saved networks
- Added password editing and saved-network deletion
- Added a full lowercase, uppercase, numeric, and symbol keyboard
- Added waiting, reconnect, and recovery notifications for internet monitoring
- Added `1.1.1.1` as the default ping target

v1.0:
- Added ESP-AT Wi-Fi scanning and network selection
- Added internet availability checks every 60 seconds
- Added visual status and notification when connectivity returns
