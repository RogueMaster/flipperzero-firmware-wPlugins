#include "../blackhat_app_i.h"

BlackhatItem items[] = {
    {"Shell", {""}, 1, NULL, SHELL_CMD, false},
    {"Scan for Scripts", {""}, 1, NULL, SCAN_CMD, false},
    {"Run Script", {""}, 1, NULL, CHG_RUN_CMD_SCREEN, false},
    {"BHtui (Screen Only)", {""}, 1, NULL, BHTUI_CMD, false},
    {"Connect WiFi", {"wlan0", "wlan1", "wlan2", "stop"}, 4, NULL, WIFI_CON_CMD, FOCUS_CONSOLE_END},
    {"Set inet SSID", {""}, 1, NULL, SET_INET_SSID_CMD, true},
    {"Set inet Password", {""}, 1, NULL, SET_INET_PWD_CMD, true},
    {"Set AP SSID", {""}, 1, NULL, SET_AP_SSID_CMD, true},
    {"List Networks", {"wlan0", "wlan1", "wlan2"}, 3, NULL, LIST_AP_CMD, FOCUS_CONSOLE_END},
    {"Wifi Device Info", {""}, 1, NULL, DEV_CMD, false},
    {"Deauth Broadcast", {"wlan0", "wlan1", "wlan2"}, 3, NULL, DEAUTH_CMD, FOCUS_CONSOLE_END},
    {"Enable AP", {"wlan0", "wlan1", "wlan2", "stop"}, 4, NULL, START_AP_CMD, FOCUS_CONSOLE_END},
    {"Start Kismet",
     {"wlan0", "wlan1", "wlan2", "stop"},
     4,
     NULL,
     START_KISMET_CMD,
     FOCUS_CONSOLE_END},
    {"Get IP", {""}, 1, NULL, GET_IP_CMD, false},
    {"SSH", {"start", "stop"}, 2, NULL, START_SSH_CMD, false},
    {"Start Evil Twin", {""}, 1, NULL, ST_EVIL_TWIN_CMD, false},
    {"Evil Portal", {"start", "stop"}, 2, NULL, ST_EVIL_PORT_CMD, false},
    {"Test Internet (ping)", {""}, 1, NULL, TEST_INET, false},
    {"Get Params", {""}, 1, NULL, GET_CMD, false},
    {"Reboot", {""}, 1, NULL, REBOOT_CMD, false},
};

static void blackhat_scene_start_var_list_enter_callback(void* context, uint32_t index) {
    furi_assert(context);
    BlackhatApp* app = context;

    furi_assert(index < NUM_MENU_ITEMS);
    const BlackhatItem* item = &items[index];

    const int selected_option_index = app->selected_option_index[index];
    furi_assert(selected_option_index < item->num_options_menu);
    app->selected_tx_string = item->actual_command;
    app->text_input_req = item->text_input_req;
    app->selected_menu_index = index;

    app->selected_option_item_text = item->selected_option;

    if(!strcmp(item->actual_command, BHTUI_CMD)) {
        scene_manager_next_scene(app->scene_manager, BlackhatSceneTui);
    } else {
        scene_manager_next_scene(app->scene_manager, BlackhatAppViewConsoleOutput);
    }
}

static void blackhat_scene_start_var_list_change_callback(VariableItem* item) {
    furi_assert(item);

    BlackhatApp* app = variable_item_get_context(item);
    furi_assert(app);

    app->selected_menu_index = variable_item_list_get_selected_item_index(app->var_item_list);

    const BlackhatItem* menu_item = &items[app->selected_menu_index];

    uint8_t item_index = variable_item_get_current_value_index(item);
    furi_assert(item_index < menu_item->num_options_menu);

    variable_item_set_current_value_text(item, menu_item->options_menu[item_index]);
    items[app->selected_menu_index].selected_option = menu_item->options_menu[item_index];

    app->selected_option_index[app->selected_menu_index] = item_index;
}

void blackhat_scene_start_on_enter(void* context) {
    BlackhatApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;

    variable_item_list_set_enter_callback(
        var_item_list, blackhat_scene_start_var_list_enter_callback, app);

    VariableItem* item;
    for(int i = 0; i < NUM_MENU_ITEMS; ++i) {
        item = variable_item_list_add(
            var_item_list,
            items[i].item_string,
            items[i].num_options_menu,
            blackhat_scene_start_var_list_change_callback,
            app);

        items[i].selected_option = items[i].options_menu[0];

        variable_item_set_current_value_index(item, app->selected_option_index[i]);
        variable_item_set_current_value_text(
            item, items[i].options_menu[app->selected_option_index[i]]);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, BlackhatAppViewVarItemList);
}

bool blackhat_scene_start_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void blackhat_scene_start_on_exit(void* context) {
    BlackhatApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
