#include "flippass_scene_password_generator.h"

#include "../flippass.h"
#include "../plugins/flippass_password_gen_plugin.h"
#include "flippass_scene.h"
#include "flippass_scene_status.h"

#include <stdio.h>
#include <string.h>

enum {
    FlipPassPasswordGenRowLength = 0,
    FlipPassPasswordGenRowComplexity,
    FlipPassPasswordGenRowGenerate,
};

#define FLIPPASS_PASSWORD_GEN_ENTRY_PASSWORD_INDEX 2U
#define FLIPPASS_PASSWORD_GEN_FIELD_VALUE_INDEX    2U

static const uint16_t flippass_password_gen_lengths[] = {
    4U,
    8U,
    12U,
    16U,
    20U,
    24U,
    32U,
    48U,
    64U,
    128U,
    FLIPPASS_PASSWORD_GEN_MAX_LENGTH,
};

static const FlipPassPasswordGenCharset flippass_password_gen_charset_options[] = {
    FlipPassPasswordGenCharsetUtf8,
    FlipPassPasswordGenCharsetLayout,
    FlipPassPasswordGenCharsetFull,
    FlipPassPasswordGenCharsetAlnum,
    FlipPassPasswordGenCharsetAlpha,
    FlipPassPasswordGenCharsetSymbols,
    FlipPassPasswordGenCharsetHex,
    FlipPassPasswordGenCharsetNumeric,
    FlipPassPasswordGenCharsetOctal,
    FlipPassPasswordGenCharsetBinary,
};

static const char* flippass_password_gen_charset_text(FlipPassPasswordGenCharset charset) {
    switch(charset) {
    case FlipPassPasswordGenCharsetUtf8:
        return "UTF-8";
    case FlipPassPasswordGenCharsetLayout:
        return "Layout";
    case FlipPassPasswordGenCharsetAlnum:
        return "Alnum";
    case FlipPassPasswordGenCharsetAlpha:
        return "Alpha";
    case FlipPassPasswordGenCharsetSymbols:
        return "Symbols";
    case FlipPassPasswordGenCharsetNumeric:
        return "Decimal";
    case FlipPassPasswordGenCharsetHex:
        return "Hex";
    case FlipPassPasswordGenCharsetOctal:
        return "Octal";
    case FlipPassPasswordGenCharsetBinary:
        return "Binary";
    case FlipPassPasswordGenCharsetFull:
    default:
        return "Full";
    }
}

static uint8_t flippass_password_gen_length_to_index(uint16_t length) {
    uint8_t default_index = 0U;
    for(uint8_t index = 0U; index < COUNT_OF(flippass_password_gen_lengths); index++) {
        if(flippass_password_gen_lengths[index] == FLIPPASS_PASSWORD_GEN_DEFAULT_LENGTH) {
            default_index = index;
        }
        if(length == flippass_password_gen_lengths[index]) {
            return index;
        }
    }
    return default_index;
}

static uint8_t flippass_password_gen_charset_to_index(FlipPassPasswordGenCharset charset) {
    uint8_t default_index = 0U;
    for(uint8_t index = 0U; index < COUNT_OF(flippass_password_gen_charset_options); index++) {
        if(flippass_password_gen_charset_options[index] == FLIPPASS_PASSWORD_GEN_DEFAULT_CHARSET) {
            default_index = index;
        }
        if(charset == flippass_password_gen_charset_options[index]) {
            return index;
        }
    }
    return default_index;
}

static void flippass_password_gen_normalize_settings(App* app) {
    if(app->password_gen_charset > FLIPPASS_PASSWORD_GEN_CHARSET_MAX) {
        app->password_gen_charset = FLIPPASS_PASSWORD_GEN_DEFAULT_CHARSET;
    }
    if(app->password_gen_length == 0U ||
       app->password_gen_length > FLIPPASS_PASSWORD_GEN_MAX_LENGTH) {
        app->password_gen_length = FLIPPASS_PASSWORD_GEN_DEFAULT_LENGTH;
    }
}

static const FlipPassPasswordGenPluginV1*
    flippass_password_gen_plugin_load(App* app, FuriString* error) {
    const FlipperAppPluginDescriptor* descriptor = flippass_module_ensure(
        app,
        FlipPassModuleSlotPasswordGen,
        NULL,
        FLIPPASS_PASSWORD_GEN_PLUGIN_APP_ID,
        FLIPPASS_PASSWORD_GEN_PLUGIN_API_VERSION,
        error);
    if(descriptor == NULL || descriptor->entry_point == NULL) {
        return NULL;
    }

    const FlipPassPasswordGenPluginV1* plugin = descriptor->entry_point;
    if(plugin->api_version != FLIPPASS_PASSWORD_GEN_PLUGIN_API_VERSION || plugin->begin == NULL ||
       plugin->finish == NULL || plugin->abort == NULL) {
        furi_string_set_str(error, "FlipPass password generator plugin has an incompatible API.");
        return NULL;
    }

    return plugin;
}

static FlipPassPasswordGenPluginRequestV1 flippass_password_gen_make_request(const App* app) {
    FlipPassPasswordGenPluginRequestV1 request = {
        .api_version = FLIPPASS_PASSWORD_GEN_PLUGIN_API_VERSION,
        .target = app->password_gen_target,
        .charset = app->password_gen_charset,
        .length = app->password_gen_length,
        .keyboard_layout_path = (app->keyboard_layout_path != NULL) ?
                                    furi_string_get_cstr(app->keyboard_layout_path) :
                                    NULL,
    };

    if(request.length == 0U || request.length > FLIPPASS_PASSWORD_GEN_MAX_LENGTH) {
        request.length = FLIPPASS_PASSWORD_GEN_DEFAULT_LENGTH;
    }
    if(request.charset > FLIPPASS_PASSWORD_GEN_CHARSET_MAX) {
        request.charset = FLIPPASS_PASSWORD_GEN_DEFAULT_CHARSET;
    }
    return request;
}

static void flippass_password_gen_cleanup_plugin(App* app) {
    const FlipperAppPluginDescriptor* descriptor =
        app->module_loader.slot[FlipPassModuleSlotPasswordGen].descriptor;
    if(descriptor != NULL && descriptor->entry_point != NULL) {
        const FlipPassPasswordGenPluginV1* plugin = descriptor->entry_point;
        if(plugin->abort != NULL) {
            plugin->abort();
        }
    }
    flippass_module_unload(app, FlipPassModuleSlotPasswordGen);
}

static void flippass_password_gen_clear_target(App* app) {
    app->password_gen_target = FlipPassPasswordGenTargetNone;
}

void flippass_password_generator_prepare(App* app, FlipPassPasswordGenTarget target) {
    furi_assert(app);

    app->password_gen_target = target;
    flippass_password_gen_normalize_settings(app);
    app->password_gen_selected_index = FlipPassPasswordGenRowLength;
}

static void
    flippass_password_gen_apply_result(App* app, FlipPassPasswordGenPluginResultV1* result) {
    if(app->password_gen_target == FlipPassPasswordGenTargetEntryPassword) {
        snprintf(
            app->editor_entry_password, sizeof(app->editor_entry_password), "%s", result->password);
        app->editor_selected_index = FLIPPASS_PASSWORD_GEN_ENTRY_PASSWORD_INDEX;
    } else if(app->password_gen_target == FlipPassPasswordGenTargetProtectedCustomFieldValue) {
        snprintf(
            app->editor_custom_field_value,
            sizeof(app->editor_custom_field_value),
            "%s",
            result->password);
        app->editor_selected_index = FLIPPASS_PASSWORD_GEN_FIELD_VALUE_INDEX;
    }

    memzero(result->password, sizeof(result->password));
}

static void
    flippass_password_gen_show_error(App* app, const char* message, uint32_t return_scene) {
    flippass_scene_status_show(
        app,
        "Generate Failed",
        message != NULL && message[0] != '\0' ? message : "Password generation failed.",
        return_scene);
    scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
}

static bool flippass_password_gen_generate_now(App* app) {
    FuriString* error = furi_string_alloc();
    const FlipPassPasswordGenPluginV1* plugin = flippass_password_gen_plugin_load(app, error);
    FlipPassPasswordGenPluginRequestV1 request = flippass_password_gen_make_request(app);
    FlipPassPasswordGenPluginResultV1 result = {0};
    bool ok = false;

    if(plugin != NULL && plugin->begin(&request, error) && plugin->finish(&result, error)) {
        flippass_password_gen_apply_result(app, &result);
        ok = true;
    }

    flippass_password_gen_cleanup_plugin(app);

    if(ok) {
        flippass_password_gen_clear_target(app);
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, FlipPassScene_Editor);
    } else {
        flippass_password_gen_show_error(
            app, furi_string_get_cstr(error), FlipPassScene_PasswordGenerator);
    }

    memzero(&result, sizeof(result));
    furi_string_free(error);
    return ok;
}

static void flippass_password_gen_length_change_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);
    char text[8];

    app->password_gen_length = flippass_password_gen_lengths[index];
    snprintf(text, sizeof(text), "%u", (unsigned int)app->password_gen_length);
    variable_item_set_current_value_text(item, text);
}

static void flippass_password_gen_charset_change_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);

    app->password_gen_charset = flippass_password_gen_charset_options[index];
    variable_item_set_current_value_text(
        item, flippass_password_gen_charset_text(app->password_gen_charset));
}

static void flippass_password_gen_enter_callback(void* context, uint32_t index) {
    App* app = context;

    app->password_gen_selected_index = index;
    view_dispatcher_send_custom_event(app->view_dispatcher, index + 1U);
}

static void flippass_password_gen_build_form(App* app) {
    variable_item_list_reset(app->variable_item_list);

    VariableItem* length = variable_item_list_add(
        app->variable_item_list,
        "Length",
        COUNT_OF(flippass_password_gen_lengths),
        flippass_password_gen_length_change_callback,
        app);
    variable_item_set_current_value_index(
        length, flippass_password_gen_length_to_index(app->password_gen_length));
    flippass_password_gen_length_change_callback(length);

    VariableItem* complexity = variable_item_list_add(
        app->variable_item_list,
        "Complexity",
        COUNT_OF(flippass_password_gen_charset_options),
        flippass_password_gen_charset_change_callback,
        app);
    variable_item_set_current_value_index(
        complexity, flippass_password_gen_charset_to_index(app->password_gen_charset));
    flippass_password_gen_charset_change_callback(complexity);

    VariableItem* generate =
        variable_item_list_add(app->variable_item_list, "Generate", 1U, NULL, app);
    variable_item_set_current_value_text(generate, " ");

    variable_item_list_set_enter_callback(
        app->variable_item_list, flippass_password_gen_enter_callback, app);
    if(app->password_gen_selected_index > FlipPassPasswordGenRowGenerate) {
        app->password_gen_selected_index = FlipPassPasswordGenRowGenerate;
    }
    variable_item_list_set_selected_item(
        app->variable_item_list, (uint8_t)app->password_gen_selected_index);
}

void flippass_scene_password_generator_on_enter(void* context) {
    App* app = context;

    flippass_ui_ensure_variable_item_list(app);
    flippass_password_gen_build_form(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewVariableItemList);
}

bool flippass_scene_password_generator_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        flippass_password_gen_clear_target(app);
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, FlipPassScene_Editor);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        const uint32_t index = event.event - 1U;
        if(index == FlipPassPasswordGenRowGenerate) {
            flippass_save_settings(app);
            flippass_password_gen_generate_now(app);
            return true;
        }
        return true;
    }

    return false;
}

void flippass_scene_password_generator_on_exit(void* context) {
    App* app = context;
    if(app->variable_item_list != NULL) {
        app->password_gen_selected_index =
            variable_item_list_get_selected_item_index(app->variable_item_list);
        variable_item_list_reset(app->variable_item_list);
    }
}
