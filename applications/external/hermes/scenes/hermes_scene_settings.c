#include "../hermes_i.h"

static const char* const bool_names[] = {"OFF", "ON"};

static void hermes_settings_port_changed(VariableItem* item) {
    HermesApp* app = variable_item_get_context(item);
    app->port = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, hermes_port_pins(app->port));
}

static void hermes_settings_tx_changed(VariableItem* item) {
    HermesApp* app = variable_item_get_context(item);
    app->tx_enabled = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, bool_names[app->tx_enabled ? 1 : 0]);
}

static void hermes_settings_enter_changed(VariableItem* item) {
    HermesApp* app = variable_item_get_context(item);
    app->enter_mode = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, uart_tap_enter_name(app->enter_mode));
}

static void hermes_settings_echo_changed(VariableItem* item) {
    HermesApp* app = variable_item_get_context(item);
    app->local_echo = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, bool_names[app->local_echo ? 1 : 0]);
}

static void hermes_settings_logging_changed(VariableItem* item) {
    HermesApp* app = variable_item_get_context(item);
    app->logging = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, bool_names[app->logging ? 1 : 0]);
}

static void hermes_settings_sound_changed(VariableItem* item) {
    HermesApp* app = variable_item_get_context(item);
    app->sound = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, bool_names[app->sound ? 1 : 0]);
}

static void hermes_settings_led_changed(VariableItem* item) {
    HermesApp* app = variable_item_get_context(item);
    app->led = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, bool_names[app->led ? 1 : 0]);
}

void hermes_scene_settings_on_enter(void* context) {
    HermesApp* app = context;
    VariableItemList* list = app->var_item_list;
    VariableItem* item;

    variable_item_list_reset(list);

    item =
        variable_item_list_add(list, "Port", HermesPortCount, hermes_settings_port_changed, app);
    variable_item_set_current_value_index(item, app->port);
    variable_item_set_current_value_text(item, hermes_port_pins(app->port));

    item = variable_item_list_add(list, "Transmit", 2, hermes_settings_tx_changed, app);
    variable_item_set_current_value_index(item, app->tx_enabled ? 1 : 0);
    variable_item_set_current_value_text(item, bool_names[app->tx_enabled ? 1 : 0]);

    item = variable_item_list_add(
        list, "Enter sends", UartTapEnterCount, hermes_settings_enter_changed, app);
    variable_item_set_current_value_index(item, app->enter_mode);
    variable_item_set_current_value_text(item, uart_tap_enter_name(app->enter_mode));

    item = variable_item_list_add(list, "Local echo", 2, hermes_settings_echo_changed, app);
    variable_item_set_current_value_index(item, app->local_echo ? 1 : 0);
    variable_item_set_current_value_text(item, bool_names[app->local_echo ? 1 : 0]);

    item = variable_item_list_add(list, "Log to SD", 2, hermes_settings_logging_changed, app);
    variable_item_set_current_value_index(item, app->logging ? 1 : 0);
    variable_item_set_current_value_text(item, bool_names[app->logging ? 1 : 0]);

    item = variable_item_list_add(list, "Sound", 2, hermes_settings_sound_changed, app);
    variable_item_set_current_value_index(item, app->sound ? 1 : 0);
    variable_item_set_current_value_text(item, bool_names[app->sound ? 1 : 0]);

    item = variable_item_list_add(list, "LED", 2, hermes_settings_led_changed, app);
    variable_item_set_current_value_index(item, app->led ? 1 : 0);
    variable_item_set_current_value_text(item, bool_names[app->led ? 1 : 0]);

    view_dispatcher_switch_to_view(app->view_dispatcher, HermesViewSettings);
}

bool hermes_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void hermes_scene_settings_on_exit(void* context) {
    HermesApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
