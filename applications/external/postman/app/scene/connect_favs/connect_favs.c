#include "../../app.h"
#include "../../structs.h"
#include "../../utils/trimwhitespace/trimwhitespace.h"
#include <furi.h>
#include <furi_hal.h>
#include <gui/modules/submenu.h>

void submenu_callback_select_fav_wifi(void* context, uint32_t index) {
    FURI_LOG_T(TAG, "submenu_callback_select_fav_wifi");
    furi_assert(context);
    App* app = context;

    // Set the selected SSID
    strncpy(
        app->wifi_list.selected_ssid,
        app->csv_networks[index].ssid,
        sizeof(app->wifi_list.selected_ssid) - 1);

    //Copy the csv list to the wifi list for connect details sync up

    for(size_t i = 0; i < app->csv_networks_count; i++) {
        strncpy(
            app->wifi_list.networks[i].ssid,
            app->csv_networks[i].ssid,
            sizeof(app->wifi_list.networks[i].ssid) - 1);
    }

    // Copy the selected password to the password ssid

    for(size_t i = 0; i < app->csv_networks_count; i++) {
        if(strcmp(app->wifi_list.selected_ssid, app->csv_networks[i].ssid) == 0) {
            strncpy(
                app->wifi_list.password_ssid,
                app->csv_networks[i].password,
                sizeof(app->wifi_list.password_ssid) - 1);
        }
    }

    // Handle the custom event to move to the connect details scene
    scene_manager_handle_custom_event(app->scene_manager, AppEvent_Connect_Favs);
}

void submenu_callback_no_fav_wifi(void* context, uint32_t index) {
    FURI_LOG_T(TAG, "submenu_callback_no_fav_wifi");
    furi_assert(index);
    App* app = context;
    scene_manager_search_and_switch_to_previous_scene(app->scene_manager, MainMenu);
}

void update_fav_wifi_menu(App* app) {
    FURI_LOG_T(TAG, "update_fav_wifi_menu");

    // Clear the submenu and add the actual WiFi networks
    if(app->csv_networks && app->csv_networks_count > 0 && app->csv_networks[0].ssid[0] != '\0') {
        submenu_reset(app->submenu);
        submenu_set_header(app->submenu, "Favorite WiFi's");

        // Add WiFi networks to submenu with updated state
        for(size_t i = 0; i < app->csv_networks_count && app->csv_networks[i].ssid[0] != '\0';
            i++) {
            char display_name[MAX_SSID_LENGTH + 12]; // Extra space for "(connected)"

            // Trim SSIDs
            trim_whitespace(app->csv_networks[i].ssid);
            trim_whitespace(app->wifi_list.connected_ssid);

            int comparison_result =
                strcmp(app->csv_networks[i].ssid, app->wifi_list.connected_ssid);

            if(comparison_result == 0) {
                snprintf(
                    display_name,
                    sizeof(display_name),
                    "%s (connected)",
                    app->csv_networks[i].ssid);
            } else {
                snprintf(display_name, sizeof(display_name), "%s", app->csv_networks[i].ssid);
            }
            submenu_add_item(app->submenu, display_name, i, submenu_callback_select_fav_wifi, app);
        }
    } else {
        submenu_add_item(
            app->submenu, "No favorite WiFi networks found", 0, submenu_callback_no_fav_wifi, app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, AppView_Connect_Favs);
}

void scene_on_enter_connect_favs(void* context) {
    FURI_LOG_T(TAG, "scene_on_enter_connect_favs");
    App* app = context;

    // Log the connected_ssid name

    FURI_LOG_D("DEBUG", "Connected to: %s", app->wifi_list.connected_ssid);

    // Update the fav wifi menu
    update_fav_wifi_menu(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, AppView_Connect_Favs);
}

bool scene_on_event_connect_favs(void* context, SceneManagerEvent event) {
    FURI_LOG_T(TAG, "scene_on_event_connect_favs");
    App* app = context;
    bool consumed = false;
    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        case AppEvent_Connect_Favs:
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

void scene_on_exit_connect_favs(void* context) {
    FURI_LOG_T(TAG, "scene_on_exit_connect_favs");
    App* app = context;
    for(size_t i = 0; i < app->csv_networks_count && app->csv_networks[i].ssid[0] != '\0'; i++) {
    }

    submenu_reset(app->submenu);
}
