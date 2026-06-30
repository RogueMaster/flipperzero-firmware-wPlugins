#include "../xremote.h"
#include <lib/toolbox/value_index.h>

static const char* infrared_pin_text[] = {
    "Flipper",
    "2 (A7)",
    "Detect",
};

static const char* infrared_otg_text[] = {
    "OFF",
    "ON",
};

const char* const led_text[2] = {
    "OFF",
    "ON",
};
const uint32_t led_value[2] = {
    XRemoteLedOff,
    XRemoteLedOn,
};

const char* const loop_text[2] = {
    "OFF",
    "ON",
};
const uint32_t loop_value[2] = {
    XRemoteLoopOff,
    XRemoteLoopOn,
};

const char* const settings_text[2] = {
    "OFF",
    "ON",
};
const uint32_t settings_value[2] = {
    XRemoteSettingsOff,
    XRemoteSettingsOn,
};

static void xremote_scene_settings_set_ir_pin(VariableItem* item) {
    XRemote* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, infrared_pin_text[index]);
    app->ir_tx_pin = index;
    view_dispatcher_send_custom_event(
        app->view_dispatcher,
        xremote_custom_menu_event_pack(XRemoteCustomEventTypeIrGpioPinChanged, index));
}

static void xremote_scene_settings_set_ir_is_otg_enabled(VariableItem* item) {
    XRemote* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, infrared_otg_text[index]);
    app->ir_is_otg_enabled = index;
    view_dispatcher_send_custom_event(
        app->view_dispatcher,
        xremote_custom_menu_event_pack(XRemoteCustomEventTypeIrGpioOtgChanged, index));
}

static void xremote_scene_settings_set_led(VariableItem* item) {
    XRemote* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, led_text[index]);
    app->led = led_value[index];
}

static void xremote_scene_settings_set_save_settings(VariableItem* item) {
    XRemote* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, settings_text[index]);
    app->save_settings = settings_value[index];
}

static void xremote_scene_settings_set_loop(VariableItem* item) {
    XRemote* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, loop_text[index]);
    app->loop_transmit = loop_value[index];
}

static void xremote_scene_settings_set_ir_timing(VariableItem* item) {
    XRemote* app = variable_item_get_context(item);
    uint32_t index = variable_item_get_current_value_index(item);

    snprintf(app->ir_timing_char, 20, "%lu", (index * 100));
    variable_item_set_current_value_text(item, app->ir_timing_char);
    app->ir_timing = (index * 100);
}

static void xremote_scene_settings_set_sg_timing(VariableItem* item) {
    XRemote* app = variable_item_get_context(item);
    uint32_t index = variable_item_get_current_value_index(item);

    snprintf(app->sg_timing_char, 20, "%lu", (index * 100));
    variable_item_set_current_value_text(item, app->sg_timing_char);
    app->sg_timing = (index * 100);
}

void xremote_scene_settings_submenu_callback(void* context, uint32_t index) {
    XRemote* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void xremote_scene_settings_init(void* context) {
    XRemote* app = context;    
    VariableItem* item;
    uint8_t value_index;

    // LED Effects on/off
    item = variable_item_list_add(
        app->variable_item_list, "LED FX", 2, xremote_scene_settings_set_led, app);
    value_index = value_index_uint32(app->led, led_value, 2);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, led_text[value_index]);

    /* NEW: Loop saved command functionality */
    item = variable_item_list_add(
        app->variable_item_list, "Loop Transmit", 2, xremote_scene_settings_set_loop, app);
    value_index = value_index_uint32(app->loop_transmit, loop_value, 2);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, loop_text[value_index]);

    // Save Settings to File
    item = variable_item_list_add(
        app->variable_item_list, "Save Settings", 2, xremote_scene_settings_set_save_settings, app);
    value_index = value_index_uint32(app->save_settings, settings_value, 2);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, settings_text[value_index]);

    // Infrared GPIO Board
    item = variable_item_list_add(
        app->variable_item_list, 
        "External IR", 
        COUNT_OF(infrared_pin_text), 
        xremote_scene_settings_set_ir_pin, 
        app);
    value_index = app->ir_tx_pin;
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, infrared_pin_text[value_index]);
    
    // Infrared GPIO 5V
    item = variable_item_list_add(
        app->variable_item_list,
        "5V on IR GPIO",
        COUNT_OF(infrared_otg_text),
        xremote_scene_settings_set_ir_is_otg_enabled,
        app);
    
    if(app->ir_tx_pin < FuriHalInfraredTxPinMax) {
        value_index = app->ir_is_otg_enabled;
        variable_item_set_current_value_index(item, value_index);
        variable_item_set_current_value_text(item, infrared_otg_text[value_index]);
    } else {
        variable_item_set_values_count(item, 1);
        variable_item_set_current_value_index(item, 0);
        variable_item_set_current_value_text(item, "Auto");
    }

    // Set Infrared Timer
    item = variable_item_list_add(
        app->variable_item_list, "IR Time ms", 30, xremote_scene_settings_set_ir_timing, app);

    variable_item_set_current_value_index(item, (uint8_t)(app->ir_timing / 100));
    snprintf(app->ir_timing_char, 20, "%lu", app->ir_timing);
    variable_item_set_current_value_text(item, app->ir_timing_char);

    // Set SubGhz Timer
    item = variable_item_list_add(
        app->variable_item_list, "SubG. Time ms", 30, xremote_scene_settings_set_sg_timing, app);

    variable_item_set_current_value_index(item, (uint8_t)(app->sg_timing / 100));
    snprintf(app->sg_timing_char, 20, "%lu", app->sg_timing);
    variable_item_set_current_value_text(item, app->sg_timing_char);    
}

void xremote_scene_settings_on_enter(void* context) {
    XRemote* app = context;
    xremote_scene_settings_init(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, XRemoteViewIdSettings);
}

bool xremote_scene_settings_on_event(void* context, SceneManagerEvent event) {
    XRemote* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        const uint16_t custom_event_type = xremote_custom_menu_event_get_type(event.event);
        
        if (custom_event_type == XRemoteCustomEventTypeIrGpioPinChanged) {
            variable_item_list_reset(app->variable_item_list);
            xremote_scene_settings_init(app);
            xremote_ir_set_tx_pin(app);
        } else if(custom_event_type == XRemoteCustomEventTypeIrGpioOtgChanged) {
            xremote_ir_enable_otg(app, app->ir_is_otg_enabled);
        }
    }
    return consumed;
}

void xremote_scene_settings_on_exit(void* context) {
    XRemote* app = context;
    variable_item_list_set_selected_item(app->variable_item_list, 0);
    variable_item_list_reset(app->variable_item_list);
}