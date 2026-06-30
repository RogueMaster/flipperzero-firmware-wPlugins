#pragma once

#include "app_types.h"
#include "settings_def.h"
#include <gui/modules/variable_item_list.h>

// Function pointer types
typedef void (*SendUartCommandCallback)(const char* command, void* context);
typedef void (*SwitchToViewCallback)(void* context, uint32_t view_id);
typedef void (*ShowConfirmationViewCallback)(void* context, ConfirmationView* view);

// Forward declare the struct
typedef struct SettingsUIContext SettingsUIContext;
// Add this near the top of settings_ui.h
typedef struct {
    AppState* state;
    SettingKey key;
} SettingsConfirmContext;

// Function declarations
void clear_log_files(void* context);
void settings_setup_gui(VariableItemList* list, SettingsUIContext* context);
bool settings_set(Settings* settings, SettingKey key, uint8_t value, void* context);
uint8_t settings_get(const Settings* settings, SettingKey key);
bool settings_custom_event_callback(void* context, uint32_t event);
