#include "text_input.h"
#include "../flippass.h"
#include "../plugins/flippass_text_input_plugin.h"

struct TextInput {
    void* instance;
    const FlipPassTextInputPluginV1* plugin;
};

static App* flippass_text_input_app = NULL;

void flippass_text_input_set_app(App* app) {
    flippass_text_input_app = app;
}

void flippass_text_input_clear_app(App* app) {
    if(flippass_text_input_app == app) {
        flippass_text_input_app = NULL;
    }
}

static const FlipPassTextInputPluginV1* flippass_text_input_plugin_get(FuriString* error) {
    if(flippass_text_input_app == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass text input plugin has no host context.");
        }
        return NULL;
    }

    const FlipperAppPluginDescriptor* descriptor = flippass_module_ensure(
        flippass_text_input_app,
        FlipPassModuleSlotTextInput,
        NULL,
        FLIPPASS_TEXT_INPUT_PLUGIN_APP_ID,
        FLIPPASS_TEXT_INPUT_PLUGIN_API_VERSION,
        error);
    if(descriptor == NULL || descriptor->entry_point == NULL) {
        if(error != NULL && furi_string_empty(error)) {
            furi_string_set_str(error, "FlipPass text input plugin is unavailable.");
        }
        return NULL;
    }

    const FlipPassTextInputPluginV1* plugin = descriptor->entry_point;
    if(plugin->api_version != FLIPPASS_TEXT_INPUT_PLUGIN_API_VERSION || plugin->alloc == NULL ||
       plugin->free == NULL || plugin->reset == NULL || plugin->get_view == NULL ||
       plugin->set_result_callback == NULL || plugin->set_minimum_length == NULL ||
       plugin->set_validator == NULL || plugin->show_illegal_symbols == NULL ||
       plugin->set_is_password == NULL || plugin->set_for_open == NULL ||
       plugin->get_validator_callback == NULL || plugin->get_validator_callback_context == NULL ||
       plugin->set_header_text == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass text input plugin has an incompatible API.");
        }
        flippass_module_unload(flippass_text_input_app, FlipPassModuleSlotTextInput);
        return NULL;
    }

    return plugin;
}

TextInput* text_input_alloc(void) {
    FuriString* error = furi_string_alloc();
    const FlipPassTextInputPluginV1* plugin = flippass_text_input_plugin_get(error);
    if(plugin == NULL) {
        if(flippass_text_input_app != NULL) {
            FLIPPASS_LOG_EVENT(
                flippass_text_input_app,
                "TEXT_INPUT_PLUGIN_LOAD_FAIL reason=%s",
                furi_string_get_cstr(error));
        }
        furi_string_free(error);
        return NULL;
    }
    furi_string_free(error);

    TextInput* text_input = malloc(sizeof(TextInput));
    if(text_input == NULL) {
        flippass_module_unload(flippass_text_input_app, FlipPassModuleSlotTextInput);
        return NULL;
    }

    text_input->plugin = plugin;
    text_input->instance = plugin->alloc();
    if(text_input->instance == NULL) {
        free(text_input);
        flippass_module_unload(flippass_text_input_app, FlipPassModuleSlotTextInput);
        return NULL;
    }

    return text_input;
}

void text_input_free(TextInput* text_input) {
    if(text_input == NULL) {
        return;
    }

    text_input->plugin->free(text_input->instance);
    free(text_input);
    if(flippass_text_input_app != NULL) {
        flippass_module_unload(flippass_text_input_app, FlipPassModuleSlotTextInput);
    }
}

void text_input_reset(TextInput* text_input) {
    if(text_input != NULL) {
        text_input->plugin->reset(text_input->instance);
    }
}

View* text_input_get_view(TextInput* text_input) {
    return (text_input != NULL) ? text_input->plugin->get_view(text_input->instance) : NULL;
}

void text_input_set_result_callback(
    TextInput* text_input,
    TextInputCallback callback,
    void* callback_context,
    char* text_buffer,
    size_t text_buffer_size,
    bool clear_default_text) {
    if(text_input != NULL) {
        text_input->plugin->set_result_callback(
            text_input->instance,
            callback,
            callback_context,
            text_buffer,
            text_buffer_size,
            clear_default_text);
    }
}

void text_input_set_minimum_length(TextInput* text_input, size_t minimum_length) {
    if(text_input != NULL) {
        text_input->plugin->set_minimum_length(text_input->instance, minimum_length);
    }
}

void text_input_set_validator(
    TextInput* text_input,
    TextInputValidatorCallback callback,
    void* callback_context) {
    if(text_input != NULL) {
        text_input->plugin->set_validator(text_input->instance, callback, callback_context);
    }
}

void text_input_show_illegal_symbols(TextInput* text_input, bool show) {
    if(text_input != NULL) {
        text_input->plugin->show_illegal_symbols(text_input->instance, show);
    }
}

void text_input_set_is_password(TextInput* text_input, bool is_password) {
    if(text_input != NULL) {
        text_input->plugin->set_is_password(text_input->instance, is_password);
    }
}

void text_input_set_for_open(TextInput* text_input, bool for_open) {
    if(text_input != NULL) {
        text_input->plugin->set_for_open(text_input->instance, for_open);
    }
}

TextInputValidatorCallback text_input_get_validator_callback(TextInput* text_input) {
    return (text_input != NULL) ?
               text_input->plugin->get_validator_callback(text_input->instance) :
               NULL;
}

void* text_input_get_validator_callback_context(TextInput* text_input) {
    return (text_input != NULL) ?
               text_input->plugin->get_validator_callback_context(text_input->instance) :
               NULL;
}

void text_input_set_header_text(TextInput* text_input, const char* text) {
    if(text_input != NULL) {
        text_input->plugin->set_header_text(text_input->instance, text);
    }
}
