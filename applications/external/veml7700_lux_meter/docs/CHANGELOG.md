V1.1 Released: Application Feature & Flow Updates
🚀 Application Flow & User Interface Enhancements
This update introduces a new application startup flow and significant improvements to the screen layouts for a better user experience.

New Startup Screen: The application now defaults to the Settings screen upon launch, with the cursor immediately positioned on a new "Start" option.

Implementation Details: This was achieved by adding SettingsItem_Start to the settings enum and a started field to the VEML7700App structure.

Simple Activation: Pressing OK on the "Start" option sets started = true and immediately transitions to the main measurement screen.

The main application loop is now guarded, only performing sensor initialization (init) and data reading (read) when started is true, ensuring the sensor is active only after user confirmation.

Settings Menu Polish: The draw_settings_screen function was updated to include the "Start" option, limit the visible items to 2, and add a side scrollbar for better navigation.

📊 Measurement & Sensor Updates
Improved Main Screen Layout: The draw_main_screen function was redesigned for clarity:

The primary lux value is now displayed in a large font.

The unit "lx" is positioned clearly below the value.

The key legend was removed from the main screen to reduce clutter.

New Measurement Channel: Added full support for the WHITE light channel (WHITE_MEAS_RESULT_REG), which is now selectable within the settings menu.
