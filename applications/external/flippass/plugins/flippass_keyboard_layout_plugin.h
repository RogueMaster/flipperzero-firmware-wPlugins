#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <flipper_application/flipper_application.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FLIPPASS_KEYBOARD_LAYOUT_PLUGIN_APP_ID      "flippass_keyboard_layout"
#define FLIPPASS_KEYBOARD_LAYOUT_PLUGIN_API_VERSION 1u

typedef struct {
    uint32_t api_version;
    void* host_context;
    const char* (*get_current_layout_path)(void* host_context);
    bool (*set_current_layout_path)(void* host_context, const char* path, bool use_alt_numpad);
    void (*log)(void* host_context, const char* module_name, const char* message);
} FlipPassKeyboardLayoutHostApiV1;

typedef struct {
    uint32_t api_version;
    bool (*load_items)(const FlipPassKeyboardLayoutHostApiV1* host_api);
    uint32_t (*item_count)(void);
    const char* (*item_label)(uint32_t index);
    uint32_t (*selected_index)(const FlipPassKeyboardLayoutHostApiV1* host_api);
    bool (*apply_selection)(const FlipPassKeyboardLayoutHostApiV1* host_api, uint32_t index);
    void (*reset)(void);
} FlipPassKeyboardLayoutPluginV1;

const FlipperAppPluginDescriptor* flippass_keyboard_layout_plugin_ep(void);

#ifdef __cplusplus
}
#endif
