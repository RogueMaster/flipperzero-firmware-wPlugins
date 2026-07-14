#include "scenes.h"
#include "../flipper_spi_terminal_app.h"
#include "../flipper_spi_terminal_config.h"
#include "../flipper_spi_terminal.h"

// Value changed callback generation
#define ADD_CONFIG_ENTRY(                                                                           \
    label, helpText, name, type, defaultValue, valueIndexFunc, field, valuesCount, values, strings) \
    static void name##_changed(VariableItem* item) {                                                \
        FlipperSPITerminalApp* app = (variable_item_get_context)(item);                             \
        uint8_t index = (variable_item_get_current_value_index)(item);                              \
        (variable_item_set_current_value_text)(item, spi_config_##name##_strings[index]);           \
        app->config.field = spi_config_##name##_values[index];                                      \
    }
#include "../flipper_spi_terminal_config_declarations.h"
#undef ADD_CONFIG_ENTRY

static void flipper_spi_terminal_scene_config_alloc_spi_config_items(FlipperSPITerminalApp* app) {
    VariableItem* item;
    size_t value_index;

    UNUSED(item);
    UNUSED(value_index);

// Add item generation
#define ADD_CONFIG_ENTRY(                                                                           \
    label, helpText, name, type, defaultValue, valueIndexFunc, field, valuesCount, values, strings) \
    item = variable_item_list_add(                                                                  \
        app->config_screen.view,                                                                    \
        label,                                                                                      \
        COUNT_OF(spi_config_##name##_values),                                                       \
        name##_changed,                                                                             \
        app);                                                                                       \
    value_index = (valueIndexFunc)(app->config.field, spi_config_##name##_values, valuesCount);     \
    variable_item_set_current_value_index(item, value_index);                                       \
    name##_changed(item);
#include "../flipper_spi_terminal_config_declarations.h"
#undef ADD_CONFIG_ENTRY
}

#define SPI_TERM_HELP_TEXT_INDEX_OFFSET 1

// Help strings
const char* const flipper_spi_terminal_scene_config_help_strings[] = {
#define ADD_CONFIG_ENTRY(                                                                           \
    label, helpText, name, type, defaultValue, valueIndexFunc, field, valuesCount, values, strings) \
    label "\n\n" helpText,
#include "../flipper_spi_terminal_config_declarations.h"
#undef ADD_CONFIG_ENTRY
};

static void flipper_spi_terminal_scene_config_on_center_button(void* context, uint32_t index) {
    SPI_TERM_CONTEXT_TO_APP(context);

    if(index < SPI_TERM_HELP_TEXT_INDEX_OFFSET ||
       index >= SPI_TERM_HELP_TEXT_INDEX_OFFSET +
                    COUNT_OF(flipper_spi_terminal_scene_config_help_strings)) {
        return;
    }

    const char* text =
        flipper_spi_terminal_scene_config_help_strings[index - SPI_TERM_HELP_TEXT_INDEX_OFFSET];
    SPI_TERM_LOG_T("Setting help text to '%s'", text);
    app->config_screen.help_string = text;

    scene_manager_next_scene(app->scene_manager, FlipperSPITerminalAppSceneConfigHelp);
}

void flipper_spi_terminal_scene_config_alloc(FlipperSPITerminalApp* app) {
    furi_check(app);

    app->config_screen.view = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        FlipperSPITerminalAppSceneConfig,
        variable_item_list_get_view(app->config_screen.view));

    // Help
    variable_item_list_add(app->config_screen.view, "Press OK for help", 0, NULL, NULL);

    // MAKE SURE TO ADJUST THE SPI_TERM_HELP_TEXT_INDEX_OFFSET VALUE, IF YOU ADD A VALUE BEFORE THIS COMMENT!

    // Add all auto generated Config values
    flipper_spi_terminal_scene_config_alloc_spi_config_items(app);

    variable_item_list_set_enter_callback(
        app->config_screen.view, flipper_spi_terminal_scene_config_on_center_button, app);
}

void flipper_spi_terminal_scene_config_free(FlipperSPITerminalApp* app) {
    furi_check(app);

    view_dispatcher_remove_view(app->view_dispatcher, FlipperSPITerminalAppSceneConfig);
    variable_item_list_free(app->config_screen.view);
}

void flipper_spi_terminal_scene_config_on_enter(void* context) {
    SPI_TERM_CONTEXT_TO_APP(context);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperSPITerminalAppSceneConfig);
}

bool flipper_spi_terminal_scene_config_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void flipper_spi_terminal_scene_config_on_exit(void* context) {
    SPI_TERM_CONTEXT_TO_APP(context);

    flipper_spi_terminal_config_save(&app->config);
}
