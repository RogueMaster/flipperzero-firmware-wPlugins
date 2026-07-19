#include "../trident_i.h"

static const char* const uart_labels[] = {"13/14", "15/16"};
#define UART_COUNT 2

static const char* const cc1101_labels[] = {"Internal", "External"};
#define CC1101_COUNT 2

static const char* const on_off[] = {"OFF", "ON"};

const char* trident_uart_channel_label(uint8_t index) {
    return uart_labels[index % UART_COUNT];
}

static void uart_changed(VariableItem* item) {
    TridentApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.uart_channel = i;
    variable_item_set_current_value_text(item, uart_labels[i]);
}

static void cc1101_changed(VariableItem* item) {
    TridentApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.cc1101_device = i;
    variable_item_set_current_value_text(item, cc1101_labels[i]);
}

static void band_changed(VariableItem* item) {
    TridentApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.subghz_band = i;
    variable_item_set_current_value_text(item, trident_subghz_band_label(i));
}

static void autoscroll_changed(VariableItem* item) {
    TridentApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.autoscroll = i;
    variable_item_set_current_value_text(item, on_off[i]);
}

static void confirm_changed(VariableItem* item) {
    TridentApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.confirm_attacks = i;
    variable_item_set_current_value_text(item, on_off[i]);
}

static void sound_changed(VariableItem* item) {
    TridentApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.sound = i;
    variable_item_set_current_value_text(item, on_off[i]);
}

static void vibro_changed(VariableItem* item) {
    TridentApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.vibro = i;
    variable_item_set_current_value_text(item, on_off[i]);
}

static void led_changed(VariableItem* item) {
    TridentApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.led = i;
    variable_item_set_current_value_text(item, on_off[i]);
}

void trident_scene_settings_on_enter(void* context) {
    TridentApp* app = context;
    VariableItemList* list = app->var_item_list;
    VariableItem* item;

    variable_item_list_reset(list);

    item = variable_item_list_add(list, "ESP32 UART pins", UART_COUNT, uart_changed, app);
    variable_item_set_current_value_index(item, app->settings.uart_channel);
    variable_item_set_current_value_text(item, uart_labels[app->settings.uart_channel]);

    item = variable_item_list_add(list, "CC1101 radio", CC1101_COUNT, cc1101_changed, app);
    variable_item_set_current_value_index(item, app->settings.cc1101_device);
    variable_item_set_current_value_text(item, cc1101_labels[app->settings.cc1101_device]);

    item = variable_item_list_add(
        list, "Sub-GHz band", TRIDENT_SUBGHZ_BAND_COUNT, band_changed, app);
    variable_item_set_current_value_index(item, app->settings.subghz_band);
    variable_item_set_current_value_text(item, trident_subghz_band_label(app->settings.subghz_band));

    item = variable_item_list_add(list, "Autoscroll", 2, autoscroll_changed, app);
    variable_item_set_current_value_index(item, app->settings.autoscroll ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[app->settings.autoscroll ? 1 : 0]);

    item = variable_item_list_add(list, "Confirm attacks", 2, confirm_changed, app);
    variable_item_set_current_value_index(item, app->settings.confirm_attacks ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[app->settings.confirm_attacks ? 1 : 0]);

    item = variable_item_list_add(list, "Sound", 2, sound_changed, app);
    variable_item_set_current_value_index(item, app->settings.sound ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[app->settings.sound ? 1 : 0]);

    item = variable_item_list_add(list, "Vibrate", 2, vibro_changed, app);
    variable_item_set_current_value_index(item, app->settings.vibro ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[app->settings.vibro ? 1 : 0]);

    item = variable_item_list_add(list, "LED", 2, led_changed, app);
    variable_item_set_current_value_index(item, app->settings.led ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[app->settings.led ? 1 : 0]);

    view_dispatcher_switch_to_view(app->view_dispatcher, TridentViewVarList);
}

bool trident_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void trident_scene_settings_on_exit(void* context) {
    TridentApp* app = context;
    trident_settings_save(&app->settings); // persist across runs
    variable_item_list_reset(app->var_item_list);
}
