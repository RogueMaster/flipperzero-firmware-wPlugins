# Ghost ESP App Settings Guide

## Overview
This document explains how to add new settings to the Ghost ESP application. The app uses a structured approach to handle settings with metadata, storage, and UI integration.

## Adding a New Setting

### 1. Define Setting Key
Add your setting to the `SettingKey` enum in `settings_def.h`:
```c
typedef enum {
    SETTING_RGB_MODE,
    SETTING_CHANNEL_HOP_DELAY,
    // Add your new setting here
    YOUR_NEW_SETTING,
    SETTINGS_COUNT  // Keep this last
} SettingKey;
```

### 2. Add Setting Storage
Add a field to the `Settings` struct in `settings_def.h`:
```c
typedef struct {
    uint8_t rgb_mode_index;
    // Add your new setting storage
    uint8_t your_new_setting_index;
} Settings;
```

### 3. Define Setting Values
If your setting has predefined values, add them in `settings_def.c`:
```c
// For enum-based settings
const char* const SETTING_VALUE_NAMES_YOUR_SETTING[] = {"Value1", "Value2", "Value3"};
```

### 4. Add Setting Metadata
Add metadata in `settings_def.c` `SETTING_METADATA` array:

#### For Regular Settings:
```c
[YOUR_NEW_SETTING] = {
    .name = "Your Setting Name",
    .data.setting = {
        .max_value = YOUR_SETTING_COUNT - 1,
        .value_names = SETTING_VALUE_NAMES_YOUR_SETTING,
        .uart_command = "setsetting -i X -v"  // If ESP needs to know about it
    },
    .is_action = false
},
```

#### For Action Buttons:
```c
[YOUR_NEW_ACTION] = {
    .name = "Your Action Name",
    .data.action = {
        .name = "Button Label",
        .command = "your_command",  // UART command if needed
        .callback = &your_callback_function  // Function to call when pressed
    },
    .is_action = true
},
```

### 5. Implement Setting Handler
Add case in `settings_set` function in `settings_ui.c`:
```c
case YOUR_NEW_SETTING:
    if(settings->your_new_setting_index != value) {
        settings->your_new_setting_index = value;
        changed = true;
    }
    break;
```

And in `settings_get`:
```c
case YOUR_NEW_SETTING:
    return settings->your_new_setting_index;
```

## Setting Types

### Regular Settings
- Used for configurable options
- Can have multiple values
- Automatically saved to storage
- Can trigger UART commands

Example:
```c
[SETTING_RGB_MODE] = {
    .name = "RGB Mode",
    .data.setting = {
        .max_value = RGB_MODE_COUNT - 1,
        .value_names = SETTING_VALUE_NAMES_RGB_MODE,
        .uart_command = "setsetting -i 1 -v"
    },
    .is_action = false
},
```

### Action Buttons
- Used for triggering actions
- Can have UART commands
- Can have callback functions
- Support confirmation dialogs

Example:
```c
[SETTING_CLEAR_LOGS] = {
    .name = "Clear Log Files",
    .data.action = {
        .name = "Clear Log Files",
        .command = NULL,
        .callback = &clear_log_files
    },
    .is_action = true
},
```

## Storage
Settings are automatically saved to the file specified by `GHOST_ESP_APP_SETTINGS_FILE` when changed. The storage system:
- Maintains settings between app launches
- Includes version control
- Handles file corruption
- Creates default settings if no file exists

## Best Practices
1. Always initialize new settings to 0 in defaults
2. Use enum for predefined values
3. Add proper validation in settings_set
4. Keep setting names clear and concise
5. Group related settings together
6. Document UART commands if used
7. Add NULL checks in callbacks
8. Use logging for debugging
9. Consider adding confirmation for destructive actions

## Example: Full Setting Implementation
Here's a complete example of adding a new setting:

```c
// In settings_def.h
typedef enum {
    SETTING_EXAMPLE_MODE,
    // ... other settings ...
    SETTINGS_COUNT
} SettingKey;

typedef enum {
    EXAMPLE_MODE_OFF,
    EXAMPLE_MODE_LOW,
    EXAMPLE_MODE_HIGH,
    EXAMPLE_MODE_COUNT
} ExampleMode;

// In settings_def.c
const char* const SETTING_VALUE_NAMES_EXAMPLE[] = {"Off", "Low", "High"};

[SETTING_EXAMPLE_MODE] = {
    .name = "Example Setting",
    .data.setting = {
        .max_value = EXAMPLE_MODE_COUNT - 1,
        .value_names = SETTING_VALUE_NAMES_EXAMPLE,
        .uart_command = "setsetting -i 9 -v"
    },
    .is_action = false
},
```