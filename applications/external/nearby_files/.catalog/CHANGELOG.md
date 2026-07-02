# v1.7

## Added
- GPS Baudrate selection (@xMasterX)
- Ability to add GPS to file (@xMasterX)

# v1.6

## Changed
- N/A

# v1.5

## Changed
- Added expansion protocol handling to fix unexpected crashes

# v1.4

## Changed
- Minor bugfixes
- Select UART channel by FW origin

# v1.3

## Changed
- Minor bugfixes

# v1.1

## Added
- GPS module information display in waiting screen
- Enhanced GPS status feedback

## Changed
- App category moved to GPIO for better organization
- Improved app relaunching mechanism (fixed hardcoded paths)

## Fixed
- Files with zero GPS coordinates are now properly filtered out
- App relaunching after launching external files now works correctly

# v1.0

## Added
- Comprehensive documentation and README
- Screenshots and video preview
- Build workflows for multiple firmware variants
- Funding information
- MIT License
- Flipper Map link in documentation

## Changed
- Removed firmware-specific code for broader compatibility
- Updated app icon and visual elements
- Enhanced documentation with detailed setup instructions

## Fixed
- Back button now opens menu instead of immediately exiting
- Removed leftover hardcoded GPS coordinates

# v0.7

## Added
- Complete GPS integration with real-time coordinate acquisition using minmea library
- Distance-based file sorting and proximity calculation using Haversine formula
- GPS status display with satellite count and enhanced user feedback
- Distance labels in file list (e.g., 45m, 1.3km, 23km) with proper formatting
- Main menu system with "Refresh List" and "About" options
- File launching functionality for .sub, .nfc, and .rfid files
- Recursive directory scanning with smart filtering (excludes assets/dotfiles)

## Changed
- Files without GPS coordinates are excluded from display
- VariableItemList UI for compact file display
- File extensions hidden in display for cleaner interface
- Optimized sorting algorithms for better performance

## Fixed
- GPS acquisition stability and crash/reboot issues resolved
- File sorting by proximity works reliably
- App restoration after launching external files
- Navigation flow improvements with proper back button handling
