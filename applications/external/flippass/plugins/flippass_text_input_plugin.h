#pragma once

#include <furi.h>
#include <flipper_application/flipper_application.h>

#include "../mntminput/text_input.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FLIPPASS_TEXT_INPUT_PLUGIN_APP_ID      "flippass_text_input"
#define FLIPPASS_TEXT_INPUT_PLUGIN_API_VERSION 1u

typedef struct {
    uint32_t api_version;
    void* (*alloc)(void);
    void (*free)(void* instance);
    void (*reset)(void* instance);
    View* (*get_view)(void* instance);
    void (*set_result_callback)(
        void* instance,
        TextInputCallback callback,
        void* callback_context,
        char* text_buffer,
        size_t text_buffer_size,
        bool clear_default_text);
    void (*set_minimum_length)(void* instance, size_t minimum_length);
    void (*set_validator)(
        void* instance,
        TextInputValidatorCallback callback,
        void* callback_context);
    void (*show_illegal_symbols)(void* instance, bool show);
    void (*set_is_password)(void* instance, bool is_password);
    void (*set_for_open)(void* instance, bool for_open);
    TextInputValidatorCallback (*get_validator_callback)(void* instance);
    void* (*get_validator_callback_context)(void* instance);
    void (*set_header_text)(void* instance, const char* text);
} FlipPassTextInputPluginV1;

const FlipperAppPluginDescriptor* flippass_text_input_plugin_ep(void);

#ifdef __cplusplus
}
#endif
