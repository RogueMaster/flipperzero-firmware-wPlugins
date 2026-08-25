#include "scene_wifi.h"

#include "../api_caller.h"
#include "../wifi/wifi_manager.h"

typedef enum {
    WifiMenuItemScan,
    WifiMenuItemSaved,
    WifiMenuItemStatus,
    WifiMenuItemDisconnect,
} WifiMenuItem;

static void api_caller_scene_wifi_item_callback(void* context, uint32_t index) {
    furi_assert(context);
    AppContext* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void api_caller_scene_wifi_on_enter(void* context) {
    furi_assert(context);
    AppContext* app = context;

    variable_item_list_reset(app->var_item_list_wifi);

    variable_item_list_add(
        app->var_item_list_wifi, locale_get(app, LocKeyWifiScanNetworks), 0, NULL, NULL);
    variable_item_list_add(
        app->var_item_list_wifi, locale_get(app, LocKeyWifiSavedNetworks), 0, NULL, NULL);

    bool connected = wifi_manager_is_connected(app);
    char ssid[64];
    snprintf(ssid, sizeof(ssid), "%s", locale_get(app, LocKeyWifiDisconnected));
    if(connected) {
        wifi_manager_get_ssid(app, ssid, sizeof(ssid));
    }
    VariableItem* item = variable_item_list_add(
        app->var_item_list_wifi, locale_get(app, LocKeyWifiConnectedTo), 1, NULL, NULL);
    variable_item_set_current_value_index(item, 0);
    variable_item_set_current_value_text(item, ssid);

    variable_item_list_add(
        app->var_item_list_wifi, locale_get(app, LocKeyWifiDisconnect), 0, NULL, NULL);

    variable_item_list_set_enter_callback(
        app->var_item_list_wifi, api_caller_scene_wifi_item_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, ApiCallerViewWifiMenu);
}

bool api_caller_scene_wifi_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    AppContext* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case WifiMenuItemScan:
            scene_manager_next_scene(app->scene_manager, ApiCallerSceneWifiScan);
            consumed = true;
            break;
        case WifiMenuItemSaved:
            scene_manager_next_scene(app->scene_manager, ApiCallerSceneWifiSaved);
            consumed = true;
            break;
        case WifiMenuItemDisconnect:
            wifi_manager_disconnect(app);
            // Re-enter this scene to refresh the connection status
            scene_manager_search_and_switch_to_another_scene(
                app->scene_manager, ApiCallerSceneWifi);
            consumed = true;
            break;
        default:
            break;
        }
    }

    return consumed;
}

void api_caller_scene_wifi_on_exit(void* context) {
    furi_assert(context);
    AppContext* app = context;
    variable_item_list_reset(app->var_item_list_wifi);
}
