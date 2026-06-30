#pragma once
#include "settings_def.h"
#include "settings_ui.h"

// Full structure definition
typedef struct SettingsUIContext SettingsUIContext;

typedef struct {
    SettingsUIContext* settings_ui_context;
    SettingKey key;
} VariableItemContext;

struct SettingsUIContext {
    Settings* settings;
    SendUartCommandCallback send_uart_command;
    SwitchToViewCallback switch_to_view;
    ShowConfirmationViewCallback show_confirmation_view;
    void* context;
    VariableItemContext item_contexts[SETTINGS_COUNT];
};
