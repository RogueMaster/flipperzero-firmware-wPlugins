#pragma once

#include <dialogs/dialogs.h>
#include <dolphin/dolphin.h>
#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/loading.h>
#include <gui/modules/text_box.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/scene_manager.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/view_stack.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <stdio.h>
#include <cfw/cfw.h>

#include "blackhat_app.h"
#include "blackhat_custom_event.h"
#include "blackhat_uart.h"
#include "scenes/blackhat_scene.h"

#define NUM_MENU_ITEMS (20)

#define BLACKHAT_TEXT_BOX_STORE_SIZE (4096)
#define UART_CH                      cfw_settings.uart_esp_channel

#define SHELL_CMD          "whoami"
#define SCAN_CMD           "bh script scan"
#define CHG_RUN_CMD_SCREEN "bh rcs"
#define RUN_CMD            "bh script run"
#define WIFI_CON_CMD       "bh wifi connect"
#define SET_INET_SSID_CMD  "bh set SSID"
#define SET_INET_PWD_CMD   "bh set PASS"
#define SET_AP_SSID_CMD    "bh set AP_SSID"
#define LIST_AP_CMD        "bh wifi list"
#define DEV_CMD            "bh wifi dev"
#define DEAUTH_CMD         "bh deauth_broadcast"
#define START_AP_CMD       "bh wifi ap"
#define START_KISMET_CMD   "bh kismet"
#define GET_IP_CMD         "bh wifi ip"
#define START_SSH_CMD      "bh ssh"
#define ST_EVIL_TWIN_CMD   "bh evil_twin"
#define ST_EVIL_PORT_CMD   "bh evil_portal"
#define TEST_INET          "bh test_inet"
#define GET_CMD            "bh get"
#define REBOOT_CMD         "reboot"
#define BHTUI_CMD          "TERM=linux bhtui > /dev/tty1 2>&1"

typedef enum {
    NO_ARGS = 0,
    INPUT_ARGS,
    TOGGLE_ARGS
} InputArgs;

typedef enum {
    FOCUS_CONSOLE_END = 0,
    FOCUS_CONSOLE_START,
    FOCUS_CONSOLE_TOGGLE
} FocusConsole;

#define MAX_OPTIONS (9)
typedef struct {
    const char* item_string;
    char* options_menu[MAX_OPTIONS];
    int num_options_menu;
    char* selected_option;
    char* actual_command;
    bool text_input_req;
} BlackhatItem;

#define ENTER_NAME_LENGTH 25

struct BlackhatApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    FuriString* text_box_store;

    // For custom scripts
    char* script_text;
    size_t script_text_ptr;
    int num_scripts;
    char* cmd[64];
    bool scanned;
    VariableItemList* script_item_list;

    size_t text_box_store_strlen;
    TextBox* text_box;

    VariableItemList* var_item_list;
    BlackhatUart* uart;
    TextInput* text_input;
    View* tui_view;
    DialogsApp* dialogs;

    int selected_menu_index;
    int selected_option_index[NUM_MENU_ITEMS];
    char* selected_tx_string;
    const char* selected_option_item_text;
    char text_store[128];
    char text_input_ch[ENTER_NAME_LENGTH];
    bool text_input_req;
    bool is_script_scan;
};

typedef enum {
    BlackhatAppViewVarItemList,
    BlackhatAppViewScriptItemList,
    BlackhatAppViewConsoleOutput,
    BlackhatAppViewStartPortal,
    BlackhatAppViewTextInput,
    BlackhatAppViewTui,
} BlackhatAppView;
