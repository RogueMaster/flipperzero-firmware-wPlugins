
#include "../../app.h"
#include "../../structs.h"
#include "../../uart/uart.h"
#include "../../utils/trimwhitespace/trimwhitespace.h"
#include <furi.h>
#include <furi_hal.h>
#include <gui/modules/submenu.h>

void submenu_callback_select_wifi(void* context, uint32_t index) {
    FURI_LOG_T(TAG, "submenu_callback_select_wifi");
    furi_assert(context);
    App* app = context;

    // Set the selected SSID
    strncpy(
        app->wifi_list.selected_ssid,
        app->wifi_list.networks[index].ssid,
        sizeof(app->wifi_list.selected_ssid) - 1);

    for(size_t i = 0; i < MAX_WIFI_CREDENTIALS; i++) {
        if(strcmp(app->wifi_list.selected_ssid, app->csv_networks[i].ssid) == 0) {
            strncpy(
                app->wifi_list.password_ssid,
                app->csv_networks[i].password,
                sizeof(app->wifi_list.password_ssid) - 1);
        }
    }

    // Handle the custom event to move to the connect details scene
    scene_manager_handle_custom_event(app->scene_manager, AppEvent_Connect_Details);
}

void submenu_callback_no_wifi(void* context, uint32_t index) {
    FURI_LOG_T(TAG, "submenu_callback_no_wifi");
    furi_assert(index);
    App* app = context;
    scene_manager_search_and_switch_to_previous_scene(app->scene_manager, MainMenu);
}

void scene_on_enter_connect(void* context) {
    FURI_LOG_T(TAG, "scene_on_enter_connect");
    App* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Available WiFi's");
    submenu_add_item(app->submenu, "Scanning ...", 0, NULL, NULL);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppView_Connect);
    // reset the wifi selected and ssid
    app->wifi_list.selected_ssid[0] = '\0';
    app->wifi_list.password_ssid[0] = '\0';

    // Add a dummy menu item indicating that the search is in progress

    // Run the listWiFiCommand to get the available WiFi networks
    if(!listWiFiCommand(app->uart, NULL)) {
        FURI_LOG_E(TAG, "Failed to list WiFi networks");
        submenu_reset(app->submenu);
        submenu_add_item(
            app->submenu, "Failed to list WiFi networks", 0, submenu_callback_no_wifi, app);
        return;
    }

    submenu_change_item_label(app->submenu, 0, "parsing list ...");

    // Clear the submenu and add the actual WiFi networks
    if(app->wifi_list.networks[0].ssid[0] != '\0') {
        submenu_reset(app->submenu);
        submenu_set_header(app->submenu, "Available WiFi's");

        // Add WiFi networks to submenu
        for(size_t i = 0; i < MAX_WIFI_NETWORKS && app->wifi_list.networks[i].ssid[0] != '\0';
            i++) {
            char display_name[MAX_SSID_LENGTH + 12]; // Extra space for "(connected)"

            // Trim SSIDs
            trim_whitespace(app->wifi_list.networks[i].ssid);
            trim_whitespace(app->wifi_list.connected_ssid);

            int comparison_result =
                strcmp(app->wifi_list.networks[i].ssid, app->wifi_list.connected_ssid);

            if(comparison_result == 0) {
                snprintf(
                    display_name,
                    sizeof(display_name),
                    "%s (connected)",
                    app->wifi_list.networks[i].ssid);
            } else {
                snprintf(
                    display_name, sizeof(display_name), "%s", app->wifi_list.networks[i].ssid);
            }
            submenu_add_item(app->submenu, display_name, i, submenu_callback_select_wifi, app);
        }
    }
}

bool scene_on_event_connect(void* context, SceneManagerEvent event) {
    FURI_LOG_T(TAG, "scene_on_event_connect");
    App* app = context;
    bool consumed = false;
    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        case AppEvent_Connect_Details:
            // Add log before switching to Connect
            FURI_LOG_I(TAG, "Switching to Connect");
            scene_manager_next_scene(app->scene_manager, Connect_Details);
            consumed = true;
            break;
        }
        break;

    default: // eg. SceneManagerEventTypeBack, SceneManagerEventTypeTick
        consumed = false;
        break;
    }
    return consumed;
}

void scene_on_exit_connect(void* context) {
    FURI_LOG_T(TAG, "scene_on_exit_connect");
    App* app = context;
    for(size_t i = 0; i < MAX_WIFI_NETWORKS && app->wifi_list.networks[i].ssid[0] != '\0'; i++) {
    }

    submenu_reset(app->submenu);
}
