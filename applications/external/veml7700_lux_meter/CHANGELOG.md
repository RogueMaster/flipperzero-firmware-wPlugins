# Changelog

All notable changes to the VEML7700 Lux Meter application will be documented in this file.

---

## [v1.1] - 2025-11-15

### Added
- **Configurable Refresh Rate**: Users can now adjust measurement refresh interval from 100ms to 2000ms in 100ms increments via Settings menu
  - Implemented new `refresh_rate_ms` field in app structure (uint16_t)
  - Added `SettingsItem_RefreshRate` to settings enum
  - Modified main loop to use dynamic delay instead of hardcoded 500ms
  - Default value set to 500ms for balanced performance
  - Allows users to optimize battery life vs responsiveness based on their needs

- **Enhanced Navigation System**: Redesigned Back button behavior for better UX
  - Short press: Navigate back one screen in history (Main ↔ Settings)
  - Long press: Exit application from any screen
  - Implemented using `InputTypeLong` event detection
  - Prevents accidental app exits while maintaining quick navigation
  - Feels more intuitive and aligned with modern mobile UI patterns

- **Startup Flow Redesign**: Application now launches directly into Settings screen
  - Added `SettingsItem_Start` as first menu option
  - Implemented `started` boolean flag in `VEML7700App` structure
  - Sensor initialization deferred until user explicitly starts measurements
  - Main loop guards sensor operations with `if(started)` check
  
- **Start Confirmation Screen**: New `AppState_StartConfirm` state added
  - Displays confirmation dialog before starting measurements
  - Prevents accidental sensor activation
  - Clean transition flow: Settings → StartConfirm → Main

- **WHITE Channel Support**: Full implementation of white light measurement channel
  - Added `WHITE_MEAS_RESULT_REG` (0x05) register definition
  - New `channel` field in app structure (0=ALS, 1=WHITE)
  - `SettingsItem_Channel` added to settings menu
  - Dynamic register selection in `read_veml7700()` based on user choice
  - Channel indicator displayed on main screen

- **Dark Mode**: Toggle-able dark mode for better viewing in low light
  - Inverts screen colors (black background, white text)
  - Added `SettingsItem_DarkMode` with checkbox UI
  - Implemented color inversion in `draw_main_screen()`

### Changed
- **Refactored Input Handling**: Separated short/long press logic for better code organization
  - Input callback now properly handles `InputTypeShort`, `InputTypeRepeat`, and `InputTypeLong`
  - Each input type has dedicated processing block
  - Cleaner switch-case structure for state-specific behaviors

- **Main Screen Redesign**: Completely overhauled measurement display
  - Large font for lux value (FontBigNumbers)
  - Unit "lx" displayed below value in smaller font
  - Removed cluttered key legend
  - Added channel label (ALS/WHITE) at bottom
  - Cleaner, more focused layout

- **Settings Menu Improvements**:
  - Limited visible items to 2 for better readability
  - Added scrollbar indicator on right side
  - Implemented proper item highlighting with inverted colors
  - Arrow indicator (">") for selected item
  - Scroll position calculation with proper bounds checking

- **I2C Address Configuration**: Added support for alternate address
  - Toggle between 0x10 and 0x11 via settings
  - Useful for devices with multiple sensors or address conflicts

### Fixed
- Settings cursor now properly wraps at boundaries
- Scrollbar position calculation handles edge cases correctly
- Gain compensation applied correctly for all gain settings

### Technical Details
- Refactored `draw_settings_screen()` with smarter viewport calculations
- Settings items rendered dynamically based on cursor position
- Proper mutex protection for all sensor data access
- Scrollbar math: `slider_position = (start_item * max_pos) / denom`
- Main loop delay switched from constant to variable: `furi_delay_ms(app->refresh_rate_ms)`
- Input event types properly discriminated for navigation vs exit actions
- Added comprehensive inline code documentation for educational purposes
- Comments written in natural developer style for authenticity

---

## [v1.0] - Initial Release

### Features
- Basic VEML7700 sensor integration via I2C
- Real-time lux measurement display
- Configurable gain settings (1/8, 1/4, 1, 2)
- Integration time: 100ms (ALS_IT_100MS_VAL)
- Power saving mode enabled (ALS_POWER_SAVE_MODE_1_VAL)
- GUI with ViewPort rendering
- Mutex-protected sensor access
- Settings menu with basic navigation
- About screen

### Sensor Configuration
- Default I2C address: 0x10 (7-bit)
- ALS_CONF_REG (0x00) configuration register
- ALS_MEAS_RESULT_REG (0x04) for ambient light readings
- Gain compensation calculations for accurate lux values
- I2C timeout: 100ms

### UI Components
- Main measurement screen
- Settings screen
- About screen
- Font support: Primary, Secondary, BigNumbers
- Canvas-based rendering

---

## Development Notes

### Code Style
- C++ with extern "C" for entry point
- Flipper Zero SDK integration (furi, gui, furi_hal_i2c)
- Static cast usage for type safety
- Const correctness where applicable

### Architecture
- State machine pattern (AppState enum)
- Callback-based input handling
- Mutex synchronization for thread safety
- Proper resource cleanup on exit

### Sensor Math
Gain compensation formulas:
- 1/8: `lux = raw * 0.005572`
- 1/4: `lux = raw * 0.011144`
- 1:   `lux = raw * 0.044576`
- 2:   `lux = raw * 0.089152`

Based on VEML7700 datasheet calculations for 100ms integration time.

---

## Future Considerations

Potential features for future releases:
- Data logging to SD card
- Min/Max value tracking
- Calibration offset
- Graph view with historical data
- Multiple integration time options
- Custom gain compensation values
- Export measurements via serial

---

*Maintained by: Dr Mosfet*  
*Hardware: Flipper Zero*  
*Sensor: VEML7700 Ambient Light Sensor*
