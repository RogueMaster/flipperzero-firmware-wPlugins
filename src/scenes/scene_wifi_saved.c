#include "scene_wifi_saved.h"

#include "../api_caller.h"

#define SCENE_WIFI_SAVED_EMPTY "Nessuna rete salvata"

static void api_caller_scene_wifi_saved_item_callback(void* context, uint32_t index) {
    furi_assert(context);
    AppContext* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void api_caller_scene_wifi_saved_on_enter(void* context) {
    furi_assert(context);
    AppContext* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Reti salvate");

    if(app->wifi_history_count == 0) {
        submenu_add_item(
            app->submenu,
            SCENE_WIFI_SAVED_EMPTY,
            0,
            api_caller_scene_wifi_saved_item_callback,
            app);
    } else {
        for(uint8_t i = 0; i < app->wifi_history_count; i++) {
            submenu_add_item(
                app->submenu,
                app->wifi_history[i].ssid,
                i,
                api_caller_scene_wifi_saved_item_callback,
                app);
        }
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, ApiCallerViewWifiScan);
}

bool api_caller_scene_wifi_saved_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    AppContext* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        uint32_t index = event.event;
        if(index < app->wifi_history_count) {
            snprintf(app->ssid, sizeof(app->ssid), "%s", app->wifi_history[index].ssid);
            snprintf(
                app->password, sizeof(app->password), "%s", app->wifi_history[index].password);
            // Skip the password input in the connect scene
            scene_manager_set_scene_state(app->scene_manager, ApiCallerSceneWifiConnect, 1);
            scene_manager_next_scene(app->scene_manager, ApiCallerSceneWifiConnect);
            consumed = true;
        }
    }

    return consumed;
}

void api_caller_scene_wifi_saved_on_exit(void* context) {
    furi_assert(context);
    AppContext* app = context;
    submenu_reset(app->submenu);
}
