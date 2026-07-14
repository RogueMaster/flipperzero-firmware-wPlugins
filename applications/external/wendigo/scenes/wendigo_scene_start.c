#include "../wendigo_app_i.h"
#include "../wendigo_scan.h"
#include <dolphin/dolphin.h>

/** Public method from wendigo_scene_pnl_list.c */
extern void wendigo_scene_pnl_list_set_device(wendigo_device* dev, WendigoApp* app);

/** NUM_MENU_ITEMS defined in wendigo_app_i.h - if you add an entry here,
 * increment it!
 */
static const WendigoItem items[START_MENU_ITEMS] = {
    {"Setup", {""}, 1, OPEN_SETUP, BOTH_MODES},
    {"Scan", {"WiFi", "BT", "Status"}, 3, OPEN_SCAN, TEXT_MODE},
    {"Devices",
     {"All", "Bluetooth", "WiFi", "BT Classic", "BLE", "WiFi AP", "WiFi STA"},
     7,
     LIST_DEVICES,
     BOTH_MODES},
    {"Selected Devices",
     {"All", "Bluetooth", "WiFi", "BT Classic", "BLE", "WiFi AP", "WiFi STA"},
     7,
     LIST_SELECTED_DEVICES,
     BOTH_MODES},
    {"Probed SSIDs", {""}, 1, PNL_LIST, TEXT_MODE},
    {"UART Terminal", {""}, 1, UART_TERMINAL, TEXT_MODE},
    {"Help", {"About", "Version"}, 2, OPEN_HELP, TEXT_MODE},
};

#define SETUP_IDX       (0)
#define SCAN_IDX        (1)
#define PNL_IDX         (4)
#define SCAN_WIFI_IDX   (0)
#define SCAN_BT_IDX     (1)
#define SCAN_STATUS_IDX (2)
#define SCAN_START_STR  "Start"
#define SCAN_STOP_STR   "Stop"
#define ABOUT_IDX       (0)
#define ESP_VER_IDX     (1)
#define LOCKED_MSG      "Stop\nScanning\nFirst!"
#define DEVICE_ALL_IDX  (0)
#define DEVICE_BT_IDX   (1)
#define DEVICE_WIFI_IDX (2)
#define DEVICE_HCI_IDX  (3)
#define DEVICE_BLE_IDX  (4)
#define DEVICE_AP_IDX   (5)
#define DEVICE_STA_IDX  (6)

static uint8_t menu_items_num = 0;
static uint8_t item_indexes[START_MENU_ITEMS] = {0};
static VariableItem* pnl_view = NULL;

uint8_t wendigo_device_mask(uint8_t selected_option) {
    switch(selected_option) {
    case DEVICE_ALL_IDX:
        return DEVICE_ALL;
    case DEVICE_BT_IDX:
        return DEVICE_BT_CLASSIC | DEVICE_BT_LE;
    case DEVICE_WIFI_IDX:
        return DEVICE_WIFI_AP | DEVICE_WIFI_STA;
    case DEVICE_HCI_IDX:
        return DEVICE_BT_CLASSIC;
    case DEVICE_BLE_IDX:
        return DEVICE_BT_LE;
    case DEVICE_AP_IDX:
        return DEVICE_WIFI_AP;
    case DEVICE_STA_IDX:
        return DEVICE_WIFI_STA;
    default:
        return 0;
    }
}

void wendigo_display_pnl_count(WendigoApp* app) {
    if(pnl_view == NULL) {
        wendigo_log(MSG_WARN, "wendigo_display_pnl_count() called with NULL pnl_view.");
        return;
    }
    if(app->current_view != WendigoAppViewVarItemList) {
        wendigo_log(
            MSG_WARN,
            "wendigo_display_pnl_count() called when wendigo_scene_start not displayed.");
    }
    char countStr[4];
    bzero(countStr, sizeof(countStr));
    uint8_t ssid_count = 0;
    if(networks_count > 0 && networks != NULL) {
        ssid_count = networks_count;
    }
    snprintf(countStr, sizeof(countStr), "%d", ssid_count);
    variable_item_set_current_value_text(pnl_view, countStr);
}

/** Callback invoked when a menu item is selected */
static void wendigo_scene_start_var_list_enter_callback(void* context, uint32_t index) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_start_var_list_enter_callback()");
    furi_assert(context);
    WendigoApp* app = context;

    furi_assert(index < menu_items_num);
    uint8_t item_index = item_indexes[index];
    furi_assert(item_index < START_MENU_ITEMS);
    const WendigoItem* item = &items[item_index];

    const int selected_option_index = app->selected_option_index[index];
    furi_assert(selected_option_index < item->num_options_menu);
    // TODO: Remove these (I think)
    app->selected_tx_string = item->options_menu[selected_option_index];
    app->is_command = false;
    app->is_custom_tx_string = false;
    app->selected_menu_index = index;

    /* Reward Dolphin for running a command */
    dolphin_deed(DolphinDeedGpioUartBridge);

    switch(item->action) {
    case OPEN_SETUP:
        view_dispatcher_send_custom_event(app->view_dispatcher, Wendigo_EventSetup);
        FURI_LOG_T(
            WENDIGO_TAG,
            "End wendigo_scene_start_var_list_enter_callback(): Displaying Setup menu.");
        return;
    case OPEN_SCAN:
        VariableItem* myItem;
        WendigoRadio* radio;
        /* Disable/Enable settings menu when starting/stopping scanning */
        if(selected_option_index == SCAN_WIFI_IDX || selected_option_index == SCAN_BT_IDX) {
            if(selected_option_index == SCAN_WIFI_IDX) {
                radio = &app->interfaces[IF_WIFI];
                wendigo_set_scanning_interface(app, IF_WIFI, !radio->scanning);
            } else {
                radio = &app->interfaces[IF_BLE];
                /* Because BLE and BT Classic are managed by a single option,
                    * we disable both only if both are running, otherwise assume
                    * we want both to be started. */
                bool stopping =
                    (app->interfaces[IF_BLE].scanning && app->interfaces[IF_BT_CLASSIC].scanning);
                wendigo_set_scanning_interface(app, IF_BLE, !stopping);
                wendigo_set_scanning_interface(app, IF_BT_CLASSIC, !stopping);
            }
            /* Lock or unlock the settings menu based on app->is_scanning */
            myItem = variable_item_list_get(app->var_item_list, SETUP_IDX);
            variable_item_set_locked(myItem, app->is_scanning, LOCKED_MSG);
            /* Set selected option text to Start or Stop */
            myItem = variable_item_list_get(app->var_item_list, SCAN_IDX);
            uint8_t optionLen = strlen(item->options_menu[app->selected_option_index[index]]) + 2;
            if(radio->scanning) {
                optionLen += strlen(SCAN_STOP_STR);
            } else {
                optionLen += strlen(SCAN_START_STR);
            }
            char* optionStr = malloc(sizeof(char) * optionLen);
            if(optionStr != NULL) {
                snprintf(
                    optionStr,
                    optionLen,
                    "%s %s",
                    (radio->scanning) ? SCAN_STOP_STR : SCAN_START_STR,
                    item->options_menu[app->selected_option_index[index]]);
                variable_item_set_current_value_text(myItem, optionStr);
                free(optionStr);
            }
        } else if(selected_option_index == SCAN_STATUS_IDX) {
            view_dispatcher_send_custom_event(app->view_dispatcher, Wendigo_EventDisplayStatus);
            FURI_LOG_T(
                WENDIGO_TAG,
                "End wendigo_scene_start_var_list_enter_callback(): Displaying status.");
            return;
        }
        break;
    case LIST_DEVICES:
        /* Find selected option to determine device mask */
        wendigo_scene_device_list_set_current_devices_mask(
            wendigo_device_mask(selected_option_index));
        view_dispatcher_send_custom_event(app->view_dispatcher, Wendigo_EventListDevices);
        FURI_LOG_T(
            WENDIGO_TAG,
            "End wendigo_scene_start_var_list_enter_callback(): Displaying device list.");
        return;
    case LIST_SELECTED_DEVICES:
        wendigo_scene_device_list_set_current_devices_mask(
            wendigo_device_mask(selected_option_index) | DEVICE_SELECTED_ONLY);
        view_dispatcher_send_custom_event(app->view_dispatcher, Wendigo_EventListDevices);
        FURI_LOG_T(
            WENDIGO_TAG,
            "End wendigo_scene_start_var_list_enter_callback(): Displaying selected device lists.");
        return;
    case UART_TERMINAL:
        view_dispatcher_send_custom_event(app->view_dispatcher, Wendigo_EventStartConsole);
        FURI_LOG_T(
            WENDIGO_TAG,
            "End wendigo_scene_start_var_list_enter_callback(): Displaying device tracking.");
        return;
    case PNL_LIST:
        wendigo_scene_pnl_list_set_device(NULL, app);
        view_dispatcher_send_custom_event(app->view_dispatcher, Wendigo_EventListNetworks);
        FURI_LOG_T(
            WENDIGO_TAG,
            "End wendigo_scene_start_var_list_enter_callback(): Displaying Preferred Network List.");
        return;
    case OPEN_HELP:
        switch(selected_option_index) {
        case ABOUT_IDX:
            view_dispatcher_send_custom_event(app->view_dispatcher, Wendigo_EventStartHelp);
            break;
        case ESP_VER_IDX:
            /* Ensure wendigo_scan.c receives transmitted data */
            // TODO: The UART callback is only modified by wendigo_app.c and wendigo_scene_console_output.c - If ConsoleOutput is removed then this can be
            wendigo_uart_set_binary_cb(app->uart);
            wendigo_version(app);
            break;
        default:
            char* msg = malloc(sizeof(char) * 72);
            if(msg != NULL) {
                snprintf(
                    msg,
                    72,
                    "wendigo_scene_start_var_list_enter_callback(): Invalid help option %d.",
                    selected_option_index);
                wendigo_log(MSG_ERROR, msg);
                free(msg);
            }
            break;
        }
        FURI_LOG_T(
            WENDIGO_TAG,
            "End wendigo_scene_start_var_list_enter_callback(): Displaying a help page.");
        return;
    default:
        FURI_LOG_T(
            WENDIGO_TAG,
            "End wendigo_scene_start_var_list_enter_callback(): Unknown action %d.",
            item->action);
        return;
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_start_var_list_enter_callback()");
}

/** Callback invoked when a menu option is changed */
static void wendigo_scene_start_var_list_change_callback(VariableItem* item) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_start_var_list_change_callback()");
    furi_assert(item);

    WendigoApp* app = variable_item_get_context(item);
    furi_assert(app);

    uint8_t item_index = item_indexes[app->selected_menu_index];
    const WendigoItem* menu_item = &items[item_index];
    uint8_t option_index = variable_item_get_current_value_index(item);
    furi_assert(option_index < menu_item->num_options_menu);
    if(item_index == SCAN_IDX && option_index < SCAN_STATUS_IDX) {
        /* Append Start or Stop to the radio name */
        bool optionStarting;
        uint8_t optionLen = strlen(menu_item->options_menu[option_index]) + 2;
        if((option_index == SCAN_WIFI_IDX && !app->interfaces[IF_WIFI].scanning) ||
           (option_index == SCAN_BT_IDX &&
            (!app->interfaces[IF_BT_CLASSIC].scanning || !app->interfaces[IF_BLE].scanning))) {
            optionStarting = true;
            optionLen += strlen(SCAN_START_STR);
        } else {
            optionStarting = false;
            optionLen += strlen(SCAN_STOP_STR);
        }
        char* optionStr = malloc(sizeof(char) * optionLen);
        if(optionStr == NULL) {
            variable_item_set_current_value_text(item, menu_item->options_menu[option_index]);
        } else {
            snprintf(
                optionStr,
                optionLen,
                "%s %s",
                (optionStarting) ? SCAN_START_STR : SCAN_STOP_STR,
                menu_item->options_menu[option_index]);
            variable_item_set_current_value_text(item, optionStr);
            free(optionStr);
        }
    } else {
        variable_item_set_current_value_text(item, menu_item->options_menu[option_index]);
    }
    app->selected_option_index[app->selected_menu_index] = option_index;
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_start_var_list_change_callback()");
}

/** Callback invoked when the view is launched */
void wendigo_scene_start_on_enter(void* context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_start_on_enter()");
    WendigoApp* app = context;
    app->current_view = WendigoAppViewVarItemList;

    variable_item_list_reset(app->var_item_list);
    variable_item_list_set_header(app->var_item_list, NULL);
    variable_item_list_set_enter_callback(
        app->var_item_list, wendigo_scene_start_var_list_enter_callback, app);

    VariableItem* item;
    menu_items_num = 0;
    for(uint8_t i = 0; i < START_MENU_ITEMS; ++i) {
        bool enabled = false;
        if(app->hex_mode && (items[i].mode_mask & HEX_MODE)) {
            enabled = true;
        }
        if(!app->hex_mode && (items[i].mode_mask & TEXT_MODE)) {
            enabled = true;
        }

        if(enabled) {
            item = variable_item_list_add(
                app->var_item_list,
                items[i].item_string,
                items[i].num_options_menu,
                wendigo_scene_start_var_list_change_callback,
                app);
            variable_item_set_current_value_index(item, app->selected_option_index[i]);
            variable_item_set_current_value_text(
                item, items[i].options_menu[app->selected_option_index[i]]);

            item_indexes[menu_items_num++] = i;
            if(i == SETUP_IDX && app->is_scanning) {
                variable_item_set_locked(item, true, LOCKED_MSG);
            } else if(i == SCAN_IDX && app->selected_option_index[i] < SCAN_STATUS_IDX) {
                /* Update the selected interface based on scanning status */
                bool scanning;
                if(app->selected_option_index[i] == SCAN_WIFI_IDX) {
                    scanning = app->interfaces[IF_WIFI].scanning;
                } else {
                    scanning =
                        (app->interfaces[IF_BLE].scanning ||
                         app->interfaces[IF_BT_CLASSIC].scanning);
                }
                /* Append "start" or "stop" to the current option text */
                uint8_t optionLen =
                    strlen(items[i].options_menu[app->selected_option_index[i]]) + 2;
                if(scanning) {
                    optionLen += strlen(SCAN_STOP_STR);
                } else {
                    optionLen += strlen(SCAN_START_STR);
                }
                char* scanningStr = malloc(sizeof(char) * optionLen);
                if(scanningStr != NULL) {
                    snprintf(
                        scanningStr,
                        optionLen,
                        "%s %s",
                        (scanning) ? SCAN_STOP_STR : SCAN_START_STR,
                        items[i].options_menu[app->selected_option_index[i]]);
                    variable_item_set_current_value_text(item, scanningStr);
                    free(scanningStr);
                }
            } else if(i == PNL_IDX) {
                pnl_view = item;
                if(pnl_view != NULL && networks_count > 0 && networks != NULL) {
                    /* Update the count of probed networks */
                    wendigo_display_pnl_count(app);
                }
            }
        }
    }
    variable_item_list_set_selected_item(
        app->var_item_list, scene_manager_get_scene_state(app->scene_manager, WendigoSceneStart));
    view_dispatcher_switch_to_view(app->view_dispatcher, WendigoAppViewVarItemList);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_start_on_enter()");
}

bool wendigo_scene_start_on_event(void* context, SceneManagerEvent event) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_start_on_event()");
    WendigoApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case Wendigo_EventSetup:
            scene_manager_set_scene_state(
                app->scene_manager, WendigoSceneStart, app->selected_menu_index);
            scene_manager_next_scene(app->scene_manager, WendigoSceneSetup);
            break;
        case Wendigo_EventStartKeyboardText:
            scene_manager_set_scene_state(
                app->scene_manager, WendigoSceneStart, app->selected_menu_index);
            scene_manager_next_scene(app->scene_manager, WendigoSceneTextInput);
            break;
        case Wendigo_EventStartKeyboardHex:
            scene_manager_set_scene_state(
                app->scene_manager, WendigoSceneStart, app->selected_menu_index);
            scene_manager_next_scene(app->scene_manager, WendigoSceneHexInput);
            break;
        case Wendigo_EventStartConsole:
            scene_manager_set_scene_state(
                app->scene_manager, WendigoSceneStart, app->selected_menu_index);
            scene_manager_next_scene(app->scene_manager, WendigoSceneConsoleOutput);
            break;
        case Wendigo_EventStartHelp:
            scene_manager_set_scene_state(
                app->scene_manager, WendigoSceneStart, app->selected_menu_index);
            scene_manager_next_scene(app->scene_manager, WendigoSceneHelp);
            break;
        case Wendigo_EventListDevices:
            scene_manager_set_scene_state(
                app->scene_manager, WendigoSceneStart, app->selected_menu_index);
            scene_manager_next_scene(app->scene_manager, WendigoSceneDeviceList);
            break;
        case Wendigo_EventDisplayStatus:
            scene_manager_set_scene_state(
                app->scene_manager, WendigoSceneStart, app->selected_menu_index);
            scene_manager_next_scene(app->scene_manager, WendigoSceneStatus);
            break;
        case Wendigo_EventListNetworks:
            scene_manager_set_scene_state(
                app->scene_manager, WendigoSceneStart, app->selected_menu_index);
            scene_manager_next_scene(app->scene_manager, WendigoScenePNLList);
            break;
        case Wendigo_EventRefreshPNLCount:
            if(app->current_view == WendigoAppViewVarItemList) {
                wendigo_display_pnl_count(app);
            } else {
                wendigo_log(
                    MSG_WARN,
                    "Event Wendigo_EventRefreshPNLCount received but start view not displayed.");
            }
            break;
        default:
            /* Do nothing */
            break;
        }
        consumed = true;
    } else if(event.type == SceneManagerEventTypeTick) {
        app->selected_menu_index = variable_item_list_get_selected_item_index(app->var_item_list);
        consumed = true;
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_start_on_event()");
    return consumed;
}

void wendigo_scene_start_on_exit(void* context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_start_on_exit()");
    WendigoApp* app = context;
    variable_item_list_reset(app->var_item_list);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_start_on_exit()");
}
