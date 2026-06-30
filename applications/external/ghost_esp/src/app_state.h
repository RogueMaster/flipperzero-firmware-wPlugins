#pragma once
#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_box.h>
#include <gui/modules/text_input.h>
#include <dialogs/dialogs.h>
#include "gui_modules/mainmenu.h"
#include "settings_def.h"
#include "app_types.h"
#include "settings_ui_types.h"

#define INPUT_BUFFER_SIZE 128

typedef enum {
    VIEW_MAIN = 0,
    VIEW_WIFI = 1,
    VIEW_BLE = 2,
    VIEW_GPS = 3,
    VIEW_SETTINGS_CONFIG = 4,
    VIEW_SETTINGS = VIEW_SETTINGS_CONFIG,
    VIEW_TEXT_BOX = 5,
    VIEW_TEXT_INPUT = 6,
    VIEW_CONFIRMATION = 7,
    VIEW_SETTINGS_ACTIONS = 8,
    VIEW_WIFI_SCANNING = 10,
    VIEW_WIFI_CAPTURE = 11,
    VIEW_WIFI_ATTACK = 12,
    VIEW_WIFI_NETWORK = 13,
    VIEW_WIFI_SETTINGS = 14,
    VIEW_AERIAL = 15,
    VIEW_BLE_SCANNING = 20,
    VIEW_BLE_CAPTURE = 21,
    VIEW_BLE_ATTACK = 22,
    VIEW_IR = 30,
    VIEW_IR_REMOTES = 31,
    VIEW_IR_BUTTONS = 32,
    VIEW_IR_UNIVERSALS = 33,
    VIEW_STATUS_IDLE = 40,
    // WiFi Settings sub-categories
    VIEW_WIFI_SETTINGS_LED = 50,
    VIEW_WIFI_SETTINGS_SD = 51,
    VIEW_WIFI_SETTINGS_MGMT = 52,
    VIEW_WIFI_SETTINGS_DEVICE = 53,
    VIEW_WIFI_SETTINGS_MISC = 54,
    // WiFi Network sub-categories
    VIEW_WIFI_NETWORK_PORTAL = 55,
    VIEW_WIFI_NETWORK_CONN = 56,
    VIEW_WIFI_NETWORK_SCAN = 57,
    VIEW_WIFI_NETWORK_IOT = 58,
    VIEW_WIFI_NETWORK_WEBUI = 59,
    // BLE Scanning sub-categories
    VIEW_BLE_SCAN_DETECT = 60,
    VIEW_BLE_SCAN_AIRTAG = 61,
    VIEW_BLE_SCAN_GATT = 62,
    VIEW_BLE_SCAN_ADV = 63,
    // GPS sub-categories
    VIEW_GPS_CONFIG = 64,
    VIEW_GPS_WARDRIVE = 65,
} ViewId;

typedef struct {
    bool enabled; // Master switch for filtering
    bool show_ble_status;
    bool show_wifi_status;
    bool show_flipper_devices;
    bool show_wifi_networks;
    bool strip_ansi_codes;
    bool add_prefixes; // Whether to add [BLE], [WIFI] etc prefixes
} FilterConfig;

typedef struct {
    uint32_t index;
    char name[64];
} IrRemoteEntry;

typedef struct {
    uint32_t index;
    char name[32];
    char proto[16];
} IrSignalEntry;

typedef struct {
    uint32_t index;
    char name[32];
    char proto[16];
} IrUniversalEntry;

struct AppState {
    // Views
    ViewDispatcher* view_dispatcher;
    DialogsApp* dialogs;
    MainMenu* main_menu;
    Submenu* wifi_menu;
    Submenu* wifi_scanning_menu;
    Submenu* wifi_capture_menu;
    Submenu* wifi_attack_menu;
    Submenu* wifi_network_menu;
    Submenu* wifi_settings_menu;
    Submenu* status_idle_menu;
    Submenu* ble_menu;
    Submenu* ble_scanning_menu;
    Submenu* ble_capture_menu;
    Submenu* ble_attack_menu;
    Submenu* aerial_menu;
    Submenu* gps_menu;
    Submenu* ir_menu;
    Submenu* ir_remotes_menu;
    Submenu* ir_buttons_menu;
    Submenu* ir_universals_menu;
    // Sub-category submenus
    Submenu* wifi_settings_led_menu;
    Submenu* wifi_settings_sd_menu;
    Submenu* wifi_settings_mgmt_menu;
    Submenu* wifi_settings_device_menu;
    Submenu* wifi_settings_misc_menu;
    Submenu* wifi_network_portal_menu;
    Submenu* wifi_network_conn_menu;
    Submenu* wifi_network_scan_menu;
    Submenu* wifi_network_iot_menu;
    Submenu* wifi_network_webui_menu;
    Submenu* ble_scan_detect_menu;
    Submenu* ble_scan_airtag_menu;
    Submenu* ble_scan_gatt_menu;
    Submenu* ble_scan_adv_menu;
    Submenu* gps_config_menu;
    Submenu* gps_wardrive_menu;
    VariableItemList* settings_menu;
    TextBox* text_box;
    TextInput* text_input;
    ConfirmationView* confirmation_view;
    FuriMutex* buffer_mutex;
    // UART Context
    UartContext* uart_context;
    // FilterConfig is small enough to embed directly
    FilterConfig filter_config;

    // Settings
    Settings settings;
    SettingsUIContext settings_ui_context;
    Submenu* settings_actions_menu;

    // State
    uint32_t current_index;
    uint8_t current_view;
    uint8_t previous_view;
    bool text_box_user_scrolled;
    bool text_box_pause_hint_shown;
    ViewInputCallback text_box_original_input;
    void* text_box_original_context;
    uint32_t last_wifi_category_index;
    uint32_t last_wifi_scanning_index;
    uint32_t last_wifi_capture_index;
    uint32_t last_wifi_attack_index;
    uint32_t last_wifi_network_index;
    uint32_t last_wifi_settings_index;
    uint32_t last_ble_category_index;
    uint32_t last_ble_scanning_index;
    uint32_t last_ble_capture_index;
    uint32_t last_ble_attack_index;
    uint32_t last_aerial_category_index;
    uint32_t last_gps_index;
    uint32_t last_ir_index;
    // Sub-category last indices
    uint32_t last_wifi_settings_led_index;
    uint32_t last_wifi_settings_sd_index;
    uint32_t last_wifi_settings_mgmt_index;
    uint32_t last_wifi_settings_device_index;
    uint32_t last_wifi_settings_misc_index;
    uint32_t last_wifi_network_portal_index;
    uint32_t last_wifi_network_conn_index;
    uint32_t last_wifi_network_scan_index;
    uint32_t last_wifi_network_iot_index;
    uint32_t last_wifi_network_webui_index;
    uint32_t last_ble_scan_detect_index;
    uint32_t last_ble_scan_airtag_index;
    uint32_t last_ble_scan_gatt_index;
    uint32_t last_ble_scan_adv_index;
    uint32_t last_gps_config_index;
    uint32_t last_gps_wardrive_index;
    uint32_t ir_current_remote_index;
    IrRemoteEntry ir_remotes[32];
    size_t ir_remote_count;
    IrSignalEntry ir_signals[64];
    size_t ir_signal_count;
    IrUniversalEntry ir_universals[64];
    size_t ir_universal_count;
    char ir_current_universal_file[64];
    bool ir_universal_buttons_mode;
    bool ir_file_buttons_mode;
    uint8_t* ir_file_buffer;
    size_t ir_file_buffer_size;
    char ir_file_path[128];
    size_t ir_signal_block_offsets[64];
    size_t ir_signal_block_lengths[64];
    char* input_buffer;
    const char* uart_command;
    char* textBoxBuffer;
    size_t buffer_length;
    size_t buffer_capacity;
    size_t buffer_size;
    uint8_t connect_input_stage;
    char connect_ssid[128];
    char confirmation_message[256];
    bool came_from_settings;
    void* active_confirm_context; // To track confirmation context for cleanup
};
