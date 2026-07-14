#include "./connect_details.h"
#include "../../app.h"
#include "../../csv/csv_wifi/csv_wifi.h"
#include "../../structs.h"
#include "../../uart/uart.h"
#include <furi.h>
#include <furi_hal.h>
#include <gui/modules/submenu.h>

bool check_if_current_view_is_active(App* app) {
    // Check if board is On
    bool board_on = app->status == BOARD_CONNECTED_WIFI_ON;
    // Check if the selected ssid is the connected ssid
    bool connected_to_the_view =
        strcmp(app->wifi_list.selected_ssid, app->wifi_list.connected_ssid) == 0;

    return board_on && connected_to_the_view;
}

bool check_if_wifi_has_csv_password(App* app) {
    for(size_t i = 0; i < MAX_WIFI_CREDENTIALS; i++) {
        if(strcmp(app->csv_networks[i].ssid, app->wifi_list.selected_ssid) == 0) {
            return app->csv_networks[i].password[0] != '\0';
        }
    }
    return false;
}

bool check_if_password_set(App* app) {
    return app->wifi_list.password_ssid[0] != '\0';
}

char get_wifi_csv_password(App* app) {
    for(size_t i = 0; i < MAX_WIFI_CREDENTIALS; i++) {
        if(strcmp(app->csv_networks[i].ssid, app->wifi_list.selected_ssid) == 0) {
            return app->csv_networks[i].password[0];
        }
    }
    return '\0';
}

void submenu_callback_disconnect_connect(void* context, uint32_t index) {
    UNUSED(index);
    FURI_LOG_T(TAG, "submenu_callback_disconnect_connect");
    App* app = context;

    // 3 cases, board is active and connected to the selected ssid
    bool active = check_if_current_view_is_active(app);
    bool is_connected_not_selected =
        strcmp(app->wifi_list.selected_ssid, app->wifi_list.connected_ssid) != 0 &&
        app->status == BOARD_CONNECTED_WIFI_ON;
    bool is_not_active = app->status == BOARD_CONNECTED_WIFI_OFF;
    // board is not active

    if(is_connected_not_selected) {
        // Set the selected ssid to the connected ssid
        active = true;
        is_not_active = true;
    }

    if(active) {
        // Disconnect logic here
        FURI_LOG_I(TAG, "Disconnecting from WiFi");
        submenu_change_item_label(app->submenu, 0, "Disconnecting ...");
        if(disconnectWiFiCommand(app->uart, NULL)) {
            app->status = BOARD_CONNECTED_WIFI_OFF;
            app->wifi_list.connected_ssid[0] = '\0';
            scene_on_enter_connect_details(context);
        } else {
            FURI_LOG_E(TAG, "Failed to disconnect from WiFi");
        }
    }

    if(is_not_active) {
        // Connect logic here
        FURI_LOG_I(TAG, "Connecting to WiFi");
        submenu_change_item_label(app->submenu, 0, "Connecting ...");
        // Creating the string for connect cmd, WIFI_CONNECT: <ssid> <password>
        char connect_cmd[CONNECT_CMD_BUFFER_SIZE];

        int ret = snprintf(
            connect_cmd,
            sizeof(connect_cmd),
            "WIFI_CONNECT %s %s",
            app->wifi_list.selected_ssid,
            app->wifi_list.password_ssid);

        if(ret < 0 || ret >= (int)sizeof(connect_cmd)) {
            // Handle error: the output was truncated or an encoding error occurred
            // You can log an error message or take appropriate action here
            printf("Error: connect_cmd buffer is too small or encoding error "
                   "occurred.\n");
        }

        if(connectCommand(app->uart, connect_cmd)) {
            app->status = BOARD_CONNECTED_WIFI_ON;
            strncpy(
                app->wifi_list.connected_ssid,
                app->wifi_list.selected_ssid,
                sizeof(app->wifi_list.connected_ssid) - 1);
            app->wifi_list.connected_ssid[sizeof(app->wifi_list.connected_ssid) - 1] =
                '\0'; // Ensure null-termination
            scene_on_enter_connect_details(context);
        } else {
            FURI_LOG_E(TAG, "Failed to connect to WiFi");
        }
    }

    // Refresh the scene
    scene_manager_handle_custom_event(app->scene_manager, AppEvent_Connect_Details);
}

void submenu_callback_set_password(void* context, uint32_t index) {
    UNUSED(index);
    FURI_LOG_T(TAG, "submenu_callback_set_password");
    App* app = context;

    //Copy over the the password to text_intput_store

    if(check_if_wifi_has_csv_password(app)) {
        strncpy(
            app->text_input_store,
            app->wifi_list.password_ssid,
            sizeof(app->text_input_store) - 1);
        app->text_input_store[sizeof(app->text_input_store) - 1] = '\0';
        app->is_custom_tx_string = true; // Ensure null-termination
    }

    app->text_input_state = TextInputState_WifiPassword;
    scene_manager_next_scene(app->scene_manager, Text_Input);
}

void submenu_callback_save_to_csv(void* context, uint32_t index) {
    UNUSED(index);
    FURI_LOG_T(TAG, "submenu_callback_save_to_csv");
    App* app = context;

    WifiCredential wifi = {0};
    strncpy(wifi.ssid, app->wifi_list.selected_ssid, MAX_SSID_LENGTH - 1);
    strncpy(wifi.password, app->wifi_list.password_ssid, MAX_PASSWORD_LENGTH - 1);
    wifi.is_default = false;

    if(!write_wifi_to_csv(app, &wifi)) {
        FURI_LOG_E(TAG, "Failed to save WiFi details to CSV");
    }
    // sync csv with flipper memory
    sync_csv_to_mem(app);
    // refresh the menu
    scene_on_enter_connect_details(context);
}

void submenu_callback_forget_network(void* context, uint32_t index) {
    UNUSED(index);
    FURI_LOG_T(TAG, "submenu_callback_forget_network");
    App* app = context;

    if(!delete_wifi_from_csv(app, app->wifi_list.selected_ssid)) {
        FURI_LOG_E(TAG, "Failed to forget network");
    }
    // sync csv with flipper memory
    sync_csv_to_mem(app);

    // refresh the menu
    scene_on_enter_connect_details(context);
}

void submenu_callback_exit(void* context, uint32_t index) {
    UNUSED(index);
    FURI_LOG_T(TAG, "submenu_callback_exit");
    App* app = context;
    scene_manager_next_scene(app->scene_manager, MainMenu);
}

void scene_on_enter_connect_details(void* context) {
    FURI_LOG_T(TAG, "scene_on_enter_connect_details");
    App* app = context;

    bool active = check_if_current_view_is_active(app);

    submenu_reset(app->submenu);
    char header[256];
    if(active) {
        snprintf(header, sizeof(header), "%s (connected)", app->wifi_list.selected_ssid);
    } else {
        snprintf(header, sizeof(header), "%s", app->wifi_list.selected_ssid);
    }

    submenu_set_header(app->submenu, header);
    bool wifi_csv_found = check_if_wifi_has_csv_password(app);
    bool password_set = check_if_password_set(app);
    //Load password from csv
    char password = get_wifi_csv_password(app);
    if(password != '\0') {
        strncpy(app->wifi_list.password_ssid, &password, 1);
    }

    if(active) {
        submenu_add_item(
            app->submenu,
            "Disconnect",
            Details_Disconnect,
            submenu_callback_disconnect_connect,
            app);
    } else {
        submenu_add_item(
            app->submenu, "Connect", Details_Disconnect, submenu_callback_disconnect_connect, app);
    }

    if(wifi_csv_found) {
        submenu_add_item(
            app->submenu, "Edit Password", Details_SetPassword, submenu_callback_set_password, app);

        submenu_add_item(
            app->submenu, "Forget network", Details_Forget, submenu_callback_forget_network, app);
    } else {
        submenu_add_item(
            app->submenu,
            !password_set ? "Set Password" : "Edit Password",
            Details_SetPassword,
            submenu_callback_set_password,
            app);

        password_set ? submenu_add_item(
                           app->submenu,
                           "Save to flipper",
                           Details_SaveToCsv,
                           submenu_callback_save_to_csv,
                           app) :
                       NULL;
    }

    submenu_add_item(app->submenu, "Exit to main menu", Details_Exit, submenu_callback_exit, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, AppView_Connect_Details);
}

bool scene_on_event_connect_details(void* context, SceneManagerEvent event) {
    FURI_LOG_T(TAG, "scene_on_event_connect_details");
    App* app = context;
    UNUSED(app);
    bool consumed = false;
    switch(event.type) {
    case SceneManagerEventTypeBack:
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, MainMenu);
        consumed = true;
        break;
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        case AppView_Connect_Details:
            scene_on_enter_connect_details(context);
            consumed = true;
            break;
        }
        break;

    default:
        consumed = false;
        break;
    }
    return consumed;
}

void scene_on_exit_connect_details(void* context) {
    FURI_LOG_T(TAG, "scene_on_exit_connect_details");
    App* app = context;
    submenu_reset(app->submenu);
}
