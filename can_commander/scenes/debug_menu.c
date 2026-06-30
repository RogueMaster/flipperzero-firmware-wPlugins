#include "../can_commander.h"

#include <stdio.h>
#include <string.h>

typedef enum {
    DebugConnect = 0,
    DebugBusConfig,
    DebugBusFilters,
    DebugWifiSettings,
    DebugLedBrightness,
    DebugStats,
    DebugPing,
    DebugGetInfo,
} DebugMenuIndex;

static void cancommander_scene_debug_menu_brightness_changed(VariableItem* item) {
    App* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    uint8_t brightness = (uint8_t)(index + 1U);

    app->led_brightness = brightness;

    char text[8] = {0};
    snprintf(text, sizeof(text), "%u", (unsigned)brightness);
    variable_item_set_current_value_text(item, text);

    if(app_require_connected(app)) {
        CcStatusCode status = CcStatusUnknown;
        cc_client_led_set_brightness(app->client, brightness, &status);
    }
}

static void cancommander_scene_debug_menu_enter_callback(void* context, uint32_t index) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void cancommander_scene_debug_menu_on_enter(void* context) {
    App* app = context;

    variable_item_list_reset(app->var_list);
    variable_item_list_set_header(app->var_list, "Settings");

    /* Action items: num_values=0 means no left/right, just clickable */
    variable_item_list_add(app->var_list, "Connect/Reconnect", 0, NULL, app);
    variable_item_list_add(app->var_list, "Bus Config", 0, NULL, app);
    variable_item_list_add(app->var_list, "Bus Filters", 0, NULL, app);
    variable_item_list_add(app->var_list, "WiFi Settings", 0, NULL, app);

    /* LED Brightness: 10 values (1-10) */
    VariableItem* brightness_item = variable_item_list_add(
        app->var_list,
        "LED Brightness",
        10,
        cancommander_scene_debug_menu_brightness_changed,
        app);
    variable_item_set_current_value_index(brightness_item, app->led_brightness - 1U);
    char brightness_text[8] = {0};
    snprintf(brightness_text, sizeof(brightness_text), "%u", (unsigned)app->led_brightness);
    variable_item_set_current_value_text(brightness_item, brightness_text);

    /* More action items */
    variable_item_list_add(app->var_list, "Stats", 0, NULL, app);
    variable_item_list_add(app->var_list, "Ping", 0, NULL, app);
    variable_item_list_add(app->var_list, "Get Info", 0, NULL, app);

    variable_item_list_set_enter_callback(
        app->var_list, cancommander_scene_debug_menu_enter_callback, app);

    variable_item_list_set_selected_item(
        app->var_list,
        scene_manager_get_scene_state(app->scene_manager, cancommander_scene_debug_menu));

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewVarList);
}

bool cancommander_scene_debug_menu_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type != SceneManagerEventTypeCustom) {
        return false;
    }

    scene_manager_set_scene_state(app->scene_manager, cancommander_scene_debug_menu, event.event);

    switch(event.event) {
    case DebugConnect:
        if(app_connect(app, true)) {
            app_verify_firmware_version(app);
            if(app->fw_version_warn_pending) {
                app->fw_version_warn_shown = true;
            } else {
                app_set_status(app, "Connected at %lu baud", (unsigned long)CC_UART_BAUD);
            }
        } else {
            app_set_status(app, "Connection failed");
        }
        scene_manager_next_scene(app->scene_manager, cancommander_scene_status);
        return true;

    case DebugBusConfig:
        scene_manager_next_scene(app->scene_manager, cancommander_scene_bus_cfg_menu);
        return true;

    case DebugBusFilters:
        scene_manager_next_scene(app->scene_manager, cancommander_scene_bus_filter_menu);
        return true;

    case DebugWifiSettings:
        scene_manager_next_scene(app->scene_manager, cancommander_scene_wifi_menu);
        return true;

    case DebugLedBrightness:
        /* Handled by change callback, no action on enter */
        return true;

    case DebugStats:
        app_action_stats(app);
        scene_manager_next_scene(
            app->scene_manager,
            app->connected ? cancommander_scene_monitor : cancommander_scene_status);
        return true;

    case DebugPing:
        app_action_ping(app);
        scene_manager_next_scene(app->scene_manager, cancommander_scene_status);
        return true;

    case DebugGetInfo:
        app_action_get_info(app);
        scene_manager_next_scene(
            app->scene_manager,
            app->connected ? cancommander_scene_monitor : cancommander_scene_status);
        return true;

    default:
        return false;
    }
}

void cancommander_scene_debug_menu_on_exit(void* context) {
    App* app = context;
    variable_item_list_reset(app->var_list);
}
