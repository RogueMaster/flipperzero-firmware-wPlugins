#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <furi_hal_light.h>
#include <furi_hal_power.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <furi/core/stream_buffer.h>
#include <furi/core/memmgr.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_input.h>
#include <furi_hal_usb.h>
#include <furi_hal_usb_cdc.h>
#include <usb_cdc.h>
#include <furi_hal_vibro.h>
#include <dialogs/dialogs.h>
#include "qrcodegen.h"

#define TAG "Lab_C5"

#define USB_RUNTIME_GUARD_INTERVAL_MS 50

static size_t simple_app_bounded_strlen(const char* s, size_t max_len) {
    size_t len = 0;
    while(len < max_len && s[len] != '\0') {
        len++;
    }
    return len;
}

typedef enum {
    ScreenMenu,
    ScreenSerial,
    ScreenResults,
    ScreenSetupScanner,
    ScreenSetupScannerTiming,
    ScreenSetupKarma,
    ScreenSetupLed,
    ScreenSetupBoot,
    ScreenSetupSdManager,
    ScreenHelpQr,
    ScreenConsole,
    ScreenGps,
    ScreenPackageMonitor,
    ScreenChannelView,
    ScreenConfirmBlackout,
    ScreenConfirmSnifferDos,
    ScreenConfirmExit,
    ScreenKarmaMenu,
    ScreenEvilTwinMenu,
    ScreenEvilTwinPassList,
    ScreenEvilTwinPassQr,
    ScreenPortalMenu,
    ScreenBtLocatorList,
    ScreenPasswords,
    ScreenPasswordsSelect,
} AppScreen;

typedef enum {
    MenuStateSections,
    MenuStateItems,
} MenuState;

typedef enum {
    BlackoutPhaseIdle,
    BlackoutPhaseStarting,
    BlackoutPhaseScanning,
    BlackoutPhaseSorting,
    BlackoutPhaseAttacking,
    BlackoutPhaseStopping,
    BlackoutPhaseFinished,
} BlackoutPhase;

typedef enum {
    SnifferDogPhaseIdle,
    SnifferDogPhaseStarting,
    SnifferDogPhaseHunting,
    SnifferDogPhaseStopping,
    SnifferDogPhaseError,
    SnifferDogPhaseFinished,
} SnifferDogPhase;
#define LAB_C5_VERSION_TEXT "0.40"

#define SCAN_SSID_HIDDEN_LABEL        "Hidden"
#define SCAN_RESULTS_DEFAULT_CAPACITY 64
#define SCAN_RESULTS_MAX_CAPACITY     64
#define SCAN_BAND_24                  0
#define SCAN_BAND_5                   1
#define SCAN_BAND_UNKNOWN             0xFF
#define SCAN_LINE_BUFFER_SIZE         192
#define SCAN_SSID_MAX_LEN             33
#define SCAN_CHANNEL_MAX_LEN          8
#define SCAN_TYPE_MAX_LEN             16
#define SCAN_FIELD_BUFFER_LEN         48
#define SCAN_VENDOR_MAX_LEN           48
#define SCAN_POWER_MIN_DBM            (-110)
#define SCAN_POWER_MAX_DBM            0
#define SCAN_POWER_STEP               1
#define BACKLIGHT_ON_LEVEL            255
#define BACKLIGHT_OFF_LEVEL           0
#define GPS_UTC_OFFSET_MIN_MINUTES    (-12 * 60)
#define GPS_UTC_OFFSET_MAX_MINUTES    (14 * 60)

#define SERIAL_BUFFER_SIZE              1024
#define UART_STREAM_SIZE                512
#define MENU_VISIBLE_COUNT              6
#define MENU_VISIBLE_COUNT_SNIFFERS     4
#define MENU_VISIBLE_COUNT_ATTACKS      4
#define MENU_VISIBLE_COUNT_SETUP        4
#define MENU_SECTION_VISIBLE_COUNT      6
#define MENU_SECTION_BASE_Y             14
#define MENU_SECTION_SPACING            9
#define PACKAGE_MONITOR_MAX_HISTORY     96
#define MENU_TITLE_Y                    12
#define MENU_ITEM_BASE_Y                24
#define MENU_ITEM_SPACING               12
#define MENU_SCROLL_TRACK_Y             14
#define SERIAL_VISIBLE_LINES            6
#define SERIAL_LINE_CHAR_LIMIT          26
#define SERIAL_TEXT_LINE_HEIGHT         10
#define DISPLAY_WIDTH                   128
#define DISPLAY_HEIGHT                  64
#define WARD_DRIVE_CONSOLE_LINES        3
#define BT_SCAN_CONSOLE_LINES           3
#define SNIFFER_CONSOLE_LINES           3
#define SNIFFER_MAX_APS                 32
#define SNIFFER_MAX_CLIENTS             64
#define SNIFFER_MAX_SELECTED_STATIONS   32
#define SNIFFER_RESULTS_ARROW_THRESHOLD 200
#define PROBE_MAX_SSIDS                 32
#define PROBE_MAX_CLIENTS               64
#define BT_LOCATOR_MAX_DEVICES          24
#define BT_LOCATOR_VISIBLE_COUNT        6
#define BT_LOCATOR_WARMUP_MS            10000
#define BT_LOCATOR_NAME_MAX_LEN         24
#define BT_SCAN_PREVIEW_MAX             32
#define BT_SCAN_LINE_BUFFER_LEN         96
#define BT_LOCATOR_SCAN_LINE_LEN        128
#define BT_LOCATOR_SCROLL_TEXT_LEN      64
#define RESULT_DEFAULT_MAX_LINES        4
#define RESULT_DEFAULT_LINE_HEIGHT      12
#define RESULT_DEFAULT_CHAR_LIMIT       (SERIAL_LINE_CHAR_LIMIT - 3)
#define RESULT_START_Y                  12
#define RESULT_ENTRY_SPACING            0
#define RESULT_PREFIX_X                 2
#define RESULT_TEXT_X                   5
#define RESULT_SCROLL_WIDTH             3
#define RESULT_SCROLL_GAP               0
#define RESULT_SCROLL_INTERVAL_MS       200
#define RESULT_SCROLL_EDGE_PAUSE_STEPS  3
#define OVERLAY_TITLE_CHAR_LIMIT        22
#define DEAUTH_GUARD_BLINK_MS           500
#define DEAUTH_GUARD_VIBRO_MS           400
#define DEAUTH_GUARD_BLINK_TOGGLES      6
#define DEAUTH_GUARD_IDLE_MONITOR_MS    60000
#define DEAUTH_RUNNING_BLINK_MS         500
#define CONSOLE_VISIBLE_LINES           4
#define MENU_SECTION_SCANNER            0
#define MENU_SECTION_SNIFFERS           1
#define MENU_SECTION_ATTACKS            2
#define MENU_SECTION_MONITORING         3
#define MENU_SECTION_BLUETOOTH          4
#define MENU_SECTION_SETUP              5
#define SCANNER_FILTER_VISIBLE_COUNT    3
#define SCANNER_SCAN_COMMAND            "scan_networks"
#define SCAN_RESULTS_COMMAND            "show_scan_results"
#define LAB_C5_CONFIG_DIR_PATH          "apps_assets/labC5"
#define LAB_C5_CONFIG_FILE_PATH         LAB_C5_CONFIG_DIR_PATH "/config.txt"
#define FLIPPER_SD_BASE_PATH            EXT_PATH(LAB_C5_CONFIG_DIR_PATH)
#define FLIPPER_SD_DEBUG_PATH           FLIPPER_SD_BASE_PATH "/debug_last.txt"
#define FLIPPER_SD_DEBUG_LOG_PATH       FLIPPER_SD_BASE_PATH "/debug_cli.txt"

#define BOARD_PING_INTERVAL_MS   4000
#define BOARD_PING_TIMEOUT_MS    3000
#define BOARD_PING_FAILURE_LIMIT 2

#define PACKAGE_MONITOR_CHANNELS_24GHZ 14
#define PACKAGE_MONITOR_CHANNELS_5GHZ  23
#define PACKAGE_MONITOR_TOTAL_CHANNELS \
    (PACKAGE_MONITOR_CHANNELS_24GHZ + PACKAGE_MONITOR_CHANNELS_5GHZ)
#define PACKAGE_MONITOR_DEFAULT_CHANNEL            1
#define PACKAGE_MONITOR_COMMAND                    "packet_monitor"
#define PACKAGE_MONITOR_BAR_SPACING                2
#define PACKAGE_MONITOR_CHANNEL_SWITCH_COOLDOWN_MS 200

#define CHANNEL_VIEW_COMMAND     "channel_view"
#define CHANNEL_VIEW_LINE_BUFFER 64

#define SCANNER_CHANNEL_TIME_MIN_MS      10
#define SCANNER_CHANNEL_TIME_MAX_MS      2000
#define SCANNER_CHANNEL_TIME_STEP_MS     10
#define SCANNER_CHANNEL_TIME_DEFAULT_MIN 100
#define SCANNER_CHANNEL_TIME_DEFAULT_MAX 300

static const uint8_t channel_view_channels_24ghz[] =
    {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
static const uint8_t channel_view_channels_5ghz[] = {36,  40,  44,  48,  52,  56,  60,  64,  100,
                                                     104, 108, 112, 116, 120, 124, 128, 132, 136,
                                                     140, 144, 149, 153, 157, 161, 165};

#define CHANNEL_VIEW_24GHZ_CHANNEL_COUNT \
    (sizeof(channel_view_channels_24ghz) / sizeof(channel_view_channels_24ghz[0]))
#define CHANNEL_VIEW_5GHZ_CHANNEL_COUNT \
    (sizeof(channel_view_channels_5ghz) / sizeof(channel_view_channels_5ghz[0]))

#define CHANNEL_VIEW_VISIBLE_COLUMNS_24 6
#define CHANNEL_VIEW_VISIBLE_COLUMNS_5  5

#define BOOT_COMMAND_OPTION_COUNT 7
static const char* boot_command_options[BOOT_COMMAND_OPTION_COUNT] = {
    "start_blackout",
    "start_sniffer_dog",
    "channel_view",
    "packet_monitor",
    "start_sniffer",
    "scan_networks",
    "start_wardrive",
};

#define HINT_LINE_HEIGHT                12
#define EVIL_TWIN_MAX_HTML_FILES        16
#define EVIL_TWIN_HTML_NAME_MAX         32
#define EVIL_TWIN_POPUP_VISIBLE_LINES   3
#define EVIL_TWIN_MENU_OPTION_COUNT     5
#define EVIL_TWIN_VISIBLE_COUNT         2
#define EVIL_TWIN_STATUS_LINE_BUFFER    160
#define EVIL_TWIN_STATUS_NOTE_MAX       16
#define EVIL_TWIN_PASS_MAX_ENTRIES      32
#define EVIL_TWIN_PASS_MAX_LEN          64
#define EVIL_TWIN_PASS_VISIBLE_LINES    4
#define EVIL_TWIN_QR_MAX_VERSION        10
#define EVIL_TWIN_QR_BUFFER_LEN         qrcodegen_BUFFER_LEN_FOR_VERSION(EVIL_TWIN_QR_MAX_VERSION)
#define EVIL_TWIN_QR_TEXT_MAX           256
#define KARMA_MAX_PROBES                32
#define KARMA_PROBE_NAME_MAX            48
#define KARMA_POPUP_VISIBLE_LINES       3
#define KARMA_MENU_OPTION_COUNT         5
#define PORTAL_MENU_OPTION_COUNT        4
#define PORTAL_VISIBLE_COUNT            2
#define PORTAL_MAX_SSID_PRESETS         48
#define PORTAL_SSID_POPUP_VISIBLE_LINES 3
#define PORTAL_PASSWORD_MAX_LEN         48
#define PORTAL_STATUS_LINE_BUFFER       160
#define PORTAL_USERNAME_MAX_LEN         48
#define PASSWORDS_MAX_LINES             32
#define PASSWORDS_LINE_CHAR_LIMIT       96
#define PASSWORDS_VISIBLE_LINES         4
#define PASSWORDS_LINE_VISIBLE_CHARS    26
#define KARMA_MAX_HTML_FILES            EVIL_TWIN_MAX_HTML_FILES
#define KARMA_HTML_NAME_MAX             EVIL_TWIN_HTML_NAME_MAX
#define KARMA_SNIFFER_DURATION_MIN_SEC  5
#define KARMA_SNIFFER_DURATION_MAX_SEC  180
#define KARMA_SNIFFER_DURATION_STEP     5
#define KARMA_AUTO_LIST_DELAY_MS        500
#define DOUBLE_BACK_EXIT_MS             700
#define SD_MANAGER_MAX_FILES            24
#define SD_MANAGER_FILE_NAME_MAX        64
#define SD_MANAGER_POPUP_VISIBLE_LINES  3
#define SD_MANAGER_FOLDER_LABEL_MAX     24
#define SD_MANAGER_PATH_MAX             160
#define SD_LIST_BUFFER_LEN              160

typedef enum {
    MenuActionCommand,
    MenuActionCommandWithTargets,
    MenuActionResults,
    MenuActionToggleBacklight,
    MenuActionToggleOtgPower,
    MenuActionToggleShowRam,
    MenuActionToggleDebugMark,
    MenuActionOpenScannerSetup,
    MenuActionOpenScannerTiming,
    MenuActionOpenLedSetup,
    MenuActionOpenBootSetup,
    MenuActionOpenDownload,
    MenuActionOpenSdManager,
    MenuActionOpenHelp,

    MenuActionOpenConsole,
    MenuActionOpenGps,
    MenuActionOpenPackageMonitor,
    MenuActionOpenChannelView,
    MenuActionConfirmBlackout,
    MenuActionConfirmSnifferDos,
    MenuActionOpenEvilTwinMenu,
    MenuActionOpenKarmaMenu,
    MenuActionOpenPortalMenu,
    MenuActionOpenBtLocator,
} MenuAction;

typedef enum {
    PasswordsSourcePortal,
    PasswordsSourceEvilTwin,
} PasswordsSource;

typedef enum {
    ChannelViewBand24,
    ChannelViewBand5,
} ChannelViewBand;

typedef enum {
    ChannelViewSectionNone,
    ChannelViewSection24,
    ChannelViewSection5,
} ChannelViewSection;

typedef enum {
    ScannerOptionShowSSID,
    ScannerOptionShowBSSID,
    ScannerOptionShowChannel,
    ScannerOptionShowSecurity,
    ScannerOptionShowPower,
    ScannerOptionShowBand,
    ScannerOptionShowVendor,
    ScannerOptionMinPower,
    ScannerOptionCount,
} ScannerOption;

typedef struct {
    uint16_t number;
    char ssid[SCAN_SSID_MAX_LEN];
    uint8_t bssid[6];
    uint8_t channel;
    char security[SCAN_TYPE_MAX_LEN];
    int16_t power_dbm;
    uint8_t band;
    bool power_valid;
    bool selected;
} ScanResult;

#define VENDOR_CACHE_SIZE     16
#define VENDOR_CACHE_NAME_MAX 24

typedef struct {
    uint8_t oui[3];
    char name[VENDOR_CACHE_NAME_MAX];
    uint32_t last_tick;
    bool valid;
} VendorCacheEntry;

typedef struct {
    char mac[18];
    char name[32];
    int rssi;
    bool has_name;
} BtScanPreview;

typedef struct {
    char mac[18];
    int rssi;
    bool has_rssi;
    char name[BT_LOCATOR_NAME_MAX_LEN];
} BtLocatorDevice;

typedef struct {
    uint8_t id;
    char name[EVIL_TWIN_HTML_NAME_MAX];
} EvilTwinHtmlEntry;

typedef struct {
    uint16_t id;
    char ssid[SCAN_SSID_MAX_LEN];
    char pass[EVIL_TWIN_PASS_MAX_LEN];
} EvilTwinPassEntry;

typedef struct {
    uint8_t id;
    char name[KARMA_PROBE_NAME_MAX];
} KarmaProbeEntry;

typedef struct {
    uint8_t id;
    char name[KARMA_HTML_NAME_MAX];
} KarmaHtmlEntry;

typedef struct {
    uint8_t id;
    char ssid[SCAN_SSID_MAX_LEN];
} PortalSsidEntry;

typedef struct {
    char status_ssid[SCAN_SSID_MAX_LEN];
    int32_t client_count;
    uint32_t password_count;
    char last_password[PORTAL_PASSWORD_MAX_LEN];
    uint32_t username_count;
    char last_username[PORTAL_USERNAME_MAX_LEN];
    char line_buffer[PORTAL_STATUS_LINE_BUFFER];
    size_t line_length;
} PortalStatus;

typedef struct {
    char status_ssid[SCAN_SSID_MAX_LEN];
    int32_t client_count;
    uint32_t password_count;
    char last_password[PORTAL_PASSWORD_MAX_LEN];
    char status_note[EVIL_TWIN_STATUS_NOTE_MAX];
    char line_buffer[EVIL_TWIN_STATUS_LINE_BUFFER];
    size_t line_length;
} EvilTwinStatus;

typedef struct {
    char ssid[SCAN_SSID_MAX_LEN];
    uint8_t channel;
    uint16_t client_start;
    uint16_t client_count;
    uint16_t expected_count;
} SnifferApEntry;

typedef struct {
    char mac[18];
} SnifferClientEntry;

typedef enum {
    GpsAntennaUnknown = 0,
    GpsAntennaOk,
    GpsAntennaOpen,
} GpsAntennaStatus;

typedef struct {
    bool has_fix;
    bool has_coords;
    bool has_time;
    bool has_date;
    int satellites;
    GpsAntennaStatus antenna_status;
    char lat[16];
    char lon[16];
    char time[16];
    char date[16];
    char time_text[16];
    char time_line[40];
    char line[40];
    char offset_text[16];
    char sats_line[20];
    char line_buffer[128];
    size_t line_length;
} SimpleAppGpsState;

typedef struct {
    bool status_is_numeric;
    char status_text[32];
    char line_buffer[96];
    size_t line_length;
} SimpleAppWardriveState;

typedef struct {
    char ssid[SCAN_SSID_MAX_LEN];
    uint16_t client_start;
    uint16_t client_count;
    uint16_t expected_count;
} ProbeSsidEntry;

typedef struct {
    const char* label;
    const char* path;
} SdFolderOption;

typedef struct {
    char name[SD_MANAGER_FILE_NAME_MAX];
} SdFileEntry;

typedef struct {
    bool exit_app;
    MenuState menu_state;
    uint32_t section_index;
    uint32_t section_offset;
    uint32_t item_index;
    uint32_t item_offset;
    AppScreen screen;
    FuriHalSerialHandle* serial;
    FuriMutex* serial_mutex;
    FuriStreamBuffer* rx_stream;
    ViewPort* viewport;
    Gui* gui;
    char serial_buffer[SERIAL_BUFFER_SIZE];
    size_t serial_len;
    size_t serial_scroll;
    bool serial_follow_tail;
    bool serial_targets_hint;
    bool blackout_view_active;
    bool blackout_full_console;
    bool blackout_running;
    BlackoutPhase blackout_phase;
    uint32_t blackout_networks;
    uint32_t blackout_cycle;
    uint8_t blackout_channel;
    bool blackout_has_channel;
    char blackout_note[32];
    char blackout_line_buffer[96];
    size_t blackout_line_length;
    uint32_t blackout_last_update_tick;
    bool sniffer_dog_view_active;
    bool sniffer_dog_full_console;
    bool sniffer_dog_running;
    SnifferDogPhase sniffer_dog_phase;
    uint32_t sniffer_dog_deauth_count;
    uint8_t sniffer_dog_channel;
    bool sniffer_dog_has_channel;
    int sniffer_dog_rssi;
    bool sniffer_dog_has_rssi;
    char sniffer_dog_note[32];
    char sniffer_dog_line_buffer[128];
    size_t sniffer_dog_line_length;
    uint32_t sniffer_dog_last_update_tick;
    bool deauth_view_active;
    bool deauth_full_console;
    bool deauth_running;
    uint32_t deauth_targets;
    bool deauth_overlay_allowed;
    char deauth_ssid[SCAN_SSID_MAX_LEN];
    char deauth_bssid[18];
    uint8_t deauth_channel;
    bool deauth_has_channel;
    size_t deauth_list_offset;
    size_t deauth_list_scroll_x;
    uint32_t deauth_blink_last_tick;
    char deauth_note[32];
    bool handshake_view_active;
    bool handshake_full_console;
    bool handshake_running;
    uint32_t handshake_targets;
    uint32_t handshake_captured;
    char handshake_ssid[SCAN_SSID_MAX_LEN];
    uint8_t handshake_channel;
    bool handshake_has_channel;
    int handshake_rssi;
    bool handshake_has_rssi;
    bool handshake_overlay_allowed;
    uint32_t handshake_blink_last_tick;
    bool handshake_vibro_done;
    char handshake_note[32];
    bool sae_view_active;
    bool sae_full_console;
    bool sae_running;
    bool sae_overlay_allowed;
    char sae_ssid[SCAN_SSID_MAX_LEN];
    uint8_t sae_channel;
    bool sae_has_channel;
    char sae_note[16];
    char handshake_line_buffer[160];
    size_t handshake_line_length;
    char deauth_line_buffer[128];
    size_t deauth_line_length;
    char sae_line_buffer[128];
    size_t sae_line_length;
    uint32_t deauth_last_update_tick;
    bool wardrive_view_active;
    SimpleAppWardriveState* wardrive_state;
    bool gps_view_active;
    bool gps_console_mode;
    bool gps_time_24h;
    int16_t gps_utc_offset_minutes;
    bool gps_dst_enabled;
    bool gps_zda_tz_enabled;
    SimpleAppGpsState* gps_state;
    bool otg_power_enabled;
    bool otg_power_initial_state;
    bool bt_scan_view_active;
    bool bt_scan_summary_seen;
    bool airtag_scan_mode;
    bool bt_scan_full_console;
    uint32_t bt_scan_last_update_tick;
    bool bt_scan_show_list;
    BtScanPreview* bt_scan_preview;
    size_t bt_scan_preview_count;
    size_t bt_scan_list_offset;
    bool bt_scan_running;
    bool bt_scan_has_data;
    int bt_scan_airtags;
    int bt_scan_smarttags;
    int bt_scan_total;
    bool bt_locator_console_mode;
    bool bt_locator_mode;
    bool bt_locator_has_rssi;
    int bt_locator_rssi;
    uint32_t bt_locator_start_tick;
    uint32_t bt_locator_last_console_tick;
    char bt_locator_mac[18];
    char bt_locator_saved_mac[18];
    bool bt_locator_preserve_mac;
    bool bt_locator_list_loading;
    bool bt_locator_list_ready;
    size_t bt_locator_count;
    size_t bt_locator_index;
    size_t bt_locator_offset;
    int32_t bt_locator_selected; // -1 none, otherwise index
    char bt_locator_current_name[BT_LOCATOR_NAME_MAX_LEN];
    char* bt_locator_scroll_text;
    size_t bt_locator_scroll_offset;
    int bt_locator_scroll_dir;
    uint8_t bt_locator_scroll_hold;
    uint32_t bt_locator_scroll_last_tick;
    char* bt_locator_scan_line;
    size_t bt_locator_scan_len;
    BtLocatorDevice* bt_locator_devices;
    char* bt_scan_line_buffer;
    size_t bt_scan_line_length;
    bool deauth_guard_view_active;
    bool deauth_guard_full_console;
    bool deauth_guard_has_detection;
    bool deauth_guard_has_rssi;
    int deauth_guard_last_channel;
    int deauth_guard_last_rssi;
    uint32_t deauth_guard_started_tick;
    uint32_t deauth_guard_last_detection_tick;
    uint32_t deauth_guard_blink_until;
    uint32_t deauth_guard_last_blink_tick;
    bool deauth_guard_blink_state;
    uint8_t deauth_guard_blink_count;
    uint32_t deauth_guard_vibro_until;
    bool deauth_guard_vibro_on;
    char deauth_guard_last_ssid[SCAN_SSID_MAX_LEN];
    char deauth_guard_line_buffer[SCAN_LINE_BUFFER_SIZE];
    size_t deauth_guard_line_length;
    bool deauth_guard_monitoring;
    uint32_t deauth_guard_detection_count;
    bool last_command_sent;
    bool sniffer_view_active;
    bool sniffer_full_console;
    bool sniffer_running;
    bool sniffer_has_data;
    bool sniffer_scan_done;
    bool sniffer_results_active;
    bool sniffer_results_loading;
    uint32_t sniffer_packet_count;
    uint32_t sniffer_networks;
    uint32_t sniffer_last_update_tick;
    char sniffer_mode[12];
    char sniffer_line_buffer[96];
    size_t sniffer_line_length;
    SnifferApEntry* sniffer_aps;
    SnifferClientEntry* sniffer_clients;
    size_t sniffer_ap_count;
    size_t sniffer_client_count;
    size_t sniffer_results_ap_index;
    size_t sniffer_results_client_offset;
    char sniffer_selected_macs[SNIFFER_MAX_SELECTED_STATIONS][18];
    size_t sniffer_selected_count;
    bool probe_results_active;
    bool probe_full_console;
    bool probe_results_loading;
    ProbeSsidEntry* probe_ssids;
    SnifferClientEntry* probe_clients;
    size_t probe_ssid_count;
    size_t probe_client_count;
    size_t probe_ssid_index;
    size_t probe_client_offset;
    uint32_t probe_total;
    bool confirm_blackout_yes;
    bool confirm_sniffer_dos_yes;
    bool confirm_exit_yes;
    uint32_t last_attack_index;
    bool exit_confirm_active;
    uint32_t exit_confirm_tick;
    size_t evil_twin_menu_index;
    size_t evil_twin_menu_offset;
    EvilTwinHtmlEntry* evil_twin_html_entries;
    size_t evil_twin_html_count;
    size_t evil_twin_html_popup_index;
    size_t evil_twin_html_popup_offset;
    bool evil_twin_popup_active;
    bool evil_twin_listing_active;
    bool evil_twin_list_header_seen;
    char evil_twin_list_buffer[64];
    size_t evil_twin_list_length;
    uint8_t evil_twin_selected_html_id;
    char evil_twin_selected_html_name[EVIL_TWIN_HTML_NAME_MAX];
    EvilTwinStatus* evil_twin_status;
    EvilTwinPassEntry* evil_twin_pass_entries;
    size_t evil_twin_pass_count;
    size_t evil_twin_pass_index;
    size_t evil_twin_pass_offset;
    bool evil_twin_pass_listing_active;
    char evil_twin_pass_list_buffer[160];
    size_t evil_twin_pass_list_length;
    bool evil_twin_qr_valid;
    char evil_twin_qr_error[24];
    char evil_twin_qr_ssid[SCAN_SSID_MAX_LEN];
    char evil_twin_qr_pass[EVIL_TWIN_PASS_MAX_LEN];
    uint8_t* evil_twin_qr;
    uint8_t* evil_twin_qr_temp;
    bool help_qr_ready;
    bool help_qr_valid;
    char help_qr_error[24];
    uint8_t* help_qr;
    uint8_t* help_qr_temp;
    size_t portal_menu_index;
    char portal_ssid[SCAN_SSID_MAX_LEN];
    size_t portal_menu_offset;
    bool portal_input_requested;
    bool portal_input_active;
    PortalSsidEntry* portal_ssid_entries;
    size_t portal_ssid_count;
    size_t portal_ssid_popup_index;
    size_t portal_ssid_popup_offset;
    bool portal_ssid_popup_active;
    bool portal_ssid_listing_active;
    bool portal_ssid_list_header_seen;
    char portal_ssid_list_buffer[64];
    size_t portal_ssid_list_length;
    bool portal_ssid_missing;
    bool portal_running;
    bool portal_full_console;
    PortalStatus* portal_status;
    bool passwords_listing_active;
    AppScreen passwords_return_screen;
    AppScreen passwords_select_return_screen;
    PasswordsSource passwords_source;
    size_t passwords_select_index;
    char passwords_title[24];
    char (*passwords_lines)[PASSWORDS_LINE_CHAR_LIMIT];
    size_t passwords_line_count;
    size_t passwords_scroll;
    size_t passwords_scroll_x;
    size_t passwords_max_line_len;
    char passwords_line_buffer[PORTAL_STATUS_LINE_BUFFER];
    size_t passwords_line_length;
    bool evil_twin_running;
    bool evil_twin_full_console;
    size_t sd_folder_index;
    size_t sd_folder_offset;
    bool sd_folder_popup_active;
    SdFileEntry* sd_files;
    size_t sd_file_count;
    size_t sd_file_popup_index;
    size_t sd_file_popup_offset;
    bool sd_file_popup_active;
    bool sd_listing_active;
    bool sd_list_header_seen;
    char sd_list_buffer[SD_LIST_BUFFER_LEN];
    size_t sd_list_length;
    char sd_current_folder_label[SD_MANAGER_FOLDER_LABEL_MAX];
    char sd_current_folder_path[SD_MANAGER_PATH_MAX];
    char sd_delete_target_name[SD_MANAGER_FILE_NAME_MAX];
    char sd_delete_target_path[SD_MANAGER_PATH_MAX];
    bool sd_delete_confirm_active;
    bool sd_delete_confirm_yes;
    bool sd_refresh_pending;
    uint32_t sd_refresh_tick;
    uint8_t sd_scroll_offset;
    int8_t sd_scroll_direction;
    uint8_t sd_scroll_hold;
    uint32_t sd_scroll_last_tick;
    uint8_t sd_scroll_char_limit;
    char sd_scroll_text[SD_MANAGER_FILE_NAME_MAX];
    size_t karma_menu_index;
    size_t karma_menu_offset;
    KarmaProbeEntry* karma_probes;
    size_t karma_probe_count;
    size_t karma_probe_popup_index;
    size_t karma_probe_popup_offset;
    bool karma_probe_popup_active;
    bool karma_probe_listing_active;
    bool karma_probe_list_header_seen;
    char karma_probe_list_buffer[64];
    size_t karma_probe_list_length;
    uint8_t karma_selected_probe_id;
    char karma_selected_probe_name[KARMA_PROBE_NAME_MAX];
    KarmaHtmlEntry* karma_html_entries;
    size_t karma_html_count;
    size_t karma_html_popup_index;
    size_t karma_html_popup_offset;
    bool karma_html_popup_active;
    bool karma_html_listing_active;
    bool karma_html_list_header_seen;
    char karma_html_list_buffer[64];
    size_t karma_html_list_length;
    uint8_t karma_selected_html_id;
    char karma_selected_html_name[KARMA_HTML_NAME_MAX];
    bool karma_sniffer_running;
    uint32_t karma_sniffer_stop_tick;
    uint32_t karma_sniffer_duration_sec;
    bool karma_pending_probe_refresh;
    uint32_t karma_pending_probe_tick;
    bool karma_status_active;
    ScanResult* scan_results;
    size_t scan_results_capacity;
    size_t scan_result_count;
    size_t scan_result_index;
    size_t scan_result_offset;
    uint16_t* scan_selected_numbers;
    size_t scan_selected_count;
    bool scan_results_loading;
    char scan_line_buffer[SCAN_LINE_BUFFER_SIZE];
    size_t scan_line_len;
    uint16_t* visible_result_indices;
    size_t visible_result_count;
    bool scanner_view_active;
    bool scanner_full_console;
    bool scanner_scan_running;
    bool scanner_rescan_hint;
    uint16_t scanner_total;
    uint16_t scanner_band_24;
    uint16_t scanner_band_5;
    uint16_t scanner_open_count;
    uint16_t scanner_hidden_count;
    bool scanner_best_rssi_valid;
    int16_t scanner_best_rssi;
    char scanner_best_ssid[SCAN_SSID_MAX_LEN];
    VendorCacheEntry vendor_cache[VENDOR_CACHE_SIZE];
    uint32_t scanner_last_update_tick;
    bool scanner_show_ssid;
    bool scanner_show_bssid;
    bool scanner_show_channel;
    bool scanner_show_security;
    bool scanner_show_power;
    bool scanner_show_band;
    bool scanner_show_vendor;
    bool vendor_scan_enabled;
    bool vendor_read_pending;
    char vendor_read_buffer[32];
    size_t vendor_read_length;
    int16_t scanner_min_power;
    uint16_t scanner_min_channel_time;
    uint16_t scanner_max_channel_time;
    size_t scanner_timing_index;
    bool scanner_timing_min_pending;
    bool scanner_timing_max_pending;
    char scanner_timing_read_buffer[32];
    size_t scanner_timing_read_length;
    size_t scanner_setup_index;
    bool scanner_adjusting_power;
    bool backlight_enabled;
    bool show_ram_overlay;
    bool debug_mark_enabled;
    bool debug_log_initialized;
    size_t heap_start_free;
    bool backlight_insomnia;
    bool led_enabled;
    uint8_t led_level;
    size_t led_setup_index;
    bool boot_short_enabled;
    bool boot_long_enabled;
    uint8_t boot_short_command_index;
    uint8_t boot_long_command_index;
    bool boot_setup_long;
    size_t boot_setup_index;
    bool led_read_pending;
    char led_read_buffer[64];
    size_t led_read_length;
    size_t scanner_view_offset;
    uint8_t result_line_height;
    uint8_t result_char_limit;
    uint8_t result_max_lines;
    Font result_font;
    uint8_t result_scroll_offset;
    int8_t result_scroll_direction;
    uint8_t result_scroll_hold;
    uint32_t result_scroll_last_tick;
    char result_scroll_text[64];
    uint8_t overlay_title_scroll_offset;
    int8_t overlay_title_scroll_direction;
    uint8_t overlay_title_scroll_hold;
    uint32_t overlay_title_scroll_last_tick;
    char overlay_title_scroll_text[64];
    NotificationApp* notifications;
    bool backlight_notification_enforced;
    bool config_dirty;
    bool config_seen_led_enabled;
    bool config_seen_led_level;
    bool config_seen_gps_utc_offset;
    bool config_seen_gps_dst;
    bool config_seen_gps_zda_tz;
    char status_message[64];
    uint32_t status_message_until;
    bool status_message_fullscreen;
    bool board_ready_seen;
    bool board_sync_pending;
    bool board_ping_outstanding;
    uint32_t board_last_ping_tick;
    uint32_t board_last_rx_tick;
    bool board_missing_shown;
    bool board_bootstrap_active;
    bool board_bootstrap_reboot_sent;
    uint8_t board_ping_failures;
    uint32_t last_input_tick;
    uint32_t last_back_tick;
    uint32_t usb_guard_last_tick;
    bool package_monitor_active;
    uint8_t package_monitor_channel;
    uint16_t* package_monitor_history;
    size_t package_monitor_history_count;
    uint16_t package_monitor_last_value;
    bool package_monitor_dirty;
    char package_monitor_line_buffer[64];
    size_t package_monitor_line_length;
    uint32_t package_monitor_last_channel_tick;
    bool channel_view_active;
    bool channel_view_dirty;
    bool channel_view_has_data;
    bool channel_view_dataset_active;
    bool channel_view_has_status;
    ChannelViewBand channel_view_band;
    ChannelViewSection channel_view_section;
    uint16_t* channel_view_counts_24;
    uint16_t* channel_view_counts_5;
    uint16_t* channel_view_working_counts_24;
    uint16_t* channel_view_working_counts_5;
    char channel_view_line_buffer[CHANNEL_VIEW_LINE_BUFFER];
    size_t channel_view_line_length;
    char channel_view_status_text[32];
    uint8_t channel_view_offset_24;
    uint8_t channel_view_offset_5;
} SimpleApp;
static void simple_app_reset_result_scroll(SimpleApp* app);
static void simple_app_update_result_scroll(SimpleApp* app);
static void simple_app_reset_overlay_title_scroll(SimpleApp* app);
static void simple_app_update_overlay_title_scroll(SimpleApp* app);
static void simple_app_adjust_result_offset(SimpleApp* app);
static void simple_app_rebuild_visible_results(SimpleApp* app);
static bool simple_app_result_is_visible(const SimpleApp* app, const ScanResult* result);
static ScanResult* simple_app_visible_result(SimpleApp* app, size_t visible_index);
static const ScanResult*
    simple_app_visible_result_const(const SimpleApp* app, size_t visible_index);
static void simple_app_update_result_layout(SimpleApp* app);
static void simple_app_update_karma_duration_label(SimpleApp* app);
static void simple_app_update_selected_numbers(SimpleApp* app, const ScanResult* result);
static bool simple_app_bssid_is_empty(const uint8_t bssid[6]);
static bool simple_app_parse_bssid(const char* text, uint8_t out[6]);
static void simple_app_format_bssid(const uint8_t bssid[6], char* out, size_t out_size);
static uint8_t simple_app_parse_channel(const char* text);
static const char* simple_app_vendor_cache_lookup(const SimpleApp* app, const uint8_t bssid[6]);
static void
    simple_app_vendor_cache_update(SimpleApp* app, const uint8_t bssid[6], const char* vendor);
static bool simple_app_alloc_sniffer_buffers(SimpleApp* app);
static void simple_app_free_sniffer_buffers(SimpleApp* app);
static bool simple_app_alloc_probe_buffers(SimpleApp* app);
static void simple_app_free_probe_buffers(SimpleApp* app);
static uint8_t simple_app_parse_band_value(const char* band_field);
static const char* simple_app_band_label(uint8_t band);
static size_t simple_app_parse_scan_count(const char* line);
static bool simple_app_parse_bool_value(const char* value, bool current);
static void simple_app_apply_backlight(SimpleApp* app);
static void simple_app_toggle_backlight(SimpleApp* app);
static void simple_app_update_ram_label(SimpleApp* app);
static void simple_app_update_debug_label(SimpleApp* app);
static void simple_app_toggle_show_ram(SimpleApp* app);
static void simple_app_toggle_debug_mark(SimpleApp* app);
static void simple_app_update_led_label(SimpleApp* app);
static void simple_app_send_led_power_command(SimpleApp* app);
static void simple_app_send_led_level_command(SimpleApp* app);
static void simple_app_send_command_quiet(SimpleApp* app, const char* command);
static void simple_app_request_led_status(SimpleApp* app);
static bool simple_app_handle_led_status_line(SimpleApp* app, const char* line);
static void simple_app_led_feed(SimpleApp* app, char ch);
static void simple_app_update_boot_labels(SimpleApp* app);
static void simple_app_send_boot_status(SimpleApp* app, bool is_short, bool enabled);
static void simple_app_send_boot_command(SimpleApp* app, bool is_short, uint8_t command_index);
static void simple_app_request_boot_status(SimpleApp* app);
static bool simple_app_handle_boot_status_line(SimpleApp* app, const char* line);
static void simple_app_boot_feed(SimpleApp* app, char ch);
static void
    simple_app_serial_irq(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context);
static void simple_app_handle_boot_trigger(SimpleApp* app, bool is_long);
static void simple_app_on_board_online(SimpleApp* app, const char* source);
static void simple_app_draw_sync_status(const SimpleApp* app, Canvas* canvas);
static void simple_app_draw_ram_overlay(SimpleApp* app, Canvas* canvas);
static void simple_app_send_vendor_command(SimpleApp* app, bool enable);
static void simple_app_request_vendor_status(SimpleApp* app);
static bool simple_app_handle_vendor_status_line(SimpleApp* app, const char* line);
static void simple_app_vendor_feed(SimpleApp* app, char ch);
static void simple_app_request_scanner_timing(SimpleApp* app);
static void simple_app_request_channel_time(SimpleApp* app, bool request_min);
static void simple_app_send_channel_time(SimpleApp* app, bool is_min, uint16_t value);
static bool simple_app_handle_scanner_timing_line(SimpleApp* app, const char* line);
static void simple_app_scanner_timing_feed(SimpleApp* app, char ch);
static void simple_app_modify_channel_time(SimpleApp* app, bool is_min, int32_t delta);
static void simple_app_reset_wardrive_status(SimpleApp* app);
static void simple_app_process_wardrive_line(SimpleApp* app, const char* line);
static void simple_app_wardrive_feed(SimpleApp* app, char ch);
static void simple_app_reset_gps_status(SimpleApp* app);
static void simple_app_process_gps_line(SimpleApp* app, const char* line);
static void simple_app_gps_feed(SimpleApp* app, char ch);
static void simple_app_reset_blackout_status(SimpleApp* app);
static void simple_app_process_blackout_line(SimpleApp* app, const char* line);
static void simple_app_blackout_feed(SimpleApp* app, char ch);
static void simple_app_draw_blackout_overlay(SimpleApp* app, Canvas* canvas);
static void simple_app_reset_sniffer_dog_status(SimpleApp* app);
static void simple_app_process_sniffer_dog_line(SimpleApp* app, const char* line);
static void simple_app_sniffer_dog_feed(SimpleApp* app, char ch);
static void simple_app_draw_sniffer_dog_overlay(SimpleApp* app, Canvas* canvas);
static void simple_app_reset_handshake_status(SimpleApp* app);
static void simple_app_process_handshake_line(SimpleApp* app, const char* line);
static void simple_app_handshake_feed(SimpleApp* app, char ch);
static void simple_app_draw_handshake_overlay(SimpleApp* app, Canvas* canvas);
static void simple_app_reset_sae_status(SimpleApp* app);
static void simple_app_process_sae_line(SimpleApp* app, const char* line);
static void simple_app_sae_feed(SimpleApp* app, char ch);
static void simple_app_draw_sae_overlay(SimpleApp* app, Canvas* canvas);
static void simple_app_reset_deauth_status(SimpleApp* app);
static void simple_app_process_deauth_line(SimpleApp* app, const char* line);
static void simple_app_deauth_feed(SimpleApp* app, char ch);
static void simple_app_draw_deauth_overlay(SimpleApp* app, Canvas* canvas);
static void simple_app_update_otg_label(SimpleApp* app);
static void simple_app_apply_otg_power(SimpleApp* app);
static void simple_app_toggle_otg_power(SimpleApp* app);
static void simple_app_send_download(SimpleApp* app);
static bool simple_app_usb_profile_ok(void);
static void simple_app_show_usb_blocker(void);
static bool simple_app_is_start_sniffer_command(const char* command);
static bool simple_app_is_start_sniffer_noscan_command(const char* command);
static void simple_app_send_start_sniffer(SimpleApp* app);
static void simple_app_send_resume_sniffer(SimpleApp* app);
static void simple_app_send_command(SimpleApp* app, const char* command, bool go_to_serial);
static void simple_app_append_serial_data(SimpleApp* app, const uint8_t* data, size_t length);
static void simple_app_draw_wardrive_serial(SimpleApp* app, Canvas* canvas);
static void simple_app_draw_gps(SimpleApp* app, Canvas* canvas);
static void simple_app_mark_config_dirty(SimpleApp* app);
static void simple_app_package_monitor_enter(SimpleApp* app);
static void simple_app_package_monitor_start(SimpleApp* app, uint8_t channel, bool reset_history);
static void simple_app_package_monitor_stop(SimpleApp* app);
static void simple_app_package_monitor_reset(SimpleApp* app);
static void simple_app_package_monitor_process_line(SimpleApp* app, const char* line);
static void simple_app_package_monitor_feed(SimpleApp* app, char ch);
static void simple_app_draw_package_monitor(SimpleApp* app, Canvas* canvas);
static void simple_app_handle_package_monitor_input(SimpleApp* app, InputKey key);
static bool simple_app_alloc_package_monitor_buffers(SimpleApp* app);
static void simple_app_free_package_monitor_buffers(SimpleApp* app);
static bool simple_app_alloc_channel_view_buffers(SimpleApp* app);
static void simple_app_free_channel_view_buffers(SimpleApp* app);
static void simple_app_channel_view_enter(SimpleApp* app);
static void simple_app_channel_view_start(SimpleApp* app);
static void simple_app_channel_view_stop(SimpleApp* app);
static void simple_app_channel_view_reset(SimpleApp* app);
static void simple_app_channel_view_begin_dataset(SimpleApp* app);
static void simple_app_channel_view_commit_dataset(SimpleApp* app);
static void simple_app_channel_view_process_line(SimpleApp* app, const char* line);
static void simple_app_channel_view_feed(SimpleApp* app, char ch);
static void simple_app_draw_channel_view(SimpleApp* app, Canvas* canvas);
static void simple_app_handle_channel_view_input(SimpleApp* app, InputKey key);
static void simple_app_bt_locator_reset_list(SimpleApp* app);
static void simple_app_bt_locator_begin_scan(SimpleApp* app);
static void simple_app_bt_locator_feed(SimpleApp* app, char ch);
static void simple_app_draw_bt_locator_list(SimpleApp* app, Canvas* canvas);
static void simple_app_handle_bt_locator_input(SimpleApp* app, InputKey key);
static void simple_app_bt_locator_reset_scroll(SimpleApp* app);
static void simple_app_bt_locator_set_scroll_text(SimpleApp* app, const char* text);
static void simple_app_bt_locator_tick_scroll(SimpleApp* app, size_t char_limit);
static void simple_app_usb_runtime_guard(SimpleApp* app);
static bool simple_app_sd_ok(void);
static void simple_app_show_sd_busy(void);
static void simple_app_ensure_flipper_sd_dirs(Storage* storage);
static bool simple_app_append_log_line(const char* path, const char* line);
static void simple_app_debug_mark(SimpleApp* app, const char* tag);
static void simple_app_log_cli_command(SimpleApp* app, const char* source, const char* command);
static void simple_app_reset_debug_log(SimpleApp* app);
static bool simple_app_scan_alloc_buffers(SimpleApp* app, size_t capacity);
static void simple_app_scan_free_buffers(SimpleApp* app);
static void simple_app_prepare_scan_buffers(SimpleApp* app);
static void simple_app_prepare_password_buffers(SimpleApp* app);
static bool simple_app_portal_alloc_ssid_entries(SimpleApp* app);
static void simple_app_portal_free_ssid_entries(SimpleApp* app);
static void simple_app_trim(char* text);
static bool simple_app_sd_alloc_files(SimpleApp* app);
static void simple_app_sd_free_files(SimpleApp* app);
static bool simple_app_evil_twin_alloc_html_entries(SimpleApp* app);
static void simple_app_evil_twin_free_html_entries(SimpleApp* app);
static bool simple_app_karma_alloc_probes(SimpleApp* app);
static void simple_app_karma_free_probes(SimpleApp* app);
static bool simple_app_karma_alloc_html_entries(SimpleApp* app);
static void simple_app_karma_free_html_entries(SimpleApp* app);
static void simple_app_channel_view_show_status(SimpleApp* app, const char* status);
static void simple_app_reset_deauth_guard(SimpleApp* app);
static void simple_app_start_deauth_guard(SimpleApp* app);
static void simple_app_process_deauth_guard_line(SimpleApp* app, const char* line);
static void simple_app_deauth_guard_feed(SimpleApp* app, char ch);
static void simple_app_update_deauth_guard(SimpleApp* app);
static void simple_app_draw_deauth_guard(SimpleApp* app, Canvas* canvas);
static int
    simple_app_channel_view_find_channel_index(const uint8_t* list, size_t count, uint8_t channel);
static size_t simple_app_channel_view_visible_columns(ChannelViewBand band);
static uint8_t simple_app_channel_view_max_offset(ChannelViewBand band);
static uint8_t* simple_app_channel_view_offset_ptr(SimpleApp* app, ChannelViewBand band);
static bool simple_app_channel_view_adjust_offset(SimpleApp* app, ChannelViewBand band, int delta);
static void simple_app_draw_portal_menu(SimpleApp* app, Canvas* canvas);
static void simple_app_handle_portal_menu_input(SimpleApp* app, InputKey key);
static void simple_app_reset_portal_ssid_listing(SimpleApp* app);
static void simple_app_request_portal_ssid_list(SimpleApp* app);
static void simple_app_copy_portal_ssid(SimpleApp* app, const char* source);
static void simple_app_portal_prompt_ssid(SimpleApp* app);
static void simple_app_portal_sync_offset(SimpleApp* app);
static void simple_app_open_portal_ssid_popup(SimpleApp* app);
static void simple_app_finish_portal_ssid_listing(SimpleApp* app);
static void simple_app_process_portal_ssid_line(SimpleApp* app, const char* line);
static void simple_app_portal_ssid_feed(SimpleApp* app, char ch);
static void simple_app_reset_portal_status(SimpleApp* app);
static void simple_app_process_portal_status_line(SimpleApp* app, const char* line);
static void simple_app_portal_status_feed(SimpleApp* app, char ch);
static void simple_app_draw_portal_overlay(SimpleApp* app, Canvas* canvas);
static bool simple_app_portal_status_ensure(SimpleApp* app);
static void simple_app_portal_status_free(SimpleApp* app);
static void simple_app_reset_evil_twin_status(SimpleApp* app);
static void simple_app_process_evil_twin_status_line(SimpleApp* app, const char* line);
static void simple_app_evil_twin_status_feed(SimpleApp* app, char ch);
static void simple_app_draw_evil_twin_overlay(SimpleApp* app, Canvas* canvas);
static bool simple_app_evil_twin_status_ensure(SimpleApp* app);
static void simple_app_evil_twin_status_free(SimpleApp* app);
static void simple_app_request_evil_twin_pass_list(SimpleApp* app);
static void simple_app_reset_evil_twin_pass_listing(SimpleApp* app);
static void simple_app_process_evil_twin_pass_line(SimpleApp* app, const char* line);
static void simple_app_evil_twin_pass_feed(SimpleApp* app, char ch);
static void simple_app_draw_evil_twin_pass_list(SimpleApp* app, Canvas* canvas);
static void simple_app_handle_evil_twin_pass_list_input(SimpleApp* app, InputKey key);
static void simple_app_draw_evil_twin_pass_qr(SimpleApp* app, Canvas* canvas);
static void simple_app_handle_evil_twin_pass_qr_input(SimpleApp* app, InputKey key);
static bool simple_app_build_evil_twin_qr(SimpleApp* app, const char* ssid, const char* pass);
static void simple_app_reset_passwords_listing(SimpleApp* app);
static bool simple_app_passwords_alloc_lines(SimpleApp* app);
static void simple_app_passwords_free_lines(SimpleApp* app);
static void
    simple_app_request_passwords(SimpleApp* app, AppScreen return_screen, PasswordsSource source);
static void simple_app_process_passwords_line(SimpleApp* app, const char* line);
static void simple_app_passwords_feed(SimpleApp* app, char ch);
static void simple_app_draw_passwords(SimpleApp* app, Canvas* canvas);
static void simple_app_handle_passwords_input(SimpleApp* app, InputKey key);
static void simple_app_draw_passwords_select(SimpleApp* app, Canvas* canvas);
static void simple_app_handle_passwords_select_input(SimpleApp* app, InputKey key);
static void simple_app_draw_portal_ssid_popup(SimpleApp* app, Canvas* canvas);
static void simple_app_handle_portal_ssid_popup_event(SimpleApp* app, const InputEvent* event);
static bool simple_app_portal_run_text_input(SimpleApp* app);
static void simple_app_portal_text_input_result(void* context);
static bool simple_app_portal_text_input_navigation(void* context);
static void simple_app_start_portal(SimpleApp* app);
static void simple_app_open_sd_manager(SimpleApp* app);
static void simple_app_copy_sd_folder(SimpleApp* app, const SdFolderOption* folder);
static void simple_app_draw_setup_sd_manager(SimpleApp* app, Canvas* canvas);
static void simple_app_handle_setup_sd_manager_input(SimpleApp* app, InputKey key);
static void simple_app_prepare_help_qr(SimpleApp* app);
static void simple_app_draw_help_qr(SimpleApp* app, Canvas* canvas);
static void simple_app_handle_help_qr_input(SimpleApp* app, InputKey key);
static void simple_app_draw_sd_folder_popup(SimpleApp* app, Canvas* canvas);
static void simple_app_handle_sd_folder_popup_event(SimpleApp* app, const InputEvent* event);
static void simple_app_request_sd_listing(SimpleApp* app, const SdFolderOption* folder);
static void simple_app_reset_sd_listing(SimpleApp* app);
static void simple_app_finish_sd_listing(SimpleApp* app);
static void simple_app_process_sd_line(SimpleApp* app, const char* line);
static void simple_app_sd_feed(SimpleApp* app, char ch);
static void simple_app_draw_sd_file_popup(SimpleApp* app, Canvas* canvas);
static void simple_app_handle_sd_file_popup_event(SimpleApp* app, const InputEvent* event);
static void simple_app_draw_sd_delete_confirm(SimpleApp* app, Canvas* canvas);
static void simple_app_handle_sd_delete_confirm_event(SimpleApp* app, const InputEvent* event);
static void simple_app_reset_sd_scroll(SimpleApp* app);
static void simple_app_sd_scroll_buffer(
    SimpleApp* app,
    const char* text,
    size_t char_limit,
    char* out,
    size_t out_size);
static void simple_app_update_sd_scroll(SimpleApp* app);
static void simple_app_update_deauth_blink(SimpleApp* app);
static void simple_app_update_handshake_blink(SimpleApp* app);
static void simple_app_draw_evil_twin_menu(SimpleApp* app, Canvas* canvas);
static void simple_app_handle_evil_twin_menu_input(SimpleApp* app, InputKey key);
static void
    simple_app_draw_scroll_arrow(Canvas* canvas, uint8_t base_left_x, int16_t base_y, bool upwards);
static uint8_t simple_app_bt_locator_item_height(const SimpleApp* app, size_t idx);
static void simple_app_bt_locator_ensure_visible(SimpleApp* app);
static void simple_app_draw_scroll_hints(
    Canvas* canvas,
    uint8_t base_left_x,
    int16_t content_top_y,
    int16_t content_bottom_y,
    bool show_up,
    bool show_down);
static void simple_app_draw_scroll_hints_clamped(
    Canvas* canvas,
    uint8_t base_left_x,
    int16_t content_top_y,
    int16_t content_bottom_y,
    bool show_up,
    bool show_down,
    int16_t min_base_y,
    int16_t max_base_y);
static void simple_app_request_evil_twin_html_list(SimpleApp* app);
static void simple_app_start_evil_portal(SimpleApp* app);
static void simple_app_draw_evil_twin_popup(SimpleApp* app, Canvas* canvas);
static void simple_app_handle_evil_twin_popup_event(SimpleApp* app, const InputEvent* event);
static void simple_app_close_evil_twin_popup(SimpleApp* app);
static void simple_app_reset_evil_twin_listing(SimpleApp* app);
static void simple_app_finish_evil_twin_listing(SimpleApp* app);
static void simple_app_process_evil_twin_line(SimpleApp* app, const char* line);
static void simple_app_evil_twin_feed(SimpleApp* app, char ch);
static void simple_app_draw_karma_menu(SimpleApp* app, Canvas* canvas);
static void simple_app_handle_karma_menu_input(SimpleApp* app, InputKey key);
static void simple_app_start_karma_sniffer(SimpleApp* app);
static void simple_app_start_karma_attack(SimpleApp* app);
static void simple_app_request_karma_probe_list(SimpleApp* app);
static void simple_app_reset_karma_probe_listing(SimpleApp* app);
static void simple_app_finish_karma_probe_listing(SimpleApp* app);
static void simple_app_process_karma_probe_line(SimpleApp* app, const char* line);
static void simple_app_karma_probe_feed(SimpleApp* app, char ch);
static void simple_app_draw_karma_probe_popup(SimpleApp* app, Canvas* canvas);
static void simple_app_handle_karma_probe_popup_event(SimpleApp* app, const InputEvent* event);
static void simple_app_request_karma_html_list(SimpleApp* app);
static void simple_app_reset_karma_html_listing(SimpleApp* app);
static void simple_app_finish_karma_html_listing(SimpleApp* app);
static void simple_app_process_karma_html_line(SimpleApp* app, const char* line);
static void simple_app_karma_html_feed(SimpleApp* app, char ch);
static void simple_app_draw_karma_html_popup(SimpleApp* app, Canvas* canvas);
static void simple_app_handle_karma_html_popup_event(SimpleApp* app, const InputEvent* event);
static void simple_app_update_karma_sniffer(SimpleApp* app);
static void simple_app_draw_setup_karma(SimpleApp* app, Canvas* canvas);
static void simple_app_handle_setup_karma_input(SimpleApp* app, InputKey key);
static void simple_app_draw_setup_led(SimpleApp* app, Canvas* canvas);
static void simple_app_handle_setup_led_input(SimpleApp* app, InputKey key);
static void simple_app_modify_karma_duration(SimpleApp* app, int32_t delta);
static void simple_app_save_config_if_dirty(SimpleApp* app, const char* message, bool fullscreen);
static bool simple_app_save_config(SimpleApp* app, const char* success_message, bool fullscreen);
static void simple_app_load_config(SimpleApp* app);
static void simple_app_show_status_message(
    SimpleApp* app,
    const char* message,
    uint32_t duration_ms,
    bool fullscreen);
static void simple_app_clear_status_message(SimpleApp* app);
static bool simple_app_status_message_is_active(SimpleApp* app);
static void simple_app_send_command_with_targets(SimpleApp* app, const char* base_command);
static size_t simple_app_menu_visible_count(const SimpleApp* app, uint32_t section_index);
static size_t simple_app_section_visible_count(void);
static void simple_app_adjust_section_offset(SimpleApp* app);
static void simple_app_prepare_exit(SimpleApp* app);
static void simple_app_update_scroll(SimpleApp* app);
static size_t simple_app_render_display_lines(
    SimpleApp* app,
    size_t skip_lines,
    char dest[][64],
    size_t max_lines);
static void simple_app_send_ping(SimpleApp* app);
static void simple_app_handle_pong(SimpleApp* app);
static void simple_app_ping_watchdog(SimpleApp* app);

typedef struct {
    const char* label;
    const char* command;
    MenuAction action;
} MenuEntry;

typedef struct {
    const char* title;
    const MenuEntry* entries;
    size_t entry_count;
    uint8_t display_y;
    uint8_t display_height;
} MenuSection;

static const uint8_t image_icon_0_bits[] = {
    0xff, 0x03, 0xff, 0x03, 0xff, 0x03, 0x11, 0x03, 0xdd, 0x03,
    0x1d, 0x03, 0x71, 0x03, 0x1f, 0x03, 0xff, 0x03, 0xff, 0x03,
};

static const SdFolderOption sd_folder_options[] = {
    {"Handshakes", "lab/handshakes"},
    {"HTML portals", "lab/htmls"},
    {"Wardrives", "lab/wardrives"},
};

static const size_t sd_folder_option_count =
    sizeof(sd_folder_options) / sizeof(sd_folder_options[0]);

static const MenuEntry menu_entries_sniffers[] = {
    {"Start Sniffer", "start_sniffer", MenuActionCommand},
    {"Show Sniffer Results", "show_sniffer_results", MenuActionCommand},
    {"Show Probes", "show_probes", MenuActionCommand},
    {"Resume Sniffer", "start_sniffer_noscan", MenuActionCommand},
    {"Sniffer Debug", "sniffer_debug 1", MenuActionCommand},
};

static const MenuEntry menu_entries_attacks[] = {
    {"Blackout", NULL, MenuActionConfirmBlackout},
    {"Deauth", "start_deauth", MenuActionCommandWithTargets},
    {"Deauth Guard", "deauth_detector", MenuActionCommandWithTargets},
    {"Handshaker", "start_handshake", MenuActionCommandWithTargets},
    {"Evil Twin", NULL, MenuActionOpenEvilTwinMenu},
    {"Portal", NULL, MenuActionOpenPortalMenu},
    {"Karma", NULL, MenuActionOpenKarmaMenu},
    {"Sniffer", "start_sniffer", MenuActionCommand},
    {"SAE Overflow", "sae_overflow", MenuActionCommandWithTargets},
    {"Sniffer Dog", NULL, MenuActionConfirmSnifferDos},
    {"Wardrive", "start_wardrive", MenuActionCommand},
};

static const MenuEntry menu_entries_monitoring[] = {
    {"Package Monitor", NULL, MenuActionOpenPackageMonitor},
    {"Channel View", NULL, MenuActionOpenChannelView},
};

static const MenuEntry menu_entries_bluetooth[] = {
    {"AirTag Scan", "scan_airtag", MenuActionCommand},
    {"BT Scan", "scan_bt", MenuActionCommand},
    {"BT Locator", NULL, MenuActionOpenBtLocator},
};

static char menu_label_backlight[24] = "Backlight: On";
static char menu_label_otg_power[24] = "5V Power: On";
static char menu_label_led[24] = "LED: On (10)";
static char menu_label_boot_short[40] = "Boot short: Off";
static char menu_label_boot_long[40] = "Boot long: Off";
static char menu_label_ram[24] = "Show RAM: Off";
static char menu_label_debug[24] = "Debug Log: Off";

static const MenuEntry menu_entries_setup[] = {
    {menu_label_backlight, NULL, MenuActionToggleBacklight},
    {menu_label_boot_long, NULL, MenuActionOpenBootSetup},
    {menu_label_boot_short, NULL, MenuActionOpenBootSetup},
    {"Console", NULL, MenuActionOpenConsole},
    {"Download Mode", NULL, MenuActionOpenDownload},
    {"Scanner Filters", NULL, MenuActionOpenScannerSetup},
    {"Scanner Timing", NULL, MenuActionOpenScannerTiming},
    {"GPS", NULL, MenuActionOpenGps},
    {"Help", NULL, MenuActionOpenHelp},
    {menu_label_led, NULL, MenuActionOpenLedSetup},
    {menu_label_otg_power, NULL, MenuActionToggleOtgPower},
    {menu_label_ram, NULL, MenuActionToggleShowRam},
    {menu_label_debug, NULL, MenuActionToggleDebugMark},
    {"SD Manager", NULL, MenuActionOpenSdManager},
};

static const MenuSection menu_sections[] = {
    {"Scanner", NULL, 0, 10, MENU_VISIBLE_COUNT* MENU_ITEM_SPACING},
    {"Sniffers",
     menu_entries_sniffers,
     sizeof(menu_entries_sniffers) / sizeof(menu_entries_sniffers[0]),
     18,
     MENU_VISIBLE_COUNT_SNIFFERS* MENU_ITEM_SPACING},
    {"Attacks",
     menu_entries_attacks,
     sizeof(menu_entries_attacks) / sizeof(menu_entries_attacks[0]),
     26,
     MENU_VISIBLE_COUNT_ATTACKS* MENU_ITEM_SPACING},
    {"Monitoring",
     menu_entries_monitoring,
     sizeof(menu_entries_monitoring) / sizeof(menu_entries_monitoring[0]),
     34,
     MENU_VISIBLE_COUNT* MENU_ITEM_SPACING},
    {"Bluetooth",
     menu_entries_bluetooth,
     sizeof(menu_entries_bluetooth) / sizeof(menu_entries_bluetooth[0]),
     42,
     MENU_VISIBLE_COUNT* MENU_ITEM_SPACING},
    {"Setup",
     menu_entries_setup,
     sizeof(menu_entries_setup) / sizeof(menu_entries_setup[0]),
     50,
     MENU_VISIBLE_COUNT_SETUP* MENU_ITEM_SPACING},
};

static const size_t menu_section_count = sizeof(menu_sections) / sizeof(menu_sections[0]);

static size_t simple_app_menu_visible_count(const SimpleApp* app, uint32_t section_index) {
    UNUSED(app);
    if(section_index == MENU_SECTION_SNIFFERS) {
        return MENU_VISIBLE_COUNT_SNIFFERS;
    }
    if(section_index == MENU_SECTION_ATTACKS) {
        return MENU_VISIBLE_COUNT_ATTACKS;
    }
    if(section_index == MENU_SECTION_SETUP) {
        return MENU_VISIBLE_COUNT_SETUP;
    }
    return MENU_VISIBLE_COUNT;
}

static size_t simple_app_section_visible_count(void) {
    return (menu_section_count < MENU_SECTION_VISIBLE_COUNT) ? menu_section_count :
                                                               MENU_SECTION_VISIBLE_COUNT;
}

static void simple_app_adjust_section_offset(SimpleApp* app) {
    if(!app) return;
    size_t visible = simple_app_section_visible_count();
    if(visible == 0) {
        app->section_offset = 0;
        return;
    }

    if(app->section_index >= menu_section_count) {
        app->section_index = (menu_section_count > 0) ? (menu_section_count - 1) : 0;
    }

    size_t max_offset = (menu_section_count > visible) ? (menu_section_count - visible) : 0;
    if(app->section_offset > max_offset) {
        app->section_offset = max_offset;
    }

    if(app->section_index < app->section_offset) {
        app->section_offset = app->section_index;
    } else if(app->section_index >= app->section_offset + visible) {
        app->section_offset = app->section_index - visible + 1;
    }
}

static void simple_app_focus_attacks_menu(SimpleApp* app) {
    if(!app) return;
    app->screen = ScreenMenu;
    app->menu_state = MenuStateItems;
    app->section_index = MENU_SECTION_ATTACKS;
    const MenuSection* section = &menu_sections[MENU_SECTION_ATTACKS];
    if(section->entry_count == 0) {
        app->item_index = 0;
        app->item_offset = 0;
        return;
    }
    size_t entry_count = section->entry_count;
    if(app->last_attack_index >= entry_count) {
        app->last_attack_index = entry_count - 1;
    }
    app->item_index = app->last_attack_index;
    size_t visible_count = simple_app_menu_visible_count(app, MENU_SECTION_ATTACKS);
    if(visible_count == 0) visible_count = 1;
    if(app->item_index >= entry_count) {
        app->item_index = entry_count - 1;
    }
    if(entry_count <= visible_count) {
        app->item_offset = 0;
    } else {
        size_t max_offset = entry_count - visible_count;
        size_t desired_offset =
            (app->item_index >= visible_count) ? (app->item_index - visible_count + 1) : 0;
        if(desired_offset > max_offset) {
            desired_offset = max_offset;
        }
        app->item_offset = desired_offset;
    }
}

static void simple_app_truncate_text(char* text, size_t max_chars) {
    if(!text || max_chars == 0) return;
    size_t len = strlen(text);
    if(len <= max_chars) return;
    if(max_chars == 1) {
        text[0] = '\0';
        return;
    }
    text[max_chars - 1] = '.';
    text[max_chars] = '\0';
}

static void simple_app_utf8_to_ascii_pl(const char* src, char* dst, size_t dst_size) {
    if(!dst || dst_size == 0) return;
    if(!src) {
        dst[0] = '\0';
        return;
    }

    size_t out = 0;
    for(size_t i = 0; src[i] != '\0' && out + 1 < dst_size;) {
        uint8_t b0 = (uint8_t)src[i];
        if(b0 < 0x80) {
            dst[out++] = (char)b0;
            i++;
            continue;
        }

        uint8_t b1 = (uint8_t)src[i + 1];
        if(b1 == 0) {
            dst[out++] = '?';
            break;
        }

        char mapped = '\0';
        if(b0 == 0xC4) {
            if(b1 == 0x84)
                mapped = 'A'; // Ą
            else if(b1 == 0x85)
                mapped = 'a'; // ą
            else if(b1 == 0x86)
                mapped = 'C'; // Ć
            else if(b1 == 0x87)
                mapped = 'c'; // ć
            else if(b1 == 0x98)
                mapped = 'E'; // Ę
            else if(b1 == 0x99)
                mapped = 'e'; // ę
        } else if(b0 == 0xC5) {
            if(b1 == 0x81)
                mapped = 'L'; // Ł
            else if(b1 == 0x82)
                mapped = 'l'; // ł
            else if(b1 == 0x83)
                mapped = 'N'; // Ń
            else if(b1 == 0x84)
                mapped = 'n'; // ń
            else if(b1 == 0x9A)
                mapped = 'S'; // Ś
            else if(b1 == 0x9B)
                mapped = 's'; // ś
            else if(b1 == 0xB9)
                mapped = 'Z'; // Ź
            else if(b1 == 0xBA)
                mapped = 'z'; // ź
            else if(b1 == 0xBB)
                mapped = 'Z'; // Ż
            else if(b1 == 0xBC)
                mapped = 'z'; // ż
        } else if(b0 == 0xC3) {
            if(b1 == 0x93)
                mapped = 'O'; // Ó
            else if(b1 == 0xB3)
                mapped = 'o'; // ó
        }

        if(mapped != '\0') {
            dst[out++] = mapped;
            i += 2;
        } else {
            dst[out++] = '?';
            i += 1;
        }
    }

    dst[out] = '\0';
}

static void simple_app_draw_scroll_arrow(
    Canvas* canvas,
    uint8_t base_left_x,
    int16_t base_y,
    bool upwards) {
    if(!canvas) return;

    if(upwards) {
        if(base_y < 2) base_y = 2;
    } else {
        if(base_y > 61) base_y = 61;
    }

    if(base_y < 0) base_y = 0;
    if(base_y > 63) base_y = 63;

    uint8_t base = (uint8_t)base_y;
    uint8_t base_right_x = (uint8_t)(base_left_x + 4);
    uint8_t apex_x = (uint8_t)(base_left_x + 2);
    uint8_t apex_y = upwards ? (uint8_t)(base - 2) : (uint8_t)(base + 2);

    canvas_draw_line(canvas, base_left_x, base, base_right_x, base);
    canvas_draw_line(canvas, base_left_x, base, apex_x, apex_y);
    canvas_draw_line(canvas, base_right_x, base, apex_x, apex_y);
}

static void simple_app_draw_scroll_hints_clamped(
    Canvas* canvas,
    uint8_t base_left_x,
    int16_t content_top_y,
    int16_t content_bottom_y,
    bool show_up,
    bool show_down,
    int16_t min_base_y,
    int16_t max_base_y) {
    if(!canvas || (!show_up && !show_down)) return;

    if(min_base_y > max_base_y) {
        int16_t tmp = min_base_y;
        min_base_y = max_base_y;
        max_base_y = tmp;
    }

    int16_t up_base = content_top_y - 4;
    int16_t down_base = content_bottom_y + 4;

    if(up_base < min_base_y) up_base = min_base_y;
    if(up_base > max_base_y) up_base = max_base_y;

    if(down_base < min_base_y) down_base = min_base_y;
    if(down_base > max_base_y) down_base = max_base_y;

    if(show_up) {
        simple_app_draw_scroll_arrow(canvas, base_left_x, up_base, true);
    }

    if(show_down) {
        if(show_up && down_base <= up_base) {
            down_base = up_base + 4;
            if(down_base > max_base_y) down_base = max_base_y;
        }
        if(!show_up && down_base < min_base_y) {
            down_base = min_base_y;
        }
        simple_app_draw_scroll_arrow(canvas, base_left_x, down_base, false);
    }
}

static void simple_app_draw_scroll_hints(
    Canvas* canvas,
    uint8_t base_left_x,
    int16_t content_top_y,
    int16_t content_bottom_y,
    bool show_up,
    bool show_down) {
    simple_app_draw_scroll_hints_clamped(
        canvas, base_left_x, content_top_y, content_bottom_y, show_up, show_down, 10, 60);
}

static uint8_t simple_app_bt_locator_item_height(const SimpleApp* app, size_t idx) {
    if(!app || !app->bt_locator_devices || idx >= app->bt_locator_count) return 1;
    const char* name = app->bt_locator_devices[idx].name;
    return (name && name[0] != '\0') ? 2 : 1;
}

static void simple_app_bt_locator_ensure_visible(SimpleApp* app) {
    if(!app || !app->bt_locator_devices || app->bt_locator_count == 0) return;

    if(app->bt_locator_index >= app->bt_locator_count) {
        app->bt_locator_index = app->bt_locator_count - 1;
    }

    int16_t list_top = 8;
    int16_t list_bottom = DISPLAY_HEIGHT - 2;
    uint8_t line_height = SERIAL_TEXT_LINE_HEIGHT;
    size_t max_lines = (list_bottom - list_top) / line_height;
    if(max_lines == 0) max_lines = 1;

    if(app->bt_locator_offset >= app->bt_locator_count) {
        app->bt_locator_offset = 0;
    }

    // Check if current window already contains the selection
    size_t lines = 0;
    bool visible = false;
    for(size_t i = app->bt_locator_offset; i < app->bt_locator_count; i++) {
        size_t h = simple_app_bt_locator_item_height(app, i);
        if(lines + h > max_lines) break;
        if(i == app->bt_locator_index) {
            visible = true;
            break;
        }
        lines += h;
    }
    if(visible) return;

    // Find the earliest offset that still shows the selection within available lines
    size_t best_offset = app->bt_locator_offset;
    for(size_t candidate = 0; candidate <= app->bt_locator_index; candidate++) {
        lines = 0;
        visible = false;
        for(size_t i = candidate; i < app->bt_locator_count; i++) {
            size_t h = simple_app_bt_locator_item_height(app, i);
            if(lines + h > max_lines) break;
            lines += h;
            if(i == app->bt_locator_index) {
                visible = true;
                break;
            }
        }
        if(visible) {
            best_offset = candidate;
            break;
        }
    }
    app->bt_locator_offset = best_offset;
}

static void
    simple_app_copy_field(char* dest, size_t dest_size, const char* src, const char* fallback) {
    if(dest_size == 0 || !dest) return;
    const char* value = (src && src[0] != '\0') ? src : fallback;
    if(!value) value = "";
    size_t max_copy = dest_size - 1;
    if(max_copy == 0) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, value, max_copy);
    dest[max_copy] = '\0';
}

static void
    simple_app_bt_scan_preview_insert(SimpleApp* app, const char* mac, int rssi, const char* name) {
    if(!app || !mac || !app->bt_scan_preview) return;
    if(app->bt_scan_preview_count >= BT_SCAN_PREVIEW_MAX) return;
    BtScanPreview* slot = &app->bt_scan_preview[app->bt_scan_preview_count++];
    strncpy(slot->mac, mac, sizeof(slot->mac) - 1);
    slot->mac[sizeof(slot->mac) - 1] = '\0';
    slot->rssi = rssi;
    slot->has_name = (name && name[0] != '\0');
    if(slot->has_name) {
        strncpy(slot->name, name, sizeof(slot->name) - 1);
        slot->name[sizeof(slot->name) - 1] = '\0';
    } else {
        slot->name[0] = '\0';
    }
}

static uint8_t simple_app_bt_scan_item_height(const BtScanPreview* p) {
    if(!p) return 1;
    return (p->has_name && p->name[0] != '\0') ? 2 : 1;
}

static bool simple_app_bt_scan_can_scroll_down(const SimpleApp* app) {
    if(!app || !app->bt_scan_preview) return false;
    if(app->bt_scan_preview_count == 0) return false;
    if(app->bt_scan_list_offset >= app->bt_scan_preview_count) return false;
    const uint8_t list_top = 10;
    const uint8_t list_bottom = DISPLAY_HEIGHT - 2;
    uint8_t max_lines =
        (uint8_t)((list_bottom - list_top + SERIAL_TEXT_LINE_HEIGHT) / SERIAL_TEXT_LINE_HEIGHT);
    if(max_lines > 6) max_lines = 6;
    if(max_lines == 0) max_lines = 1;

    size_t lines_used = 0;
    for(size_t i = app->bt_scan_list_offset; i < app->bt_scan_preview_count; i++) {
        uint8_t h = simple_app_bt_scan_item_height(&app->bt_scan_preview[i]);
        if(lines_used + h > max_lines) {
            return true; // there is more content below
        }
        lines_used += h;
    }
    return false;
}

static void simple_app_reset_scanner_stats(SimpleApp* app) {
    if(!app) return;
    app->scanner_total = 0;
    app->scanner_band_24 = 0;
    app->scanner_band_5 = 0;
    app->scanner_open_count = 0;
    app->scanner_hidden_count = 0;
    app->scanner_best_rssi_valid = false;
    app->scanner_best_rssi = SCAN_POWER_MIN_DBM;
    app->scanner_best_ssid[0] = '\0';
    app->scanner_last_update_tick = 0;
    app->scanner_scan_running = false;
    app->scanner_rescan_hint = false;
}

static bool simple_app_security_is_open(const char* security) {
    if(!security) return false;
    while(*security && isspace((unsigned char)*security)) {
        security++;
    }
    char lowered[8];
    size_t idx = 0;
    while(security[idx] && !isspace((unsigned char)security[idx]) && idx < sizeof(lowered) - 1) {
        lowered[idx] = (char)tolower((unsigned char)security[idx]);
        idx++;
    }
    lowered[idx] = '\0';
    return strcmp(lowered, "open") == 0;
}

static void
    simple_app_update_scanner_stats(SimpleApp* app, const ScanResult* result, bool ssid_hidden) {
    if(!app || !result) return;

    app->scanner_total = (uint16_t)app->scan_result_count;

    if(result->band == SCAN_BAND_24) {
        app->scanner_band_24++;
    } else if(result->band == SCAN_BAND_5) {
        app->scanner_band_5++;
    }

    if(ssid_hidden || strcmp(result->ssid, SCAN_SSID_HIDDEN_LABEL) == 0) {
        app->scanner_hidden_count++;
    }

    if(simple_app_security_is_open(result->security)) {
        app->scanner_open_count++;
    }

    if(result->power_valid &&
       (!app->scanner_best_rssi_valid || result->power_dbm > app->scanner_best_rssi)) {
        app->scanner_best_rssi_valid = true;
        app->scanner_best_rssi = result->power_dbm;
        strncpy(app->scanner_best_ssid, result->ssid, sizeof(app->scanner_best_ssid) - 1);
        app->scanner_best_ssid[sizeof(app->scanner_best_ssid) - 1] = '\0';
    }

    app->scanner_last_update_tick = furi_get_tick();
}

static void simple_app_reset_scan_results(SimpleApp* app) {
    if(!app) return;
    if(app->scan_results) {
        memset(app->scan_results, 0, sizeof(ScanResult) * app->scan_results_capacity);
    }
    if(app->scan_selected_numbers) {
        memset(app->scan_selected_numbers, 0, sizeof(uint16_t) * app->scan_results_capacity);
    }
    memset(app->scan_line_buffer, 0, sizeof(app->scan_line_buffer));
    if(app->visible_result_indices) {
        memset(app->visible_result_indices, 0, sizeof(uint16_t) * app->scan_results_capacity);
    }
    memset(app->vendor_cache, 0, sizeof(app->vendor_cache));
    app->scan_result_count = 0;
    app->scan_result_index = 0;
    app->scan_result_offset = 0;
    app->scan_selected_count = 0;
    app->scan_line_len = 0;
    app->scan_results_loading = false;
    app->visible_result_count = 0;
    simple_app_reset_scanner_stats(app);
    simple_app_reset_result_scroll(app);
}

static bool simple_app_result_is_visible(const SimpleApp* app, const ScanResult* result) {
    if(!app || !result) return false;
    if(result->power_valid && result->power_dbm < app->scanner_min_power) {
        return false;
    }
    return true;
}

static void simple_app_rebuild_visible_results(SimpleApp* app) {
    if(!app) return;
    if(!app->scan_results || !app->visible_result_indices) return;
    app->visible_result_count = 0;
    for(size_t i = 0; i < app->scan_result_count && i < app->scan_results_capacity; i++) {
        if(simple_app_result_is_visible(app, &app->scan_results[i])) {
            app->visible_result_indices[app->visible_result_count++] = (uint16_t)i;
        }
    }
    if(app->scan_result_index >= app->visible_result_count) {
        app->scan_result_index = (app->visible_result_count > 0) ? app->visible_result_count - 1 :
                                                                   0;
    }
    if(app->scan_result_offset >= app->visible_result_count) {
        app->scan_result_offset = (app->visible_result_count > 0) ? app->visible_result_count - 1 :
                                                                    0;
    }
    if(app->scan_result_offset > app->scan_result_index) {
        app->scan_result_offset = app->scan_result_index;
    }
    simple_app_reset_result_scroll(app);
}

static void simple_app_modify_min_power(SimpleApp* app, int16_t delta) {
    if(!app) return;
    int32_t proposed = (int32_t)app->scanner_min_power + delta;
    if(proposed > SCAN_POWER_MAX_DBM) {
        proposed = SCAN_POWER_MAX_DBM;
    } else if(proposed < SCAN_POWER_MIN_DBM) {
        proposed = SCAN_POWER_MIN_DBM;
    }
    if(app->scanner_min_power != proposed) {
        app->scanner_min_power = (int16_t)proposed;
        simple_app_rebuild_visible_results(app);
        simple_app_adjust_result_offset(app);
        simple_app_mark_config_dirty(app);
    }
}

static size_t simple_app_enabled_field_count(const SimpleApp* app) {
    if(!app) return 0;
    size_t count = 0;
    if(app->scanner_show_ssid) count++;
    if(app->scanner_show_bssid) count++;
    if(app->scanner_show_channel) count++;
    if(app->scanner_show_security) count++;
    if(app->scanner_show_power) count++;
    if(app->scanner_show_band) count++;
    if(app->scanner_show_vendor) count++;
    return count;
}

static bool* simple_app_scanner_option_flag(SimpleApp* app, ScannerOption option) {
    if(!app) return NULL;
    switch(option) {
    case ScannerOptionShowSSID:
        return &app->scanner_show_ssid;
    case ScannerOptionShowBSSID:
        return &app->scanner_show_bssid;
    case ScannerOptionShowChannel:
        return &app->scanner_show_channel;
    case ScannerOptionShowSecurity:
        return &app->scanner_show_security;
    case ScannerOptionShowPower:
        return &app->scanner_show_power;
    case ScannerOptionShowBand:
        return &app->scanner_show_band;
    case ScannerOptionShowVendor:
        return &app->scanner_show_vendor;
    default:
        return NULL;
    }
}

static ScanResult* simple_app_visible_result(SimpleApp* app, size_t visible_index) {
    if(!app || !app->scan_results || !app->visible_result_indices) return NULL;
    if(visible_index >= app->visible_result_count) return NULL;
    uint16_t actual_index = app->visible_result_indices[visible_index];
    if(actual_index >= app->scan_results_capacity) return NULL;
    return &app->scan_results[actual_index];
}

static const ScanResult*
    simple_app_visible_result_const(const SimpleApp* app, size_t visible_index) {
    if(!app || !app->scan_results || !app->visible_result_indices) return NULL;
    if(visible_index >= app->visible_result_count) return NULL;
    uint16_t actual_index = app->visible_result_indices[visible_index];
    if(actual_index >= app->scan_results_capacity) return NULL;
    return &app->scan_results[actual_index];
}

static const ScanResult*
    simple_app_find_scan_result_by_number(const SimpleApp* app, uint16_t number) {
    if(!app || !app->scan_results) return NULL;
    for(size_t i = 0; i < app->scan_result_count; i++) {
        const ScanResult* result = &app->scan_results[i];
        if(result->number == number) {
            return result;
        }
    }
    return NULL;
}

static bool simple_app_get_first_selected_ssid(SimpleApp* app, char* out, size_t out_size) {
    if(!app || !out || out_size == 0) return false;
    if(app->scan_selected_count == 0 || !app->scan_selected_numbers) return false;
    const ScanResult* result =
        simple_app_find_scan_result_by_number(app, app->scan_selected_numbers[0]);
    if(!result || result->ssid[0] == '\0') return false;
    simple_app_utf8_to_ascii_pl(result->ssid, out, out_size);
    simple_app_trim(out);
    return out[0] != '\0';
}

static void simple_app_compact_scan_results(SimpleApp* app) {
    if(!app || app->scan_selected_count == 0 || !app->scan_selected_numbers ||
       !app->scan_results) {
        return;
    }

    size_t new_count = app->scan_selected_count;
    ScanResult* compact = malloc(sizeof(ScanResult) * new_count);
    uint16_t* numbers = malloc(sizeof(uint16_t) * new_count);
    if(!compact || !numbers) {
        if(compact) free(compact);
        if(numbers) free(numbers);
        return;
    }

    for(size_t i = 0; i < new_count; i++) {
        uint16_t number = app->scan_selected_numbers[i];
        numbers[i] = number;
        const ScanResult* src = simple_app_find_scan_result_by_number(app, number);
        if(src) {
            compact[i] = *src;
            compact[i].selected = true;
        } else {
            memset(&compact[i], 0, sizeof(ScanResult));
            compact[i].number = number;
            compact[i].selected = true;
        }
    }

    free(app->scan_results);
    free(app->scan_selected_numbers);
    if(app->visible_result_indices) {
        free(app->visible_result_indices);
        app->visible_result_indices = NULL;
    }

    app->scan_results = compact;
    app->scan_selected_numbers = numbers;
    app->scan_results_capacity = new_count;
    app->scan_result_count = new_count;
    app->scan_result_index = 0;
    app->scan_result_offset = 0;
    app->visible_result_count = 0;
    app->scan_results_loading = false;
}

static void simple_app_update_backlight_label(SimpleApp* app) {
    if(!app) return;
    snprintf(
        menu_label_backlight,
        sizeof(menu_label_backlight),
        "Backlight: %s",
        app->backlight_enabled ? "On" : "Off");
}

static void simple_app_update_ram_label(SimpleApp* app) {
    if(!app) return;
    snprintf(
        menu_label_ram,
        sizeof(menu_label_ram),
        "Show RAM: %s",
        app->show_ram_overlay ? "On" : "Off");
}

static void simple_app_update_debug_label(SimpleApp* app) {
    if(!app) return;
    snprintf(
        menu_label_debug,
        sizeof(menu_label_debug),
        "Debug Log: %s",
        app->debug_mark_enabled ? "On" : "Off");
}

static void simple_app_update_led_label(SimpleApp* app) {
    if(!app) return;
    uint32_t level = app->led_level;
    if(level < 1) level = 1;
    if(level > 100) level = 100;
    snprintf(
        menu_label_led,
        sizeof(menu_label_led),
        "LED: %s (%lu)",
        app->led_enabled ? "On" : "Off",
        (unsigned long)level);
}

static void simple_app_update_boot_labels(SimpleApp* app) {
    if(!app) return;
    snprintf(
        menu_label_boot_short,
        sizeof(menu_label_boot_short),
        "Boot short: %s",
        app->boot_short_enabled ? "On" : "Off");
    snprintf(
        menu_label_boot_long,
        sizeof(menu_label_boot_long),
        "Boot long: %s",
        app->boot_long_enabled ? "On" : "Off");
}

static void simple_app_send_led_power_command(SimpleApp* app) {
    if(!app || !app->serial) return;
    const char* state = app->led_enabled ? "on" : "off";
    char command[24];
    snprintf(command, sizeof(command), "led set %s", state);
    simple_app_send_command_quiet(app, command);
}

static void simple_app_send_led_level_command(SimpleApp* app) {
    if(!app || !app->serial) return;
    uint32_t level = app->led_level;
    if(level < 1) level = 1;
    if(level > 100) level = 100;
    char command[24];
    snprintf(command, sizeof(command), "led level %lu", (unsigned long)level);
    simple_app_send_command_quiet(app, command);
}

static void simple_app_send_boot_status(SimpleApp* app, bool is_short, bool enabled) {
    if(!app || !app->serial) return;
    char command[48];
    snprintf(
        command,
        sizeof(command),
        "boot_button status %s %s",
        is_short ? "short" : "long",
        enabled ? "on" : "off");
    simple_app_send_command_quiet(app, command);
}

static void simple_app_send_boot_command(SimpleApp* app, bool is_short, uint8_t command_index) {
    if(!app || !app->serial) return;
    if(command_index >= BOOT_COMMAND_OPTION_COUNT) {
        command_index = 0;
    }
    char command[48];
    snprintf(
        command,
        sizeof(command),
        "boot_button set %s %s",
        is_short ? "short" : "long",
        boot_command_options[command_index]);
    simple_app_send_command_quiet(app, command);
}

static void simple_app_request_boot_status(SimpleApp* app) {
    if(!app || !app->serial) return;
    simple_app_send_command_quiet(app, "boot_button read");
}

static bool simple_app_handle_boot_status_line(SimpleApp* app, const char* line) {
    if(!app || !line) return false;
    const char* short_status = "boot_short_status=";
    const char* short_cmd = "boot_short=";
    const char* long_status = "boot_long_status=";
    const char* long_cmd = "boot_long=";

    if(strncmp(line, short_status, strlen(short_status)) == 0) {
        app->boot_short_enabled =
            simple_app_parse_bool_value(line + strlen(short_status), app->boot_short_enabled);
        simple_app_update_boot_labels(app);
        return true;
    }
    if(strncmp(line, long_status, strlen(long_status)) == 0) {
        app->boot_long_enabled =
            simple_app_parse_bool_value(line + strlen(long_status), app->boot_long_enabled);
        simple_app_update_boot_labels(app);
        return true;
    }
    if(strncmp(line, short_cmd, strlen(short_cmd)) == 0) {
        const char* value = line + strlen(short_cmd);
        for(uint8_t i = 0; i < BOOT_COMMAND_OPTION_COUNT; i++) {
            if(strcmp(value, boot_command_options[i]) == 0) {
                app->boot_short_command_index = i;
                simple_app_update_boot_labels(app);
                return true;
            }
        }
        return true;
    }
    if(strncmp(line, long_cmd, strlen(long_cmd)) == 0) {
        const char* value = line + strlen(long_cmd);
        for(uint8_t i = 0; i < BOOT_COMMAND_OPTION_COUNT; i++) {
            if(strcmp(value, boot_command_options[i]) == 0) {
                app->boot_long_command_index = i;
                simple_app_update_boot_labels(app);
                return true;
            }
        }
        return true;
    }
    return false;
}

static void simple_app_boot_feed(SimpleApp* app, char ch) {
    static char line_buffer[96];
    static size_t line_len = 0;
    if(ch == '\r') return;
    if(ch == '\n') {
        if(line_len > 0) {
            line_buffer[line_len] = '\0';
            // Skip prompt markers and leading spaces
            const char* line_ptr = line_buffer;
            while(*line_ptr == '>' || *line_ptr == ' ' || *line_ptr == '\t') {
                line_ptr++;
            }

            if(simple_app_handle_boot_status_line(app, line_ptr)) {
                if(app->viewport) {
                    view_port_update(app->viewport);
                }
            } else if(strcmp(line_ptr, "Boot Pressed") == 0) {
                simple_app_handle_boot_trigger(app, false);
            } else if(strcmp(line_ptr, "Boot Long Pressed") == 0) {
                simple_app_handle_boot_trigger(app, true);
            } else if(strcmp(line_ptr, "BOARD READY") == 0) {
                simple_app_on_board_online(app, "boot");
            }
        }
        line_len = 0;
        return;
    }
    if(line_len + 1 >= sizeof(line_buffer)) {
        line_len = 0;
        return;
    }
    line_buffer[line_len++] = ch;
}

static void simple_app_handle_boot_trigger(SimpleApp* app, bool is_long) {
    if(!app) return;

    const char* cmd = NULL;
    bool enabled = false;
    if(is_long) {
        enabled = app->boot_long_enabled;
        if(app->boot_long_command_index < BOOT_COMMAND_OPTION_COUNT) {
            cmd = boot_command_options[app->boot_long_command_index];
        }
    } else {
        enabled = app->boot_short_enabled;
        if(app->boot_short_command_index < BOOT_COMMAND_OPTION_COUNT) {
            cmd = boot_command_options[app->boot_short_command_index];
        }
    }

    char message[64];
    if(!enabled) {
        snprintf(message, sizeof(message), "Boot %s:\ndisabled", is_long ? "long" : "short");
    } else if(cmd) {
        snprintf(message, sizeof(message), "Boot %s:\n%s", is_long ? "long" : "short", cmd);
    } else {
        snprintf(message, sizeof(message), "Boot %s\npressed", is_long ? "long" : "short");
    }

    simple_app_show_status_message(app, message, 1500, true);

    if(enabled) {
        app->screen = ScreenSerial;
        app->serial_follow_tail = true;
        app->last_command_sent = true; // allow Back to send stop for boot-triggered actions
        simple_app_update_scroll(app);
        if(app->viewport) {
            view_port_update(app->viewport);
        }
    }
}

static void simple_app_on_board_online(SimpleApp* app, const char* source) {
    if(!app) return;
    (void)source;

    bool was_ready = app->board_ready_seen;
    app->board_ready_seen = true;
    app->board_sync_pending = false;
    app->board_ping_outstanding = false;
    app->board_last_rx_tick = furi_get_tick();
    app->board_missing_shown = false;
    app->board_ping_failures = 0;

    if(app->board_bootstrap_active) {
        // End bootstrap discovery without forcing a reboot
        app->board_bootstrap_active = false;
        app->board_bootstrap_reboot_sent = false;
        simple_app_show_status_message(app, "Board ready", 1200, true);
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(!was_ready) {
        simple_app_show_status_message(app, "Board found", 1000, true);
        if(app->viewport) {
            view_port_update(app->viewport);
        }
    }

    simple_app_request_led_status(app);
}

static void simple_app_draw_sync_status(const SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;
    const char* text = "Sync...";
    if(app->board_missing_shown) {
        text = "No board";
    } else if(app->board_ready_seen) {
        text = "Sync OK";
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH - 2, 8, AlignRight, AlignBottom, text);
}

static void simple_app_draw_ram_overlay(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas || !app->show_ram_overlay) return;
    size_t free_heap = memmgr_get_free_heap();
    size_t min_heap = memmgr_get_minimum_free_heap();
    unsigned long free_kb = (unsigned long)(free_heap / 1024);
    unsigned long min_kb = (unsigned long)(min_heap / 1024);
    unsigned long start_kb = (unsigned long)(app->heap_start_free / 1024);
    char line[40];
    snprintf(line, sizeof(line), "R:%luK M:%luK S:%luK", free_kb, min_kb, start_kb);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, DISPLAY_WIDTH - 2, DISPLAY_HEIGHT - 2, AlignRight, AlignBottom, line);
}

static void simple_app_handle_pong(SimpleApp* app) {
    simple_app_on_board_online(app, "pong");
}

static void simple_app_send_ping(SimpleApp* app) {
    if(!app || !app->serial) return;
    const char* cmd = "ping\n";
    furi_hal_serial_tx(app->serial, (const uint8_t*)cmd, strlen(cmd));
    furi_hal_serial_tx_wait_complete(app->serial);
    app->board_ping_outstanding = true;
    app->board_last_ping_tick = furi_get_tick();
}

static void simple_app_ping_watchdog(SimpleApp* app) {
    if(!app) return;
    uint32_t now = furi_get_tick();

    bool status_active = simple_app_status_message_is_active(app);
    // Ping on idle menu (no modal) or whenever we are running the bootstrap discovery
    bool idle_menu = (app->screen == ScreenMenu) && (app->menu_state == MenuStateSections);
    bool allow_ping = ((idle_menu && !status_active) || app->board_bootstrap_active);

    if(allow_ping && !app->board_ping_outstanding &&
       (now - app->board_last_ping_tick) >= BOARD_PING_INTERVAL_MS) {
        simple_app_send_ping(app);
    }

    if(app->board_ping_outstanding && (now - app->board_last_ping_tick) >= BOARD_PING_TIMEOUT_MS) {
        app->board_ping_outstanding = false;

        if(app->board_bootstrap_active) {
            const char* waiting_msg = app->board_bootstrap_reboot_sent ?
                                          "Rebooting\nfor stability\nWaiting for board..." :
                                          "Board discovery\nWaiting for board...";
            simple_app_show_status_message(app, waiting_msg, 0, true);
            if(app->viewport) {
                view_port_update(app->viewport);
            }
            return;
        }

        if(app->board_ping_failures < UINT8_MAX) {
            app->board_ping_failures++;
        }
        if(app->board_ping_failures >= BOARD_PING_FAILURE_LIMIT) {
            app->board_ready_seen = false;
            if(!app->board_missing_shown) {
                app->board_missing_shown = true;
                simple_app_show_status_message(app, "No board", 1500, true);
            }
        }
    }
}

static void simple_app_send_command_quiet(SimpleApp* app, const char* command) {
    if(!app || !command || !app->serial) return;

    // Block commands until board reports ready (except reboot which triggers sync)
    if(!app->board_ready_seen) {
        // Allow reboot to kick off sync
        if(strncmp(command, "reboot", 6) != 0 && strncmp(command, "ping", 4) != 0) {
            simple_app_show_status_message(app, "Waiting for board...", 1000, true);
            return;
        }
    }
    app->board_ping_outstanding = false;

    simple_app_log_cli_command(app, "UI", command);

    char cmd[64];
    int len = snprintf(cmd, sizeof(cmd), "%s\n", command);
    if(len <= 0) return;
    if(app->rx_stream) {
        furi_stream_buffer_reset(app->rx_stream);
    }
    furi_hal_serial_tx(app->serial, (const uint8_t*)cmd, (size_t)len);
    furi_hal_serial_tx_wait_complete(app->serial);
    char log_line[96];
    int log_len = snprintf(log_line, sizeof(log_line), "TX: %s\n", command);
    if(log_len > 0) {
        simple_app_append_serial_data(app, (const uint8_t*)log_line, (size_t)log_len);
    }
}

static void simple_app_request_led_status(SimpleApp* app) {
    if(!app || !app->serial) return;
    app->led_read_pending = true;
    app->led_read_length = 0;
    app->led_read_buffer[0] = '\0';
    simple_app_send_command_quiet(app, "led read");
}

static bool simple_app_handle_led_status_line(SimpleApp* app, const char* line) {
    if(!app || !line) return false;
    const char* status_marker = "LED status:";
    const char* status_ptr = strstr(line, status_marker);
    if(!status_ptr) return false;
    status_ptr += strlen(status_marker);
    while(*status_ptr == ' ' || *status_ptr == '\t') {
        status_ptr++;
    }
    bool enabled = app->led_enabled;
    if(status_ptr[0] != '\0') {
        if((status_ptr[0] == 'o' || status_ptr[0] == 'O') &&
           (status_ptr[1] == 'n' || status_ptr[1] == 'N')) {
            enabled = true;
        } else if(
            (status_ptr[0] == 'o' || status_ptr[0] == 'O') &&
            (status_ptr[1] == 'f' || status_ptr[1] == 'F')) {
            enabled = false;
        }
    }

    uint32_t level = app->led_level;
    const char* brightness_marker = "brightness";
    const char* brightness_ptr = strstr(line, brightness_marker);
    if(brightness_ptr) {
        brightness_ptr += strlen(brightness_marker);
        while(*brightness_ptr == ' ' || *brightness_ptr == ':' || *brightness_ptr == ',' ||
              *brightness_ptr == '\t') {
            brightness_ptr++;
        }
        char digits[6] = {0};
        size_t idx = 0;
        while(brightness_ptr[idx] && idx < sizeof(digits) - 1) {
            if(isdigit((unsigned char)brightness_ptr[idx])) {
                digits[idx] = brightness_ptr[idx];
            } else {
                break;
            }
            idx++;
        }
        if(idx > 0) {
            digits[idx] = '\0';
            long parsed = strtol(digits, NULL, 10);
            if(parsed >= 0) {
                level = (uint32_t)parsed;
            }
        }
    }

    if(level < 1) level = 1;
    if(level > 100) level = 100;

    app->led_enabled = enabled;
    app->led_level = (uint8_t)level;
    simple_app_update_led_label(app);
    simple_app_mark_config_dirty(app);
    simple_app_save_config_if_dirty(app, NULL, false);

    if(app->screen == ScreenMenu && app->menu_state == MenuStateItems &&
       app->section_index == MENU_SECTION_SETUP && app->viewport) {
        view_port_update(app->viewport);
    } else if(app->screen == ScreenSetupLed && app->viewport) {
        view_port_update(app->viewport);
    }
    return true;
}

static void simple_app_led_feed(SimpleApp* app, char ch) {
    if(!app || !app->led_read_pending) return;
    if(ch == '\r') return;
    if(ch == '\n') {
        app->led_read_buffer[app->led_read_length] = '\0';
        if(simple_app_handle_led_status_line(app, app->led_read_buffer)) {
            app->led_read_pending = false;
        }
        app->led_read_length = 0;
        return;
    }
    if(app->led_read_length + 1 >= sizeof(app->led_read_buffer)) {
        app->led_read_length = 0;
        return;
    }
    app->led_read_buffer[app->led_read_length++] = ch;
}

static void simple_app_send_vendor_command(SimpleApp* app, bool enable) {
    if(!app || !app->serial) return;
    app->vendor_read_pending = true;
    app->vendor_read_length = 0;
    app->vendor_read_buffer[0] = '\0';
    char command[24];
    snprintf(command, sizeof(command), "vendor set %s", enable ? "on" : "off");
    simple_app_send_command_quiet(app, command);
}

static void simple_app_request_vendor_status(SimpleApp* app) {
    if(!app || !app->serial) return;
    app->vendor_read_pending = true;
    app->vendor_read_length = 0;
    app->vendor_read_buffer[0] = '\0';
    simple_app_send_command_quiet(app, "vendor read");
}

static bool simple_app_handle_vendor_status_line(SimpleApp* app, const char* line) {
    if(!app || !line) return false;
    static const char prefix[] = "Vendor scan:";
    size_t prefix_len = sizeof(prefix) - 1;
    if(strncmp(line, prefix, prefix_len) != 0) {
        return false;
    }

    const char* status = line + prefix_len;
    while(*status == ' ' || *status == '\t') {
        status++;
    }

    bool enabled = app->vendor_scan_enabled;
    if((status[0] == 'o' || status[0] == 'O') && (status[1] == 'n' || status[1] == 'N')) {
        enabled = true;
    } else if((status[0] == 'o' || status[0] == 'O') && (status[1] == 'f' || status[1] == 'F')) {
        enabled = false;
    } else if(
        status[0] == '1' || status[0] == 'Y' || status[0] == 'y' || status[0] == 'T' ||
        status[0] == 't') {
        enabled = true;
    } else if(
        status[0] == '0' || status[0] == 'N' || status[0] == 'n' || status[0] == 'F' ||
        status[0] == 'f') {
        enabled = false;
    }

    app->vendor_scan_enabled = enabled;
    bool changed = (app->scanner_show_vendor != enabled);
    app->scanner_show_vendor = enabled;
    if(changed) {
        simple_app_update_result_layout(app);
        simple_app_rebuild_visible_results(app);
        simple_app_adjust_result_offset(app);
        if(app->viewport && app->screen == ScreenSetupScanner) {
            view_port_update(app->viewport);
        }
    }
    return true;
}

static void simple_app_vendor_feed(SimpleApp* app, char ch) {
    if(!app || !app->vendor_read_pending) return;
    if(ch == '\r') return;
    if(ch == '\n') {
        if(app->vendor_read_length > 0) {
            app->vendor_read_buffer[app->vendor_read_length] = '\0';
            if(simple_app_handle_vendor_status_line(app, app->vendor_read_buffer)) {
                app->vendor_read_pending = false;
            }
        }
        app->vendor_read_length = 0;
        return;
    }
    if(app->vendor_read_length + 1 >= sizeof(app->vendor_read_buffer)) {
        app->vendor_read_length = 0;
        return;
    }
    app->vendor_read_buffer[app->vendor_read_length++] = ch;
}

static uint16_t simple_app_clamp_channel_time(uint32_t value) {
    if(value < SCANNER_CHANNEL_TIME_MIN_MS) {
        value = SCANNER_CHANNEL_TIME_MIN_MS;
    } else if(value > SCANNER_CHANNEL_TIME_MAX_MS) {
        value = SCANNER_CHANNEL_TIME_MAX_MS;
    }
    return (uint16_t)value;
}

static bool simple_app_extract_first_uint(const char* line, uint32_t* value_out) {
    if(!line || !value_out) return false;
    const char* cursor = line;
    while(*cursor && !isdigit((unsigned char)*cursor)) {
        cursor++;
    }
    if(*cursor == '\0') return false;
    char* endptr = NULL;
    uint32_t parsed = (uint32_t)strtoul(cursor, &endptr, 10);
    if(endptr == cursor) return false;
    *value_out = parsed;
    return true;
}

static bool
    simple_app_set_channel_time(SimpleApp* app, bool is_min, uint16_t value, bool from_remote) {
    if(!app) return false;
    uint16_t clamped = simple_app_clamp_channel_time(value);
    uint16_t before_min = app->scanner_min_channel_time;
    uint16_t before_max = app->scanner_max_channel_time;

    if(is_min) {
        app->scanner_min_channel_time = clamped;
        if(app->scanner_max_channel_time < clamped) {
            app->scanner_max_channel_time = clamped;
        }
    } else {
        app->scanner_max_channel_time = clamped;
        if(app->scanner_min_channel_time > clamped) {
            app->scanner_min_channel_time = clamped;
        }
    }

    bool changed = (before_min != app->scanner_min_channel_time) ||
                   (before_max != app->scanner_max_channel_time);

    if(changed && !from_remote) {
        simple_app_mark_config_dirty(app);
    }

    if(changed && app->viewport && app->screen == ScreenSetupScannerTiming) {
        view_port_update(app->viewport);
    }

    return changed;
}

static void simple_app_request_channel_time(SimpleApp* app, bool request_min) {
    if(!app || !app->serial) return;
    if(request_min) {
        app->scanner_timing_min_pending = true;
    } else {
        app->scanner_timing_max_pending = true;
    }
    app->scanner_timing_read_length = 0;
    app->scanner_timing_read_buffer[0] = '\0';
    simple_app_send_command_quiet(
        app, request_min ? "channel_time read min" : "channel_time read max");
}

static void simple_app_request_scanner_timing(SimpleApp* app) {
    if(!app || !app->serial) return;
    simple_app_request_channel_time(app, true);
    simple_app_request_channel_time(app, false);
}

static void simple_app_send_channel_time(SimpleApp* app, bool is_min, uint16_t value) {
    if(!app || !app->serial) return;
    char command[48];
    snprintf(
        command,
        sizeof(command),
        "channel_time set %s %u",
        is_min ? "min" : "max",
        (unsigned)value);
    simple_app_send_command_quiet(app, command);
    simple_app_request_channel_time(app, is_min);
}

static bool simple_app_handle_scanner_timing_line(SimpleApp* app, const char* line) {
    if(!app || !line) return false;

    char lower[96];
    size_t len = simple_app_bounded_strlen(line, sizeof(lower) - 1);
    for(size_t i = 0; i < len; i++) {
        lower[i] = (char)tolower((unsigned char)line[i]);
    }
    lower[len] = '\0';

    bool mentions_min = strstr(lower, "min") != NULL;
    bool mentions_max = strstr(lower, "max") != NULL;

    uint32_t parsed = 0;
    bool has_number = simple_app_extract_first_uint(line, &parsed);

    // If no explicit min/max marker, use pending flags to decide target.
    if(!mentions_min && !mentions_max) {
        if(app->scanner_timing_min_pending && !app->scanner_timing_max_pending) {
            mentions_min = true;
        } else if(app->scanner_timing_max_pending && !app->scanner_timing_min_pending) {
            mentions_max = true;
        } else if(app->scanner_timing_min_pending) {
            mentions_min = true;
        } else if(app->scanner_timing_max_pending) {
            mentions_max = true;
        }
    }

    if(!mentions_min && !mentions_max) return false;
    if(!has_number) return false;

    bool handled = false;
    if(mentions_min) {
        handled = simple_app_set_channel_time(app, true, (uint16_t)parsed, true) || handled;
        app->scanner_timing_min_pending = false;
    }
    if(mentions_max) {
        handled = simple_app_set_channel_time(app, false, (uint16_t)parsed, true) || handled;
        app->scanner_timing_max_pending = false;
    }

    return handled;
}

static void simple_app_scanner_timing_feed(SimpleApp* app, char ch) {
    if(!app || (!app->scanner_timing_min_pending && !app->scanner_timing_max_pending)) return;
    if(ch == '\r') return;
    if(ch == '\n') {
        if(app->scanner_timing_read_length > 0) {
            app->scanner_timing_read_buffer[app->scanner_timing_read_length] = '\0';
            simple_app_handle_scanner_timing_line(app, app->scanner_timing_read_buffer);
        }
        app->scanner_timing_read_length = 0;
        return;
    }
    if(app->scanner_timing_read_length + 1 >= sizeof(app->scanner_timing_read_buffer)) {
        app->scanner_timing_read_length = 0;
        return;
    }
    app->scanner_timing_read_buffer[app->scanner_timing_read_length++] = ch;
}

static void simple_app_modify_channel_time(SimpleApp* app, bool is_min, int32_t delta) {
    if(!app || delta == 0) return;
    uint16_t target = is_min ? app->scanner_min_channel_time : app->scanner_max_channel_time;
    int32_t proposed = (int32_t)target + delta;
    if(proposed < 0) {
        proposed = 0;
    }
    uint16_t before_min = app->scanner_min_channel_time;
    uint16_t before_max = app->scanner_max_channel_time;

    if(simple_app_set_channel_time(app, is_min, (uint16_t)proposed, false)) {
        if(app->scanner_min_channel_time != before_min) {
            simple_app_send_channel_time(app, true, app->scanner_min_channel_time);
        }
        if(app->scanner_max_channel_time != before_max) {
            simple_app_send_channel_time(app, false, app->scanner_max_channel_time);
        }
    }
}

static SimpleAppWardriveState* simple_app_wardrive_state_ensure(SimpleApp* app) {
    if(!app) return NULL;
    if(app->wardrive_state) return app->wardrive_state;
    app->wardrive_state = malloc(sizeof(SimpleAppWardriveState));
    if(!app->wardrive_state) return NULL;
    memset(app->wardrive_state, 0, sizeof(SimpleAppWardriveState));
    return app->wardrive_state;
}

static void simple_app_wardrive_state_free(SimpleApp* app) {
    if(!app || !app->wardrive_state) return;
    free(app->wardrive_state);
    app->wardrive_state = NULL;
}

static SimpleAppGpsState* simple_app_gps_state_ensure(SimpleApp* app) {
    if(!app) return NULL;
    if(app->gps_state) return app->gps_state;
    app->gps_state = malloc(sizeof(SimpleAppGpsState));
    if(!app->gps_state) return NULL;
    memset(app->gps_state, 0, sizeof(SimpleAppGpsState));
    app->gps_state->satellites = -1;
    return app->gps_state;
}

static void simple_app_gps_state_free(SimpleApp* app) {
    if(!app || !app->gps_state) return;
    free(app->gps_state);
    app->gps_state = NULL;
}

static void simple_app_set_wardrive_status(SimpleApp* app, const char* text, bool numeric) {
    if(!app || !text || !app->wardrive_state) return;
    snprintf(
        app->wardrive_state->status_text, sizeof(app->wardrive_state->status_text), "%s", text);
    app->wardrive_state->status_is_numeric = numeric;
}

static void simple_app_reset_wardrive_status(SimpleApp* app) {
    if(!app || !app->wardrive_state) return;
    simple_app_set_wardrive_status(app, "0", true);
    app->wardrive_state->line_length = 0;
}

static void simple_app_reset_gps_status(SimpleApp* app) {
    if(!app) return;
    app->gps_console_mode = false;
    if(!app->gps_state) return;
    app->gps_state->has_fix = false;
    app->gps_state->has_coords = false;
    app->gps_state->has_time = false;
    app->gps_state->has_date = false;
    app->gps_state->satellites = -1;
    app->gps_state->antenna_status = GpsAntennaUnknown;
    app->gps_state->lat[0] = '\0';
    app->gps_state->lon[0] = '\0';
    app->gps_state->time[0] = '\0';
    app->gps_state->date[0] = '\0';
    app->gps_state->line_length = 0;
}

static void simple_app_gps_clear_coords(SimpleApp* app) {
    if(!app || !app->gps_state) return;
    app->gps_state->has_coords = false;
    app->gps_state->lat[0] = '\0';
    app->gps_state->lon[0] = '\0';
}

static bool simple_app_gps_format_time(const char* raw, char* out, size_t out_len) {
    if(!raw || !out || out_len == 0) return false;
    if(strlen(raw) < 6) return false;
    if(!isdigit((unsigned char)raw[0]) || !isdigit((unsigned char)raw[1]) ||
       !isdigit((unsigned char)raw[2]) || !isdigit((unsigned char)raw[3]) ||
       !isdigit((unsigned char)raw[4]) || !isdigit((unsigned char)raw[5])) {
        return false;
    }
    int hh = (raw[0] - '0') * 10 + (raw[1] - '0');
    int mm = (raw[2] - '0') * 10 + (raw[3] - '0');
    int ss = (raw[4] - '0') * 10 + (raw[5] - '0');
    snprintf(out, out_len, "%02d:%02d:%02d", hh, mm, ss);
    return true;
}

static int16_t simple_app_clamp_gps_utc_offset(int32_t minutes) {
    if(minutes < GPS_UTC_OFFSET_MIN_MINUTES) {
        return GPS_UTC_OFFSET_MIN_MINUTES;
    }
    if(minutes > GPS_UTC_OFFSET_MAX_MINUTES) {
        return GPS_UTC_OFFSET_MAX_MINUTES;
    }
    return (int16_t)minutes;
}

static void simple_app_gps_format_offset(const SimpleApp* app, char* out, size_t out_len) {
    if(!out || out_len == 0) return;
    int minutes = app ? app->gps_utc_offset_minutes : 0;
    if(app && app->gps_dst_enabled) {
        minutes += 60;
    }
    char sign = '+';
    if(minutes < 0) {
        sign = '-';
        minutes = -minutes;
    }
    int hh = minutes / 60;
    int mm = minutes % 60;
    if(mm == 0) {
        snprintf(out, out_len, "UTC%c%d%s", sign, hh, app && app->gps_dst_enabled ? " DST" : "");
    } else {
        snprintf(
            out,
            out_len,
            "UTC%c%d:%02d%s",
            sign,
            hh,
            mm,
            app && app->gps_dst_enabled ? " DST" : "");
    }
}

static bool simple_app_gps_format_date(const char* raw, char* out, size_t out_len) {
    if(!raw || !out || out_len == 0) return false;
    if(strlen(raw) < 6) return false;
    if(!isdigit((unsigned char)raw[0]) || !isdigit((unsigned char)raw[1]) ||
       !isdigit((unsigned char)raw[2]) || !isdigit((unsigned char)raw[3]) ||
       !isdigit((unsigned char)raw[4]) || !isdigit((unsigned char)raw[5])) {
        return false;
    }
    snprintf(out, out_len, "%c%c/%c%c/%c%c", raw[0], raw[1], raw[2], raw[3], raw[4], raw[5]);
    return true;
}

static bool
    simple_app_gps_format_coord(const char* raw, const char* hemi, char* out, size_t out_len) {
    if(!raw || !hemi || !out || out_len == 0) return false;
    if(raw[0] == '\0' || hemi[0] == '\0') return false;
    double value = strtod(raw, NULL);
    const double hundred = (double)100.0;
    const double sixty = (double)60.0;
    int degrees = (int)(value / hundred);
    double minutes = value - ((double)degrees * hundred);
    double decimal = (double)degrees + (minutes / sixty);
    char h = (char)toupper((unsigned char)hemi[0]);
    if(h == 'S' || h == 'W') decimal = -decimal;
    snprintf(out, out_len, "%.5f", decimal);
    return true;
}

static bool simple_app_gps_format_time_display(const SimpleApp* app, char* out, size_t out_len) {
    if(!app || !out || out_len == 0) return false;
    if(!app->gps_state || strlen(app->gps_state->time) < 8) return false;
    int hh = 0;
    int mm = 0;
    int ss = 0;
    if(sscanf(app->gps_state->time, "%2d:%2d:%2d", &hh, &mm, &ss) != 3) return false;

    int dst_minutes = app->gps_dst_enabled ? 60 : 0;
    int total_seconds =
        hh * 3600 + mm * 60 + ss + (app->gps_utc_offset_minutes + dst_minutes) * 60;
    const int day_seconds = 24 * 3600;
    total_seconds %= day_seconds;
    if(total_seconds < 0) {
        total_seconds += day_seconds;
    }
    hh = total_seconds / 3600;
    mm = (total_seconds % 3600) / 60;
    ss = total_seconds % 60;

    if(app->gps_time_24h) {
        snprintf(out, out_len, "%02d:%02d:%02d", hh, mm, ss);
        return true;
    }

    const char* suffix = (hh >= 12) ? "PM" : "AM";
    int hh12 = hh % 12;
    if(hh12 == 0) hh12 = 12;
    snprintf(out, out_len, "%02d:%02d:%02d %s", hh12, mm, ss, suffix);
    return true;
}

static bool simple_app_strcasestr(const char* haystack, const char* needle) {
    if(!haystack || !needle || needle[0] == '\0') return false;
    size_t needle_len = strlen(needle);
    for(const char* h = haystack; *h != '\0'; h++) {
        size_t i = 0;
        while(i < needle_len && h[i] != '\0' &&
              tolower((unsigned char)h[i]) == tolower((unsigned char)needle[i])) {
            i++;
        }
        if(i == needle_len) return true;
    }
    return false;
}

static void simple_app_process_gps_line(SimpleApp* app, const char* line) {
    if(!app || !line || !app->gps_view_active || !app->gps_state) return;

    const char* nmea = NULL;
    if(strncmp(line, "[GPS RAW]", 9) == 0) {
        nmea = line + 9;
        while(*nmea == ' ')
            nmea++;
    } else {
        return;
    }

    if(!nmea || nmea[0] != '$') return;

    char buffer[128];
    strncpy(buffer, nmea, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char* tokens[16];
    size_t token_count = 0;
    char* token = strtok(buffer, ",");
    while(token && token_count < sizeof(tokens) / sizeof(tokens[0])) {
        char* checksum = strchr(token, '*');
        if(checksum) *checksum = '\0';
        tokens[token_count++] = token;
        token = strtok(NULL, ",");
    }
    if(token_count == 0) return;

    SimpleAppGpsState* gps = app->gps_state;

    if(strcmp(tokens[0], "$GNGGA") == 0 || strcmp(tokens[0], "$GPGGA") == 0) {
        if(token_count > 7) {
            if(tokens[6][0] != '\0') {
                int fix_quality = atoi(tokens[6]);
                gps->has_fix = fix_quality > 0;
                if(!gps->has_fix) {
                    simple_app_gps_clear_coords(app);
                }
            }
            if(tokens[7][0] != '\0') {
                gps->satellites = atoi(tokens[7]);
            } else {
                gps->satellites = -1;
            }
        }
        if(token_count > 1 && tokens[1][0] != '\0') {
            if(simple_app_gps_format_time(tokens[1], gps->time, sizeof(gps->time))) {
                gps->has_time = true;
            }
        }
        if(token_count > 5 && tokens[2][0] != '\0' && tokens[3][0] != '\0' &&
           tokens[4][0] != '\0' && tokens[5][0] != '\0') {
            bool ok_lat =
                simple_app_gps_format_coord(tokens[2], tokens[3], gps->lat, sizeof(gps->lat));
            bool ok_lon =
                simple_app_gps_format_coord(tokens[4], tokens[5], gps->lon, sizeof(gps->lon));
            gps->has_coords = ok_lat && ok_lon;
        }
    } else if(strcmp(tokens[0], "$GNRMC") == 0 || strcmp(tokens[0], "$GPRMC") == 0) {
        if(token_count > 2 && tokens[2][0] != '\0') {
            gps->has_fix = (tokens[2][0] == 'A');
            if(!gps->has_fix) {
                simple_app_gps_clear_coords(app);
            }
        }
        if(token_count > 1 && tokens[1][0] != '\0') {
            if(simple_app_gps_format_time(tokens[1], gps->time, sizeof(gps->time))) {
                gps->has_time = true;
            }
        }
        if(token_count > 9 && tokens[9][0] != '\0') {
            if(simple_app_gps_format_date(tokens[9], gps->date, sizeof(gps->date))) {
                gps->has_date = true;
            }
        }
        if(token_count > 6 && tokens[3][0] != '\0' && tokens[4][0] != '\0' &&
           tokens[5][0] != '\0' && tokens[6][0] != '\0' && gps->has_fix) {
            bool ok_lat =
                simple_app_gps_format_coord(tokens[3], tokens[4], gps->lat, sizeof(gps->lat));
            bool ok_lon =
                simple_app_gps_format_coord(tokens[5], tokens[6], gps->lon, sizeof(gps->lon));
            gps->has_coords = ok_lat && ok_lon;
        }
    } else if(strcmp(tokens[0], "$GNZDA") == 0 || strcmp(tokens[0], "$GPZDA") == 0) {
        if(token_count > 1 && tokens[1][0] != '\0') {
            if(simple_app_gps_format_time(tokens[1], gps->time, sizeof(gps->time))) {
                gps->has_time = true;
            }
        }
        if(token_count > 4 && tokens[2][0] != '\0' && tokens[3][0] != '\0' &&
           tokens[4][0] != '\0') {
            char date_raw[7];
            snprintf(date_raw, sizeof(date_raw), "%s%s%s", tokens[2], tokens[3], tokens[4]);
            if(simple_app_gps_format_date(date_raw, gps->date, sizeof(gps->date))) {
                gps->has_date = true;
            }
        }
        if(app->gps_zda_tz_enabled && token_count > 5 && tokens[5][0] != '\0') {
            long zone_hours = strtol(tokens[5], NULL, 10);
            long zone_minutes = 0;
            if(token_count > 6 && tokens[6][0] != '\0') {
                zone_minutes = strtol(tokens[6], NULL, 10);
            }
            if(zone_minutes < 0) {
                zone_minutes = -zone_minutes;
            }
            int32_t offset_minutes = (int32_t)zone_hours * 60;
            if(zone_hours < 0) {
                offset_minutes -= (int32_t)zone_minutes;
            } else {
                offset_minutes += (int32_t)zone_minutes;
            }
            int16_t clamped = simple_app_clamp_gps_utc_offset(offset_minutes);
            if(clamped != app->gps_utc_offset_minutes) {
                app->gps_utc_offset_minutes = clamped;
                simple_app_mark_config_dirty(app);
            }
        }
    } else if(strcmp(tokens[0], "$GPGSV") == 0 || strcmp(tokens[0], "$GNGSV") == 0) {
        if(token_count > 3 && tokens[3][0] != '\0') {
            gps->satellites = atoi(tokens[3]);
        }
    } else if(strcmp(tokens[0], "$GPTXT") == 0 || strcmp(tokens[0], "$GNTXT") == 0) {
        if(token_count > 4 && tokens[4][0] != '\0') {
            if(simple_app_strcasestr(tokens[4], "ANTENNA OPEN")) {
                gps->antenna_status = GpsAntennaOpen;
            } else if(simple_app_strcasestr(tokens[4], "ANTENNA OK")) {
                gps->antenna_status = GpsAntennaOk;
            }
        }
    }

    if(app->screen == ScreenGps && app->viewport) {
        view_port_update(app->viewport);
    }
}

static void simple_app_gps_feed(SimpleApp* app, char ch) {
    if(!app) return;
    if(!app->gps_view_active || !app->gps_state) {
        if(app->gps_state) {
            app->gps_state->line_length = 0;
        }
        return;
    }
    if(ch == '\r') return;
    if(ch == '\n') {
        if(app->gps_state->line_length > 0) {
            app->gps_state->line_buffer[app->gps_state->line_length] = '\0';
            simple_app_process_gps_line(app, app->gps_state->line_buffer);
        }
        app->gps_state->line_length = 0;
        return;
    }
    if(app->gps_state->line_length + 1 >= sizeof(app->gps_state->line_buffer)) {
        app->gps_state->line_length = 0;
        return;
    }
    app->gps_state->line_buffer[app->gps_state->line_length++] = ch;
}

static void simple_app_reset_blackout_status(SimpleApp* app) {
    if(!app) return;
    app->blackout_full_console = false;
    app->blackout_running = false;
    app->blackout_phase = BlackoutPhaseIdle;
    app->blackout_networks = 0;
    app->blackout_cycle = 0;
    app->blackout_channel = 0;
    app->blackout_has_channel = false;
    app->blackout_note[0] = '\0';
    app->blackout_line_length = 0;
    app->blackout_last_update_tick = 0;
}

static void simple_app_blackout_set_phase(SimpleApp* app, BlackoutPhase phase, const char* note) {
    if(!app) return;
    app->blackout_phase = phase;
    app->blackout_running = (phase != BlackoutPhaseIdle && phase != BlackoutPhaseFinished);
    app->blackout_last_update_tick = furi_get_tick();
    if(note && note[0] != '\0') {
        strncpy(app->blackout_note, note, sizeof(app->blackout_note) - 1);
        app->blackout_note[sizeof(app->blackout_note) - 1] = '\0';
    } else if(note) {
        app->blackout_note[0] = '\0';
    }
}

static void simple_app_reset_sniffer_dog_status(SimpleApp* app) {
    if(!app) return;
    app->sniffer_dog_full_console = false;
    app->sniffer_dog_running = false;
    app->sniffer_dog_phase = SnifferDogPhaseIdle;
    app->sniffer_dog_deauth_count = 0;
    app->sniffer_dog_channel = 0;
    app->sniffer_dog_has_channel = false;
    app->sniffer_dog_rssi = 0;
    app->sniffer_dog_has_rssi = false;
    app->sniffer_dog_note[0] = '\0';
    app->sniffer_dog_line_length = 0;
    app->sniffer_dog_last_update_tick = 0;
}

static void
    simple_app_sniffer_dog_set_phase(SimpleApp* app, SnifferDogPhase phase, const char* note) {
    if(!app) return;
    app->sniffer_dog_phase = phase;
    app->sniffer_dog_running =
        (phase != SnifferDogPhaseIdle && phase != SnifferDogPhaseFinished &&
         phase != SnifferDogPhaseError);
    app->sniffer_dog_last_update_tick = furi_get_tick();
    if(note && note[0] != '\0') {
        strncpy(app->sniffer_dog_note, note, sizeof(app->sniffer_dog_note) - 1);
        app->sniffer_dog_note[sizeof(app->sniffer_dog_note) - 1] = '\0';
    } else if(note) {
        app->sniffer_dog_note[0] = '\0';
    }
}

static bool simple_app_blackout_parse_channel(const char* line, uint8_t* out_channel) {
    if(!line || !out_channel) return false;
    const char* found = strstr(line, "channel");
    if(found) {
        const char* digits = found + strlen("channel");
        while(*digits && !isdigit((unsigned char)*digits)) {
            digits++;
        }
        if(isdigit((unsigned char)*digits)) {
            unsigned long value = strtoul(digits, NULL, 10);
            if(value > 0 && value <= 255) {
                *out_channel = (uint8_t)value;
                return true;
            }
        }
    }

    const char* cursor = line;
    while((found = strstr(cursor, "ch")) != NULL) {
        char before = (found == line) ? ' ' : *(found - 1);
        bool boundary =
            (before == ' ' || before == '[' || before == '(' || before == ':' || before == '#');
        if(!boundary) {
            cursor = found + 2;
            continue;
        }
        const char* digits = found + 2;
        while(*digits && !isdigit((unsigned char)*digits)) {
            digits++;
        }
        if(!isdigit((unsigned char)*digits)) {
            cursor = found + 2;
            continue;
        }
        unsigned long value = strtoul(digits, NULL, 10);
        if(value == 0 || value > 255) {
            cursor = found + 2;
            continue;
        }
        *out_channel = (uint8_t)value;
        return true;
    }
    return false;
}

static bool simple_app_blackout_extract_networks(const char* line, uint32_t* out_count) {
    if(!line || !out_count) return false;
    if(strstr(line, "networks") == NULL) return false;
    const char* digits = line;
    while(*digits && !isdigit((unsigned char)*digits)) {
        digits++;
    }
    if(!isdigit((unsigned char)*digits)) return false;
    unsigned long value = strtoul(digits, NULL, 10);
    *out_count = (uint32_t)value;
    return true;
}

static void simple_app_process_sniffer_dog_line(SimpleApp* app, const char* line) {
    if(!app || !line) return;

    while(*line == '>' || *line == ' ') {
        line++;
    }
    if(*line == '\0') return;

    if(strstr(line, "Starting Sniffer Dog mode") != NULL) {
        app->sniffer_dog_view_active = true;
        simple_app_sniffer_dog_set_phase(app, SnifferDogPhaseStarting, "");
        return;
    }

    if(strstr(line, "Sniffer Dog started") != NULL) {
        app->sniffer_dog_view_active = true;
        simple_app_sniffer_dog_set_phase(app, SnifferDogPhaseHunting, "");
        return;
    }

    if(strstr(line, "Sniffer Dog already active") != NULL) {
        app->sniffer_dog_view_active = true;
        simple_app_sniffer_dog_set_phase(app, SnifferDogPhaseHunting, "Already active");
        return;
    }

    if(strstr(line, "Regular sniffer is active") != NULL) {
        app->sniffer_dog_view_active = true;
        simple_app_sniffer_dog_set_phase(app, SnifferDogPhaseError, "Stop sniffer");
        return;
    }

    if(strstr(line, "Failed to create Sniffer Dog channel hopping task") != NULL) {
        app->sniffer_dog_view_active = true;
        simple_app_sniffer_dog_set_phase(app, SnifferDogPhaseError, "Task failed");
        return;
    }

    if(strstr(line, "Stopping Sniffer Dog task") != NULL ||
       strstr(line, "Sniffer Dog task forcefully stopped") != NULL ||
       strstr(line, "Sniffer Dog channel task ending") != NULL) {
        app->sniffer_dog_view_active = true;
        simple_app_sniffer_dog_set_phase(app, SnifferDogPhaseStopping, "Stopping");
        return;
    }

    if(strstr(line, "Sniffer Dog stopped") != NULL) {
        app->sniffer_dog_view_active = true;
        simple_app_sniffer_dog_set_phase(app, SnifferDogPhaseFinished, "Stopped");
        return;
    }

    unsigned long deauth_count = 0;
    if(sscanf(line, "[SnifferDog #%lu]", &deauth_count) == 1) {
        app->sniffer_dog_view_active = true;
        app->sniffer_dog_deauth_count = (uint32_t)deauth_count;
        app->sniffer_dog_running = true;
        app->sniffer_dog_last_update_tick = furi_get_tick();

        const char* ch_ptr = strstr(line, "Ch=");
        if(ch_ptr) {
            int channel = atoi(ch_ptr + 3);
            if(channel > 0 && channel <= 255) {
                app->sniffer_dog_channel = (uint8_t)channel;
                app->sniffer_dog_has_channel = true;
            }
        }

        const char* rssi_ptr = strstr(line, "RSSI=");
        if(rssi_ptr) {
            int rssi = atoi(rssi_ptr + 5);
            app->sniffer_dog_rssi = rssi;
            app->sniffer_dog_has_rssi = true;
        }
        simple_app_sniffer_dog_set_phase(app, SnifferDogPhaseHunting, "");
        return;
    }
}

static void simple_app_sniffer_dog_feed(SimpleApp* app, char ch) {
    if(!app) return;
    if(ch == '\r') return;
    if(ch == '\n') {
        if(app->sniffer_dog_line_length > 0) {
            app->sniffer_dog_line_buffer[app->sniffer_dog_line_length] = '\0';
            simple_app_process_sniffer_dog_line(app, app->sniffer_dog_line_buffer);
        }
        app->sniffer_dog_line_length = 0;
        return;
    }
    if(app->sniffer_dog_line_length + 1 >= sizeof(app->sniffer_dog_line_buffer)) {
        app->sniffer_dog_line_length = 0;
        return;
    }
    app->sniffer_dog_line_buffer[app->sniffer_dog_line_length++] = ch;
}

static void simple_app_deauth_trim(char* text) {
    if(!text) return;
    char* start = text;
    while(*start && isspace((unsigned char)*start))
        start++;
    char* end = start + strlen(start);
    while(end > start && isspace((unsigned char)*(end - 1))) {
        end--;
    }
    size_t len = (size_t)(end - start);
    if(start != text) {
        memmove(text, start, len);
    }
    text[len] = '\0';
}

static void simple_app_deauth_set_note(SimpleApp* app, const char* note) {
    if(!app || !note) return;
    strncpy(app->deauth_note, note, sizeof(app->deauth_note) - 1);
    app->deauth_note[sizeof(app->deauth_note) - 1] = '\0';
    app->deauth_last_update_tick = furi_get_tick();
}

static void simple_app_reset_deauth_status(SimpleApp* app) {
    if(!app) return;
    app->deauth_full_console = false;
    app->deauth_running = false;
    app->deauth_targets = 0;
    app->deauth_overlay_allowed = false;
    app->deauth_channel = 0;
    app->deauth_has_channel = false;
    app->deauth_list_offset = 0;
    app->deauth_list_scroll_x = 0;
    app->deauth_blink_last_tick = 0;
    app->deauth_note[0] = '\0';
    app->deauth_line_length = 0;
    app->deauth_last_update_tick = 0;
    app->deauth_ssid[0] = '\0';
    app->deauth_bssid[0] = '\0';
}

static void simple_app_reset_handshake_status(SimpleApp* app) {
    if(!app) return;
    app->handshake_view_active = false;
    app->handshake_full_console = false;
    app->handshake_running = false;
    app->handshake_targets = 0;
    app->handshake_captured = 0;
    app->handshake_channel = 0;
    app->handshake_has_channel = false;
    app->handshake_rssi = 0;
    app->handshake_has_rssi = false;
    app->handshake_overlay_allowed = false;
    app->handshake_blink_last_tick = 0;
    app->handshake_vibro_done = false;
    app->handshake_note[0] = '\0';
    app->handshake_line_length = 0;
    app->handshake_ssid[0] = '\0';
}

static void simple_app_reset_sae_status(SimpleApp* app) {
    if(!app) return;
    app->sae_view_active = false;
    app->sae_full_console = false;
    app->sae_running = false;
    app->sae_overlay_allowed = false;
    app->sae_has_channel = false;
    app->sae_channel = 0;
    app->sae_note[0] = '\0';
    app->sae_line_length = 0;
    app->sae_ssid[0] = '\0';
}

static bool simple_app_deauth_extract_targets(const char* line, uint32_t* out_count) {
    if(!line || !out_count) return false;
    const char* digits = line;
    while(*digits && !isdigit((unsigned char)*digits)) {
        digits++;
    }
    if(!isdigit((unsigned char)*digits)) return false;
    unsigned long value = strtoul(digits, NULL, 10);
    *out_count = (uint32_t)value;
    return true;
}

static void simple_app_process_deauth_line(SimpleApp* app, const char* line) {
    if(!app || !line) return;
    if(!app->deauth_overlay_allowed) return;

    while(*line == '>' || *line == ' ') {
        line++;
    }
    if(*line == '\0') return;

    if(strstr(line, "Evil twin: no selected APs") != NULL) {
        app->deauth_view_active = true;
        app->deauth_running = false;
        simple_app_deauth_set_note(app, "No targets");
        return;
    }

    if(strstr(line, "Deauth attack already running") != NULL) {
        app->deauth_view_active = true;
        app->deauth_running = true;
        simple_app_deauth_set_note(app, "Already");
        return;
    }

    if(strstr(line, "Deauth attack started") != NULL) {
        app->deauth_view_active = true;
        app->deauth_running = true;
        simple_app_deauth_set_note(app, "Running");
        return;
    }

    if(strstr(line, "Starting deauth attack") != NULL) {
        app->deauth_view_active = true;
        app->deauth_running = true;
        simple_app_deauth_set_note(app, "Starting");
        return;
    }

    if(strstr(line, "Stopping deauth attack task") != NULL) {
        app->deauth_view_active = true;
        app->deauth_running = false;
        simple_app_deauth_set_note(app, "Stopping");
        return;
    }

    if(strstr(line, "Deauth attack task forcefully stopped") != NULL) {
        app->deauth_view_active = true;
        app->deauth_running = false;
        simple_app_deauth_set_note(app, "Force stop");
        return;
    }

    if(strstr(line, "Deauth attack task finished") != NULL ||
       strstr(line, "Deauth attack stopped") != NULL) {
        app->deauth_view_active = true;
        app->deauth_running = false;
        simple_app_deauth_set_note(app, "Stopped");
        return;
    }

    if(strstr(line, "Attacking") != NULL && strstr(line, "network") != NULL) {
        uint32_t targets = 0;
        if(simple_app_deauth_extract_targets(line, &targets)) {
            app->deauth_targets = targets;
        }
        app->deauth_view_active = true;
        app->deauth_last_update_tick = furi_get_tick();
        return;
    }

    if(strstr(line, "Target BSSID[") != NULL) {
        char ssid[SCAN_SSID_MAX_LEN];
        char bssid[18];
        int channel = 0;
        int matched = sscanf(
            line, "Target BSSID[%*u]: %32[^,], BSSID: %17[^,], Channel: %d", ssid, bssid, &channel);
        if(matched >= 2) {
            simple_app_deauth_trim(ssid);
            simple_app_deauth_trim(bssid);
            strncpy(app->deauth_ssid, ssid, sizeof(app->deauth_ssid) - 1);
            app->deauth_ssid[sizeof(app->deauth_ssid) - 1] = '\0';
            strncpy(app->deauth_bssid, bssid, sizeof(app->deauth_bssid) - 1);
            app->deauth_bssid[sizeof(app->deauth_bssid) - 1] = '\0';
        }
        if(matched == 3 && channel > 0 && channel <= 255) {
            app->deauth_channel = (uint8_t)channel;
            app->deauth_has_channel = true;
        }
        app->deauth_view_active = true;
        app->deauth_last_update_tick = furi_get_tick();
        return;
    }
}

static void simple_app_deauth_feed(SimpleApp* app, char ch) {
    if(!app) return;
    if(ch == '\r') return;
    if(ch == '\n') {
        if(app->deauth_line_length > 0) {
            app->deauth_line_buffer[app->deauth_line_length] = '\0';
            simple_app_process_deauth_line(app, app->deauth_line_buffer);
        }
        app->deauth_line_length = 0;
        return;
    }
    if(app->deauth_line_length + 1 >= sizeof(app->deauth_line_buffer)) {
        app->deauth_line_length = 0;
        return;
    }
    app->deauth_line_buffer[app->deauth_line_length++] = ch;
}

static bool simple_app_alloc_bt_buffers(SimpleApp* app) {
    if(!app) return false;
    if(app->bt_scan_preview && app->bt_locator_devices && app->bt_locator_scan_line &&
       app->bt_scan_line_buffer && app->bt_locator_scroll_text) {
        return true;
    }
    app->bt_scan_preview = calloc(BT_SCAN_PREVIEW_MAX, sizeof(BtScanPreview));
    app->bt_locator_devices = calloc(BT_LOCATOR_MAX_DEVICES, sizeof(BtLocatorDevice));
    app->bt_locator_scan_line = calloc(BT_LOCATOR_SCAN_LINE_LEN, 1);
    app->bt_scan_line_buffer = calloc(BT_SCAN_LINE_BUFFER_LEN, 1);
    app->bt_locator_scroll_text = calloc(BT_LOCATOR_SCROLL_TEXT_LEN, 1);
    if(!app->bt_scan_preview || !app->bt_locator_devices || !app->bt_locator_scan_line ||
       !app->bt_scan_line_buffer || !app->bt_locator_scroll_text) {
        if(app->bt_scan_preview) {
            free(app->bt_scan_preview);
            app->bt_scan_preview = NULL;
        }
        if(app->bt_locator_devices) {
            free(app->bt_locator_devices);
            app->bt_locator_devices = NULL;
        }
        if(app->bt_locator_scan_line) {
            free(app->bt_locator_scan_line);
            app->bt_locator_scan_line = NULL;
        }
        if(app->bt_scan_line_buffer) {
            free(app->bt_scan_line_buffer);
            app->bt_scan_line_buffer = NULL;
        }
        if(app->bt_locator_scroll_text) {
            free(app->bt_locator_scroll_text);
            app->bt_locator_scroll_text = NULL;
        }
        return false;
    }
    return true;
}

static void simple_app_free_bt_buffers(SimpleApp* app) {
    if(!app) return;
    if(app->bt_scan_preview) {
        free(app->bt_scan_preview);
        app->bt_scan_preview = NULL;
    }
    if(app->bt_locator_devices) {
        free(app->bt_locator_devices);
        app->bt_locator_devices = NULL;
    }
    if(app->bt_locator_scan_line) {
        free(app->bt_locator_scan_line);
        app->bt_locator_scan_line = NULL;
    }
    if(app->bt_scan_line_buffer) {
        free(app->bt_scan_line_buffer);
        app->bt_scan_line_buffer = NULL;
    }
    if(app->bt_locator_scroll_text) {
        free(app->bt_locator_scroll_text);
        app->bt_locator_scroll_text = NULL;
    }
}

static bool simple_app_alloc_evil_twin_qr_buffers(SimpleApp* app) {
    if(!app) return false;
    if(app->evil_twin_qr && app->evil_twin_qr_temp) return true;
    app->evil_twin_qr = malloc(EVIL_TWIN_QR_BUFFER_LEN);
    app->evil_twin_qr_temp = malloc(EVIL_TWIN_QR_BUFFER_LEN);
    if(!app->evil_twin_qr || !app->evil_twin_qr_temp) {
        if(app->evil_twin_qr) {
            free(app->evil_twin_qr);
            app->evil_twin_qr = NULL;
        }
        if(app->evil_twin_qr_temp) {
            free(app->evil_twin_qr_temp);
            app->evil_twin_qr_temp = NULL;
        }
        return false;
    }
    return true;
}

static void simple_app_free_evil_twin_qr_buffers(SimpleApp* app) {
    if(!app) return;
    if(app->evil_twin_qr) {
        free(app->evil_twin_qr);
        app->evil_twin_qr = NULL;
    }
    if(app->evil_twin_qr_temp) {
        free(app->evil_twin_qr_temp);
        app->evil_twin_qr_temp = NULL;
    }
    app->evil_twin_qr_valid = false;
    app->evil_twin_qr_error[0] = '\0';
}

static bool simple_app_alloc_help_qr_buffers(SimpleApp* app) {
    if(!app) return false;
    if(app->help_qr && app->help_qr_temp) return true;
    app->help_qr = malloc(EVIL_TWIN_QR_BUFFER_LEN);
    app->help_qr_temp = malloc(EVIL_TWIN_QR_BUFFER_LEN);
    if(!app->help_qr || !app->help_qr_temp) {
        if(app->help_qr) {
            free(app->help_qr);
            app->help_qr = NULL;
        }
        if(app->help_qr_temp) {
            free(app->help_qr_temp);
            app->help_qr_temp = NULL;
        }
        return false;
    }
    return true;
}

static void simple_app_free_help_qr_buffers(SimpleApp* app) {
    if(!app) return;
    if(app->help_qr) {
        free(app->help_qr);
        app->help_qr = NULL;
    }
    if(app->help_qr_temp) {
        free(app->help_qr_temp);
        app->help_qr_temp = NULL;
    }
    app->help_qr_valid = false;
    app->help_qr_ready = false;
    app->help_qr_error[0] = '\0';
}

static void simple_app_handshake_set_note(SimpleApp* app, const char* note) {
    if(!app || !note) return;
    strncpy(app->handshake_note, note, sizeof(app->handshake_note) - 1);
    app->handshake_note[sizeof(app->handshake_note) - 1] = '\0';
}

static void simple_app_handshake_buzz_once(SimpleApp* app) {
    if(!app || app->handshake_vibro_done) return;
    app->handshake_vibro_done = true;
    furi_hal_vibro_on(true);
    furi_delay_ms(40);
    furi_hal_vibro_on(false);
}

static void simple_app_handshake_parse_attacking(SimpleApp* app, const char* line) {
    if(!app || !line) return;
    char ssid[SCAN_SSID_MAX_LEN];
    int channel = 0;
    int rssi = 0;
    int matched = sscanf(
        line, ">>> [%*u/%*u] Attacking '%32[^']' (Ch %d, RSSI: %d dBm) <<<", ssid, &channel, &rssi);
    if(matched >= 2) {
        simple_app_trim(ssid);
        strncpy(app->handshake_ssid, ssid, sizeof(app->handshake_ssid) - 1);
        app->handshake_ssid[sizeof(app->handshake_ssid) - 1] = '\0';
        if(channel > 0 && channel <= 255) {
            app->handshake_channel = (uint8_t)channel;
            app->handshake_has_channel = true;
        }
        if(matched >= 3) {
            app->handshake_rssi = rssi;
            app->handshake_has_rssi = true;
        }
        app->handshake_running = true;
        if(app->handshake_has_rssi) {
            char note[32];
            snprintf(
                note, sizeof(note), "Attack CH%u %d", app->handshake_channel, app->handshake_rssi);
            simple_app_handshake_set_note(app, note);
        } else if(app->handshake_has_channel) {
            char note[32];
            snprintf(note, sizeof(note), "Attack CH%u", app->handshake_channel);
            simple_app_handshake_set_note(app, note);
        } else {
            simple_app_handshake_set_note(app, "Attacking");
        }
    }
}

static bool simple_app_alloc_sniffer_buffers(SimpleApp* app) {
    if(!app) return false;
    if(app->sniffer_aps && app->sniffer_clients) return true;
    app->sniffer_aps = calloc(SNIFFER_MAX_APS, sizeof(SnifferApEntry));
    app->sniffer_clients = calloc(SNIFFER_MAX_CLIENTS, sizeof(SnifferClientEntry));
    if(!app->sniffer_aps || !app->sniffer_clients) {
        simple_app_free_sniffer_buffers(app);
        return false;
    }
    return true;
}

static void simple_app_free_sniffer_buffers(SimpleApp* app) {
    if(!app) return;
    if(app->sniffer_aps) {
        free(app->sniffer_aps);
        app->sniffer_aps = NULL;
    }
    if(app->sniffer_clients) {
        free(app->sniffer_clients);
        app->sniffer_clients = NULL;
    }
}

static bool simple_app_alloc_probe_buffers(SimpleApp* app) {
    if(!app) return false;
    if(app->probe_ssids && app->probe_clients) return true;
    app->probe_ssids = calloc(PROBE_MAX_SSIDS, sizeof(ProbeSsidEntry));
    app->probe_clients = calloc(PROBE_MAX_CLIENTS, sizeof(SnifferClientEntry));
    if(!app->probe_ssids || !app->probe_clients) {
        simple_app_free_probe_buffers(app);
        return false;
    }
    return true;
}

static void simple_app_free_probe_buffers(SimpleApp* app) {
    if(!app) return;
    if(app->probe_ssids) {
        free(app->probe_ssids);
        app->probe_ssids = NULL;
    }
    if(app->probe_clients) {
        free(app->probe_clients);
        app->probe_clients = NULL;
    }
}

static void simple_app_sae_set_note(SimpleApp* app, const char* note) {
    if(!app) return;
    if(note) {
        strncpy(app->sae_note, note, sizeof(app->sae_note) - 1);
        app->sae_note[sizeof(app->sae_note) - 1] = '\0';
    } else {
        app->sae_note[0] = '\0';
    }
}

static void simple_app_process_sae_line(SimpleApp* app, const char* line) {
    if(!app || !line) return;
    if(!app->sae_overlay_allowed) return;

    if(strstr(line, "WPA3 SAE Overflow Attack") != NULL ||
       strstr(line, "SAE attack started") != NULL ||
       strstr(line, "SAE overflow task started") != NULL) {
        app->sae_view_active = true;
        app->sae_running = true;
        simple_app_sae_set_note(app, "Running");
    }

    if(strstr(line, "Stopping SAE overflow task") != NULL ||
       strstr(line, "SAE overflow: Stop requested") != NULL ||
       strstr(line, "SAE overflow task finished") != NULL ||
       strstr(line, "SAE overflow task forcefully stopped") != NULL) {
        app->sae_view_active = true;
        app->sae_running = false;
        simple_app_sae_set_note(app, NULL);
    }

    const char* ssid_ptr = strstr(line, "SSID='");
    if(ssid_ptr) {
        ssid_ptr += 6;
        const char* ssid_end = strchr(ssid_ptr, '\'');
        if(ssid_end && ssid_end > ssid_ptr) {
            size_t len = (size_t)(ssid_end - ssid_ptr);
            if(len >= sizeof(app->sae_ssid)) len = sizeof(app->sae_ssid) - 1;
            memcpy(app->sae_ssid, ssid_ptr, len);
            app->sae_ssid[len] = '\0';
            app->sae_view_active = true;
        }
        const char* ch_ptr = strstr(ssid_end ? ssid_end : ssid_ptr, "Ch=");
        if(ch_ptr) {
            int channel = atoi(ch_ptr + 3);
            if(channel > 0) {
                app->sae_channel = (uint8_t)channel;
                app->sae_has_channel = true;
            }
        }
    }
}

static void simple_app_sae_feed(SimpleApp* app, char ch) {
    if(!app) return;
    if(!app->sae_overlay_allowed) {
        app->sae_line_length = 0;
        return;
    }
    if(ch == '\r') return;
    if(ch == '\n') {
        if(app->sae_line_length > 0) {
            app->sae_line_buffer[app->sae_line_length] = '\0';
            simple_app_process_sae_line(app, app->sae_line_buffer);
        }
        app->sae_line_length = 0;
        return;
    }
    if(app->sae_line_length + 1 >= sizeof(app->sae_line_buffer)) {
        app->sae_line_length = 0;
        return;
    }
    app->sae_line_buffer[app->sae_line_length++] = ch;
}

static void simple_app_process_handshake_line(SimpleApp* app, const char* line) {
    if(!app || !line) return;
    if(!app->handshake_overlay_allowed) return;

    while(*line == '>' || *line == ' ') {
        line++;
    }
    if(*line == '\0') return;

    if(strstr(line, "Starting WPA Handshake Capture") != NULL) {
        app->handshake_view_active = true;
        app->handshake_running = true;
        simple_app_handshake_set_note(app, "Starting");
        return;
    }
    if(strstr(line, "Enabling AP mode") != NULL) {
        app->handshake_view_active = true;
        app->handshake_running = true;
        simple_app_handshake_set_note(app, "AP enable");
        return;
    }
    if(strstr(line, "AP mode enabled") != NULL) {
        app->handshake_view_active = true;
        app->handshake_running = true;
        simple_app_handshake_set_note(app, "AP ready");
        return;
    }
    if(strstr(line, "Handshake attack task started") != NULL) {
        app->handshake_view_active = true;
        app->handshake_running = true;
        simple_app_handshake_set_note(app, "Running");
        return;
    }
    if(strstr(line, "Handshake attack task finished") != NULL) {
        app->handshake_view_active = true;
        app->handshake_running = false;
        simple_app_handshake_set_note(app, "Finished");
        return;
    }
    if(strstr(line, "Attack complete") != NULL ||
       strstr(line, "All selected networks captured") != NULL) {
        app->handshake_view_active = true;
        app->handshake_running = false;
        simple_app_handshake_set_note(app, "Complete");
        return;
    }
    if(strstr(line, "Attack Cycle Complete") != NULL) {
        app->handshake_view_active = true;
        simple_app_handshake_set_note(app, "Cycle done");
        return;
    }
    if(strstr(line, "Selected mode:") != NULL || strstr(line, "Selected Networks Mode") != NULL) {
        app->handshake_view_active = true;
        app->handshake_running = true;
        simple_app_handshake_set_note(app, "Running");
        return;
    }
    if(strstr(line, "Attacking ") != NULL && strstr(line, "networks") != NULL) {
        app->handshake_view_active = true;
        app->handshake_running = true;
        simple_app_handshake_set_note(app, "Running");
        return;
    }
    if(strstr(line, "Saving handshake") != NULL ||
       strstr(line, "Attempting to save handshake") != NULL) {
        app->handshake_view_active = true;
        simple_app_handshake_set_note(app, "Saving");
        return;
    }
    if(strstr(line, "No handshake for") != NULL) {
        app->handshake_view_active = true;
        app->handshake_running = true;
        simple_app_handshake_set_note(app, "No handshake");
        return;
    }
    if(strstr(line, "Handshake captured for") != NULL) {
        app->handshake_view_active = true;
        app->handshake_running = true;
        simple_app_handshake_set_note(app, "Captured");
        simple_app_handshake_buzz_once(app);
        return;
    }
    if(strstr(line, "HANDSHAKE IS COMPLETE AND VALID") != NULL ||
       strstr(line, "Complete 4-way handshake saved") != NULL) {
        app->handshake_view_active = true;
        app->handshake_running = true;
        simple_app_handshake_set_note(app, "Valid");
        simple_app_handshake_buzz_once(app);
        return;
    }

    unsigned long targets = 0;
    if(sscanf(line, "Targets: %lu network(s)", &targets) == 1) {
        app->handshake_targets = (uint32_t)targets;
        app->handshake_view_active = true;
        if(app->handshake_targets == 0) {
            simple_app_handshake_set_note(app, "No targets");
        }
        return;
    }

    unsigned long captured = 0;
    if(sscanf(line, "Handshakes captured so far: %lu", &captured) == 1) {
        app->handshake_captured = (uint32_t)captured;
        app->handshake_view_active = true;
        return;
    }

    const char* captured_ptr = strstr(line, "Handshake #");
    if(captured_ptr) {
        unsigned captured_index = 0;
        if(sscanf(captured_ptr, "Handshake #%u captured!", &captured_index) == 1) {
            app->handshake_captured = captured_index;
            app->handshake_view_active = true;
            simple_app_handshake_buzz_once(app);
            return;
        }
    }

    if(strstr(line, ">>> [") != NULL && strstr(line, "Attacking '") != NULL) {
        app->handshake_view_active = true;
        simple_app_handshake_parse_attacking(app, line);
        return;
    }
}

static void simple_app_handshake_feed(SimpleApp* app, char ch) {
    if(!app) return;
    if(ch == '\r') return;
    if(ch == '\n') {
        if(app->handshake_line_length > 0) {
            app->handshake_line_buffer[app->handshake_line_length] = '\0';
            simple_app_process_handshake_line(app, app->handshake_line_buffer);
        }
        app->handshake_line_length = 0;
        return;
    }
    if(app->handshake_line_length + 1 >= sizeof(app->handshake_line_buffer)) {
        app->handshake_line_length = 0;
        return;
    }
    app->handshake_line_buffer[app->handshake_line_length++] = ch;
}

static void simple_app_process_wardrive_line(SimpleApp* app, const char* line) {
    if(!app || !line || !app->wardrive_view_active || !app->wardrive_state) return;

    if(strstr(line, "Wardrive task started") != NULL) {
        simple_app_set_wardrive_status(app, "Starting", false);
        return;
    }

    if(strstr(line, "Waiting for GPS fix") != NULL) {
        simple_app_set_wardrive_status(app, "waiting for GPS signal", false);
        return;
    }

    const char* logged_ptr = strstr(line, "Logged ");
    if(!logged_ptr) return;
    logged_ptr += strlen("Logged ");
    while(*logged_ptr == ' ') {
        logged_ptr++;
    }

    char digits[12];
    size_t idx = 0;
    while(isdigit((unsigned char)logged_ptr[idx]) && idx < sizeof(digits) - 1) {
        digits[idx] = logged_ptr[idx];
        idx++;
    }
    if(idx == 0) return;
    digits[idx] = '\0';

    if(strstr(line, "networks to") != NULL) {
        simple_app_set_wardrive_status(app, digits, true);
    }
}

static void simple_app_wardrive_feed(SimpleApp* app, char ch) {
    if(!app) return;
    if(!app->wardrive_view_active || !app->wardrive_state) {
        if(app->wardrive_state) {
            app->wardrive_state->line_length = 0;
        }
        return;
    }
    if(ch == '\r') return;
    if(ch == '\n') {
        if(app->wardrive_state->line_length > 0) {
            app->wardrive_state->line_buffer[app->wardrive_state->line_length] = '\0';
            simple_app_process_wardrive_line(app, app->wardrive_state->line_buffer);
        }
        app->wardrive_state->line_length = 0;
        return;
    }
    if(app->wardrive_state->line_length + 1 >= sizeof(app->wardrive_state->line_buffer)) {
        app->wardrive_state->line_length = 0;
        return;
    }
    app->wardrive_state->line_buffer[app->wardrive_state->line_length++] = ch;
}

static void simple_app_process_blackout_line(SimpleApp* app, const char* line) {
    if(!app || !line) return;

    while(*line == '>' || *line == ' ') {
        line++;
    }
    if(*line == '\0') return;

    if(strstr(line, "Blackout attack task started") != NULL ||
       strstr(line, "Starting blackout attack") != NULL) {
        app->blackout_view_active = true;
        if(app->blackout_cycle == 0) app->blackout_cycle = 1;
        simple_app_blackout_set_phase(app, BlackoutPhaseStarting, "");
        return;
    }

    if(strstr(line, "Starting blackout cycle") != NULL) {
        app->blackout_view_active = true;
        if(app->blackout_cycle == 0) {
            app->blackout_cycle = 1;
        }
        simple_app_blackout_set_phase(app, BlackoutPhaseScanning, "");
        return;
    }

    if(strstr(line, "Stopping blackout attack task") != NULL) {
        app->blackout_view_active = true;
        simple_app_blackout_set_phase(app, BlackoutPhaseStopping, "Stopping");
        return;
    }

    if(strstr(line, "Blackout attack task finished") != NULL) {
        app->blackout_view_active = true;
        simple_app_blackout_set_phase(app, BlackoutPhaseFinished, "Done");
        return;
    }

    if(!app->blackout_view_active) {
        return;
    }

    if(strstr(line, "Found") != NULL && strstr(line, "networks") != NULL &&
       strstr(line, "sorting by channel") != NULL) {
        uint32_t networks = 0;
        if(simple_app_blackout_extract_networks(line, &networks)) {
            app->blackout_networks = networks;
        }
        app->blackout_view_active = true;
        simple_app_blackout_set_phase(app, BlackoutPhaseSorting, "");
        return;
    }

    if(strstr(line, "Starting deauth attack") != NULL) {
        uint32_t networks = 0;
        if(simple_app_blackout_extract_networks(line, &networks)) {
            app->blackout_networks = networks;
        }
        app->blackout_view_active = true;
        simple_app_blackout_set_phase(app, BlackoutPhaseAttacking, "");
        return;
    }

    if(strstr(line, "3-minute attack cycle completed") != NULL) {
        app->blackout_view_active = true;
        app->blackout_cycle++;
        simple_app_blackout_set_phase(app, BlackoutPhaseScanning, "");
        return;
    }

    if(strstr(line, "Scan timeout") != NULL) {
        app->blackout_view_active = true;
        simple_app_blackout_set_phase(app, BlackoutPhaseScanning, "Scan timeout");
        return;
    }

    if(strstr(line, "No scan results available") != NULL) {
        app->blackout_view_active = true;
        simple_app_blackout_set_phase(app, BlackoutPhaseScanning, "No results");
        return;
    }

    if(strstr(line, "Failed to start scan") != NULL) {
        app->blackout_view_active = true;
        simple_app_blackout_set_phase(app, BlackoutPhaseScanning, "Scan failed");
        return;
    }

    if(strstr(line, "Stop requested") != NULL ||
       strstr(line, "Stopping blackout attack task") != NULL) {
        app->blackout_view_active = true;
        simple_app_blackout_set_phase(app, BlackoutPhaseStopping, "Stopping");
        return;
    }

    uint8_t channel = 0;
    if(simple_app_blackout_parse_channel(line, &channel)) {
        app->blackout_channel = channel;
        app->blackout_has_channel = true;
    }
}

static void simple_app_blackout_feed(SimpleApp* app, char ch) {
    if(!app) return;
    if(ch == '\r') return;
    if(ch == '\n') {
        if(app->blackout_line_length > 0) {
            app->blackout_line_buffer[app->blackout_line_length] = '\0';
            simple_app_process_blackout_line(app, app->blackout_line_buffer);
        }
        app->blackout_line_length = 0;
        return;
    }
    if(app->blackout_line_length + 1 >= sizeof(app->blackout_line_buffer)) {
        app->blackout_line_length = 0;
        return;
    }
    app->blackout_line_buffer[app->blackout_line_length++] = ch;
}

static void simple_app_reset_sniffer_status(SimpleApp* app) {
    if(!app) return;
    app->sniffer_running = false;
    app->sniffer_has_data = false;
    app->sniffer_scan_done = false;
    app->sniffer_packet_count = 0;
    app->sniffer_networks = 0;
    app->sniffer_last_update_tick = 0;
    app->sniffer_mode[0] = '\0';
    app->sniffer_line_length = 0;
    app->sniffer_full_console = false;
}

static void simple_app_reset_sniffer_results(SimpleApp* app) {
    if(!app) return;
    if(app->sniffer_aps) {
        memset(app->sniffer_aps, 0, sizeof(SnifferApEntry) * SNIFFER_MAX_APS);
    }
    if(app->sniffer_clients) {
        memset(app->sniffer_clients, 0, sizeof(SnifferClientEntry) * SNIFFER_MAX_CLIENTS);
    }
    app->sniffer_ap_count = 0;
    app->sniffer_client_count = 0;
    app->sniffer_results_ap_index = 0;
    app->sniffer_results_client_offset = 0;
    app->sniffer_results_loading = false;
    app->sniffer_selected_count = 0;
    memset(app->sniffer_selected_macs, 0, sizeof(app->sniffer_selected_macs));
}

static void simple_app_reset_probe_results(SimpleApp* app) {
    if(!app) return;
    if(app->probe_ssids) {
        memset(app->probe_ssids, 0, sizeof(ProbeSsidEntry) * PROBE_MAX_SSIDS);
    }
    if(app->probe_clients) {
        memset(app->probe_clients, 0, sizeof(SnifferClientEntry) * PROBE_MAX_CLIENTS);
    }
    app->probe_ssid_count = 0;
    app->probe_client_count = 0;
    app->probe_ssid_index = 0;
    app->probe_client_offset = 0;
    app->probe_total = 0;
    app->probe_results_loading = false;
    app->probe_full_console = false;
}

static void simple_app_clear_sniffer_data(SimpleApp* app) {
    if(!app) return;
    app->sniffer_view_active = false;
    app->sniffer_results_active = false;
    app->probe_results_active = false;
    app->sniffer_full_console = false;
    app->probe_full_console = false;
    simple_app_reset_sniffer_status(app);
    app->sniffer_ap_count = 0;
    app->sniffer_client_count = 0;
    app->sniffer_results_ap_index = 0;
    app->sniffer_results_client_offset = 0;
    app->sniffer_results_loading = false;
    app->probe_ssid_count = 0;
    app->probe_client_count = 0;
    app->probe_ssid_index = 0;
    app->probe_client_offset = 0;
    app->probe_total = 0;
    app->probe_results_loading = false;
    simple_app_free_sniffer_buffers(app);
    simple_app_free_probe_buffers(app);
}

static int simple_app_find_sniffer_selected(const SimpleApp* app, const char* mac) {
    if(!app || !mac || mac[0] == '\0') return -1;
    for(size_t i = 0; i < app->sniffer_selected_count; i++) {
        if(strcmp(app->sniffer_selected_macs[i], mac) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static bool simple_app_sniffer_station_is_selected(const SimpleApp* app, const char* mac) {
    return simple_app_find_sniffer_selected(app, mac) >= 0;
}

static void simple_app_toggle_sniffer_selected(SimpleApp* app, const char* mac) {
    if(!app || !mac || mac[0] == '\0' || !strchr(mac, ':')) return;
    int existing = simple_app_find_sniffer_selected(app, mac);
    if(existing >= 0) {
        for(size_t i = (size_t)existing; i + 1 < app->sniffer_selected_count; i++) {
            strncpy(
                app->sniffer_selected_macs[i],
                app->sniffer_selected_macs[i + 1],
                sizeof(app->sniffer_selected_macs[i]) - 1);
            app->sniffer_selected_macs[i][sizeof(app->sniffer_selected_macs[i]) - 1] = '\0';
        }
        if(app->sniffer_selected_count > 0) {
            app->sniffer_selected_count--;
            app->sniffer_selected_macs[app->sniffer_selected_count][0] = '\0';
        }
        return;
    }
    if(app->sniffer_selected_count >= SNIFFER_MAX_SELECTED_STATIONS) {
        simple_app_show_status_message(app, "Selection full", 1200, true);
        return;
    }
    strncpy(
        app->sniffer_selected_macs[app->sniffer_selected_count],
        mac,
        sizeof(app->sniffer_selected_macs[app->sniffer_selected_count]) - 1);
    app->sniffer_selected_macs[app->sniffer_selected_count]
                              [sizeof(app->sniffer_selected_macs[app->sniffer_selected_count]) - 1] =
        '\0';
    app->sniffer_selected_count++;
}

static void simple_app_clear_station_selection(SimpleApp* app) {
    if(!app) return;
    if(app->sniffer_selected_count == 0) return;
    app->sniffer_selected_count = 0;
    memset(app->sniffer_selected_macs, 0, sizeof(app->sniffer_selected_macs));
    simple_app_send_command_quiet(app, "unselect_stations");
}

static void simple_app_send_select_stations(SimpleApp* app) {
    if(!app) return;
    char command[256];
    size_t written = snprintf(command, sizeof(command), "select_stations");
    if(written >= sizeof(command)) {
        command[sizeof(command) - 1] = '\0';
        written = strlen(command);
    }

    size_t added_count = 0;
    for(size_t i = 0; i < app->sniffer_selected_count && written < sizeof(command) - 1; i++) {
        const char* mac = app->sniffer_selected_macs[i];
        if(!mac || mac[0] == '\0' || !strchr(mac, ':')) continue;
        int added = snprintf(command + written, sizeof(command) - written, " %s", mac);
        if(added < 0) break;
        written += (size_t)added;
        if(written >= sizeof(command)) {
            written = sizeof(command) - 1;
            command[written] = '\0';
            break;
        }
        added_count++;
    }

    if(added_count == 0) {
        simple_app_show_status_message(app, "No stations\nselected", 1200, true);
        return;
    }

    simple_app_send_command(app, command, false);
    app->last_attack_index = 1; // Deauth entry
    simple_app_focus_attacks_menu(app);
    if(app->viewport) {
        view_port_update(app->viewport);
    }
}

static void simple_app_sniffer_trim(char* text) {
    if(!text) return;
    char* start = text;
    while(*start && isspace((unsigned char)*start))
        start++;
    char* end = start + strlen(start);
    while(end > start && isspace((unsigned char)*(end - 1))) {
        end--;
    }
    size_t len = (size_t)(end - start);
    if(start != text) {
        memmove(text, start, len);
    }
    text[len] = '\0';
}

static int simple_app_hex_value(char value) {
    if(value >= '0' && value <= '9') return value - '0';
    if(value >= 'a' && value <= 'f') return 10 + (value - 'a');
    if(value >= 'A' && value <= 'F') return 10 + (value - 'A');
    return -1;
}

static bool simple_app_parse_mac_bytes(const char* mac, uint8_t out[6]) {
    if(!mac || !out) return false;
    size_t out_idx = 0;
    int nibble = -1;
    for(size_t i = 0; mac[i] != '\0'; i++) {
        char c = mac[i];
        if(c == ':' || isspace((unsigned char)c)) {
            continue;
        }
        int value = simple_app_hex_value(c);
        if(value < 0) {
            return false;
        }
        if(nibble < 0) {
            nibble = value;
        } else {
            if(out_idx >= 6) {
                return false;
            }
            out[out_idx++] = (uint8_t)((nibble << 4) | value);
            nibble = -1;
        }
    }
    return (out_idx == 6 && nibble < 0);
}

static bool
    simple_app_sniffer_extract_mac_token(const char* line, char* out_mac, size_t out_mac_size) {
    if(!line || !out_mac || out_mac_size == 0) return false;

    char buf[64];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    simple_app_sniffer_trim(buf);

    const char* start = buf;
    while(*start != '\0' && !isxdigit((unsigned char)*start))
        start++;
    if(*start == '\0') return false;

    const char* end = start;
    while(*end != '\0' && !isspace((unsigned char)*end) && *end != ',' && *end != ')' &&
          *end != '(') {
        end++;
    }

    size_t len = (size_t)(end - start);
    if(len == 0) return false;
    if(len >= out_mac_size) len = out_mac_size - 1;
    memcpy(out_mac, start, len);
    out_mac[len] = '\0';
    simple_app_sniffer_trim(out_mac);
    return out_mac[0] != '\0';
}

static bool
    simple_app_sniffer_parse_header(SimpleApp* app, const char* line, SnifferApEntry* out_entry) {
    if(!app || !line || !out_entry) return false;
    char buf[64];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    simple_app_sniffer_trim(buf);
    line = buf;
    const char* ch_ptr = strstr(line, "CH");
    const char* colon_ptr = strchr(line, ':');
    if(!ch_ptr || !colon_ptr) return false;
    if(ch_ptr > colon_ptr) return false;

    unsigned channel = 0;
    unsigned expected = 0;
    if(sscanf(ch_ptr, "CH%u: %u", &channel, &expected) < 1) {
        return false;
    }
    if(channel > UINT8_MAX) channel = UINT8_MAX;

    const char* comma = strchr(line, ',');
    size_t ssid_len = 0;
    const char* ssid_end = NULL;
    if(comma && comma < ch_ptr) {
        ssid_end = comma;
    } else {
        ssid_end = ch_ptr;
        while(ssid_end > line && isspace((unsigned char)*(ssid_end - 1))) {
            ssid_end--;
        }
    }

    if(ssid_end && ssid_end > line) {
        ssid_len = (size_t)(ssid_end - line);
        if(ssid_len >= sizeof(out_entry->ssid)) {
            ssid_len = sizeof(out_entry->ssid) - 1;
        }
        memcpy(out_entry->ssid, line, ssid_len);
    } else {
        ssid_len = 0;
    }
    out_entry->ssid[ssid_len] = '\0';
    simple_app_sniffer_trim(out_entry->ssid);
    if(out_entry->ssid[0] == '\0') {
        strncpy(out_entry->ssid, "<hidden>", sizeof(out_entry->ssid) - 1);
        out_entry->ssid[sizeof(out_entry->ssid) - 1] = '\0';
    }

    out_entry->channel = (uint8_t)channel;
    out_entry->expected_count = (uint16_t)expected;
    out_entry->client_start = 0;
    out_entry->client_count = 0;
    return true;
}

static bool simple_app_sniffer_is_mac(const char* line) {
    if(!line) return false;
    char buf[32];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    simple_app_sniffer_trim(buf);
    line = buf;
    size_t len = strlen(line);
    if(len < 11 || len > 20) return false;
    int octets = 0;
    for(size_t i = 0; line[i] != '\0'; i++) {
        char c = line[i];
        if(c == ':') continue;
        if(isxdigit((unsigned char)c)) {
            if(i == 0 || line[i - 1] == ':' || isspace((unsigned char)line[i - 1])) {
                octets++;
            }
        } else if(isspace((unsigned char)c)) {
            continue;
        } else {
            return false;
        }
    }
    return octets >= 2;
}

static void simple_app_process_sniffer_line(SimpleApp* app, const char* line) {
    if(!app || !line || (!app->sniffer_view_active && !app->sniffer_results_active)) return;

    if(app->sniffer_results_active) {
        if(!app->sniffer_aps || !app->sniffer_clients) {
            if(!simple_app_alloc_sniffer_buffers(app)) {
                simple_app_show_status_message(app, "OOM: sniffer\nbuffers", 1500, true);
                return;
            }
        }
        if(line[0] == '>') {
            app->sniffer_results_loading = false;
            return;
        }

        SnifferApEntry entry;
        if(simple_app_sniffer_parse_header(app, line, &entry)) {
            if(app->sniffer_ap_count < SNIFFER_MAX_APS) {
                entry.client_start = (uint16_t)app->sniffer_client_count;
                app->sniffer_aps[app->sniffer_ap_count++] = entry;
                app->sniffer_results_loading = false;
            }
            return;
        }

        if(simple_app_sniffer_is_mac(line) && app->sniffer_ap_count > 0) {
            if(app->sniffer_client_count < SNIFFER_MAX_CLIENTS) {
                SnifferClientEntry* client = &app->sniffer_clients[app->sniffer_client_count];
                if(!simple_app_sniffer_extract_mac_token(line, client->mac, sizeof(client->mac))) {
                    return;
                }
                app->sniffer_client_count++;
                SnifferApEntry* last_ap = &app->sniffer_aps[app->sniffer_ap_count - 1];
                if(last_ap->client_count < UINT16_MAX) {
                    last_ap->client_count++;
                }
                if(last_ap->expected_count < last_ap->client_count) {
                    last_ap->expected_count = last_ap->client_count;
                }
                app->sniffer_results_loading = false;
            }
            return;
        }
    }

    uint32_t value = 0;
    if(sscanf(line, "Sniffer packet count: %lu", &value) == 1 ||
       sscanf(line, "Sniffer packet count:%lu", &value) == 1) {
        app->sniffer_packet_count = value;
        app->sniffer_has_data = true;
        app->sniffer_running = true;
        app->sniffer_last_update_tick = furi_get_tick();
        return;
    }

    if(sscanf(line, "WiFi scan completed. Found %lu networks", &value) == 1 ||
       sscanf(line, "Processing %lu scan results", &value) == 1 ||
       sscanf(line, "Retrieved %lu network records", &value) == 1) {
        app->sniffer_networks = value;
        app->sniffer_running = true;
        return;
    }

    if(strstr(line, "Starting sniffer using existing scan results") != NULL) {
        strncpy(app->sniffer_mode, "NOSCAN", sizeof(app->sniffer_mode) - 1);
        app->sniffer_mode[sizeof(app->sniffer_mode) - 1] = '\0';
        app->sniffer_running = true;
        app->sniffer_scan_done = true;
        app->sniffer_last_update_tick = furi_get_tick();
        return;
    }

    const char* start_prefix = "Starting sniffer in ";
    const char* start_ptr = strstr(line, start_prefix);
    if(start_ptr) {
        start_ptr += strlen(start_prefix);
        char mode_buf[sizeof(app->sniffer_mode)] = {0};
        size_t idx = 0;
        while(start_ptr[idx] != '\0' && !isspace((unsigned char)start_ptr[idx]) &&
              idx < sizeof(mode_buf) - 1) {
            mode_buf[idx] = start_ptr[idx];
            idx++;
        }
        mode_buf[idx] = '\0';
        if(mode_buf[0] != '\0') {
            strncpy(app->sniffer_mode, mode_buf, sizeof(app->sniffer_mode) - 1);
            app->sniffer_mode[sizeof(app->sniffer_mode) - 1] = '\0';
        }
        app->sniffer_running = true;
        app->sniffer_scan_done = false;
        app->sniffer_packet_count = 0;
        app->sniffer_has_data = false;
        app->sniffer_last_update_tick = furi_get_tick();
        return;
    }

    if(strstr(line, "Sniffer started") != NULL ||
       strstr(line, "Starting background WiFi scan") != NULL) {
        app->sniffer_running = true;
        app->sniffer_last_update_tick = furi_get_tick();
        return;
    }

    if(strstr(line, "Sniffer: Now monitoring") != NULL) {
        app->sniffer_scan_done = true;
        app->sniffer_running = true;
        app->sniffer_last_update_tick = furi_get_tick();
        return;
    }

    if(strstr(line, "Sniffer: Scan complete") != NULL) {
        app->sniffer_scan_done = true;
        app->sniffer_running = true;
        app->sniffer_last_update_tick = furi_get_tick();
        return;
    }

    if(strstr(line, "Sniffer stopped") != NULL) {
        app->sniffer_running = false;
        app->sniffer_last_update_tick = furi_get_tick();
        return;
    }
}

static void simple_app_sniffer_feed(SimpleApp* app, char ch) {
    if(!app) return;
    if(!app->sniffer_view_active && !app->sniffer_results_active) {
        app->sniffer_line_length = 0;
        return;
    }
    if(ch == '\r') return;
    if(ch == '\n') {
        if(app->sniffer_line_length > 0) {
            app->sniffer_line_buffer[app->sniffer_line_length] = '\0';
            simple_app_process_sniffer_line(app, app->sniffer_line_buffer);
        }
        app->sniffer_line_length = 0;
        return;
    }
    if(app->sniffer_line_length + 1 >= sizeof(app->sniffer_line_buffer)) {
        app->sniffer_line_length = 0;
        return;
    }
    app->sniffer_line_buffer[app->sniffer_line_length++] = ch;
}

static int simple_app_find_probe_ssid(SimpleApp* app, const char* ssid) {
    if(!app || !ssid || !app->probe_ssids) return -1;
    for(size_t i = 0; i < app->probe_ssid_count; i++) {
        if(strncmp(app->probe_ssids[i].ssid, ssid, sizeof(app->probe_ssids[i].ssid)) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static void simple_app_process_probe_line(SimpleApp* app, const char* line) {
    if(!app || !line || !app->probe_results_active) return;
    if(!app->probe_ssids || !app->probe_clients) {
        if(!simple_app_alloc_probe_buffers(app)) {
            simple_app_show_status_message(app, "OOM: probe\nbuffers", 1500, true);
            return;
        }
    }

    if(strncmp(line, "Probe requests:", 15) == 0) {
        uint32_t total = 0;
        if(sscanf(line, "Probe requests: %lu", &total) == 1) {
            app->probe_total = total;
        }
        return;
    }

    if(line[0] == '>' || line[0] == '\0') {
        app->probe_results_loading = false;
        return;
    }

    const char* open = strchr(line, '(');
    const char* close = strchr(line, ')');
    if(!open || !close || close <= open) return;

    char ssid[SCAN_SSID_MAX_LEN];
    size_t ssid_len = (size_t)(open - line);
    if(ssid_len >= sizeof(ssid)) ssid_len = sizeof(ssid) - 1;
    memcpy(ssid, line, ssid_len);
    ssid[ssid_len] = '\0';
    simple_app_sniffer_trim(ssid);
    if(ssid[0] == '\0') {
        strncpy(ssid, "<hidden>", sizeof(ssid) - 1);
        ssid[sizeof(ssid) - 1] = '\0';
    }

    char mac[18];
    size_t mac_len = (size_t)(close - open - 1);
    if(mac_len >= sizeof(mac)) mac_len = sizeof(mac) - 1;
    memcpy(mac, open + 1, mac_len);
    mac[mac_len] = '\0';
    simple_app_sniffer_trim(mac);
    if(mac[0] == '\0') return;

    int existing = simple_app_find_probe_ssid(app, ssid);
    ProbeSsidEntry* entry = NULL;
    if(existing >= 0) {
        entry = &app->probe_ssids[existing];
    } else if(app->probe_ssid_count < PROBE_MAX_SSIDS) {
        entry = &app->probe_ssids[app->probe_ssid_count++];
        memset(entry, 0, sizeof(*entry));
        strncpy(entry->ssid, ssid, sizeof(entry->ssid) - 1);
        entry->ssid[sizeof(entry->ssid) - 1] = '\0';
        entry->client_start = (uint16_t)app->probe_client_count;
    }

    if(!entry) return;

    if(app->probe_client_count < PROBE_MAX_CLIENTS) {
        SnifferClientEntry* client = &app->probe_clients[app->probe_client_count++];
        strncpy(client->mac, mac, sizeof(client->mac) - 1);
        client->mac[sizeof(client->mac) - 1] = '\0';
        if(entry->client_count < UINT16_MAX) {
            entry->client_count++;
        }
        if(entry->expected_count < entry->client_count) {
            entry->expected_count = entry->client_count;
        }
    } else {
        if(entry->expected_count < UINT16_MAX) entry->expected_count++;
    }

    app->probe_results_loading = false;
}

static void simple_app_probe_feed(SimpleApp* app, char ch) {
    static char line_buf[96];
    static size_t line_len = 0;
    if(!app || !app->probe_results_active) {
        line_len = 0;
        return;
    }
    if(ch == '\r') return;
    if(ch == '\n') {
        line_buf[line_len] = '\0';
        simple_app_process_probe_line(app, line_buf);
        line_len = 0;
        return;
    }
    if(line_len + 1 >= sizeof(line_buf)) {
        line_len = 0;
        return;
    }
    line_buf[line_len++] = ch;
}

static void simple_app_reset_bt_scan_summary(SimpleApp* app) {
    if(!app) return;
    app->bt_scan_airtags = 0;
    app->bt_scan_smarttags = 0;
    app->bt_scan_total = 0;
    app->bt_scan_line_length = 0;
    app->bt_scan_summary_seen = false;
    app->airtag_scan_mode = false;
    app->bt_scan_full_console = false;
    app->bt_scan_last_update_tick = 0;
    app->bt_scan_running = false;
    app->bt_scan_has_data = false;
    app->bt_scan_show_list = false;
    app->bt_scan_preview_count = 0;
    app->bt_scan_list_offset = 0;
    if(app->bt_scan_preview) {
        memset(app->bt_scan_preview, 0, sizeof(BtScanPreview) * BT_SCAN_PREVIEW_MAX);
    }
    app->bt_locator_console_mode = false;
    app->bt_locator_mode = false;
    app->bt_locator_has_rssi = false;
    app->bt_locator_rssi = 0;
    app->bt_locator_start_tick = 0;
    app->bt_locator_last_console_tick = 0;
    app->bt_locator_mac[0] = '\0';
    if(app->bt_locator_preserve_mac && app->bt_locator_saved_mac[0] != '\0') {
        strncpy(app->bt_locator_mac, app->bt_locator_saved_mac, sizeof(app->bt_locator_mac) - 1);
        app->bt_locator_mac[sizeof(app->bt_locator_mac) - 1] = '\0';
    } else {
        app->bt_locator_saved_mac[0] = '\0';
        app->bt_locator_preserve_mac = false;
    }
    app->bt_locator_list_loading = false;
    app->bt_locator_list_ready = false;
    app->bt_locator_count = 0;
    app->bt_locator_index = 0;
    app->bt_locator_offset = 0;
    app->bt_locator_selected = -1;
    app->bt_locator_scan_len = 0;
    if(app->bt_locator_scan_line) {
        memset(app->bt_locator_scan_line, 0, BT_LOCATOR_SCAN_LINE_LEN);
    }
    if(app->bt_locator_devices) {
        memset(app->bt_locator_devices, 0, sizeof(BtLocatorDevice) * BT_LOCATOR_MAX_DEVICES);
    }
}

static void simple_app_process_bt_scan_line(SimpleApp* app, const char* line) {
    if(!app || !line || !app->bt_scan_view_active) return;

    int airtags = 0;
    int smarttags = 0;
    int total = 0;
    if(app->bt_locator_mode) {
        char mac[18] = {0};
        int rssi = 0;
        if(sscanf(line, " %17s RSSI: %d", mac, &rssi) == 2) {
            app->bt_locator_rssi = rssi;
            app->bt_locator_has_rssi = true;
            app->bt_scan_last_update_tick = furi_get_tick();
        } else if(sscanf(line, " %17s RSSI: N/A", mac) == 1) {
            app->bt_locator_has_rssi = false;
            app->bt_scan_last_update_tick = furi_get_tick();
        }
        return;
    }

    if(app->airtag_scan_mode) {
        if(sscanf(line, " %d , %d", &airtags, &smarttags) == 2 ||
           sscanf(line, " %d,%d", &airtags, &smarttags) == 2) {
            app->bt_scan_airtags = airtags;
            app->bt_scan_smarttags = smarttags;
            app->bt_scan_total = airtags + smarttags;
            app->bt_scan_last_update_tick = furi_get_tick();
            app->bt_scan_has_data = true;
            app->bt_scan_running = true;
        }
        return;
    }

    if(sscanf(
           line,
           "Summary: %d AirTags, %d SmartTags, %d total devices",
           &airtags,
           &smarttags,
           &total) == 3) {
        app->bt_scan_airtags = airtags;
        app->bt_scan_smarttags = smarttags;
        app->bt_scan_total = total;
        app->bt_scan_summary_seen = true;
        app->bt_scan_last_update_tick = furi_get_tick();
        app->bt_scan_has_data = true;
        app->bt_scan_running = false;
        return;
    }

    if(sscanf(line, "Found %d devices", &total) == 1) {
        app->bt_scan_total = total;
        app->bt_scan_last_update_tick = furi_get_tick();
        app->bt_scan_has_data = true;
        app->bt_scan_running = true;
    }

    // Parse individual device lines for preview: "<num>. <MAC>  RSSI: <rssi> dBm [Name: ...]"
    int idx = 0;
    char mac[18] = {0};
    int rssi = 0;
    if(sscanf(line, " %d . %17s RSSI: %d", &idx, mac, &rssi) == 3 ||
       sscanf(line, " %d. %17s RSSI: %d", &idx, mac, &rssi) == 3) {
        const char* name_ptr = strstr(line, "Name:");
        char name_buf[32] = {0};
        if(name_ptr) {
            name_ptr += strlen("Name:");
            while(*name_ptr == ' ')
                name_ptr++;
            strncpy(name_buf, name_ptr, sizeof(name_buf) - 1);
            name_buf[sizeof(name_buf) - 1] = '\0';
        }
        simple_app_bt_scan_preview_insert(app, mac, rssi, name_buf);
    }

    if(app->bt_locator_mode) {
        const char* name_ptr = strstr(line, "Name:");
        if(name_ptr) {
            name_ptr += strlen("Name:");
            while(*name_ptr == ' ')
                name_ptr++;
            char name_buf[32] = {0};
            strncpy(name_buf, name_ptr, sizeof(name_buf) - 1);
            name_buf[sizeof(name_buf) - 1] = '\0';
            size_t nb_len = strlen(name_buf);
            while(nb_len > 0 && isspace((unsigned char)name_buf[nb_len - 1])) {
                name_buf[nb_len - 1] = '\0';
                nb_len--;
            }
            if(name_buf[0] != '\0') {
                strncpy(
                    app->bt_locator_current_name,
                    name_buf,
                    sizeof(app->bt_locator_current_name) - 1);
                app->bt_locator_current_name[sizeof(app->bt_locator_current_name) - 1] = '\0';
            }
        }
    }
}

static void simple_app_bt_scan_feed(SimpleApp* app, char ch) {
    if(!app || !app->bt_scan_line_buffer) return;
    if(!app->bt_scan_view_active) {
        app->bt_scan_line_length = 0;
        return;
    }
    if(app->bt_locator_mode) {
        app->bt_locator_last_console_tick = furi_get_tick();
    }
    if(ch == '\r') return;
    if(ch == '\n') {
        if(app->bt_scan_line_length > 0) {
            app->bt_scan_line_buffer[app->bt_scan_line_length] = '\0';
            simple_app_process_bt_scan_line(app, app->bt_scan_line_buffer);
        }
        app->bt_scan_line_length = 0;
        return;
    }
    if(app->bt_scan_line_length + 1 >= BT_SCAN_LINE_BUFFER_LEN) {
        app->bt_scan_line_length = 0;
        return;
    }
    app->bt_scan_line_buffer[app->bt_scan_line_length++] = ch;
}

static void simple_app_reset_deauth_guard(SimpleApp* app) {
    if(!app) return;
    bool was_blinking = app->deauth_guard_blink_until > 0;
    app->deauth_guard_view_active = false;
    app->deauth_guard_full_console = false;
    app->deauth_guard_has_detection = false;
    app->deauth_guard_has_rssi = false;
    app->deauth_guard_last_channel = -1;
    app->deauth_guard_last_rssi = 0;
    app->deauth_guard_started_tick = 0;
    app->deauth_guard_last_detection_tick = 0;
    app->deauth_guard_monitoring = false;
    app->deauth_guard_blink_until = 0;
    app->deauth_guard_last_blink_tick = 0;
    app->deauth_guard_blink_state = false;
    app->deauth_guard_blink_count = 0;
    if(app->deauth_guard_vibro_on) {
        furi_hal_vibro_on(false);
    }
    app->deauth_guard_vibro_on = false;
    app->deauth_guard_detection_count = 0;
    app->deauth_guard_line_length = 0;
    memset(app->deauth_guard_last_ssid, 0, sizeof(app->deauth_guard_last_ssid));
    memset(app->deauth_guard_line_buffer, 0, sizeof(app->deauth_guard_line_buffer));
    if(was_blinking) {
        simple_app_apply_backlight(app);
    }
}

static void simple_app_start_deauth_guard(SimpleApp* app) {
    if(!app) return;
    simple_app_reset_deauth_guard(app);
    app->deauth_guard_view_active = true;
    app->deauth_guard_started_tick = furi_get_tick();
    app->deauth_guard_last_detection_tick = app->deauth_guard_started_tick;
    app->serial_targets_hint = false;
}

static void simple_app_process_deauth_guard_line(SimpleApp* app, const char* line) {
    if(!app || !line || !app->deauth_guard_view_active) return;
    // Ignore startup and prompt noise
    if(strncmp(line, "[MEM]", 5) == 0) return;
    if(strncmp(line, "Deauth Detector started", 24) == 0) return;
    if(strncmp(line, "Output: [DEAUTH]", 16) == 0) return;
    if(strncmp(line, "Use 'stop' to stop.", 19) == 0) return;
    if(strncmp(line, "->", 2) == 0) return;

    if(strstr(line, "[DEAUTH]") == NULL) return;
    if(strstr(line, "RSSI:") == NULL || strstr(line, "CH:") == NULL) return;

    int channel = -1;
    int rssi = 0;
    bool has_rssi = false;

    const char* ch_ptr = strstr(line, "CH:");
    if(ch_ptr) {
        channel = atoi(ch_ptr + 3);
    }

    const char* rssi_ptr = strstr(line, "RSSI:");
    if(rssi_ptr) {
        rssi = atoi(rssi_ptr + 5);
        has_rssi = true;
    }

    char ssid[SCAN_SSID_MAX_LEN];
    ssid[0] = '\0';
    const char* ap_ptr = strstr(line, "AP:");
    if(ap_ptr) {
        ap_ptr += 3;
        while(*ap_ptr == ' ' || *ap_ptr == '|') {
            ap_ptr++;
        }
        const char* end = ap_ptr;
        while(*end && *end != '(' && *end != '|' && *end != '\r' && *end != '\n') {
            end++;
        }
        size_t len = (size_t)(end - ap_ptr);
        while(len > 0 && isspace((unsigned char)ap_ptr[len - 1])) {
            len--;
        }
        if(len >= sizeof(ssid)) {
            len = sizeof(ssid) - 1;
        }
        memcpy(ssid, ap_ptr, len);
        ssid[len] = '\0';
    }
    if(ssid[0] == '\0') {
        snprintf(ssid, sizeof(ssid), "Unknown");
    }

    strncpy(app->deauth_guard_last_ssid, ssid, sizeof(app->deauth_guard_last_ssid) - 1);
    app->deauth_guard_last_ssid[sizeof(app->deauth_guard_last_ssid) - 1] = '\0';
    app->deauth_guard_last_channel = channel;
    app->deauth_guard_has_rssi = has_rssi;
    app->deauth_guard_last_rssi = rssi;
    app->deauth_guard_has_detection = true;
    app->deauth_guard_detection_count++;

    uint32_t now = furi_get_tick();
    app->deauth_guard_last_detection_tick = now;
    app->deauth_guard_blink_count = 0;
    app->deauth_guard_blink_until =
        now + furi_ms_to_ticks(DEAUTH_GUARD_BLINK_MS * DEAUTH_GUARD_BLINK_TOGGLES);
    app->deauth_guard_last_blink_tick = now;
    app->deauth_guard_blink_state = true;

    if(!app->deauth_guard_vibro_on) {
        app->deauth_guard_vibro_on = true;
        app->deauth_guard_vibro_until = now + furi_ms_to_ticks(DEAUTH_GUARD_VIBRO_MS);
        furi_hal_vibro_on(true);
    } else {
        app->deauth_guard_vibro_until = now + furi_ms_to_ticks(DEAUTH_GUARD_VIBRO_MS);
    }

    if(app->viewport) {
        view_port_update(app->viewport);
    }
}

static void simple_app_deauth_guard_feed(SimpleApp* app, char ch) {
    if(!app) return;
    if(!app->deauth_guard_view_active) {
        app->deauth_guard_line_length = 0;
        return;
    }
    if(ch == '\r') return;
    if(ch == '\n') {
        if(app->deauth_guard_line_length > 0) {
            app->deauth_guard_line_buffer[app->deauth_guard_line_length] = '\0';
            simple_app_process_deauth_guard_line(app, app->deauth_guard_line_buffer);
        }
        app->deauth_guard_line_length = 0;
        return;
    }
    if(app->deauth_guard_line_length + 1 >= sizeof(app->deauth_guard_line_buffer)) {
        app->deauth_guard_line_length = 0;
        return;
    }
    app->deauth_guard_line_buffer[app->deauth_guard_line_length++] = ch;
}

static void simple_app_update_deauth_guard(SimpleApp* app) {
    if(!app) return;
    if(!app->deauth_guard_view_active) {
        if(app->deauth_guard_blink_until > 0) {
            app->deauth_guard_blink_until = 0;
            app->deauth_guard_blink_state = false;
            simple_app_apply_backlight(app);
        }
        return;
    }

    if(!app->backlight_enabled || app->backlight_notification_enforced) {
        // Still allow LED/vibro even if backlight is enforced off/on.
        app->deauth_guard_blink_state = true;
    }

    uint32_t now = furi_get_tick();
    if(app->deauth_guard_blink_until == 0) {
        // Track idle monitoring state once per loop.
        if(!app->deauth_guard_has_detection) {
            uint32_t since_start = now - app->deauth_guard_started_tick;
            app->deauth_guard_monitoring =
                (since_start >= furi_ms_to_ticks(DEAUTH_GUARD_IDLE_MONITOR_MS));
        } else {
            app->deauth_guard_monitoring = false;
        }
        return;
    }
    if(now >= app->deauth_guard_blink_until) {
        app->deauth_guard_blink_until = 0;
        app->deauth_guard_blink_state = false;
        simple_app_apply_backlight(app);
        if(app->deauth_guard_vibro_on) {
            furi_hal_vibro_on(false);
            app->deauth_guard_vibro_on = false;
            app->deauth_guard_vibro_until = 0;
        }
        return;
    }

    uint32_t interval = furi_ms_to_ticks(DEAUTH_GUARD_BLINK_MS);
    if(interval == 0) interval = 1;
    if(now - app->deauth_guard_last_blink_tick >= interval) {
        app->deauth_guard_last_blink_tick = now;
        app->deauth_guard_blink_state = !app->deauth_guard_blink_state;
        if(app->deauth_guard_blink_count < DEAUTH_GUARD_BLINK_TOGGLES) {
            app->deauth_guard_blink_count++;
        }
        uint8_t level = app->deauth_guard_blink_state ? BACKLIGHT_ON_LEVEL : BACKLIGHT_OFF_LEVEL;
        furi_hal_light_set(LightBacklight, level);
    }

    if(app->deauth_guard_vibro_on && now >= app->deauth_guard_vibro_until) {
        furi_hal_vibro_on(false);
        app->deauth_guard_vibro_on = false;
        app->deauth_guard_vibro_until = 0;
    }

    // Update idle monitoring flag after blink handling to avoid extra reads.
    if(!app->deauth_guard_has_detection) {
        uint32_t since_start = now - app->deauth_guard_started_tick;
        app->deauth_guard_monitoring =
            (since_start >= furi_ms_to_ticks(DEAUTH_GUARD_IDLE_MONITOR_MS));
    } else {
        app->deauth_guard_monitoring = false;
    }
}

static void simple_app_bt_locator_reset_list(SimpleApp* app) {
    if(!app) return;
    app->bt_locator_list_loading = false;
    app->bt_locator_list_ready = false;
    app->bt_locator_console_mode = false;
    app->bt_locator_count = 0;
    app->bt_locator_index = 0;
    app->bt_locator_offset = 0;
    app->bt_locator_selected = -1;
    app->bt_locator_current_name[0] = '\0';
    if(app->bt_locator_scroll_text) {
        app->bt_locator_scroll_text[0] = '\0';
    }
    app->bt_locator_scroll_offset = 0;
    app->bt_locator_scroll_dir = 1;
    app->bt_locator_scroll_hold = RESULT_SCROLL_EDGE_PAUSE_STEPS;
    app->bt_locator_scroll_last_tick = furi_get_tick();
    app->bt_locator_start_tick = 0;
    app->bt_locator_last_console_tick = 0;
    app->bt_locator_scan_len = 0;
    if(app->bt_locator_scan_line) {
        memset(app->bt_locator_scan_line, 0, BT_LOCATOR_SCAN_LINE_LEN);
    }
    if(app->bt_locator_devices) {
        memset(app->bt_locator_devices, 0, sizeof(BtLocatorDevice) * BT_LOCATOR_MAX_DEVICES);
    }
    app->bt_locator_saved_mac[0] = '\0';
    app->bt_locator_preserve_mac = false;
}

static void
    simple_app_bt_locator_add(SimpleApp* app, const char* mac, int rssi, const char* name) {
    if(!app || !mac || mac[0] == '\0' || !app->bt_locator_devices) return;
    // Update if exists
    for(size_t i = 0; i < app->bt_locator_count; i++) {
        if(strncmp(app->bt_locator_devices[i].mac, mac, sizeof(app->bt_locator_devices[i].mac)) ==
           0) {
            app->bt_locator_devices[i].rssi = rssi;
            app->bt_locator_devices[i].has_rssi = true;
            if(name && name[0] != '\0') {
                strncpy(
                    app->bt_locator_devices[i].name,
                    name,
                    sizeof(app->bt_locator_devices[i].name) - 1);
                app->bt_locator_devices[i].name[sizeof(app->bt_locator_devices[i].name) - 1] =
                    '\0';
            }
            return;
        }
    }
    if(app->bt_locator_count >= BT_LOCATOR_MAX_DEVICES) return;
    strncpy(
        app->bt_locator_devices[app->bt_locator_count].mac,
        mac,
        sizeof(app->bt_locator_devices[app->bt_locator_count].mac));
    app->bt_locator_devices[app->bt_locator_count]
        .mac[sizeof(app->bt_locator_devices[app->bt_locator_count].mac) - 1] = '\0';
    app->bt_locator_devices[app->bt_locator_count].rssi = rssi;
    app->bt_locator_devices[app->bt_locator_count].has_rssi = true;
    if(name && name[0] != '\0') {
        strncpy(
            app->bt_locator_devices[app->bt_locator_count].name,
            name,
            sizeof(app->bt_locator_devices[app->bt_locator_count].name) - 1);
        app->bt_locator_devices[app->bt_locator_count]
            .name[sizeof(app->bt_locator_devices[app->bt_locator_count].name) - 1] = '\0';
    }
    app->bt_locator_count++;

    // Keep list sorted by RSSI desc (strongest first)
    for(size_t pos = app->bt_locator_count; pos > 1; pos--) {
        size_t i = pos - 1;
        size_t prev = i - 1;
        bool swap = false;
        if(app->bt_locator_devices[i].has_rssi && app->bt_locator_devices[prev].has_rssi) {
            swap = app->bt_locator_devices[i].rssi > app->bt_locator_devices[prev].rssi;
        } else if(app->bt_locator_devices[i].has_rssi && !app->bt_locator_devices[prev].has_rssi) {
            swap = true;
        }
        if(swap) {
            struct {
                char mac[18];
                int rssi;
                bool has_rssi;
                char name[BT_LOCATOR_NAME_MAX_LEN];
            } tmp = {0};
            memcpy(tmp.mac, app->bt_locator_devices[i].mac, sizeof(tmp.mac));
            tmp.rssi = app->bt_locator_devices[i].rssi;
            tmp.has_rssi = app->bt_locator_devices[i].has_rssi;
            memcpy(tmp.name, app->bt_locator_devices[i].name, sizeof(tmp.name));

            memcpy(
                app->bt_locator_devices[i].mac,
                app->bt_locator_devices[prev].mac,
                sizeof(tmp.mac));
            app->bt_locator_devices[i].rssi = app->bt_locator_devices[prev].rssi;
            app->bt_locator_devices[i].has_rssi = app->bt_locator_devices[prev].has_rssi;
            memcpy(
                app->bt_locator_devices[i].name,
                app->bt_locator_devices[prev].name,
                sizeof(tmp.name));

            memcpy(app->bt_locator_devices[prev].mac, tmp.mac, sizeof(tmp.mac));
            app->bt_locator_devices[prev].rssi = tmp.rssi;
            app->bt_locator_devices[prev].has_rssi = tmp.has_rssi;
            memcpy(app->bt_locator_devices[prev].name, tmp.name, sizeof(tmp.name));
        } else {
            break;
        }
    }
    // Restore selection mac if it matches newly inserted entry
    if(app->bt_locator_preserve_mac && app->bt_locator_mac[0] == '\0' &&
       app->bt_locator_saved_mac[0] != '\0') {
        strncpy(app->bt_locator_mac, app->bt_locator_saved_mac, sizeof(app->bt_locator_mac) - 1);
        app->bt_locator_mac[sizeof(app->bt_locator_mac) - 1] = '\0';
    }
}

static void simple_app_bt_locator_finish(SimpleApp* app) {
    if(!app) return;
    app->bt_locator_list_loading = false;
    app->bt_locator_list_ready = true;
    app->bt_locator_console_mode = true;
    if(app->bt_locator_count == 0) {
        app->bt_locator_index = 0;
        app->bt_locator_offset = 0;
    } else if(app->bt_locator_index >= app->bt_locator_count) {
        app->bt_locator_index = app->bt_locator_count - 1;
    }
    simple_app_bt_locator_ensure_visible(app);
    if(app->viewport && app->screen == ScreenBtLocatorList) {
        view_port_update(app->viewport);
    }
}

static void simple_app_bt_locator_reset_scroll(SimpleApp* app) {
    if(!app || !app->bt_locator_scroll_text) return;
    app->bt_locator_scroll_text[0] = '\0';
    app->bt_locator_scroll_offset = 0;
    app->bt_locator_scroll_dir = 1;
    app->bt_locator_scroll_hold = RESULT_SCROLL_EDGE_PAUSE_STEPS;
    app->bt_locator_scroll_last_tick = furi_get_tick();
}

static void simple_app_bt_locator_set_scroll_text(SimpleApp* app, const char* text) {
    if(!app || !app->bt_locator_scroll_text || !text) return;
    strncpy(app->bt_locator_scroll_text, text, BT_LOCATOR_SCROLL_TEXT_LEN - 1);
    app->bt_locator_scroll_text[BT_LOCATOR_SCROLL_TEXT_LEN - 1] = '\0';
    app->bt_locator_scroll_offset = 0;
    app->bt_locator_scroll_dir = 1;
    app->bt_locator_scroll_hold = RESULT_SCROLL_EDGE_PAUSE_STEPS;
    app->bt_locator_scroll_last_tick = furi_get_tick();
}

static void simple_app_bt_locator_tick_scroll(SimpleApp* app, size_t char_limit) {
    if(!app || !app->bt_locator_scroll_text) return;
    size_t len = strlen(app->bt_locator_scroll_text);
    if(len <= char_limit || char_limit == 0) {
        app->bt_locator_scroll_offset = 0;
        return;
    }

    uint32_t interval_ticks = furi_ms_to_ticks(RESULT_SCROLL_INTERVAL_MS);
    if(interval_ticks == 0) interval_ticks = 1;
    uint32_t now = furi_get_tick();
    if((now - app->bt_locator_scroll_last_tick) < interval_ticks) {
        return;
    }
    app->bt_locator_scroll_last_tick = now;

    if(app->bt_locator_scroll_hold > 0) {
        app->bt_locator_scroll_hold--;
        return;
    }

    size_t max_offset = len - char_limit;
    bool changed = false;
    if(app->bt_locator_scroll_dir > 0) {
        if(app->bt_locator_scroll_offset < max_offset) {
            app->bt_locator_scroll_offset++;
            changed = true;
            if(app->bt_locator_scroll_offset >= max_offset) {
                app->bt_locator_scroll_dir = -1;
                app->bt_locator_scroll_hold = RESULT_SCROLL_EDGE_PAUSE_STEPS;
            }
        } else {
            app->bt_locator_scroll_dir = -1;
            app->bt_locator_scroll_hold = RESULT_SCROLL_EDGE_PAUSE_STEPS;
        }
    } else {
        if(app->bt_locator_scroll_offset > 0) {
            app->bt_locator_scroll_offset--;
            changed = true;
            if(app->bt_locator_scroll_offset == 0) {
                app->bt_locator_scroll_dir = 1;
                app->bt_locator_scroll_hold = RESULT_SCROLL_EDGE_PAUSE_STEPS;
            }
        } else {
            app->bt_locator_scroll_dir = 1;
            app->bt_locator_scroll_hold = RESULT_SCROLL_EDGE_PAUSE_STEPS;
        }
    }

    if(changed && app->viewport && app->screen == ScreenBtLocatorList) {
        view_port_update(app->viewport);
    }
}

static void simple_app_bt_locator_feed(SimpleApp* app, char ch) {
    if(!app || !app->bt_locator_list_loading || !app->bt_locator_scan_line) return;
    if(ch == '\r') return;
    if(ch == '\n') {
        if(app->bt_locator_scan_len > 0) {
            app->bt_locator_scan_line[app->bt_locator_scan_len] = '\0';
            const char* line = app->bt_locator_scan_line;
            if(strstr(line, "Summary:") != NULL) {
                simple_app_bt_locator_finish(app);
            } else {
                int idx = 0;
                char mac[18] = {0};
                int rssi = 0;
                if(sscanf(line, " %d . %17s  RSSI: %d", &idx, mac, &rssi) == 3 ||
                   sscanf(line, " %d. %17s  RSSI: %d", &idx, mac, &rssi) == 3) {
                    const char* name_ptr = strstr(line, "Name:");
                    char name_buf[32] = {0};
                    if(name_ptr) {
                        name_ptr += strlen("Name:");
                        while(*name_ptr == ' ')
                            name_ptr++;
                        strncpy(name_buf, name_ptr, sizeof(name_buf) - 1);
                        // trim trailing spaces
                        size_t nb_len = strlen(name_buf);
                        while(nb_len > 0 && isspace((unsigned char)name_buf[nb_len - 1])) {
                            name_buf[nb_len - 1] = '\0';
                            nb_len--;
                        }
                    }
                    simple_app_bt_locator_add(app, mac, rssi, name_buf[0] ? name_buf : NULL);
                    if(app->viewport && app->screen == ScreenBtLocatorList) {
                        view_port_update(app->viewport);
                    }
                }
            }
        }
        app->bt_locator_scan_len = 0;
        return;
    }
    if(app->bt_locator_scan_len + 1 >= BT_LOCATOR_SCAN_LINE_LEN) {
        app->bt_locator_scan_len = 0;
        return;
    }
    app->bt_locator_scan_line[app->bt_locator_scan_len++] = ch;
}

static void simple_app_bt_locator_begin_scan(SimpleApp* app) {
    if(!app) return;
    if(!simple_app_alloc_bt_buffers(app)) {
        simple_app_show_status_message(app, "OOM: BT buffers", 1500, true);
        return;
    }
    simple_app_reset_bt_scan_summary(app);
    simple_app_bt_locator_reset_list(app);
    app->bt_locator_current_name[0] = '\0';
    app->bt_locator_console_mode = true;
    app->bt_locator_list_loading = true;
    app->bt_locator_list_ready = false;
    app->bt_locator_mode = false;
    simple_app_bt_locator_reset_scroll(app);
    app->screen = ScreenSerial;
    if(app->viewport) {
        view_port_update(app->viewport);
    }
    simple_app_send_command(app, "scan_bt", false);
    // send_command resets scan state; reassert locator flags so UI stays in "scanning" console mode
    app->bt_locator_console_mode = true;
    app->bt_locator_list_loading = true;
    app->bt_locator_list_ready = false;
    app->bt_locator_mode = false;
}

static void simple_app_update_otg_label(SimpleApp* app) {
    if(!app) return;
    snprintf(
        menu_label_otg_power,
        sizeof(menu_label_otg_power),
        "5V Power: %s",
        app->otg_power_enabled ? "On" : "Off");
}

static void simple_app_apply_otg_power(SimpleApp* app) {
    if(!app) return;
    bool currently_enabled = furi_hal_power_is_otg_enabled();
    if(app->otg_power_enabled && !currently_enabled) {
        furi_hal_power_enable_otg();
    } else if(!app->otg_power_enabled && currently_enabled) {
        furi_hal_power_disable_otg();
    }
    simple_app_update_otg_label(app);
}

static void simple_app_toggle_otg_power(SimpleApp* app) {
    if(!app) return;
    app->otg_power_enabled = !app->otg_power_enabled;
    simple_app_apply_otg_power(app);
    simple_app_mark_config_dirty(app);
}

static void simple_app_send_download(SimpleApp* app) {
    if(!app) return;
    simple_app_send_command_quiet(app, "download");
    simple_app_show_status_message(app, "Download sent", 2000, true);
}

static bool simple_app_usb_profile_ok(void) {
    const FuriHalUsbInterface* current = furi_hal_usb_get_config();
    if(!current) return true;
    return current == &usb_cdc_single;
}

static bool simple_app_sd_ok(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return true;
    bool ok = (storage_sd_status(storage) == FSE_OK);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

static void simple_app_show_sd_busy(void) {
    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
    if(dialogs) {
        DialogMessage* msg = dialog_message_alloc();
        if(msg) {
            dialog_message_set_header(msg, "SD busy", 64, 8, AlignCenter, AlignTop);
            dialog_message_set_text(
                msg,
                "Close qFlipper/FileMgr\nor disable USB storage,\nthen relaunch.",
                64,
                32,
                AlignCenter,
                AlignCenter);
            dialog_message_show(dialogs, msg);
            dialog_message_free(msg);
        }
        furi_record_close(RECORD_DIALOGS);
    }
}

static void simple_app_ensure_flipper_sd_dirs(Storage* storage) {
    if(!storage) return;
    storage_simply_mkdir(storage, EXT_PATH("apps_assets"));
    storage_simply_mkdir(storage, FLIPPER_SD_BASE_PATH);
}

static bool simple_app_append_log_line(const char* path, const char* line) {
    if(!path || !line) return false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;
    simple_app_ensure_flipper_sd_dirs(storage);

    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(file, path, FSAM_WRITE, FSOM_OPEN_APPEND) ||
       storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        size_t len = strlen(line);
        if(len > 0) {
            ok = (storage_file_write(file, line, len) == len);
        }
        if(ok && line[len - 1] != '\n') {
            const char nl = '\n';
            ok = (storage_file_write(file, &nl, 1) == 1);
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

static void simple_app_debug_mark(SimpleApp* app, const char* tag) {
    if(!app || !app->debug_mark_enabled) return;
    if(!tag || tag[0] == '\0') return;
    if(!simple_app_sd_ok()) return;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return;
    simple_app_ensure_flipper_sd_dirs(storage);

    File* file = storage_file_alloc(storage);
    if(file) {
        char line[96];
        unsigned long tick = (unsigned long)furi_get_tick();
        int len = snprintf(line, sizeof(line), "%lu %s\n", tick, tag);
        if(len < 0) len = 0;
        if((size_t)len > sizeof(line)) len = (int)sizeof(line);
        if(storage_file_open(file, FLIPPER_SD_DEBUG_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            if(len > 0) {
                storage_file_write(file, line, (size_t)len);
            }
            storage_file_close(file);
        }
        storage_file_free(file);
    }
    furi_record_close(RECORD_STORAGE);
}

static void simple_app_log_cli_command(SimpleApp* app, const char* source, const char* command) {
    if(!app || !app->debug_mark_enabled || !command || command[0] == '\0') return;
    if(!simple_app_sd_ok()) return;

    if(!app->debug_log_initialized) {
        simple_app_reset_debug_log(app);
        app->debug_log_initialized = true;
    }

    char cmd[96];
    size_t out = 0;
    for(size_t i = 0; command[i] != '\0' && out < sizeof(cmd) - 1; i++) {
        char c = command[i];
        if(c == '\n' || c == '\r') {
            c = ';';
        } else if(c == '\t') {
            c = ' ';
        }
        cmd[out++] = c;
    }
    cmd[out] = '\0';
    if(cmd[0] == '\0') return;

    size_t free_kb = (size_t)(memmgr_get_free_heap() / 1024);
    size_t min_kb = (size_t)(memmgr_get_minimum_free_heap() / 1024);
    size_t start_kb = (size_t)(app->heap_start_free / 1024);
    char line[160];
    unsigned long tick = (unsigned long)furi_get_tick();
    snprintf(
        line,
        sizeof(line),
        "%lu %s %s R:%luK M:%luK S:%luK",
        tick,
        source ? source : "SRC",
        cmd,
        (unsigned long)free_kb,
        (unsigned long)min_kb,
        (unsigned long)start_kb);
    simple_app_append_log_line(FLIPPER_SD_DEBUG_LOG_PATH, line);
    simple_app_debug_mark(app, line);
}

static void simple_app_reset_debug_log(SimpleApp* app) {
    if(!app || !app->debug_mark_enabled) return;
    if(!simple_app_sd_ok()) return;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return;
    simple_app_ensure_flipper_sd_dirs(storage);

    File* file = storage_file_alloc(storage);
    if(file) {
        if(storage_file_open(file, FLIPPER_SD_DEBUG_LOG_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            storage_file_close(file);
        }
        if(storage_file_open(file, FLIPPER_SD_DEBUG_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            storage_file_close(file);
        }
        storage_file_free(file);
    }
    furi_record_close(RECORD_STORAGE);

    {
        char header[128];
        size_t free_kb = (size_t)(memmgr_get_free_heap() / 1024);
        size_t min_kb = (size_t)(memmgr_get_minimum_free_heap() / 1024);
        size_t start_kb = (size_t)(app->heap_start_free / 1024);
        unsigned long tick = (unsigned long)furi_get_tick();
        snprintf(
            header,
            sizeof(header),
            "%lu START R:%luK M:%luK S:%luK",
            tick,
            (unsigned long)free_kb,
            (unsigned long)min_kb,
            (unsigned long)start_kb);
        simple_app_append_log_line(FLIPPER_SD_DEBUG_LOG_PATH, header);
        simple_app_debug_mark(app, header);
    }

    {
        char allocs[160];
        snprintf(
            allocs,
            sizeof(allocs),
            "ALLOC app=%lu rx=%u vp=1 gui=1 notif=1 mutex=1",
            (unsigned long)sizeof(SimpleApp),
            (unsigned)UART_STREAM_SIZE);
        simple_app_append_log_line(FLIPPER_SD_DEBUG_LOG_PATH, allocs);
        simple_app_debug_mark(app, allocs);
    }
}

static bool simple_app_scan_alloc_buffers(SimpleApp* app, size_t capacity) {
    if(!app) return false;
    if(capacity > SCAN_RESULTS_MAX_CAPACITY) {
        capacity = SCAN_RESULTS_MAX_CAPACITY;
    }
    if(capacity == 0) {
        simple_app_scan_free_buffers(app);
        return true;
    }
    if(app->scan_results && app->scan_selected_numbers && app->visible_result_indices &&
       app->scan_results_capacity == capacity) {
        return true;
    }

    simple_app_scan_free_buffers(app);

    app->scan_results = malloc(sizeof(ScanResult) * capacity);
    app->scan_selected_numbers = malloc(sizeof(uint16_t) * capacity);
    app->visible_result_indices = malloc(sizeof(uint16_t) * capacity);

    if(!app->scan_results || !app->scan_selected_numbers || !app->visible_result_indices) {
        simple_app_scan_free_buffers(app);
        return false;
    }

    memset(app->scan_results, 0, sizeof(ScanResult) * capacity);
    memset(app->scan_selected_numbers, 0, sizeof(uint16_t) * capacity);
    memset(app->visible_result_indices, 0, sizeof(uint16_t) * capacity);
    app->scan_results_capacity = capacity;
    return true;
}

static void simple_app_scan_free_buffers(SimpleApp* app) {
    if(!app) return;
    if(app->scan_results) {
        free(app->scan_results);
        app->scan_results = NULL;
    }
    if(app->scan_selected_numbers) {
        free(app->scan_selected_numbers);
        app->scan_selected_numbers = NULL;
    }
    if(app->visible_result_indices) {
        free(app->visible_result_indices);
        app->visible_result_indices = NULL;
    }
    app->scan_results_capacity = 0;
}

static bool simple_app_alloc_package_monitor_buffers(SimpleApp* app) {
    if(!app) return false;
    if(app->package_monitor_history) return true;
    app->package_monitor_history = calloc(PACKAGE_MONITOR_MAX_HISTORY, sizeof(uint16_t));
    return app->package_monitor_history != NULL;
}

static void simple_app_free_package_monitor_buffers(SimpleApp* app) {
    if(!app) return;
    if(app->package_monitor_history) {
        free(app->package_monitor_history);
        app->package_monitor_history = NULL;
    }
}

static bool simple_app_alloc_channel_view_buffers(SimpleApp* app) {
    if(!app) return false;
    if(app->channel_view_counts_24 && app->channel_view_counts_5 &&
       app->channel_view_working_counts_24 && app->channel_view_working_counts_5) {
        return true;
    }

    app->channel_view_counts_24 = calloc(CHANNEL_VIEW_24GHZ_CHANNEL_COUNT, sizeof(uint16_t));
    app->channel_view_counts_5 = calloc(CHANNEL_VIEW_5GHZ_CHANNEL_COUNT, sizeof(uint16_t));
    app->channel_view_working_counts_24 =
        calloc(CHANNEL_VIEW_24GHZ_CHANNEL_COUNT, sizeof(uint16_t));
    app->channel_view_working_counts_5 = calloc(CHANNEL_VIEW_5GHZ_CHANNEL_COUNT, sizeof(uint16_t));

    if(!app->channel_view_counts_24 || !app->channel_view_counts_5 ||
       !app->channel_view_working_counts_24 || !app->channel_view_working_counts_5) {
        if(app->channel_view_counts_24) {
            free(app->channel_view_counts_24);
            app->channel_view_counts_24 = NULL;
        }
        if(app->channel_view_counts_5) {
            free(app->channel_view_counts_5);
            app->channel_view_counts_5 = NULL;
        }
        if(app->channel_view_working_counts_24) {
            free(app->channel_view_working_counts_24);
            app->channel_view_working_counts_24 = NULL;
        }
        if(app->channel_view_working_counts_5) {
            free(app->channel_view_working_counts_5);
            app->channel_view_working_counts_5 = NULL;
        }
        return false;
    }
    return true;
}

static void simple_app_free_channel_view_buffers(SimpleApp* app) {
    if(!app) return;
    if(app->channel_view_counts_24) {
        free(app->channel_view_counts_24);
        app->channel_view_counts_24 = NULL;
    }
    if(app->channel_view_counts_5) {
        free(app->channel_view_counts_5);
        app->channel_view_counts_5 = NULL;
    }
    if(app->channel_view_working_counts_24) {
        free(app->channel_view_working_counts_24);
        app->channel_view_working_counts_24 = NULL;
    }
    if(app->channel_view_working_counts_5) {
        free(app->channel_view_working_counts_5);
        app->channel_view_working_counts_5 = NULL;
    }
}

static void simple_app_prepare_scan_buffers(SimpleApp* app) {
    if(!app) return;
    simple_app_reset_passwords_listing(app);
    simple_app_reset_evil_twin_pass_listing(app);
    simple_app_reset_evil_twin_listing(app);
    simple_app_reset_portal_ssid_listing(app);
    simple_app_reset_sd_listing(app);
    simple_app_reset_karma_probe_listing(app);
    simple_app_reset_karma_html_listing(app);
    simple_app_free_bt_buffers(app);
    simple_app_free_package_monitor_buffers(app);
    simple_app_free_channel_view_buffers(app);
    simple_app_free_evil_twin_qr_buffers(app);
    simple_app_free_help_qr_buffers(app);
}

static void simple_app_prepare_password_buffers(SimpleApp* app) {
    if(!app) return;
    if(app->scan_results || app->scan_selected_numbers || app->visible_result_indices) {
        simple_app_scan_free_buffers(app);
        simple_app_reset_scan_results(app);
    }
    simple_app_reset_sd_listing(app);
    simple_app_reset_portal_ssid_listing(app);
    simple_app_reset_karma_probe_listing(app);
    simple_app_reset_karma_html_listing(app);
    simple_app_free_bt_buffers(app);
}

static bool simple_app_portal_alloc_ssid_entries(SimpleApp* app) {
    if(!app) return false;
    if(app->portal_ssid_entries) return true;
    app->portal_ssid_entries = calloc(PORTAL_MAX_SSID_PRESETS, sizeof(PortalSsidEntry));
    return app->portal_ssid_entries != NULL;
}

static void simple_app_portal_free_ssid_entries(SimpleApp* app) {
    if(!app) return;
    if(app->portal_ssid_entries) {
        free(app->portal_ssid_entries);
        app->portal_ssid_entries = NULL;
    }
}

static bool simple_app_sd_alloc_files(SimpleApp* app) {
    if(!app) return false;
    if(app->sd_files) return true;
    app->sd_files = calloc(SD_MANAGER_MAX_FILES, sizeof(SdFileEntry));
    return app->sd_files != NULL;
}

static void simple_app_sd_free_files(SimpleApp* app) {
    if(!app) return;
    if(app->sd_files) {
        free(app->sd_files);
        app->sd_files = NULL;
    }
}

static bool simple_app_evil_twin_alloc_html_entries(SimpleApp* app) {
    if(!app) return false;
    if(app->evil_twin_html_entries) return true;
    app->evil_twin_html_entries = calloc(EVIL_TWIN_MAX_HTML_FILES, sizeof(EvilTwinHtmlEntry));
    return app->evil_twin_html_entries != NULL;
}

static void simple_app_evil_twin_free_html_entries(SimpleApp* app) {
    if(!app) return;
    if(app->evil_twin_html_entries) {
        free(app->evil_twin_html_entries);
        app->evil_twin_html_entries = NULL;
    }
}

static bool simple_app_karma_alloc_probes(SimpleApp* app) {
    if(!app) return false;
    if(app->karma_probes) return true;
    app->karma_probes = calloc(KARMA_MAX_PROBES, sizeof(KarmaProbeEntry));
    return app->karma_probes != NULL;
}

static void simple_app_karma_free_probes(SimpleApp* app) {
    if(!app) return;
    if(app->karma_probes) {
        free(app->karma_probes);
        app->karma_probes = NULL;
    }
}

static bool simple_app_karma_alloc_html_entries(SimpleApp* app) {
    if(!app) return false;
    if(app->karma_html_entries) return true;
    app->karma_html_entries = calloc(KARMA_MAX_HTML_FILES, sizeof(KarmaHtmlEntry));
    return app->karma_html_entries != NULL;
}

static void simple_app_karma_free_html_entries(SimpleApp* app) {
    if(!app) return;
    if(app->karma_html_entries) {
        free(app->karma_html_entries);
        app->karma_html_entries = NULL;
    }
}

static void simple_app_show_low_memory(size_t free_heap, size_t needed) {
    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
    if(dialogs) {
        DialogMessage* msg = dialog_message_alloc();
        if(msg) {
            dialog_message_set_header(msg, "Low memory", 64, 2, AlignCenter, AlignTop);
            char text[96];
            snprintf(
                text,
                sizeof(text),
                "Free: %lu B\nNeed: %lu B\nClose qFlipper/USB\nand relaunch.",
                (unsigned long)free_heap,
                (unsigned long)needed);
            dialog_message_set_text(msg, text, 64, 32, AlignCenter, AlignCenter);
            dialog_message_show(dialogs, msg);
            dialog_message_free(msg);
        }
        furi_record_close(RECORD_DIALOGS);
    }
}

static void simple_app_show_usb_blocker(void) {
    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
    if(dialogs) {
        DialogMessage* msg = dialog_message_alloc();
        if(msg) {
            dialog_message_set_header(msg, "USB busy", 64, 8, AlignCenter, AlignTop);
            dialog_message_set_text(
                msg, "Switch USB to CDC/Serial\nthen relaunch.", 64, 32, AlignCenter, AlignCenter);
            dialog_message_show(dialogs, msg);
            dialog_message_free(msg);
        }
        furi_record_close(RECORD_DIALOGS);
    }
}

static void simple_app_usb_runtime_guard(SimpleApp* app) {
    if(!app) return;
    uint32_t now = furi_get_tick();
    if(app->usb_guard_last_tick != 0) {
        uint32_t interval = furi_ms_to_ticks(USB_RUNTIME_GUARD_INTERVAL_MS);
        if(interval == 0) interval = 1;
        if((now - app->usb_guard_last_tick) < interval) {
            return;
        }
    }
    app->usb_guard_last_tick = now;

    if(!simple_app_usb_profile_ok()) {
        simple_app_show_usb_blocker();
        app->exit_app = true;
        return;
    }

    if(!simple_app_sd_ok()) {
        simple_app_show_sd_busy();
        app->exit_app = true;
        return;
    }
}

static void simple_app_update_result_layout(SimpleApp* app) {
    if(!app) return;
    size_t enabled_fields = simple_app_enabled_field_count(app);
    app->result_max_lines = RESULT_DEFAULT_MAX_LINES;
    if(enabled_fields <= 2) {
        app->result_font = FontPrimary;
        app->result_line_height = 14;
        app->result_max_lines = 3;
    } else if(enabled_fields <= 4) {
        app->result_font = FontSecondary;
        app->result_line_height = RESULT_DEFAULT_LINE_HEIGHT;
    } else {
        app->result_font = FontSecondary;
        app->result_line_height = RESULT_DEFAULT_LINE_HEIGHT;
    }

    uint8_t char_width = (app->result_font == FontPrimary) ? 7 : 6;
    uint8_t available_px = DISPLAY_WIDTH - RESULT_TEXT_X - RESULT_SCROLL_WIDTH - RESULT_SCROLL_GAP;
    uint8_t computed_limit = (char_width > 0) ? (available_px / char_width) :
                                                RESULT_DEFAULT_CHAR_LIMIT;
    if(computed_limit < 10) {
        computed_limit = 10;
    }
    app->result_char_limit = computed_limit;

    if(app->result_line_height == 0) {
        app->result_line_height = RESULT_DEFAULT_LINE_HEIGHT;
    }
    if(app->result_max_lines == 0) {
        app->result_max_lines = RESULT_DEFAULT_MAX_LINES;
    }
}

static void simple_app_line_append_token(
    char* line,
    size_t line_size,
    const char* label,
    const char* value) {
    if(!line || line_size == 0 || !value || value[0] == '\0') return;
    size_t current_len = strlen(line);
    if(current_len >= line_size - 1) return;
    size_t remaining = (line_size > current_len) ? (line_size - current_len - 1) : 0;
    if(remaining == 0) return;

    const char* separator = (current_len > 0) ? "  " : "";
    size_t sep_len = strlen(separator);
    if(sep_len > remaining) sep_len = remaining;
    if(sep_len > 0) {
        memcpy(line + current_len, separator, sep_len);
        current_len += sep_len;
        remaining -= sep_len;
    }

    if(label && label[0] != '\0' && remaining > 0) {
        size_t label_len = strlen(label);
        if(label_len > remaining) label_len = remaining;
        memcpy(line + current_len, label, label_len);
        current_len += label_len;
        remaining -= label_len;
    }

    if(remaining > 0) {
        size_t value_len = strlen(value);
        if(value_len > remaining) value_len = remaining;
        memcpy(line + current_len, value, value_len);
        current_len += value_len;
    }

    if(current_len < line_size) {
        line[current_len] = '\0';
    } else {
        line[line_size - 1] = '\0';
    }
}

static size_t simple_app_build_result_lines(
    const SimpleApp* app,
    const ScanResult* result,
    char lines[][64],
    size_t max_lines) {
    if(!app || !result || max_lines == 0) return 0;

    size_t char_limit = (app->result_char_limit > 0) ? app->result_char_limit :
                                                       RESULT_DEFAULT_CHAR_LIMIT;
    if(char_limit == 0) char_limit = RESULT_DEFAULT_CHAR_LIMIT;
    if(char_limit >= 63) char_limit = 63;

    char ssid_ascii[SCAN_SSID_MAX_LEN];
    simple_app_utf8_to_ascii_pl(result->ssid, ssid_ascii, sizeof(ssid_ascii));
    bool store_lines = (lines != NULL);
    if(max_lines > RESULT_DEFAULT_MAX_LINES) {
        max_lines = RESULT_DEFAULT_MAX_LINES;
    }

    size_t emitted = 0;
    const size_t line_cap = 64;
    size_t truncate_limit = (char_limit < (line_cap - 1)) ? char_limit : (line_cap - 1);
    if(truncate_limit < 1) truncate_limit = 1;

#define SIMPLE_APP_EMIT_LINE(buffer, allow_truncate)                          \
    do {                                                                      \
        if(emitted < max_lines) {                                             \
            if(store_lines) {                                                 \
                strncpy(lines[emitted], buffer, line_cap - 1);                \
                lines[emitted][line_cap - 1] = '\0';                          \
                if(allow_truncate) {                                          \
                    simple_app_truncate_text(lines[emitted], truncate_limit); \
                }                                                             \
            }                                                                 \
            emitted++;                                                        \
        }                                                                     \
    } while(0)

    char first_line[line_cap];
    memset(first_line, 0, sizeof(first_line));
    snprintf(
        first_line,
        sizeof(first_line),
        "%u%s",
        (unsigned)result->number,
        result->selected ? "*" : "");

    const char* vendor_name =
        app->scanner_show_vendor ? simple_app_vendor_cache_lookup(app, result->bssid) : NULL;
    bool vendor_visible = (vendor_name && vendor_name[0] != '\0');
    bool vendor_in_first_line = false;
    bool ssid_visible = app->scanner_show_ssid && result->ssid[0] != '\0';
    if(ssid_visible) {
        size_t len = strlen(first_line);
        if(len < sizeof(first_line) - 1) {
            size_t remaining = sizeof(first_line) - len - 1;
            if(remaining > 0) {
                first_line[len++] = ' ';
                size_t ssid_len = strlen(ssid_ascii);
                if(ssid_len > remaining) ssid_len = remaining;
                memcpy(first_line + len, ssid_ascii, ssid_len);
                len += ssid_len;
                first_line[len] = '\0';
            }
        }
        if(vendor_visible) {
            size_t len_after_ssid = strlen(first_line);
            if(len_after_ssid < sizeof(first_line) - 1) {
                size_t remaining = sizeof(first_line) - len_after_ssid - 1;
                if(remaining > 3) {
                    size_t vendor_len = strlen(vendor_name);
                    if(vendor_len > remaining - 1) vendor_len = remaining - 1;
                    if(vendor_len > 0) {
                        first_line[len_after_ssid++] = ' ';
                        first_line[len_after_ssid++] = '(';
                        memcpy(first_line + len_after_ssid, vendor_name, vendor_len);
                        len_after_ssid += vendor_len;
                        if(len_after_ssid < sizeof(first_line) - 1) {
                            first_line[len_after_ssid++] = ')';
                            first_line[len_after_ssid] = '\0';
                            vendor_in_first_line = true;
                        } else {
                            first_line[sizeof(first_line) - 1] = '\0';
                        }
                    }
                }
            }
        }
    } else if(vendor_visible) {
        size_t len = strlen(first_line);
        if(len < sizeof(first_line) - 1) {
            size_t remaining = sizeof(first_line) - len - 1;
            if(remaining > 0) {
                first_line[len++] = ' ';
                size_t vendor_len = strlen(vendor_name);
                if(vendor_len > remaining) vendor_len = remaining;
                memcpy(first_line + len, vendor_name, vendor_len);
                len += vendor_len;
                first_line[len] = '\0';
                vendor_in_first_line = true;
            }
        }
    }

    bool bssid_in_first_line = false;
    char bssid_text[18];
    simple_app_format_bssid(result->bssid, bssid_text, sizeof(bssid_text));
    bool bssid_available = !simple_app_bssid_is_empty(result->bssid);
    if(!ssid_visible && app->scanner_show_bssid && bssid_available) {
        size_t len = strlen(first_line);
        if(len < sizeof(first_line) - 1) {
            size_t remaining = sizeof(first_line) - len - 1;
            if(remaining > 0) {
                first_line[len++] = ' ';
                size_t bssid_len = strlen(bssid_text);
                if(bssid_len > remaining) bssid_len = remaining;
                memcpy(first_line + len, bssid_text, bssid_len);
                len += bssid_len;
                first_line[len] = '\0';
                bssid_in_first_line = true;
            }
        }
    }

    SIMPLE_APP_EMIT_LINE(first_line, false);

    if(emitted < max_lines && app->scanner_show_bssid && !bssid_in_first_line && bssid_available) {
        char bssid_line[line_cap];
        memset(bssid_line, 0, sizeof(bssid_line));
        size_t len = 0;
        bssid_line[len++] = ' ';
        if(len < sizeof(bssid_line) - 1) {
            size_t remaining = sizeof(bssid_line) - len - 1;
            size_t bssid_len = strlen(bssid_text);
            if(bssid_len > remaining) bssid_len = remaining;
            memcpy(bssid_line + len, bssid_text, bssid_len);
            len += bssid_len;
            bssid_line[len] = '\0';
        } else {
            bssid_line[sizeof(bssid_line) - 1] = '\0';
        }
        SIMPLE_APP_EMIT_LINE(bssid_line, true);
    }

    if(emitted < max_lines && vendor_visible && !vendor_in_first_line) {
        char vendor_line[line_cap];
        memset(vendor_line, 0, sizeof(vendor_line));
        size_t len = 0;
        vendor_line[len++] = ' ';
        if(len < sizeof(vendor_line) - 1) {
            size_t remaining = sizeof(vendor_line) - len - 1;
            size_t vendor_len = strlen(vendor_name);
            if(vendor_len > remaining) vendor_len = remaining;
            memcpy(vendor_line + len, vendor_name, vendor_len);
            len += vendor_len;
            vendor_line[len] = '\0';
        } else {
            vendor_line[sizeof(vendor_line) - 1] = '\0';
        }
        SIMPLE_APP_EMIT_LINE(vendor_line, true);
    }

    if(emitted < max_lines) {
        char info_line[line_cap];
        memset(info_line, 0, sizeof(info_line));
        if(app->scanner_show_channel && result->channel > 0) {
            char channel_text[4];
            snprintf(channel_text, sizeof(channel_text), "%u", (unsigned)result->channel);
            simple_app_line_append_token(info_line, sizeof(info_line), NULL, channel_text);
        }
        if(app->scanner_show_band) {
            const char* band_label = simple_app_band_label(result->band);
            if(band_label[0] != '\0') {
                simple_app_line_append_token(info_line, sizeof(info_line), NULL, band_label);
            }
        }
        if(info_line[0] != '\0') {
            SIMPLE_APP_EMIT_LINE(info_line, true);
        }
    }

    if(emitted < max_lines) {
        char info_line[line_cap];
        memset(info_line, 0, sizeof(info_line));
        if(app->scanner_show_security && result->security[0] != '\0') {
            char security_value[SCAN_FIELD_BUFFER_LEN];
            memset(security_value, 0, sizeof(security_value));
            strncpy(security_value, result->security, sizeof(security_value) - 1);
            char* mixed_suffix = strstr(security_value, " Mixed");
            if(mixed_suffix) {
                *mixed_suffix = '\0';
                size_t trimmed_len = strlen(security_value);
                while(trimmed_len > 0 && (security_value[trimmed_len - 1] == ' ' ||
                                          security_value[trimmed_len - 1] == '/')) {
                    security_value[trimmed_len - 1] = '\0';
                    trimmed_len--;
                }
            }
            simple_app_line_append_token(info_line, sizeof(info_line), NULL, security_value);
        }
        if(app->scanner_show_power && result->power_valid) {
            char power_value[16];
            snprintf(power_value, sizeof(power_value), "%d dBm", (int)result->power_dbm);
            simple_app_line_append_token(info_line, sizeof(info_line), NULL, power_value);
        }
        if(info_line[0] != '\0') {
            SIMPLE_APP_EMIT_LINE(info_line, true);
        }
    }

    if(emitted == 0) {
        char fallback_line[line_cap];
        strncpy(fallback_line, "-", sizeof(fallback_line) - 1);
        fallback_line[sizeof(fallback_line) - 1] = '\0';
        SIMPLE_APP_EMIT_LINE(fallback_line, true);
    }

#undef SIMPLE_APP_EMIT_LINE

    return emitted;
}

static void simple_app_show_status_message(
    SimpleApp* app,
    const char* message,
    uint32_t duration_ms,
    bool fullscreen) {
    if(!app) return;
    if(message && message[0] != '\0') {
        strncpy(app->status_message, message, sizeof(app->status_message) - 1);
        app->status_message[sizeof(app->status_message) - 1] = '\0';
        if(duration_ms == 0) {
            app->status_message_until = UINT32_MAX;
        } else {
            uint32_t timeout_ticks = furi_ms_to_ticks(duration_ms);
            app->status_message_until = furi_get_tick() + timeout_ticks;
        }
        app->status_message_fullscreen = fullscreen;
    } else {
        app->status_message[0] = '\0';
        app->status_message_until = 0;
        app->status_message_fullscreen = false;
    }
    if(app->viewport) {
        view_port_update(app->viewport);
    }
}

static void simple_app_clear_status_message(SimpleApp* app) {
    if(!app) return;
    app->status_message[0] = '\0';
    app->status_message_until = 0;
    app->status_message_fullscreen = false;
    app->karma_status_active = false;
    if(app->viewport) {
        view_port_update(app->viewport);
    }
}

static bool simple_app_status_message_is_active(SimpleApp* app) {
    if(!app) return false;
    if(app->status_message_until == 0 || app->status_message[0] == '\0') return false;
    if(app->status_message_until != UINT32_MAX && furi_get_tick() >= app->status_message_until) {
        simple_app_clear_status_message(app);
        return false;
    }
    return true;
}

static void simple_app_mark_config_dirty(SimpleApp* app) {
    if(!app) return;
    app->config_dirty = true;
}

static bool __attribute__((unused)) simple_app_wait_for_sd(Storage* storage, uint32_t timeout_ms) {
    if(!storage) return false;
    uint32_t start = furi_get_tick();
    while(true) {
        if(storage_sd_status(storage) == FSE_OK) {
            return true;
        }
        if(timeout_ms > 0) {
            uint32_t elapsed = furi_get_tick() - start;
            if(elapsed >= furi_ms_to_ticks(timeout_ms)) {
                return false;
            }
        }
        furi_delay_ms(50);
    }
}

static bool simple_app_parse_bool_value(const char* value, bool current) {
    if(!value) return current;
    size_t len = strlen(value);
    if(len == 0) return current;

    char c0 = (char)tolower((unsigned char)value[0]);
    char c1 = (len > 1) ? (char)tolower((unsigned char)value[1]) : '\0';

    // Textual checks: on/yes/true, off/no/false
    if((c0 == 'o' && c1 == 'n') || (c0 == 'y' && c1 == 'e') || (c0 == 't' && c1 == 'r')) {
        return true;
    }
    if((c0 == 'o' && c1 == 'f') || (c0 == 'n' && c1 == 'o') || (c0 == 'f' && c1 == 'a')) {
        return false;
    }

    // Single digit shortcuts
    if(c0 == '1') return true;
    if(c0 == '0') return false;

    return atoi(value) != 0;
}

static void simple_app_trim(char* text) {
    if(!text) return;
    char* start = text;
    while(*start && isspace((unsigned char)*start)) {
        start++;
    }
    char* end = start + strlen(start);
    while(end > start && isspace((unsigned char)end[-1])) {
        end--;
    }
    *end = '\0';
    if(start != text) {
        memmove(text, start, (size_t)(end - start) + 1);
    }
}

static void simple_app_parse_config_line(SimpleApp* app, char* line) {
    if(!app || !line) return;
    simple_app_trim(line);
    if(line[0] == '\0' || line[0] == '#') return;
    char* equals = strchr(line, '=');
    if(!equals) return;
    *equals = '\0';
    char* key = line;
    char* value = equals + 1;
    simple_app_trim(key);
    simple_app_trim(value);
    if(strcmp(key, "show_ssid") == 0) {
        app->scanner_show_ssid = simple_app_parse_bool_value(value, app->scanner_show_ssid);
    } else if(strcmp(key, "show_bssid") == 0) {
        app->scanner_show_bssid = simple_app_parse_bool_value(value, app->scanner_show_bssid);
    } else if(strcmp(key, "show_channel") == 0) {
        app->scanner_show_channel = simple_app_parse_bool_value(value, app->scanner_show_channel);
    } else if(strcmp(key, "show_security") == 0) {
        app->scanner_show_security =
            simple_app_parse_bool_value(value, app->scanner_show_security);
    } else if(strcmp(key, "show_power") == 0) {
        app->scanner_show_power = simple_app_parse_bool_value(value, app->scanner_show_power);
    } else if(strcmp(key, "show_band") == 0) {
        app->scanner_show_band = simple_app_parse_bool_value(value, app->scanner_show_band);
    } else if(strcmp(key, "show_vendor") == 0) {
        app->scanner_show_vendor = simple_app_parse_bool_value(value, app->scanner_show_vendor);
    } else if(strcmp(key, "min_power") == 0) {
        app->scanner_min_power = (int16_t)strtol(value, NULL, 10);
    } else if(strcmp(key, "min_channel_time") == 0) {
        app->scanner_min_channel_time = (uint16_t)strtoul(value, NULL, 10);
    } else if(strcmp(key, "max_channel_time") == 0) {
        app->scanner_max_channel_time = (uint16_t)strtoul(value, NULL, 10);
    } else if(strcmp(key, "backlight_enabled") == 0) {
        app->backlight_enabled = simple_app_parse_bool_value(value, app->backlight_enabled);
    } else if(strcmp(key, "otg_power_enabled") == 0) {
        app->otg_power_enabled = simple_app_parse_bool_value(value, app->otg_power_enabled);
    } else if(strcmp(key, "show_ram_overlay") == 0) {
        app->show_ram_overlay = simple_app_parse_bool_value(value, app->show_ram_overlay);
    } else if(strcmp(key, "debug_mark") == 0) {
        app->debug_mark_enabled = simple_app_parse_bool_value(value, app->debug_mark_enabled);
    } else if(strcmp(key, "karma_duration") == 0) {
        app->karma_sniffer_duration_sec = (uint32_t)strtoul(value, NULL, 10);
    } else if(strcmp(key, "led_enabled") == 0) {
        app->config_seen_led_enabled = true;
        app->led_enabled = simple_app_parse_bool_value(value, app->led_enabled);
    } else if(strcmp(key, "led_level") == 0) {
        app->config_seen_led_level = true;
        long parsed = strtol(value, NULL, 10);
        if(parsed >= 0) {
            if(parsed > 255) {
                parsed = 255;
            }
            app->led_level = (uint8_t)parsed;
        }
    } else if(strcmp(key, "gps_utc_offset_min") == 0) {
        app->config_seen_gps_utc_offset = true;
        app->gps_utc_offset_minutes = simple_app_clamp_gps_utc_offset(strtol(value, NULL, 10));
    } else if(strcmp(key, "gps_dst") == 0) {
        app->config_seen_gps_dst = true;
        app->gps_dst_enabled = simple_app_parse_bool_value(value, app->gps_dst_enabled);
    } else if(strcmp(key, "gps_zda_tz") == 0) {
        app->config_seen_gps_zda_tz = true;
        app->gps_zda_tz_enabled = simple_app_parse_bool_value(value, app->gps_zda_tz_enabled);
    } else if(strcmp(key, "boot_short_status") == 0) {
        app->boot_short_enabled = simple_app_parse_bool_value(value, app->boot_short_enabled);
    } else if(strcmp(key, "boot_long_status") == 0) {
        app->boot_long_enabled = simple_app_parse_bool_value(value, app->boot_long_enabled);
    } else if(strcmp(key, "boot_short") == 0) {
        for(uint8_t i = 0; i < BOOT_COMMAND_OPTION_COUNT; i++) {
            if(strcmp(value, boot_command_options[i]) == 0) {
                app->boot_short_command_index = i;
                break;
            }
        }
    } else if(strcmp(key, "boot_long") == 0) {
        for(uint8_t i = 0; i < BOOT_COMMAND_OPTION_COUNT; i++) {
            if(strcmp(value, boot_command_options[i]) == 0) {
                app->boot_long_command_index = i;
                break;
            }
        }
    }
}

static bool simple_app_save_config(SimpleApp* app, const char* success_message, bool fullscreen) {
    if(!app) return false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;
    storage_simply_mkdir(storage, EXT_PATH("apps_assets"));
    storage_simply_mkdir(storage, EXT_PATH(LAB_C5_CONFIG_DIR_PATH));
    File* file = storage_file_alloc(storage);
    bool success = false;
    if(storage_file_open(
           file, EXT_PATH(LAB_C5_CONFIG_FILE_PATH), FSAM_READ_WRITE, FSOM_CREATE_ALWAYS)) {
        char buffer[512];
        uint8_t short_idx = (app->boot_short_command_index < BOOT_COMMAND_OPTION_COUNT) ?
                                app->boot_short_command_index :
                                0;
        uint8_t long_idx = (app->boot_long_command_index < BOOT_COMMAND_OPTION_COUNT) ?
                               app->boot_long_command_index :
                               0;
        int len = snprintf(
            buffer,
            sizeof(buffer),
            "show_ssid=%d\n"
            "show_bssid=%d\n"
            "show_channel=%d\n"
            "show_security=%d\n"
            "show_power=%d\n"
            "show_band=%d\n"
            "show_vendor=%d\n"
            "min_power=%d\n"
            "min_channel_time=%u\n"
            "max_channel_time=%u\n"
            "backlight_enabled=%d\n"
            "otg_power_enabled=%d\n"
            "show_ram_overlay=%d\n"
            "debug_mark=%d\n"
            "karma_duration=%lu\n"
            "gps_utc_offset_min=%d\n"
            "gps_dst=%d\n"
            "gps_zda_tz=%d\n"
            "led_enabled=%d\n"
            "led_level=%u\n"
            "boot_short_status=%d\n"
            "boot_short=%s\n"
            "boot_long_status=%d\n"
            "boot_long=%s\n",
            app->scanner_show_ssid ? 1 : 0,
            app->scanner_show_bssid ? 1 : 0,
            app->scanner_show_channel ? 1 : 0,
            app->scanner_show_security ? 1 : 0,
            app->scanner_show_power ? 1 : 0,
            app->scanner_show_band ? 1 : 0,
            app->scanner_show_vendor ? 1 : 0,
            (int)app->scanner_min_power,
            (unsigned)app->scanner_min_channel_time,
            (unsigned)app->scanner_max_channel_time,
            app->backlight_enabled ? 1 : 0,
            app->otg_power_enabled ? 1 : 0,
            app->show_ram_overlay ? 1 : 0,
            app->debug_mark_enabled ? 1 : 0,
            (unsigned long)app->karma_sniffer_duration_sec,
            (int)app->gps_utc_offset_minutes,
            app->gps_dst_enabled ? 1 : 0,
            app->gps_zda_tz_enabled ? 1 : 0,
            app->led_enabled ? 1 : 0,
            (unsigned)app->led_level,
            app->boot_short_enabled ? 1 : 0,
            boot_command_options[short_idx],
            app->boot_long_enabled ? 1 : 0,
            boot_command_options[long_idx]);
        if(len > 0 && len < (int)sizeof(buffer)) {
            size_t written = storage_file_write(file, buffer, (size_t)len);
            if(written == (size_t)len) {
                success = true;
            } else {
                FURI_LOG_E(TAG, "Config write short (%u/%u)", (unsigned)written, (unsigned)len);
            }
        } else {
            FURI_LOG_E(TAG, "Config buffer overflow (%d)", len);
        }
        storage_file_close(file);
    } else {
        FURI_LOG_E(TAG, "Config open for write failed");
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    if(success) {
        app->config_dirty = false;
        if(success_message) {
            simple_app_show_status_message(app, success_message, 2000, fullscreen);
        }
    } else if(success_message) {
        simple_app_show_status_message(app, "Config save failed", 2000, true);
    }
    return success;
}

static void simple_app_load_config(SimpleApp* app) {
    if(!app) return;
    app->config_seen_led_enabled = false;
    app->config_seen_led_level = false;
    app->config_seen_gps_utc_offset = false;
    app->config_seen_gps_dst = false;
    app->config_seen_gps_zda_tz = false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return;
    storage_simply_mkdir(storage, EXT_PATH("apps_assets"));
    storage_simply_mkdir(storage, EXT_PATH(LAB_C5_CONFIG_DIR_PATH));
    File* file = storage_file_alloc(storage);
    bool loaded = false;
    if(storage_file_open(file, EXT_PATH(LAB_C5_CONFIG_FILE_PATH), FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[96];
        size_t pos = 0;
        uint8_t ch = 0;
        while(storage_file_read(file, &ch, 1) == 1) {
            if(ch == '\r') continue;
            if(ch == '\n') {
                line[pos] = '\0';
                simple_app_parse_config_line(app, line);
                pos = 0;
                continue;
            }
            if(pos + 1 < sizeof(line)) {
                line[pos++] = (char)ch;
            }
        }
        if(pos > 0) {
            line[pos] = '\0';
            simple_app_parse_config_line(app, line);
        }
        storage_file_close(file);
        loaded = true;
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    if(loaded) {
        simple_app_show_status_message(app, "Config loaded", 2500, true);
        app->config_dirty = false;
    } else {
        bool created = simple_app_save_config(app, NULL, false);
        if(created) {
            simple_app_show_status_message(app, "Config created", 2500, true);
            app->config_dirty = false;
        } else {
            simple_app_show_status_message(app, "Config write failed", 2000, true);
            simple_app_mark_config_dirty(app);
        }
    }
    if(app->scanner_min_power > SCAN_POWER_MAX_DBM) {
        app->scanner_min_power = SCAN_POWER_MAX_DBM;
    } else if(app->scanner_min_power < SCAN_POWER_MIN_DBM) {
        app->scanner_min_power = SCAN_POWER_MIN_DBM;
    }
    if(app->scanner_min_channel_time < SCANNER_CHANNEL_TIME_MIN_MS) {
        app->scanner_min_channel_time = SCANNER_CHANNEL_TIME_MIN_MS;
    } else if(app->scanner_min_channel_time > SCANNER_CHANNEL_TIME_MAX_MS) {
        app->scanner_min_channel_time = SCANNER_CHANNEL_TIME_MAX_MS;
    }
    if(app->scanner_max_channel_time < SCANNER_CHANNEL_TIME_MIN_MS) {
        app->scanner_max_channel_time = SCANNER_CHANNEL_TIME_MIN_MS;
    } else if(app->scanner_max_channel_time > SCANNER_CHANNEL_TIME_MAX_MS) {
        app->scanner_max_channel_time = SCANNER_CHANNEL_TIME_MAX_MS;
    }
    if(app->scanner_min_channel_time > app->scanner_max_channel_time) {
        app->scanner_max_channel_time = app->scanner_min_channel_time;
    }
    if(app->karma_sniffer_duration_sec < KARMA_SNIFFER_DURATION_MIN_SEC) {
        app->karma_sniffer_duration_sec = KARMA_SNIFFER_DURATION_MIN_SEC;
    } else if(app->karma_sniffer_duration_sec > KARMA_SNIFFER_DURATION_MAX_SEC) {
        app->karma_sniffer_duration_sec = KARMA_SNIFFER_DURATION_MAX_SEC;
    }
    if(app->led_level < 1) {
        app->led_level = 1;
    } else if(app->led_level > 100) {
        app->led_level = 100;
    }
    app->gps_utc_offset_minutes = simple_app_clamp_gps_utc_offset(app->gps_utc_offset_minutes);
    if(app->boot_short_command_index >= BOOT_COMMAND_OPTION_COUNT) {
        app->boot_short_command_index = 1;
    }
    if(app->boot_long_command_index >= BOOT_COMMAND_OPTION_COUNT) {
        app->boot_long_command_index = 0;
    }
    if(loaded) {
        bool missing_fields = !app->config_seen_led_enabled || !app->config_seen_led_level ||
                              !app->config_seen_gps_utc_offset || !app->config_seen_gps_dst ||
                              !app->config_seen_gps_zda_tz;
        if(missing_fields) {
            simple_app_save_config(app, NULL, false);
        }
    }
    simple_app_reset_karma_probe_listing(app);
    simple_app_reset_karma_probe_listing(app);
    simple_app_update_karma_duration_label(app);
    simple_app_update_result_layout(app);
    simple_app_rebuild_visible_results(app);
}

static void simple_app_save_config_if_dirty(SimpleApp* app, const char* message, bool fullscreen) {
    if(!app) return;
    if(app->config_dirty) {
        simple_app_save_config(app, message, fullscreen);
    }
}

static void simple_app_apply_backlight(SimpleApp* app) {
    if(!app) return;
    uint8_t level = app->backlight_enabled ? BACKLIGHT_ON_LEVEL : BACKLIGHT_OFF_LEVEL;
    furi_hal_light_set(LightBacklight, level);

    if(app->backlight_enabled) {
        if(!app->backlight_insomnia) {
            furi_hal_power_insomnia_enter();
            app->backlight_insomnia = true;
        }
        if(app->notifications && !app->backlight_notification_enforced) {
            notification_message_block(app->notifications, &sequence_display_backlight_enforce_on);
            app->backlight_notification_enforced = true;
        }
    } else {
        if(app->backlight_insomnia) {
            furi_hal_power_insomnia_exit();
            app->backlight_insomnia = false;
        }
        if(app->notifications && app->backlight_notification_enforced) {
            notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
            app->backlight_notification_enforced = false;
        }
    }

    simple_app_update_backlight_label(app);
}

static void simple_app_toggle_backlight(SimpleApp* app) {
    if(!app) return;
    app->backlight_enabled = !app->backlight_enabled;
    simple_app_apply_backlight(app);
    simple_app_mark_config_dirty(app);
}

static void simple_app_toggle_show_ram(SimpleApp* app) {
    if(!app) return;
    app->show_ram_overlay = !app->show_ram_overlay;
    simple_app_update_ram_label(app);
    simple_app_mark_config_dirty(app);
    if(app->viewport) {
        view_port_update(app->viewport);
    }
}

static void simple_app_toggle_debug_mark(SimpleApp* app) {
    if(!app) return;
    app->debug_mark_enabled = !app->debug_mark_enabled;
    simple_app_update_debug_label(app);
    simple_app_mark_config_dirty(app);
    if(app->viewport) {
        view_port_update(app->viewport);
    }
}

static size_t simple_app_parse_quoted_fields(
    const char* line,
    char fields[][SCAN_FIELD_BUFFER_LEN],
    size_t max_fields) {
    if(!line || !fields || max_fields == 0) return 0;

    size_t count = 0;
    const char* ptr = line;
    while(*ptr && count < max_fields) {
        while(*ptr && *ptr != '"') {
            ptr++;
        }
        if(*ptr != '"') break;
        ptr++;
        const char* start = ptr;
        while(*ptr && *ptr != '"') {
            ptr++;
        }
        size_t len = (size_t)(ptr - start);
        if(len >= SCAN_FIELD_BUFFER_LEN) {
            len = SCAN_FIELD_BUFFER_LEN - 1;
        }
        memcpy(fields[count], start, len);
        fields[count][len] = '\0';
        if(*ptr == '"') {
            ptr++;
        }
        while(*ptr && *ptr != '"') {
            ptr++;
        }
        count++;
    }
    return count;
}

static uint8_t simple_app_parse_band_value(const char* band_field) {
    if(!band_field || band_field[0] == '\0') return SCAN_BAND_UNKNOWN;
    if(strncmp(band_field, "2.4", 3) == 0) return SCAN_BAND_24;
    if(band_field[0] == '5') return SCAN_BAND_5;
    if(strstr(band_field, "5G") != NULL || strstr(band_field, "5g") != NULL) return SCAN_BAND_5;
    return SCAN_BAND_UNKNOWN;
}

static const char* simple_app_band_label(uint8_t band) {
    if(band == SCAN_BAND_24) return "2.4G";
    if(band == SCAN_BAND_5) return "5G";
    return "";
}

static bool simple_app_bssid_is_empty(const uint8_t bssid[6]) {
    if(!bssid) return true;
    for(size_t i = 0; i < 6; i++) {
        if(bssid[i] != 0) return false;
    }
    return true;
}

static bool simple_app_parse_bssid(const char* text, uint8_t out[6]) {
    if(!out) return false;
    memset(out, 0, 6);
    if(!text || text[0] == '\0') return false;

    unsigned int values[6];
    if(sscanf(
           text,
           "%x:%x:%x:%x:%x:%x",
           &values[0],
           &values[1],
           &values[2],
           &values[3],
           &values[4],
           &values[5]) != 6) {
        return false;
    }
    for(size_t i = 0; i < 6; i++) {
        if(values[i] > 0xFF) return false;
        out[i] = (uint8_t)values[i];
    }
    return true;
}

static void simple_app_format_bssid(const uint8_t bssid[6], char* out, size_t out_size) {
    if(!out || out_size == 0) return;
    if(simple_app_bssid_is_empty(bssid)) {
        snprintf(out, out_size, "%s", "--:--:--:--:--:--");
        return;
    }
    snprintf(
        out,
        out_size,
        "%02X:%02X:%02X:%02X:%02X:%02X",
        bssid[0],
        bssid[1],
        bssid[2],
        bssid[3],
        bssid[4],
        bssid[5]);
}

static uint8_t simple_app_parse_channel(const char* text) {
    if(!text || text[0] == '\0') return 0;
    char* end = NULL;
    unsigned long value = strtoul(text, &end, 10);
    if(end == text) return 0;
    if(value > UINT8_MAX) value = UINT8_MAX;
    return (uint8_t)value;
}

static const char* simple_app_vendor_cache_lookup(const SimpleApp* app, const uint8_t bssid[6]) {
    if(!app || simple_app_bssid_is_empty(bssid)) return NULL;
    for(size_t i = 0; i < VENDOR_CACHE_SIZE; i++) {
        const VendorCacheEntry* entry = &app->vendor_cache[i];
        if(entry->valid && memcmp(entry->oui, bssid, 3) == 0) {
            return entry->name[0] != '\0' ? entry->name : NULL;
        }
    }
    return NULL;
}

static void
    simple_app_vendor_cache_update(SimpleApp* app, const uint8_t bssid[6], const char* vendor) {
    if(!app || simple_app_bssid_is_empty(bssid) || !vendor || vendor[0] == '\0') return;

    size_t candidate = VENDOR_CACHE_SIZE;
    uint32_t oldest_tick = UINT32_MAX;
    for(size_t i = 0; i < VENDOR_CACHE_SIZE; i++) {
        VendorCacheEntry* entry = &app->vendor_cache[i];
        if(entry->valid && memcmp(entry->oui, bssid, 3) == 0) {
            candidate = i;
            break;
        }
        if(!entry->valid) {
            candidate = i;
            break;
        }
        if(entry->last_tick < oldest_tick) {
            oldest_tick = entry->last_tick;
            candidate = i;
        }
    }

    if(candidate >= VENDOR_CACHE_SIZE) return;
    VendorCacheEntry* entry = &app->vendor_cache[candidate];
    memcpy(entry->oui, bssid, 3);
    strncpy(entry->name, vendor, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    entry->last_tick = furi_get_tick();
    entry->valid = true;
}

static size_t simple_app_parse_scan_count(const char* line) {
    if(!line) return 0;

    const char* found = strstr(line, "Found ");
    const char* retrieved = strstr(line, "Retrieved ");
    const char* start = found     ? (found + strlen("Found ")) :
                        retrieved ? (retrieved + strlen("Retrieved ")) :
                                    NULL;
    if(!start) return 0;

    char* end = NULL;
    unsigned long value = strtoul(start, &end, 10);
    if(end == start || value == 0) return 0;

    if(found && strstr(end, "networks") == NULL) return 0;
    if(retrieved && strstr(end, "network records") == NULL) return 0;

    return (size_t)value;
}

static void simple_app_process_scan_line(SimpleApp* app, const char* line) {
    if(!app || !line) return;

    const char* cursor = line;
    while(*cursor == ' ' || *cursor == '\t') {
        cursor++;
    }

    size_t expected_count = simple_app_parse_scan_count(cursor);
    if(expected_count > SCAN_RESULTS_MAX_CAPACITY) {
        expected_count = SCAN_RESULTS_MAX_CAPACITY;
    }
    if(expected_count > 0 && app->scan_result_count == 0 &&
       app->scan_results_capacity != expected_count) {
        if(!simple_app_scan_alloc_buffers(app, expected_count)) {
            app->scan_results_loading = false;
            app->scanner_scan_running = false;
            app->scanner_rescan_hint = false;
            simple_app_show_status_message(app, "OOM: scan buffers", 2000, true);
            if(app->viewport) view_port_update(app->viewport);
            return;
        }
    }

    if(strncmp(cursor, "Scan results", 12) == 0) {
        app->scan_results_loading = false;
        app->scanner_scan_running = false;
        app->scanner_rescan_hint = false;
        if(app->scanner_last_update_tick == 0) {
            app->scanner_last_update_tick = furi_get_tick();
        }
        if(app->screen == ScreenResults) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(strncmp(cursor, "No scan", 7) == 0 || strncmp(cursor, "No networks found", 17) == 0 ||
       strncmp(cursor, "Scan still in progress", 22) == 0) {
        app->scan_results_loading = false;
        app->scanner_scan_running = false;
        app->scanner_rescan_hint = false;
        if(app->screen == ScreenResults) {
            if(strncmp(cursor, "Scan still in progress", 22) == 0) {
                simple_app_show_status_message(app, "Scan running\nTry again soon", 1500, true);
            } else {
                simple_app_show_status_message(
                    app, "No scan results\nRun Scanner first", 1500, true);
            }
            view_port_update(app->viewport);
        }
        return;
    }

    if(cursor[0] != '"') return;
    if(!app->scan_results || app->scan_results_capacity == 0) return;
    if(app->scan_result_count >= app->scan_results_capacity) return;

    char fields[8][SCAN_FIELD_BUFFER_LEN];
    for(size_t i = 0; i < 8; i++) {
        memset(fields[i], 0, SCAN_FIELD_BUFFER_LEN);
    }

    size_t field_count = simple_app_parse_quoted_fields(cursor, fields, 8);
    if(field_count < 8) return;

    bool ssid_hidden = (fields[1][0] == '\0');

    ScanResult* result = &app->scan_results[app->scan_result_count];
    memset(result, 0, sizeof(ScanResult));
    result->number = (uint16_t)strtoul(fields[0], NULL, 10);
    simple_app_copy_field(result->ssid, sizeof(result->ssid), fields[1], SCAN_SSID_HIDDEN_LABEL);
    simple_app_copy_field(result->security, sizeof(result->security), fields[5], "Unknown");
    simple_app_parse_bssid(fields[3], result->bssid);
    result->channel = simple_app_parse_channel(fields[4]);
    result->band = simple_app_parse_band_value(fields[7]);

    if(fields[2][0] != '\0') {
        char vendor_ascii[SCAN_VENDOR_MAX_LEN];
        simple_app_utf8_to_ascii_pl(fields[2], vendor_ascii, sizeof(vendor_ascii));
        simple_app_trim(vendor_ascii);
        simple_app_vendor_cache_update(app, result->bssid, vendor_ascii);
    }

    const char* power_str = fields[6];
    char* power_end = NULL;
    long power_value = strtol(power_str, &power_end, 10);
    if(power_str[0] != '\0' && power_end && *power_end == '\0') {
        if(power_value < SCAN_POWER_MIN_DBM) {
            power_value = SCAN_POWER_MIN_DBM;
        } else if(power_value > SCAN_POWER_MAX_DBM) {
            power_value = SCAN_POWER_MAX_DBM;
        }
        result->power_dbm = (int16_t)power_value;
        result->power_valid = true;
    } else {
        result->power_dbm = 0;
        result->power_valid = false;
    }
    result->selected = false;

    app->scan_result_count++;
    simple_app_update_scanner_stats(app, result, ssid_hidden);
    app->scanner_scan_running = app->scan_results_loading;
    simple_app_rebuild_visible_results(app);

    if(app->screen == ScreenResults) {
        simple_app_adjust_result_offset(app);
        view_port_update(app->viewport);
    }
}

static uint8_t simple_app_result_line_count(const SimpleApp* app, const ScanResult* result) {
    if(!app || !result) return 1;
    size_t max_lines = (app->result_max_lines > 0) ? app->result_max_lines :
                                                     RESULT_DEFAULT_MAX_LINES;
    if(max_lines == 0) max_lines = RESULT_DEFAULT_MAX_LINES;
    if(max_lines > RESULT_DEFAULT_MAX_LINES) {
        max_lines = RESULT_DEFAULT_MAX_LINES;
    }
    size_t count = simple_app_build_result_lines(app, result, NULL, max_lines);
    if(count == 0) count = 1;
    return (uint8_t)count;
}

static size_t simple_app_total_result_lines(SimpleApp* app) {
    if(!app) return 0;
    size_t total = 0;
    for(size_t i = 0; i < app->visible_result_count; i++) {
        const ScanResult* result = simple_app_visible_result_const(app, i);
        if(!result) continue;
        total += simple_app_result_line_count(app, result);
    }
    return total;
}

static size_t simple_app_result_offset_lines(SimpleApp* app) {
    if(!app) return 0;
    size_t lines = 0;
    for(size_t i = 0; i < app->scan_result_offset && i < app->visible_result_count; i++) {
        const ScanResult* result = simple_app_visible_result_const(app, i);
        if(!result) continue;
        lines += simple_app_result_line_count(app, result);
    }
    return lines;
}

static void simple_app_reset_result_scroll(SimpleApp* app) {
    if(!app) return;
    app->result_scroll_offset = 0;
    app->result_scroll_direction = 1;
    app->result_scroll_hold = RESULT_SCROLL_EDGE_PAUSE_STEPS;
    app->result_scroll_last_tick = furi_get_tick();
    app->result_scroll_text[0] = '\0';
}

static void simple_app_reset_overlay_title_scroll(SimpleApp* app) {
    if(!app) return;
    app->overlay_title_scroll_offset = 0;
    app->overlay_title_scroll_direction = 1;
    app->overlay_title_scroll_hold = RESULT_SCROLL_EDGE_PAUSE_STEPS;
    app->overlay_title_scroll_last_tick = furi_get_tick();
    app->overlay_title_scroll_text[0] = '\0';
}

static void simple_app_reset_sd_scroll(SimpleApp* app) {
    if(!app) return;
    app->sd_scroll_offset = 0;
    app->sd_scroll_direction = 1;
    app->sd_scroll_hold = RESULT_SCROLL_EDGE_PAUSE_STEPS;
    app->sd_scroll_last_tick = furi_get_tick();
    app->sd_scroll_char_limit = 0;
    app->sd_scroll_text[0] = '\0';
}

static void simple_app_update_result_scroll(SimpleApp* app) {
    if(!app) return;

    if(app->screen != ScreenResults) {
        if(app->result_scroll_text[0] != '\0' || app->result_scroll_offset != 0) {
            simple_app_reset_result_scroll(app);
        }
        return;
    }

    if(app->visible_result_count == 0) {
        if(app->result_scroll_text[0] != '\0' || app->result_scroll_offset != 0) {
            simple_app_reset_result_scroll(app);
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
        return;
    }

    const ScanResult* result = simple_app_visible_result_const(app, app->scan_result_index);
    if(!result) {
        if(app->result_scroll_text[0] != '\0' || app->result_scroll_offset != 0) {
            simple_app_reset_result_scroll(app);
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
        return;
    }

    char first_line_buffer[1][64];
    memset(first_line_buffer, 0, sizeof(first_line_buffer));
    size_t produced = simple_app_build_result_lines(app, result, first_line_buffer, 1);
    if(produced == 0) {
        if(app->result_scroll_text[0] != '\0' || app->result_scroll_offset != 0) {
            simple_app_reset_result_scroll(app);
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
        return;
    }

    const char* full_line = first_line_buffer[0];
    if(strncmp(full_line, app->result_scroll_text, sizeof(app->result_scroll_text)) != 0) {
        strncpy(app->result_scroll_text, full_line, sizeof(app->result_scroll_text) - 1);
        app->result_scroll_text[sizeof(app->result_scroll_text) - 1] = '\0';
        app->result_scroll_offset = 0;
        app->result_scroll_direction = 1;
        app->result_scroll_hold = RESULT_SCROLL_EDGE_PAUSE_STEPS;
        app->result_scroll_last_tick = furi_get_tick();
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    size_t char_limit = (app->result_char_limit > 0) ? app->result_char_limit :
                                                       RESULT_DEFAULT_CHAR_LIMIT;
    if(char_limit == 0) char_limit = 1;
    if(char_limit >= sizeof(first_line_buffer[0])) {
        char_limit = sizeof(first_line_buffer[0]) - 1;
        if(char_limit == 0) char_limit = 1;
    }

    size_t line_length = strlen(app->result_scroll_text);
    if(line_length <= char_limit) {
        if(app->result_scroll_offset != 0) {
            app->result_scroll_offset = 0;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
        return;
    }

    size_t max_offset = line_length - char_limit;
    uint32_t now = furi_get_tick();
    uint32_t interval_ticks = furi_ms_to_ticks(RESULT_SCROLL_INTERVAL_MS);
    if(interval_ticks == 0) interval_ticks = 1;
    if((now - app->result_scroll_last_tick) < interval_ticks) {
        return;
    }
    app->result_scroll_last_tick = now;

    if(app->result_scroll_hold > 0) {
        app->result_scroll_hold--;
        return;
    }

    bool changed = false;
    if(app->result_scroll_direction > 0) {
        if(app->result_scroll_offset < max_offset) {
            app->result_scroll_offset++;
            changed = true;
            if(app->result_scroll_offset >= max_offset) {
                app->result_scroll_direction = -1;
                app->result_scroll_hold = RESULT_SCROLL_EDGE_PAUSE_STEPS;
            }
        } else {
            app->result_scroll_direction = -1;
            app->result_scroll_hold = RESULT_SCROLL_EDGE_PAUSE_STEPS;
        }
    } else {
        if(app->result_scroll_offset > 0) {
            app->result_scroll_offset--;
            changed = true;
            if(app->result_scroll_offset == 0) {
                app->result_scroll_direction = 1;
                app->result_scroll_hold = RESULT_SCROLL_EDGE_PAUSE_STEPS;
            }
        } else {
            app->result_scroll_direction = 1;
            app->result_scroll_hold = RESULT_SCROLL_EDGE_PAUSE_STEPS;
        }
    }

    if(changed && app->viewport) {
        view_port_update(app->viewport);
    }
}

static void simple_app_update_overlay_title_scroll(SimpleApp* app) {
    if(!app) return;

    bool overlay_active = (app->sniffer_results_active && !app->sniffer_full_console) ||
                          (app->probe_results_active && !app->probe_full_console) ||
                          (app->evil_twin_running && !app->evil_twin_full_console) ||
                          (app->portal_running && !app->portal_full_console);

    if(!overlay_active) {
        if(app->overlay_title_scroll_text[0] != '\0' || app->overlay_title_scroll_offset != 0) {
            simple_app_reset_overlay_title_scroll(app);
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
        return;
    }

    char desired_title[64];
    desired_title[0] = '\0';

    if(app->sniffer_results_active && !app->sniffer_full_console) {
        if(!app->sniffer_aps || !app->sniffer_clients) return;
        if(app->sniffer_ap_count == 0) {
            if(app->overlay_title_scroll_text[0] != '\0' ||
               app->overlay_title_scroll_offset != 0) {
                simple_app_reset_overlay_title_scroll(app);
                if(app->viewport) {
                    view_port_update(app->viewport);
                }
            }
            return;
        }

        size_t ap_index = app->sniffer_results_ap_index;
        if(ap_index >= app->sniffer_ap_count) {
            ap_index = app->sniffer_ap_count - 1;
        }
        const SnifferApEntry* ap = &app->sniffer_aps[ap_index];
        const char* ssid = ap->ssid[0] ? ap->ssid : "<hidden>";
        snprintf(desired_title, sizeof(desired_title), "%s", ssid);
    } else if(app->probe_results_active && !app->probe_full_console) {
        if(!app->probe_ssids || !app->probe_clients) return;
        if(app->probe_ssid_count == 0) {
            if(app->overlay_title_scroll_text[0] != '\0' ||
               app->overlay_title_scroll_offset != 0) {
                simple_app_reset_overlay_title_scroll(app);
                if(app->viewport) {
                    view_port_update(app->viewport);
                }
            }
            return;
        }

        size_t ssid_index = app->probe_ssid_index;
        if(ssid_index >= app->probe_ssid_count) {
            ssid_index = app->probe_ssid_count - 1;
        }
        const ProbeSsidEntry* ssid = &app->probe_ssids[ssid_index];
        const char* ssid_text = ssid->ssid[0] ? ssid->ssid : "<hidden>";
        snprintf(desired_title, sizeof(desired_title), "%s", ssid_text);
    } else if(app->evil_twin_running && !app->evil_twin_full_console) {
        const EvilTwinStatus* status = app->evil_twin_status;
        const char* ssid = status ? status->status_ssid : NULL;
        if(!ssid || ssid[0] == '\0') {
            char selected_ssid[SCAN_SSID_MAX_LEN];
            if(simple_app_get_first_selected_ssid(app, selected_ssid, sizeof(selected_ssid))) {
                strncpy(desired_title, selected_ssid, sizeof(desired_title) - 1);
                desired_title[sizeof(desired_title) - 1] = '\0';
            } else {
                snprintf(desired_title, sizeof(desired_title), "--");
            }
        } else {
            snprintf(desired_title, sizeof(desired_title), "%s", ssid);
        }
    } else if(app->portal_running && !app->portal_full_console) {
        const PortalStatus* status = app->portal_status;
        const char* ssid = (status && status->status_ssid[0] != '\0') ? status->status_ssid :
                                                                        app->portal_ssid;
        if(!ssid || ssid[0] == '\0') {
            snprintf(desired_title, sizeof(desired_title), "--");
        } else {
            snprintf(desired_title, sizeof(desired_title), "%s", ssid);
        }
    } else {
        if(app->overlay_title_scroll_text[0] != '\0' || app->overlay_title_scroll_offset != 0) {
            simple_app_reset_overlay_title_scroll(app);
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
        return;
    }

    char desired_ascii[sizeof(desired_title)];
    simple_app_utf8_to_ascii_pl(desired_title, desired_ascii, sizeof(desired_ascii));
    strncpy(desired_title, desired_ascii, sizeof(desired_title) - 1);
    desired_title[sizeof(desired_title) - 1] = '\0';

    if(strncmp(
           desired_title,
           app->overlay_title_scroll_text,
           sizeof(app->overlay_title_scroll_text)) != 0) {
        strncpy(
            app->overlay_title_scroll_text,
            desired_title,
            sizeof(app->overlay_title_scroll_text) - 1);
        app->overlay_title_scroll_text[sizeof(app->overlay_title_scroll_text) - 1] = '\0';
        app->overlay_title_scroll_offset = 0;
        app->overlay_title_scroll_direction = 1;
        app->overlay_title_scroll_hold = RESULT_SCROLL_EDGE_PAUSE_STEPS;
        app->overlay_title_scroll_last_tick = furi_get_tick();
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    size_t char_limit = OVERLAY_TITLE_CHAR_LIMIT;
    if(char_limit >= sizeof(app->overlay_title_scroll_text)) {
        char_limit = sizeof(app->overlay_title_scroll_text) - 1;
    }
    if(char_limit == 0) char_limit = 1;

    size_t line_length = strlen(app->overlay_title_scroll_text);
    if(line_length <= char_limit) {
        if(app->overlay_title_scroll_offset != 0) {
            app->overlay_title_scroll_offset = 0;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
        return;
    }

    size_t max_offset = line_length - char_limit;
    uint32_t now = furi_get_tick();
    uint32_t interval_ticks = furi_ms_to_ticks(RESULT_SCROLL_INTERVAL_MS);
    if(interval_ticks == 0) interval_ticks = 1;
    if((now - app->overlay_title_scroll_last_tick) < interval_ticks) {
        return;
    }
    app->overlay_title_scroll_last_tick = now;

    if(app->overlay_title_scroll_hold > 0) {
        app->overlay_title_scroll_hold--;
        return;
    }

    bool changed = false;
    if(app->overlay_title_scroll_direction > 0) {
        if(app->overlay_title_scroll_offset < max_offset) {
            app->overlay_title_scroll_offset++;
            changed = true;
            if(app->overlay_title_scroll_offset >= max_offset) {
                app->overlay_title_scroll_direction = -1;
                app->overlay_title_scroll_hold = RESULT_SCROLL_EDGE_PAUSE_STEPS;
            }
        } else {
            app->overlay_title_scroll_direction = -1;
            app->overlay_title_scroll_hold = RESULT_SCROLL_EDGE_PAUSE_STEPS;
        }
    } else {
        if(app->overlay_title_scroll_offset > 0) {
            app->overlay_title_scroll_offset--;
            changed = true;
            if(app->overlay_title_scroll_offset == 0) {
                app->overlay_title_scroll_direction = 1;
                app->overlay_title_scroll_hold = RESULT_SCROLL_EDGE_PAUSE_STEPS;
            }
        } else {
            app->overlay_title_scroll_direction = 1;
            app->overlay_title_scroll_hold = RESULT_SCROLL_EDGE_PAUSE_STEPS;
        }
    }

    if(changed && app->viewport) {
        view_port_update(app->viewport);
    }
}

static void simple_app_update_sd_scroll(SimpleApp* app) {
    if(!app) return;

    bool sd_popup_active = app->sd_folder_popup_active || app->sd_file_popup_active ||
                           app->sd_delete_confirm_active;

    if(!sd_popup_active || app->sd_scroll_char_limit == 0 || app->sd_scroll_text[0] == '\0') {
        if(app->sd_scroll_text[0] != '\0' || app->sd_scroll_offset != 0) {
            simple_app_reset_sd_scroll(app);
        }
        return;
    }

    size_t len = strlen(app->sd_scroll_text);
    size_t char_limit = app->sd_scroll_char_limit;
    if(char_limit == 0) char_limit = 1;
    if(len <= char_limit) {
        if(app->sd_scroll_offset != 0) {
            simple_app_reset_sd_scroll(app);
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
        return;
    }

    size_t max_offset = len - char_limit;
    uint32_t now = furi_get_tick();
    uint32_t interval_ticks = furi_ms_to_ticks(RESULT_SCROLL_INTERVAL_MS);
    if(interval_ticks == 0) interval_ticks = 1;
    if((now - app->sd_scroll_last_tick) < interval_ticks) {
        return;
    }
    app->sd_scroll_last_tick = now;

    if(app->sd_scroll_hold > 0) {
        app->sd_scroll_hold--;
        return;
    }

    bool changed = false;
    if(app->sd_scroll_direction > 0) {
        if(app->sd_scroll_offset < max_offset) {
            app->sd_scroll_offset++;
            changed = true;
            if(app->sd_scroll_offset >= max_offset) {
                app->sd_scroll_direction = -1;
                app->sd_scroll_hold = RESULT_SCROLL_EDGE_PAUSE_STEPS;
            }
        } else {
            app->sd_scroll_direction = -1;
            app->sd_scroll_hold = RESULT_SCROLL_EDGE_PAUSE_STEPS;
        }
    } else {
        if(app->sd_scroll_offset > 0) {
            app->sd_scroll_offset--;
            changed = true;
            if(app->sd_scroll_offset == 0) {
                app->sd_scroll_direction = 1;
                app->sd_scroll_hold = RESULT_SCROLL_EDGE_PAUSE_STEPS;
            }
        } else {
            app->sd_scroll_direction = 1;
            app->sd_scroll_hold = RESULT_SCROLL_EDGE_PAUSE_STEPS;
        }
    }

    if(changed && app->viewport) {
        view_port_update(app->viewport);
    }
}

static void simple_app_update_deauth_blink(SimpleApp* app) {
    if(!app || !app->viewport) return;
    if(!app->deauth_view_active || app->deauth_full_console) return;
    if(!app->deauth_running || app->screen != ScreenSerial) return;

    uint32_t now = furi_get_tick();
    uint32_t interval = furi_ms_to_ticks(DEAUTH_RUNNING_BLINK_MS);
    if(interval == 0) interval = 1;
    if(app->deauth_blink_last_tick == 0) {
        app->deauth_blink_last_tick = now;
        return;
    }
    if(now - app->deauth_blink_last_tick >= interval) {
        app->deauth_blink_last_tick = now;
        view_port_update(app->viewport);
    }
}

static void simple_app_update_handshake_blink(SimpleApp* app) {
    if(!app || !app->viewport) return;
    if(!app->handshake_view_active || app->handshake_full_console) return;
    if(!app->handshake_running || app->screen != ScreenSerial) return;

    uint32_t now = furi_get_tick();
    uint32_t interval = furi_ms_to_ticks(DEAUTH_RUNNING_BLINK_MS);
    if(interval == 0) interval = 1;
    if(app->handshake_blink_last_tick == 0) {
        app->handshake_blink_last_tick = now;
        return;
    }
    if(now - app->handshake_blink_last_tick >= interval) {
        app->handshake_blink_last_tick = now;
        view_port_update(app->viewport);
    }
}

static void simple_app_scan_feed(SimpleApp* app, char ch) {
    if(!app || !app->scan_results_loading) return;

    if(ch == '\r') return;

    if(ch == '\n') {
        if(app->scan_line_len > 0) {
            app->scan_line_buffer[app->scan_line_len] = '\0';
            simple_app_process_scan_line(app, app->scan_line_buffer);
        }
        app->scan_line_len = 0;
        return;
    }

    if(app->scan_line_len + 1 >= sizeof(app->scan_line_buffer)) {
        app->scan_line_len = 0;
        return;
    }

    app->scan_line_buffer[app->scan_line_len++] = ch;
}

static void simple_app_update_selected_numbers(SimpleApp* app, const ScanResult* result) {
    if(!app || !result) return;
    if(result->selected) {
        for(size_t i = 0; i < app->scan_selected_count; i++) {
            if(app->scan_selected_numbers[i] == result->number) {
                return;
            }
        }
        if(app->scan_selected_count < app->scan_results_capacity) {
            app->scan_selected_numbers[app->scan_selected_count++] = result->number;
        }
    } else {
        for(size_t i = 0; i < app->scan_selected_count; i++) {
            if(app->scan_selected_numbers[i] == result->number) {
                for(size_t j = i; j + 1 < app->scan_selected_count; j++) {
                    app->scan_selected_numbers[j] = app->scan_selected_numbers[j + 1];
                }
                app->scan_selected_numbers[app->scan_selected_count - 1] = 0;
                if(app->scan_selected_count > 0) {
                    app->scan_selected_count--;
                }
                break;
            }
        }
    }
}

static void simple_app_toggle_scan_selection(SimpleApp* app, ScanResult* result) {
    if(!app || !result) return;
    result->selected = !result->selected;
    simple_app_update_selected_numbers(app, result);
    simple_app_reset_result_scroll(app);
}

static void simple_app_adjust_result_offset(SimpleApp* app) {
    if(!app) return;

    if(app->visible_result_count == 0) {
        app->scan_result_offset = 0;
        app->scan_result_index = 0;
        return;
    }

    if(app->scan_result_index >= app->visible_result_count) {
        app->scan_result_index = app->visible_result_count - 1;
    }

    if(app->scan_result_offset > app->scan_result_index) {
        app->scan_result_offset = app->scan_result_index;
    }

    while(app->scan_result_offset < app->visible_result_count) {
        size_t lines_used = 0;
        bool index_visible = false;
        size_t available_lines = (app->result_max_lines > 0) ? app->result_max_lines : 1;
        for(size_t i = app->scan_result_offset; i < app->visible_result_count; i++) {
            const ScanResult* result = simple_app_visible_result_const(app, i);
            if(!result) continue;
            uint8_t entry_lines = simple_app_result_line_count(app, result);
            if(entry_lines == 0) entry_lines = 1;
            if(lines_used + entry_lines > available_lines) break;
            lines_used += entry_lines;
            if(i == app->scan_result_index) {
                index_visible = true;
                break;
            }
        }
        if(index_visible) break;
        if(app->scan_result_offset >= app->scan_result_index) {
            break;
        }
        app->scan_result_offset++;
    }

    if(app->scan_result_offset >= app->visible_result_count) {
        app->scan_result_offset = (app->visible_result_count > 0) ? app->visible_result_count - 1 :
                                                                    0;
    }
}

static size_t simple_app_trim_oldest_line(SimpleApp* app) {
    if(!app || app->serial_len == 0) return 0;
    size_t drop = 0;
    size_t removed_lines = 0;

    while(drop < app->serial_len && app->serial_buffer[drop] != '\n') {
        drop++;
    }

    if(drop < app->serial_len) {
        drop++;
        removed_lines = 1;
    } else if(app->serial_len > 0) {
        removed_lines = 1;
    } else {
        drop = app->serial_len;
    }

    if(drop > 0) {
        memmove(app->serial_buffer, app->serial_buffer + drop, app->serial_len - drop);
        app->serial_len -= drop;
        app->serial_buffer[app->serial_len] = '\0';
    }

    return removed_lines;
}

static size_t simple_app_count_display_lines(const char* buffer, size_t length) {
    size_t lines = 0;
    size_t col = 0;

    for(size_t i = 0; i < length; i++) {
        char ch = buffer[i];
        if(ch == '\r') continue;
        if(ch == '\n') {
            lines++;
            col = 0;
            continue;
        }
        if(col >= SERIAL_LINE_CHAR_LIMIT) {
            lines++;
            col = 0;
        }
        col++;
    }

    if(col > 0) {
        lines++;
    }

    return lines;
}

static size_t simple_app_total_display_lines(SimpleApp* app) {
    if(!app->serial_mutex) return 0;
    furi_mutex_acquire(app->serial_mutex, FuriWaitForever);
    size_t total = simple_app_count_display_lines(app->serial_buffer, app->serial_len);
    furi_mutex_release(app->serial_mutex);
    return total;
}

static size_t simple_app_visible_serial_lines(const SimpleApp* app) {
    if(!app) return SERIAL_VISIBLE_LINES;
    if(app->screen == ScreenConsole) {
        return CONSOLE_VISIBLE_LINES;
    }
    if(app->screen == ScreenSerial) {
        if(app->bt_scan_view_active && !app->bt_scan_full_console) {
            return BT_SCAN_CONSOLE_LINES;
        }
        if(app->sniffer_results_active && !app->sniffer_full_console) {
            return SNIFFER_CONSOLE_LINES;
        }
        if(app->sniffer_view_active && !app->sniffer_full_console) {
            return SNIFFER_CONSOLE_LINES;
        }
        if(app->probe_results_active && !app->probe_full_console) {
            return SNIFFER_CONSOLE_LINES;
        }
        if(app->wardrive_view_active) {
            return WARD_DRIVE_CONSOLE_LINES;
        }
    }
    return SERIAL_VISIBLE_LINES;
}

static size_t simple_app_max_scroll(SimpleApp* app) {
    size_t total = simple_app_total_display_lines(app);
    size_t visible_lines = simple_app_visible_serial_lines(app);
    if(total <= visible_lines) return 0;
    return total - visible_lines;
}

static void simple_app_update_scroll(SimpleApp* app) {
    if(!app) return;
    size_t max_scroll = simple_app_max_scroll(app);
    if(app->serial_follow_tail) {
        app->serial_scroll = max_scroll;
    } else if(app->serial_scroll > max_scroll) {
        app->serial_scroll = max_scroll;
    }
    if(max_scroll == 0) {
        app->serial_follow_tail = true;
    }
}

static void simple_app_reset_serial_log(SimpleApp* app, const char* status) {
    if(!app || !app->serial_mutex) return;
    furi_mutex_acquire(app->serial_mutex, FuriWaitForever);
    int written = snprintf(
        app->serial_buffer,
        SERIAL_BUFFER_SIZE,
        "=== UART TERMINAL ===\n115200 baud\nStatus: %s\n\n",
        status ? status : "READY");
    if(written < 0) {
        written = 0;
    } else if(written >= (int)SERIAL_BUFFER_SIZE) {
        written = SERIAL_BUFFER_SIZE - 1;
    }
    app->serial_len = (size_t)written;
    app->serial_buffer[app->serial_len] = '\0';
    furi_mutex_release(app->serial_mutex);
    app->serial_scroll = 0;
    app->serial_follow_tail = true;
    app->serial_targets_hint = false;
    simple_app_update_scroll(app);
}

static void simple_app_append_serial_data(SimpleApp* app, const uint8_t* data, size_t length) {
    if(!app || !data || length == 0 || !app->serial_mutex) return;

    bool trimmed_any = false;

    furi_mutex_acquire(app->serial_mutex, FuriWaitForever);
    for(size_t i = 0; i < length; i++) {
        while(app->serial_len >= SERIAL_BUFFER_SIZE - 1) {
            trimmed_any = simple_app_trim_oldest_line(app) > 0 || trimmed_any;
        }
        app->serial_buffer[app->serial_len++] = (char)data[i];
    }
    app->serial_buffer[app->serial_len] = '\0';

    if(!app->serial_targets_hint && !app->blackout_view_active && !app->deauth_guard_view_active &&
       !app->deauth_view_active && !app->handshake_view_active && !app->sae_view_active) {
        static const char hint_phrase[] = "Scan results printed.";
        size_t phrase_len = strlen(hint_phrase);
        if(app->serial_len >= phrase_len) {
            size_t search_window = phrase_len + 64;
            if(search_window > app->serial_len) {
                search_window = app->serial_len;
            }
            const char* start = app->serial_buffer + (app->serial_len - search_window);
            if(strstr(start, hint_phrase) != NULL) {
                app->serial_targets_hint = true;
            }
        }
    }

    furi_mutex_release(app->serial_mutex);

    for(size_t i = 0; i < length; i++) {
        char ch = (char)data[i];
        simple_app_scan_feed(app, ch);
        simple_app_package_monitor_feed(app, ch);
        simple_app_channel_view_feed(app, ch);
        simple_app_portal_ssid_feed(app, ch);
        simple_app_portal_status_feed(app, ch);
        simple_app_evil_twin_status_feed(app, ch);
        simple_app_evil_twin_pass_feed(app, ch);
        simple_app_passwords_feed(app, ch);
        simple_app_sd_feed(app, ch);
        simple_app_evil_twin_feed(app, ch);
        simple_app_karma_probe_feed(app, ch);
        simple_app_karma_html_feed(app, ch);
        simple_app_led_feed(app, ch);
        simple_app_boot_feed(app, ch);
        simple_app_vendor_feed(app, ch);
        simple_app_scanner_timing_feed(app, ch);
        simple_app_deauth_guard_feed(app, ch);
        simple_app_deauth_feed(app, ch);
        simple_app_handshake_feed(app, ch);
        simple_app_sae_feed(app, ch);
        simple_app_gps_feed(app, ch);
        if(app->blackout_view_active) {
            simple_app_blackout_feed(app, ch);
        }
        if(app->sniffer_dog_view_active) {
            simple_app_sniffer_dog_feed(app, ch);
        }
        simple_app_bt_scan_feed(app, ch);
        simple_app_bt_locator_feed(app, ch);
        simple_app_wardrive_feed(app, ch);
        simple_app_sniffer_feed(app, ch);
        simple_app_probe_feed(app, ch);
    }

    if(trimmed_any && !app->serial_follow_tail) {
        size_t max_scroll = simple_app_max_scroll(app);
        if(app->serial_scroll > max_scroll) {
            app->serial_scroll = max_scroll;
        }
    }

    simple_app_update_scroll(app);
}

static bool simple_app_is_start_sniffer_command(const char* command) {
    if(!command) return false;
    const char* cursor = command;
    const char* needle = "start_sniffer";
    size_t needle_len = strlen(needle);
    while(true) {
        const char* found = strstr(cursor, needle);
        if(!found) return false;
        char before = (found == command) ? ' ' : *(found - 1);
        char after = *(found + needle_len);
        bool before_ok = (before == ' ' || before == '\n' || before == '\t');
        bool after_ok = (after == '\0' || after == ' ' || after == '\n' || after == '\t');
        if(before_ok && after_ok) return true;
        cursor = found + needle_len;
    }
}

static bool simple_app_is_start_blackout_command(const char* command) {
    if(!command) return false;
    const char* cursor = command;
    const char* needle = "start_blackout";
    size_t needle_len = strlen(needle);
    while(true) {
        const char* found = strstr(cursor, needle);
        if(!found) return false;
        char before = (found == command) ? ' ' : *(found - 1);
        char after = *(found + needle_len);
        bool before_ok = (before == ' ' || before == '\n' || before == '\t');
        bool after_ok = (after == '\0' || after == ' ' || after == '\n' || after == '\t');
        if(before_ok && after_ok) return true;
        cursor = found + needle_len;
    }
}

static bool simple_app_is_start_sniffer_noscan_command(const char* command) {
    if(!command) return false;
    const char* cursor = command;
    const char* needle = "start_sniffer_noscan";
    size_t needle_len = strlen(needle);
    while(true) {
        const char* found = strstr(cursor, needle);
        if(!found) return false;
        char before = (found == command) ? ' ' : *(found - 1);
        char after = *(found + needle_len);
        bool before_ok = (before == ' ' || before == '\n' || before == '\t');
        bool after_ok = (after == '\0' || after == ' ' || after == '\n' || after == '\t');
        if(before_ok && after_ok) return true;
        cursor = found + needle_len;
    }
}

static bool simple_app_is_start_sniffer_dog_command(const char* command) {
    if(!command) return false;
    const char* cursor = command;
    const char* needle = "start_sniffer_dog";
    size_t needle_len = strlen(needle);
    while(true) {
        const char* found = strstr(cursor, needle);
        if(!found) return false;
        char before = (found == command) ? ' ' : *(found - 1);
        char after = *(found + needle_len);
        bool before_ok = (before == ' ' || before == '\n' || before == '\t');
        bool after_ok = (after == '\0' || after == ' ' || after == '\n' || after == '\t');
        if(before_ok && after_ok) return true;
        cursor = found + needle_len;
    }
}

static bool simple_app_is_start_deauth_command(const char* command) {
    if(!command) return false;
    const char* cursor = command;
    const char* needle = "start_deauth";
    size_t needle_len = strlen(needle);
    while(true) {
        const char* found = strstr(cursor, needle);
        if(!found) return false;
        char before = (found == command) ? ' ' : *(found - 1);
        char after = *(found + needle_len);
        bool before_ok = (before == ' ' || before == '\n' || before == '\t');
        bool after_ok = (after == '\0' || after == ' ' || after == '\n' || after == '\t');
        if(before_ok && after_ok) return true;
        cursor = found + needle_len;
    }
}

static bool simple_app_is_start_handshake_command(const char* command) {
    if(!command) return false;
    const char* cursor = command;
    const char* needle = "start_handshake";
    size_t needle_len = strlen(needle);
    while(true) {
        const char* found = strstr(cursor, needle);
        if(!found) return false;
        char before = (found == command) ? ' ' : *(found - 1);
        char after = *(found + needle_len);
        bool before_ok = (before == ' ' || before == '\n' || before == '\t');
        bool after_ok = (after == '\0' || after == ' ' || after == '\n' || after == '\t');
        if(before_ok && after_ok) return true;
        cursor = found + needle_len;
    }
}

static bool simple_app_is_start_sae_command(const char* command) {
    if(!command) return false;
    const char* cursor = command;
    const char* needle = "sae_overflow";
    size_t needle_len = strlen(needle);
    while(true) {
        const char* found = strstr(cursor, needle);
        if(!found) return false;
        char before = (found == command) ? ' ' : *(found - 1);
        char after = *(found + needle_len);
        bool before_ok = (before == ' ' || before == '\n' || before == '\t');
        bool after_ok = (after == '\0' || after == ' ' || after == '\n' || after == '\t');
        if(before_ok && after_ok) return true;
        cursor = found + needle_len;
    }
}

static void simple_app_send_start_sniffer(SimpleApp* app) {
    if(!app) return;
    if(app->scan_selected_count == 0) {
        simple_app_send_command(app, "start_sniffer", true);
        return;
    }

    char select_command[160];
    size_t written = snprintf(select_command, sizeof(select_command), "select_networks");
    if(written >= sizeof(select_command)) written = sizeof(select_command) - 1;

    for(size_t i = 0; i < app->scan_selected_count && written < sizeof(select_command) - 1; i++) {
        int added = snprintf(
            select_command + written,
            sizeof(select_command) - written,
            " %u",
            (unsigned)app->scan_selected_numbers[i]);
        if(added < 0) break;
        written += (size_t)added;
        if(written >= sizeof(select_command)) {
            written = sizeof(select_command) - 1;
            select_command[written] = '\0';
            break;
        }
    }

    char combined_command[256];
    int combined_written = snprintf(
        combined_command, sizeof(combined_command), "%s\n%s", select_command, "start_sniffer");
    if(combined_written < 0) {
        simple_app_send_command(app, "start_sniffer", true);
    } else {
        if((size_t)combined_written >= sizeof(combined_command)) {
            combined_command[sizeof(combined_command) - 1] = '\0';
        }
        simple_app_send_command(app, combined_command, true);
    }
}

static void simple_app_send_resume_sniffer(SimpleApp* app) {
    if(!app) return;
    if(app->scan_selected_count == 0) {
        simple_app_send_command(app, "start_sniffer_noscan", true);
        return;
    }

    char select_command[160];
    size_t written = snprintf(select_command, sizeof(select_command), "select_networks");
    if(written >= sizeof(select_command)) written = sizeof(select_command) - 1;

    for(size_t i = 0; i < app->scan_selected_count && written < sizeof(select_command) - 1; i++) {
        int added = snprintf(
            select_command + written,
            sizeof(select_command) - written,
            " %u",
            (unsigned)app->scan_selected_numbers[i]);
        if(added < 0) break;
        written += (size_t)added;
        if(written >= sizeof(select_command)) {
            written = sizeof(select_command) - 1;
            select_command[written] = '\0';
            break;
        }
    }

    char combined_command[256];
    int combined_written = snprintf(
        combined_command,
        sizeof(combined_command),
        "%s\n%s",
        select_command,
        "start_sniffer_noscan");
    if(combined_written < 0) {
        simple_app_send_command(app, "start_sniffer_noscan", true);
    } else {
        if((size_t)combined_written >= sizeof(combined_command)) {
            combined_command[sizeof(combined_command) - 1] = '\0';
        }
        simple_app_send_command(app, combined_command, true);
    }
}

static void simple_app_send_command(SimpleApp* app, const char* command, bool go_to_serial) {
    if(!app || !command || command[0] == '\0') return;

    bool is_wardrive_command = strstr(command, "start_wardrive") != NULL;
    bool is_stop_command = strcmp(command, "stop") == 0;
    bool is_start_evil_twin = strstr(command, "start_evil_twin") != NULL;
    bool is_start_portal = strstr(command, "start_portal") != NULL;
    bool is_start_sniffer = simple_app_is_start_sniffer_command(command);
    bool is_start_sniffer_noscan = simple_app_is_start_sniffer_noscan_command(command);
    bool is_start_sniffer_dog = simple_app_is_start_sniffer_dog_command(command);
    bool is_start_blackout = simple_app_is_start_blackout_command(command);
    bool is_start_deauth = simple_app_is_start_deauth_command(command);
    bool is_start_handshake = simple_app_is_start_handshake_command(command);
    bool is_start_sae = simple_app_is_start_sae_command(command);
    bool is_sniffer_results_command = strstr(command, "show_sniffer_results") != NULL;
    bool is_probe_results_command = strstr(command, "show_probes") != NULL;
    bool is_airtag_scan_command = strstr(command, "scan_airtag") != NULL;
    bool is_bt_scan_command = is_airtag_scan_command || strstr(command, "scan_bt") != NULL;
    bool is_wifi_scan_command = strstr(command, SCANNER_SCAN_COMMAND) != NULL;
    bool is_deauth_guard_command = strstr(command, "deauth_detector") != NULL;
    bool is_start_gps_raw = strstr(command, "start_gps_raw") != NULL;
    if(is_wifi_scan_command) {
        simple_app_prepare_scan_buffers(app);
        simple_app_scan_free_buffers(app);
        if(!simple_app_scan_alloc_buffers(app, SCAN_RESULTS_DEFAULT_CAPACITY)) {
            app->scan_results_loading = false;
            app->scanner_view_active = false;
            app->scanner_full_console = false;
            app->scanner_scan_running = false;
            simple_app_show_status_message(app, "OOM: scan buffers", 2000, true);
            if(app->viewport) view_port_update(app->viewport);
            return;
        }
        simple_app_reset_scan_results(app);
        app->scan_results_loading = true;
        app->scanner_view_active = true;
        app->scanner_full_console = false;
        app->scanner_scan_running = true;
        app->scanner_last_update_tick = 0;
    } else if(app->scanner_view_active) {
        app->scanner_view_active = false;
        app->scanner_full_console = false;
        app->scanner_scan_running = false;
    }
    if(is_wardrive_command) {
        app->wardrive_view_active = true;
        if(!simple_app_wardrive_state_ensure(app)) {
            simple_app_show_status_message(app, "OOM: wardrive", 1500, true);
            app->wardrive_view_active = false;
            return;
        }
        simple_app_reset_wardrive_status(app);
    } else if(app->wardrive_view_active) {
        app->wardrive_view_active = false;
        simple_app_reset_wardrive_status(app);
    }
    if(is_start_gps_raw) {
        app->gps_view_active = true;
        if(!simple_app_gps_state_ensure(app)) {
            simple_app_show_status_message(app, "OOM: GPS", 1500, true);
            app->gps_view_active = false;
            return;
        }
        simple_app_reset_gps_status(app);
    } else if(is_stop_command) {
        app->gps_view_active = false;
        simple_app_reset_gps_status(app);
    }
    if(is_bt_scan_command) {
        if(!simple_app_alloc_bt_buffers(app)) {
            simple_app_show_status_message(app, "OOM: BT buffers", 1500, true);
            return;
        }
        app->bt_scan_view_active = true;
        simple_app_reset_bt_scan_summary(app);
        app->airtag_scan_mode = is_airtag_scan_command;
        app->bt_scan_running = true;
        app->bt_scan_has_data = false;
        app->bt_scan_last_update_tick = 0;
        app->bt_scan_show_list = false;
        if(!is_airtag_scan_command) {
            const char* mac_ptr = strstr(command, "scan_bt");
            if(mac_ptr) {
                mac_ptr += strlen("scan_bt");
                while(*mac_ptr == ' ')
                    mac_ptr++;
                if(strchr(mac_ptr, ':')) {
                    strncpy(app->bt_locator_mac, mac_ptr, sizeof(app->bt_locator_mac));
                    app->bt_locator_mac[sizeof(app->bt_locator_mac) - 1] = '\0';
                    app->bt_locator_mode = true;
                    app->bt_locator_has_rssi = false;
                    app->bt_locator_start_tick = furi_get_tick();
                    app->bt_locator_last_console_tick = app->bt_locator_start_tick;
                }
            }
        }
    } else if(app->bt_scan_view_active) {
        app->bt_scan_view_active = false;
        simple_app_reset_bt_scan_summary(app);
        simple_app_free_bt_buffers(app);
    }
    if(is_deauth_guard_command) {
        simple_app_start_deauth_guard(app);
    } else if(is_stop_command || app->deauth_guard_view_active) {
        simple_app_reset_deauth_guard(app);
    }
    if(is_start_sniffer || is_start_sniffer_noscan) {
        if(!simple_app_alloc_sniffer_buffers(app)) {
            simple_app_show_status_message(app, "OOM: sniffer\nbuffers", 1500, true);
            return;
        }
        simple_app_reset_sniffer_status(app);
        app->sniffer_view_active = true;
        app->sniffer_running = true;
        app->sniffer_last_update_tick = furi_get_tick();
        app->sniffer_results_active = false;
        simple_app_reset_sniffer_results(app);
        app->probe_results_active = false;
        simple_app_reset_probe_results(app);
    } else if(is_sniffer_results_command) {
        if(!simple_app_alloc_sniffer_buffers(app)) {
            simple_app_show_status_message(app, "OOM: sniffer\nbuffers", 1500, true);
            return;
        }
        simple_app_reset_sniffer_results(app);
        app->sniffer_results_active = true;
        app->sniffer_results_loading = true;
        app->sniffer_full_console = false;
        app->probe_results_active = false;
        simple_app_reset_probe_results(app);
    } else if(is_probe_results_command) {
        if(!simple_app_alloc_probe_buffers(app)) {
            simple_app_show_status_message(app, "OOM: probe\nbuffers", 1500, true);
            return;
        }
        simple_app_reset_probe_results(app);
        app->probe_results_active = true;
        app->probe_results_loading = true;
        app->probe_full_console = false;
        app->sniffer_results_active = false;
    } else if(is_stop_command) {
        app->sniffer_view_active = false;
        app->sniffer_results_active = false;
        app->probe_results_active = false;
        simple_app_reset_sniffer_status(app);
        simple_app_reset_sniffer_results(app);
        simple_app_reset_probe_results(app);
    } else {
        if(!is_sniffer_results_command) {
            app->sniffer_results_active = false;
            simple_app_reset_sniffer_results(app);
        }
        if(!is_probe_results_command) {
            app->probe_results_active = false;
            simple_app_reset_probe_results(app);
        }
    }

    app->serial_targets_hint = false;
    if(is_start_handshake) {
        app->handshake_view_active = true;
        app->handshake_full_console = false;
        simple_app_reset_handshake_status(app);
        app->handshake_view_active = true;
        app->handshake_running = true;
        app->handshake_overlay_allowed = true;
        app->handshake_vibro_done = false;
        simple_app_handshake_set_note(app, "Starting");
        if(app->scan_results || app->scan_selected_numbers || app->visible_result_indices) {
            if(app->scan_selected_count > 0 && app->scan_selected_numbers && app->scan_results) {
                simple_app_compact_scan_results(app);
            } else {
                simple_app_scan_free_buffers(app);
                simple_app_reset_scan_results(app);
            }
        }
    } else if(is_stop_command) {
        simple_app_reset_handshake_status(app);
    } else {
        simple_app_reset_handshake_status(app);
    }
    if(is_start_sae) {
        app->sae_view_active = true;
        app->sae_full_console = false;
        simple_app_reset_sae_status(app);
        app->sae_view_active = true;
        app->sae_running = true;
        app->sae_overlay_allowed = true;
        simple_app_sae_set_note(app, "Running");
    } else if(is_stop_command) {
        simple_app_reset_sae_status(app);
    } else {
        simple_app_reset_sae_status(app);
    }
    if(is_start_deauth) {
        app->deauth_view_active = true;
        app->deauth_full_console = false;
        simple_app_reset_deauth_status(app);
        app->deauth_view_active = true;
        simple_app_deauth_set_note(app, "Starting");
        app->deauth_list_offset = 0;
        app->deauth_list_scroll_x = 0;
        app->deauth_overlay_allowed = true;
    } else if(is_stop_command) {
        app->deauth_view_active = false;
        simple_app_reset_deauth_status(app);
    } else {
        app->deauth_view_active = false;
        simple_app_reset_deauth_status(app);
    }
    if(is_start_blackout) {
        app->blackout_view_active = true;
        app->blackout_full_console = false;
        simple_app_reset_blackout_status(app);
        if(app->blackout_cycle == 0) app->blackout_cycle = 1;
        simple_app_blackout_set_phase(app, BlackoutPhaseStarting, "");
    } else if(is_stop_command) {
        app->blackout_view_active = false;
        simple_app_reset_blackout_status(app);
    } else {
        app->blackout_view_active = false;
        simple_app_reset_blackout_status(app);
    }
    if(is_start_sniffer_dog) {
        app->sniffer_dog_view_active = true;
        app->sniffer_dog_full_console = false;
        simple_app_reset_sniffer_dog_status(app);
        simple_app_sniffer_dog_set_phase(app, SnifferDogPhaseStarting, "");
    } else if(is_stop_command) {
        app->sniffer_dog_view_active = false;
        simple_app_reset_sniffer_dog_status(app);
    } else {
        app->sniffer_dog_view_active = false;
        simple_app_reset_sniffer_dog_status(app);
    }
    if(is_start_portal) {
        if(!simple_app_portal_status_ensure(app)) {
            simple_app_show_status_message(app, "OOM: portal status", 1500, true);
        }
        app->portal_running = true;
        app->portal_full_console = false;
        simple_app_reset_portal_status(app);
    } else if(is_stop_command || go_to_serial) {
        app->portal_running = false;
        app->portal_full_console = false;
        simple_app_portal_status_free(app);
    }
    if(is_start_evil_twin) {
        if(!simple_app_evil_twin_status_ensure(app)) {
            simple_app_show_status_message(app, "OOM: evil status", 1500, true);
        }
        app->evil_twin_running = true;
        app->evil_twin_full_console = false;
        simple_app_reset_evil_twin_status(app);
        if(app->evil_twin_status) {
            strncpy(
                app->evil_twin_status->status_note,
                "Starting",
                sizeof(app->evil_twin_status->status_note) - 1);
            app->evil_twin_status->status_note[sizeof(app->evil_twin_status->status_note) - 1] =
                '\0';
            char ssid_hint[SCAN_SSID_MAX_LEN];
            if(simple_app_get_first_selected_ssid(app, ssid_hint, sizeof(ssid_hint))) {
                strncpy(
                    app->evil_twin_status->status_ssid,
                    ssid_hint,
                    sizeof(app->evil_twin_status->status_ssid) - 1);
                app->evil_twin_status->status_ssid[sizeof(app->evil_twin_status->status_ssid) - 1] =
                    '\0';
            }
        }
    } else if(is_stop_command || go_to_serial) {
        app->evil_twin_running = false;
        app->evil_twin_full_console = false;
        simple_app_evil_twin_status_free(app);
    }

    simple_app_log_cli_command(app, "SYS", command);

    char cmd[160];
    snprintf(cmd, sizeof(cmd), "%s\n", command);

    furi_hal_serial_tx(app->serial, (const uint8_t*)cmd, strlen(cmd));
    furi_hal_serial_tx_wait_complete(app->serial);

    furi_stream_buffer_reset(app->rx_stream);
    simple_app_reset_serial_log(app, "COMMAND SENT");

    char log_line[96];
    int log_len = snprintf(log_line, sizeof(log_line), "TX: %s\n", command);
    if(log_len > 0) {
        simple_app_append_serial_data(app, (const uint8_t*)log_line, (size_t)log_len);
    }

    app->last_command_sent = true;
    if(go_to_serial) {
        app->screen = ScreenSerial;
        app->serial_follow_tail = true;
        simple_app_update_scroll(app);
    }
}

static bool simple_app_command_requires_select_networks(const char* base_command) {
    if(!base_command || base_command[0] == '\0') return false;
    return (
        strcmp(base_command, "start_deauth") == 0 ||
        strcmp(base_command, "start_evil_twin") == 0 || strcmp(base_command, "sae_overflow") == 0);
}

static void simple_app_send_command_with_targets(SimpleApp* app, const char* base_command) {
    if(!app || !base_command || base_command[0] == '\0') return;
    bool requires_select_networks = simple_app_command_requires_select_networks(base_command);
    if(app->scan_selected_count == 0 && requires_select_networks) {
        simple_app_show_status_message(app, "Please select\nnetwork first", 1500, true);
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(requires_select_networks) {
        char select_command[160];
        size_t written = snprintf(select_command, sizeof(select_command), "select_networks");
        if(written >= sizeof(select_command)) {
            select_command[sizeof(select_command) - 1] = '\0';
            written = strlen(select_command);
        }

        for(size_t i = 0; i < app->scan_selected_count && written < sizeof(select_command) - 1;
            i++) {
            int added = snprintf(
                select_command + written,
                sizeof(select_command) - written,
                " %u",
                (unsigned)app->scan_selected_numbers[i]);
            if(added < 0) {
                break;
            }
            written += (size_t)added;
            if(written >= sizeof(select_command)) {
                written = sizeof(select_command) - 1;
                select_command[written] = '\0';
                break;
            }
        }

        char combined_command[256];
        int combined_written = snprintf(
            combined_command, sizeof(combined_command), "%s\n%s", select_command, base_command);
        if(combined_written < 0) {
            simple_app_send_command(app, base_command, true);
        } else {
            if((size_t)combined_written >= sizeof(combined_command)) {
                combined_command[sizeof(combined_command) - 1] = '\0';
            }
            simple_app_send_command(app, combined_command, true);
        }
        return;
    }

    char command[96];
    size_t written = snprintf(command, sizeof(command), "%s", base_command);
    if(written >= sizeof(command)) {
        command[sizeof(command) - 1] = '\0';
        written = strlen(command);
    }

    for(size_t i = 0; i < app->scan_selected_count && written < sizeof(command) - 1; i++) {
        int added = snprintf(
            command + written,
            sizeof(command) - written,
            " %u",
            (unsigned)app->scan_selected_numbers[i]);
        if(added < 0) {
            break;
        }
        written += (size_t)added;
        if(written >= sizeof(command)) {
            written = sizeof(command) - 1;
            command[written] = '\0';
            break;
        }
    }

    simple_app_send_command(app, command, true);
}

static void simple_app_send_stop_if_needed(SimpleApp* app) {
    if(!app || !app->last_command_sent) return;
    simple_app_send_command(app, "stop", false);
    app->last_command_sent = false;
    app->blackout_view_active = false;
    simple_app_reset_blackout_status(app);
    app->deauth_view_active = false;
    simple_app_reset_deauth_status(app);
    simple_app_reset_handshake_status(app);
    simple_app_reset_sae_status(app);
    app->sniffer_dog_view_active = false;
    simple_app_reset_sniffer_dog_status(app);
    app->bt_scan_running = false;
    app->scanner_scan_running = false;
    app->bt_scan_show_list = false;
    simple_app_free_bt_buffers(app);
    simple_app_free_package_monitor_buffers(app);
    simple_app_free_channel_view_buffers(app);
    app->sniffer_view_active = false;
    app->sniffer_results_active = false;
    app->probe_results_active = false;
    simple_app_reset_sniffer_status(app);
    simple_app_reset_sniffer_results(app);
    simple_app_reset_probe_results(app);
    app->gps_view_active = false;
    simple_app_reset_gps_status(app);
}

static void simple_app_send_stop_preserve_results(SimpleApp* app) {
    if(!app) return;
    simple_app_send_command_quiet(app, "stop");
    app->last_command_sent = false;
    app->sniffer_running = false;
    app->sniffer_view_active = false;
    app->blackout_view_active = false;
    simple_app_reset_blackout_status(app);
    app->deauth_view_active = false;
    simple_app_reset_deauth_status(app);
    simple_app_reset_handshake_status(app);
    simple_app_reset_sae_status(app);
    app->sniffer_dog_view_active = false;
    simple_app_reset_sniffer_dog_status(app);
    app->bt_scan_running = false;
    app->scanner_scan_running = false;
    simple_app_free_bt_buffers(app);
    simple_app_free_package_monitor_buffers(app);
    simple_app_free_channel_view_buffers(app);
}

static void simple_app_prepare_exit(SimpleApp* app) {
    if(!app) return;

    simple_app_send_stop_if_needed(app);
    simple_app_send_command_quiet(app, "unselect_networks");
    simple_app_send_command_quiet(app, "unselect_stations");
    simple_app_send_command_quiet(app, "clear_sniffer_results");
    simple_app_send_command_quiet(app, "reboot");
    simple_app_close_evil_twin_popup(app);
    simple_app_reset_evil_twin_listing(app);
    simple_app_reset_evil_twin_pass_listing(app);
    simple_app_reset_portal_ssid_listing(app);
    simple_app_reset_sd_listing(app);
    simple_app_reset_karma_probe_listing(app);
    app->karma_probe_count = 0;
    simple_app_karma_free_probes(app);
    simple_app_reset_karma_html_listing(app);
    simple_app_reset_scan_results(app);
    simple_app_scan_free_buffers(app);
    simple_app_reset_deauth_guard(app);
    app->deauth_view_active = false;
    simple_app_reset_deauth_status(app);
    simple_app_reset_handshake_status(app);
    simple_app_reset_sae_status(app);
    simple_app_free_evil_twin_qr_buffers(app);
    simple_app_free_help_qr_buffers(app);
    simple_app_free_bt_buffers(app);
    simple_app_free_package_monitor_buffers(app);
    simple_app_free_channel_view_buffers(app);
    simple_app_clear_status_message(app);

    app->sniffer_view_active = false;
    app->sniffer_results_active = false;
    app->probe_results_active = false;
    simple_app_reset_sniffer_status(app);
    simple_app_reset_sniffer_results(app);
    simple_app_reset_probe_results(app);
    simple_app_free_sniffer_buffers(app);
    simple_app_free_probe_buffers(app);
    app->portal_input_requested = false;
    app->portal_input_active = false;
    app->portal_ssid_popup_active = false;
    app->portal_running = false;
    app->portal_full_console = false;
    simple_app_reset_portal_status(app);
    simple_app_portal_status_free(app);
    simple_app_reset_passwords_listing(app);
    app->evil_twin_running = false;
    app->evil_twin_full_console = false;
    simple_app_reset_evil_twin_status(app);
    simple_app_evil_twin_status_free(app);
    app->wardrive_view_active = false;
    simple_app_reset_wardrive_status(app);
    simple_app_wardrive_state_free(app);
    app->gps_view_active = false;
    simple_app_reset_gps_status(app);
    simple_app_gps_state_free(app);
    app->karma_probe_popup_active = false;
    app->karma_html_popup_active = false;
    app->sd_folder_popup_active = false;
    app->sd_file_popup_active = false;
    app->sd_delete_confirm_active = false;
    app->evil_twin_popup_active = false;

    if(app->rx_stream) {
        furi_stream_buffer_reset(app->rx_stream);
    }
}

static void simple_app_request_scan_results(SimpleApp* app, const char* command) {
    if(!app) return;
    simple_app_scan_free_buffers(app);
    if(!simple_app_scan_alloc_buffers(app, SCAN_RESULTS_DEFAULT_CAPACITY)) {
        simple_app_show_status_message(app, "OOM: scan buffers", 2000, true);
        return;
    }
    simple_app_reset_scan_results(app);
    app->scan_results_loading = true;
    app->serial_targets_hint = false;
    if(command && command[0] != '\0') {
        simple_app_send_command(app, command, false);
    }
    app->screen = ScreenResults;
    view_port_update(app->viewport);
}

static void simple_app_console_enter(SimpleApp* app) {
    if(!app) return;
    app->screen = ScreenConsole;
    app->serial_follow_tail = true;
    simple_app_update_scroll(app);
    view_port_update(app->viewport);
}

static void simple_app_console_leave(SimpleApp* app) {
    if(!app) return;
    app->screen = ScreenMenu;
    app->menu_state = MenuStateItems;
    app->section_index = MENU_SECTION_SETUP;
    app->item_index = menu_sections[MENU_SECTION_SETUP].entry_count - 1;
    size_t visible_count = simple_app_menu_visible_count(app, MENU_SECTION_SETUP);
    if(visible_count == 0) visible_count = 1;
    if(menu_sections[MENU_SECTION_SETUP].entry_count > visible_count) {
        size_t max_offset = menu_sections[MENU_SECTION_SETUP].entry_count - visible_count;
        app->item_offset = max_offset;
    } else {
        app->item_offset = 0;
    }
    app->serial_follow_tail = true;
    simple_app_update_scroll(app);
    view_port_update(app->viewport);
}

static void simple_app_draw_console(SimpleApp* app, Canvas* canvas) {
    if(!app) return;
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    char display_lines[CONSOLE_VISIBLE_LINES][64];
    size_t lines_filled = simple_app_render_display_lines(
        app, app->serial_scroll, display_lines, CONSOLE_VISIBLE_LINES);
    uint8_t y = 8;
    for(size_t i = 0; i < CONSOLE_VISIBLE_LINES; i++) {
        const char* line = (i < lines_filled) ? display_lines[i] : "";
        if(lines_filled == 0 && i == 0) {
            canvas_draw_str(canvas, 2, y, "No UART data");
        } else {
            canvas_draw_str(canvas, 2, y, line[0] ? line : " ");
        }
        y += SERIAL_TEXT_LINE_HEIGHT;
    }

    size_t total_lines = simple_app_total_display_lines(app);
    if(total_lines > CONSOLE_VISIBLE_LINES) {
        size_t max_scroll = simple_app_max_scroll(app);
        bool show_up = (app->serial_scroll > 0);
        bool show_down = (app->serial_scroll < max_scroll);
        if(show_up || show_down) {
            uint8_t arrow_x = DISPLAY_WIDTH - 6;
            int16_t content_top = 8;
            int16_t visible_rows = (CONSOLE_VISIBLE_LINES > 0) ? (CONSOLE_VISIBLE_LINES - 1) : 0;
            int16_t content_bottom = 8 + (int16_t)(visible_rows * SERIAL_TEXT_LINE_HEIGHT);
            simple_app_draw_scroll_hints(
                canvas, arrow_x, content_top, content_bottom, show_up, show_down);
        }
    }

    if(simple_app_status_message_is_active(app) && !app->status_message_fullscreen) {
        canvas_draw_str(canvas, 2, 52, app->status_message);
    }

    canvas_draw_str(canvas, 2, 62, "Back=Exit  Up/Down scroll");
}

static void simple_app_draw_confirm_blackout(SimpleApp* app, Canvas* canvas) {
    if(!app) return;
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, DISPLAY_WIDTH / 2, 12, AlignCenter, AlignCenter, "Blackout will disconnect");
    canvas_draw_str_aligned(
        canvas, DISPLAY_WIDTH / 2, 24, AlignCenter, AlignCenter, "all clients on scanned APs");

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 38, AlignCenter, AlignCenter, "Confirm?");

    canvas_set_font(canvas, FontSecondary);
    const char* option_line = app->confirm_blackout_yes ? "No        > Yes" : "> No        Yes";
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 50, AlignCenter, AlignCenter, option_line);
}

static void simple_app_draw_confirm_sniffer_dos(SimpleApp* app, Canvas* canvas) {
    if(!app) return;
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, DISPLAY_WIDTH / 2, 12, AlignCenter, AlignCenter, "Sniffer Dog will flood");
    canvas_draw_str_aligned(
        canvas, DISPLAY_WIDTH / 2, 24, AlignCenter, AlignCenter, "clients found by sniffer");

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 38, AlignCenter, AlignCenter, "Confirm?");

    canvas_set_font(canvas, FontSecondary);
    const char* option_line = app->confirm_sniffer_dos_yes ? "No        > Yes" : "> No        Yes";
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 50, AlignCenter, AlignCenter, option_line);
}

static void simple_app_draw_confirm_exit(SimpleApp* app, Canvas* canvas) {
    if(!app) return;
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, DISPLAY_WIDTH / 2, 14, AlignCenter, AlignCenter, "Exit and clean up?");
    canvas_draw_str_aligned(
        canvas, DISPLAY_WIDTH / 2, 26, AlignCenter, AlignCenter, "Current tasks will stop");
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 42, AlignCenter, AlignCenter, "Confirm?");
    canvas_set_font(canvas, FontSecondary);
    const char* option_line = app->confirm_exit_yes ? "No        > Yes" : "> No        Yes";
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 54, AlignCenter, AlignCenter, option_line);
}

static void simple_app_handle_console_input(SimpleApp* app, InputKey key) {
    if(!app) return;
    if(key == InputKeyBack) {
        simple_app_send_stop_if_needed(app);
        simple_app_console_leave(app);
        return;
    }

    if(key == InputKeyUp) {
        if(app->serial_scroll > 0) {
            app->serial_scroll--;
            app->serial_follow_tail = false;
            view_port_update(app->viewport);
        }
        return;
    }

    if(key == InputKeyLeft) {
        size_t step = CONSOLE_VISIBLE_LINES;
        if(app->serial_scroll > 0) {
            if(app->serial_scroll > step) {
                app->serial_scroll -= step;
            } else {
                app->serial_scroll = 0;
            }
            app->serial_follow_tail = false;
        }
        view_port_update(app->viewport);
        return;
    } else if(key == InputKeyRight) {
        size_t step = CONSOLE_VISIBLE_LINES;
        size_t max_scroll = simple_app_max_scroll(app);
        if(app->serial_scroll < max_scroll) {
            app->serial_scroll =
                (app->serial_scroll + step < max_scroll) ? app->serial_scroll + step : max_scroll;
            app->serial_follow_tail = (app->serial_scroll == max_scroll);
        } else {
            app->serial_follow_tail = true;
            simple_app_update_scroll(app);
        }
        view_port_update(app->viewport);
        return;
    } else if(key == InputKeyDown) {
        size_t max_scroll = simple_app_max_scroll(app);
        if(app->serial_scroll < max_scroll) {
            app->serial_scroll++;
            app->serial_follow_tail = (app->serial_scroll == max_scroll);
            view_port_update(app->viewport);
        } else {
            app->serial_follow_tail = true;
            simple_app_update_scroll(app);
            view_port_update(app->viewport);
        }
        return;
    } else if(key == InputKeyOk) {
        app->serial_follow_tail = true;
        simple_app_update_scroll(app);
        view_port_update(app->viewport);
    }
}

static void simple_app_handle_gps_input(SimpleApp* app, InputKey key) {
    if(!app) return;
    if(key == InputKeyOk) {
        app->gps_time_24h = !app->gps_time_24h;
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }
    if(key == InputKeyUp || key == InputKeyDown) {
        app->gps_dst_enabled = !app->gps_dst_enabled;
        simple_app_mark_config_dirty(app);
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }
    if(key == InputKeyLeft || key == InputKeyRight) {
        int delta = (key == InputKeyRight) ? 60 : -60;
        int16_t updated = simple_app_clamp_gps_utc_offset(app->gps_utc_offset_minutes + delta);
        if(updated != app->gps_utc_offset_minutes) {
            app->gps_utc_offset_minutes = updated;
            simple_app_mark_config_dirty(app);
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
        return;
    }
    if(key == InputKeyBack) {
        simple_app_send_stop_if_needed(app);
        app->screen = ScreenMenu;
        app->gps_view_active = false;
        app->gps_console_mode = false;
        simple_app_reset_gps_status(app);
        if(app->viewport) {
            view_port_update(app->viewport);
        }
    }
}

static void simple_app_handle_confirm_exit_input(SimpleApp* app, InputKey key) {
    if(!app) return;
    if(key == InputKeyBack) {
        app->confirm_exit_yes = false;
        app->screen = ScreenMenu;
        app->menu_state = MenuStateSections;
        view_port_update(app->viewport);
        return;
    }

    if(key == InputKeyLeft || key == InputKeyRight) {
        app->confirm_exit_yes = !app->confirm_exit_yes;
        view_port_update(app->viewport);
        return;
    }

    if(key == InputKeyOk) {
        if(app->confirm_exit_yes) {
            simple_app_prepare_exit(app);
            app->exit_app = true;
        } else {
            app->confirm_exit_yes = false;
            app->screen = ScreenMenu;
            app->menu_state = MenuStateSections;
            view_port_update(app->viewport);
        }
    }
}

static void simple_app_handle_confirm_blackout_input(SimpleApp* app, InputKey key) {
    if(!app) return;
    if(key == InputKeyBack) {
        simple_app_send_stop_if_needed(app);
        app->confirm_blackout_yes = false;
        simple_app_focus_attacks_menu(app);
        view_port_update(app->viewport);
        return;
    }

    if(key == InputKeyLeft || key == InputKeyRight) {
        app->confirm_blackout_yes = !app->confirm_blackout_yes;
        view_port_update(app->viewport);
        return;
    }

    if(key == InputKeyOk) {
        if(app->confirm_blackout_yes) {
            app->confirm_blackout_yes = false;
            app->blackout_view_active = true;
            simple_app_reset_blackout_status(app);
            if(app->blackout_cycle == 0) app->blackout_cycle = 1;
            simple_app_blackout_set_phase(app, BlackoutPhaseStarting, "");
            app->serial_targets_hint = false;
            simple_app_send_command(app, "start_blackout", true);
        } else {
            simple_app_focus_attacks_menu(app);
            view_port_update(app->viewport);
        }
    }
}

static void simple_app_handle_confirm_sniffer_dos_input(SimpleApp* app, InputKey key) {
    if(!app) return;
    if(key == InputKeyBack) {
        simple_app_send_stop_if_needed(app);
        app->confirm_sniffer_dos_yes = false;
        simple_app_focus_attacks_menu(app);
        view_port_update(app->viewport);
        return;
    }

    if(key == InputKeyLeft || key == InputKeyRight) {
        app->confirm_sniffer_dos_yes = !app->confirm_sniffer_dos_yes;
        view_port_update(app->viewport);
        return;
    }

    if(key == InputKeyOk) {
        if(app->confirm_sniffer_dos_yes) {
            app->confirm_sniffer_dos_yes = false;
            app->sniffer_dog_view_active = true;
            simple_app_reset_sniffer_dog_status(app);
            simple_app_sniffer_dog_set_phase(app, SnifferDogPhaseStarting, "");
            simple_app_send_command(app, "start_sniffer_dog", true);
        } else {
            simple_app_focus_attacks_menu(app);
            view_port_update(app->viewport);
        }
    }
}

static void simple_app_package_monitor_reset(SimpleApp* app) {
    if(!app) return;
    if(app->package_monitor_history) {
        memset(app->package_monitor_history, 0, sizeof(uint16_t) * PACKAGE_MONITOR_MAX_HISTORY);
    }
    app->package_monitor_history_count = 0;
    app->package_monitor_last_value = 0;
    app->package_monitor_line_length = 0;
    app->package_monitor_dirty = true;
    app->package_monitor_last_channel_tick = 0;
}

static void simple_app_package_monitor_process_line(SimpleApp* app, const char* line) {
    if(!app || !line) return;

    while(*line == '>' || *line == ' ') {
        line++;
    }
    if(*line == '\0') return;

    if(strstr(line, "monitor started on channel") != NULL) {
        const char* digits = line;
        while(*digits && !isdigit((unsigned char)*digits)) {
            digits++;
        }
        if(*digits) {
            unsigned long channel = strtoul(digits, NULL, 10);
            if(channel == 0 || channel > PACKAGE_MONITOR_TOTAL_CHANNELS) {
                channel = PACKAGE_MONITOR_DEFAULT_CHANNEL;
            }
            app->package_monitor_channel = (uint8_t)channel;
        }
        app->package_monitor_active = true;
        app->package_monitor_dirty = true;
        return;
    }

    if(strstr(line, "monitor stopped") != NULL) {
        app->package_monitor_active = false;
        app->package_monitor_dirty = true;
        return;
    }

    if(!app->package_monitor_history) return;

    size_t len = strlen(line);
    if(len < 4) return;

    if(strcmp(line + len - 4, "pkts") == 0) {
        const char* digits = line;
        while(*digits && !isdigit((unsigned char)*digits)) {
            digits++;
        }
        if(!*digits) return;

        unsigned long value = strtoul(digits, NULL, 10);
        if(value > UINT16_MAX) {
            value = UINT16_MAX;
        }
        app->package_monitor_last_value = (uint16_t)value;

        if(app->package_monitor_history_count < PACKAGE_MONITOR_MAX_HISTORY) {
            app->package_monitor_history[app->package_monitor_history_count++] = (uint16_t)value;
        } else {
            memmove(
                app->package_monitor_history,
                app->package_monitor_history + 1,
                (PACKAGE_MONITOR_MAX_HISTORY - 1) * sizeof(app->package_monitor_history[0]));
            app->package_monitor_history[PACKAGE_MONITOR_MAX_HISTORY - 1] = (uint16_t)value;
        }
        app->package_monitor_dirty = true;
    }
}

static void simple_app_package_monitor_feed(SimpleApp* app, char ch) {
    if(!app) return;

    if(ch == '\r') return;

    if(ch == '\n') {
        if(app->package_monitor_line_length > 0) {
            app->package_monitor_line_buffer[app->package_monitor_line_length] = '\0';
            simple_app_package_monitor_process_line(app, app->package_monitor_line_buffer);
        }
        app->package_monitor_line_length = 0;
        return;
    }

    if(app->package_monitor_line_length + 1 >= sizeof(app->package_monitor_line_buffer)) {
        app->package_monitor_line_length = 0;
        return;
    }

    app->package_monitor_line_buffer[app->package_monitor_line_length++] = ch;
}

static void simple_app_package_monitor_start(SimpleApp* app, uint8_t channel, bool reset_history) {
    if(!app) return;

    if(channel < 1) {
        channel = PACKAGE_MONITOR_DEFAULT_CHANNEL;
    }
    if(channel > PACKAGE_MONITOR_TOTAL_CHANNELS) {
        channel = PACKAGE_MONITOR_TOTAL_CHANNELS;
    }

    simple_app_send_stop_if_needed(app);
    if(!simple_app_alloc_package_monitor_buffers(app)) {
        simple_app_show_status_message(app, "OOM: monitor", 1500, true);
        if(app->viewport) view_port_update(app->viewport);
        return;
    }
    if(reset_history) {
        simple_app_package_monitor_reset(app);
    }

    char command[32];
    snprintf(command, sizeof(command), "%s %u", PACKAGE_MONITOR_COMMAND, (unsigned)channel);
    simple_app_send_command(app, command, false);

    app->package_monitor_channel = channel;
    app->package_monitor_active = true;
    app->package_monitor_dirty = true;
    uint32_t now = furi_get_tick();
    if(reset_history) {
        uint32_t cooldown = furi_ms_to_ticks(PACKAGE_MONITOR_CHANNEL_SWITCH_COOLDOWN_MS);
        app->package_monitor_last_channel_tick = (now > cooldown) ? (now - cooldown) : 0;
    } else {
        app->package_monitor_last_channel_tick = now;
    }
}

static void simple_app_package_monitor_stop(SimpleApp* app) {
    if(!app) return;
    if(app->package_monitor_active || app->last_command_sent) {
        simple_app_send_stop_if_needed(app);
    }
    app->package_monitor_active = false;
    app->package_monitor_dirty = true;
}

static void simple_app_package_monitor_enter(SimpleApp* app) {
    if(!app) return;
    if(!simple_app_alloc_package_monitor_buffers(app)) {
        simple_app_show_status_message(app, "OOM: monitor", 1500, true);
        if(app->viewport) view_port_update(app->viewport);
        return;
    }
    if(app->package_monitor_channel < 1 ||
       app->package_monitor_channel > PACKAGE_MONITOR_TOTAL_CHANNELS) {
        app->package_monitor_channel = PACKAGE_MONITOR_DEFAULT_CHANNEL;
    }
    app->screen = ScreenPackageMonitor;
    simple_app_package_monitor_start(app, app->package_monitor_channel, true);
    if(app->viewport) {
        view_port_update(app->viewport);
    }
}

static void simple_app_draw_package_monitor(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;

    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 4, 12, "Package Monitor");

    if(!app->package_monitor_history) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, DISPLAY_WIDTH / 2, 40, AlignCenter, AlignCenter, "OOM: history");
        return;
    }

    uint8_t channel = app->package_monitor_channel;
    if(channel < 1 || channel > PACKAGE_MONITOR_TOTAL_CHANNELS) {
        channel = PACKAGE_MONITOR_DEFAULT_CHANNEL;
    }

    char channel_text[32];
    snprintf(
        channel_text,
        sizeof(channel_text),
        "Ch %02u/%02u",
        (unsigned)channel,
        (unsigned)PACKAGE_MONITOR_TOTAL_CHANNELS);
    char value_text[24];
    snprintf(value_text, sizeof(value_text), "%upkts", (unsigned)app->package_monitor_last_value);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 24, channel_text);
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH - 2, 24, AlignRight, AlignBottom, value_text);

    const int16_t frame_x = 2;
    const int16_t frame_y = 28;
    const int16_t frame_w = DISPLAY_WIDTH - 4;
    const int16_t frame_h = 34;

    canvas_draw_line(canvas, frame_x, frame_y, frame_x + frame_w, frame_y);
    canvas_draw_line(canvas, frame_x, frame_y + frame_h, frame_x + frame_w, frame_y + frame_h);
    canvas_draw_line(canvas, frame_x, frame_y, frame_x, frame_y + frame_h);
    canvas_draw_line(canvas, frame_x + frame_w, frame_y, frame_x + frame_w, frame_y + frame_h);

    const int16_t graph_left = frame_x + 1;
    const int16_t graph_right = frame_x + frame_w - 1;
    const int16_t graph_top = frame_y + 1;
    const int16_t graph_bottom = frame_y + frame_h - 1;
    const int16_t graph_width_px = graph_right - graph_left + 1;
    const int16_t graph_height_px = graph_bottom - graph_top + 1;

    if(graph_width_px <= 0 || graph_height_px <= 0) {
        app->package_monitor_dirty = false;
        return;
    }

    if(app->package_monitor_history_count == 0) {
        const char* message = app->package_monitor_active ? "Waiting for data" :
                                                            "Press OK to restart";
        canvas_draw_str_aligned(
            canvas,
            graph_left + graph_width_px / 2,
            graph_top + graph_height_px / 2,
            AlignCenter,
            AlignCenter,
            message);
        app->package_monitor_dirty = false;
        return;
    }

    uint16_t max_value = 0;
    for(size_t i = 0; i < app->package_monitor_history_count; i++) {
        if(app->package_monitor_history[i] > max_value) {
            max_value = app->package_monitor_history[i];
        }
    }
    if(max_value == 0) {
        max_value = 1;
    }

    size_t sample_count = app->package_monitor_history_count;
    size_t slot_capacity = (PACKAGE_MONITOR_BAR_SPACING > 0) ?
                               ((size_t)(graph_width_px - 1) / PACKAGE_MONITOR_BAR_SPACING) + 1 :
                               (size_t)graph_width_px;
    if(slot_capacity == 0) {
        slot_capacity = 1;
    }
    size_t visible_samples = (sample_count < slot_capacity) ? sample_count : slot_capacity;
    size_t start_index = (sample_count > visible_samples) ? (sample_count - visible_samples) : 0;

    for(size_t idx = start_index; idx < sample_count; idx++) {
        size_t relative = idx - start_index;
        int16_t x = graph_right - (int16_t)(relative * PACKAGE_MONITOR_BAR_SPACING);
        if(x < graph_left) {
            break;
        }

        uint16_t value = app->package_monitor_history[idx];
        int16_t bar_height = 0;
        if(value > 0) {
            bar_height = (int16_t)((value * (uint32_t)(graph_height_px - 1)) / max_value);
            if(bar_height == 0) {
                bar_height = 1;
            }
        }
        int16_t bar_top = graph_bottom - bar_height;
        if(bar_top < graph_top) {
            bar_top = graph_top;
        }
        canvas_draw_line(canvas, x, graph_bottom, x, bar_top);
    }

    app->package_monitor_dirty = false;
}

static void simple_app_handle_package_monitor_input(SimpleApp* app, InputKey key) {
    if(!app) return;

    if(key == InputKeyBack) {
        simple_app_package_monitor_stop(app);
        simple_app_free_package_monitor_buffers(app);
        app->menu_state = MenuStateItems;
        app->section_index = MENU_SECTION_MONITORING;
        app->item_index = 0;
        app->item_offset = 0;
        app->screen = ScreenMenu;
        app->package_monitor_dirty = false;
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    uint32_t now = furi_get_tick();
    uint32_t cooldown_ticks = furi_ms_to_ticks(PACKAGE_MONITOR_CHANNEL_SWITCH_COOLDOWN_MS);
    bool can_switch = (app->package_monitor_last_channel_tick == 0) ||
                      ((now - app->package_monitor_last_channel_tick) >= cooldown_ticks);

    if(key == InputKeyUp) {
        if(!can_switch) {
            return;
        }
        if(app->package_monitor_channel < PACKAGE_MONITOR_TOTAL_CHANNELS) {
            uint8_t next_channel = app->package_monitor_channel + 1;
            simple_app_package_monitor_reset(app);
            simple_app_package_monitor_start(app, next_channel, false);
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
    } else if(key == InputKeyDown) {
        if(!can_switch) {
            return;
        }
        if(app->package_monitor_channel > 1) {
            uint8_t prev_channel = app->package_monitor_channel - 1;
            simple_app_package_monitor_reset(app);
            simple_app_package_monitor_start(app, prev_channel, false);
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
    } else if(key == InputKeyOk) {
        simple_app_package_monitor_start(app, app->package_monitor_channel, true);
        if(app->viewport) {
            view_port_update(app->viewport);
        }
    }
}

static int
    simple_app_channel_view_find_channel_index(const uint8_t* list, size_t count, uint8_t channel) {
    if(!list || count == 0) return -1;
    for(size_t i = 0; i < count; i++) {
        if(list[i] == channel) {
            return (int)i;
        }
    }
    return -1;
}

static size_t simple_app_channel_view_visible_columns(ChannelViewBand band) {
    return (band == ChannelViewBand5) ? CHANNEL_VIEW_VISIBLE_COLUMNS_5 :
                                        CHANNEL_VIEW_VISIBLE_COLUMNS_24;
}

static uint8_t simple_app_channel_view_max_offset(ChannelViewBand band) {
    size_t total = (band == ChannelViewBand5) ? CHANNEL_VIEW_5GHZ_CHANNEL_COUNT :
                                                CHANNEL_VIEW_24GHZ_CHANNEL_COUNT;
    size_t visible = simple_app_channel_view_visible_columns(band);
    if(total <= visible) {
        return 0;
    }
    return (uint8_t)(total - visible);
}

static uint8_t* simple_app_channel_view_offset_ptr(SimpleApp* app, ChannelViewBand band) {
    return (band == ChannelViewBand5) ? &app->channel_view_offset_5 : &app->channel_view_offset_24;
}

static bool
    simple_app_channel_view_adjust_offset(SimpleApp* app, ChannelViewBand band, int delta) {
    if(!app || delta == 0) return false;
    uint8_t* offset = simple_app_channel_view_offset_ptr(app, band);
    uint8_t max_offset = simple_app_channel_view_max_offset(band);
    int new_offset = (int)*offset + delta;
    if(new_offset < 0) {
        new_offset = 0;
    } else if(new_offset > (int)max_offset) {
        new_offset = (int)max_offset;
    }
    if(new_offset == (int)*offset) {
        return false;
    }
    *offset = (uint8_t)new_offset;
    app->channel_view_dirty = true;
    return true;
}

static void simple_app_channel_view_show_status(SimpleApp* app, const char* status) {
    if(!app) return;
    if(status && status[0] != '\0') {
        strncpy(app->channel_view_status_text, status, sizeof(app->channel_view_status_text) - 1);
        app->channel_view_status_text[sizeof(app->channel_view_status_text) - 1] = '\0';
        app->channel_view_has_status = true;
    } else {
        app->channel_view_status_text[0] = '\0';
        app->channel_view_has_status = false;
    }
    app->channel_view_dirty = true;
}

static void simple_app_channel_view_reset(SimpleApp* app) {
    if(!app) return;
    if(app->channel_view_counts_24) {
        memset(
            app->channel_view_counts_24, 0, sizeof(uint16_t) * CHANNEL_VIEW_24GHZ_CHANNEL_COUNT);
    }
    if(app->channel_view_counts_5) {
        memset(app->channel_view_counts_5, 0, sizeof(uint16_t) * CHANNEL_VIEW_5GHZ_CHANNEL_COUNT);
    }
    if(app->channel_view_working_counts_24) {
        memset(
            app->channel_view_working_counts_24,
            0,
            sizeof(uint16_t) * CHANNEL_VIEW_24GHZ_CHANNEL_COUNT);
    }
    if(app->channel_view_working_counts_5) {
        memset(
            app->channel_view_working_counts_5,
            0,
            sizeof(uint16_t) * CHANNEL_VIEW_5GHZ_CHANNEL_COUNT);
    }
    app->channel_view_line_length = 0;
    app->channel_view_section = ChannelViewSectionNone;
    app->channel_view_dataset_active = false;
    app->channel_view_has_data = false;
    app->channel_view_has_status = false;
    app->channel_view_status_text[0] = '\0';
    app->channel_view_dirty = true;
}

static void simple_app_channel_view_begin_dataset(SimpleApp* app) {
    if(!app) return;
    if(!app->channel_view_working_counts_24 || !app->channel_view_working_counts_5) return;
    memset(
        app->channel_view_working_counts_24,
        0,
        sizeof(uint16_t) * CHANNEL_VIEW_24GHZ_CHANNEL_COUNT);
    memset(
        app->channel_view_working_counts_5, 0, sizeof(uint16_t) * CHANNEL_VIEW_5GHZ_CHANNEL_COUNT);
    app->channel_view_section = ChannelViewSectionNone;
    app->channel_view_dataset_active = true;
}

static void simple_app_channel_view_commit_dataset(SimpleApp* app) {
    if(!app) return;
    if(!app->channel_view_counts_24 || !app->channel_view_counts_5 ||
       !app->channel_view_working_counts_24 || !app->channel_view_working_counts_5) {
        return;
    }
    memcpy(
        app->channel_view_counts_24,
        app->channel_view_working_counts_24,
        sizeof(uint16_t) * CHANNEL_VIEW_24GHZ_CHANNEL_COUNT);
    memcpy(
        app->channel_view_counts_5,
        app->channel_view_working_counts_5,
        sizeof(uint16_t) * CHANNEL_VIEW_5GHZ_CHANNEL_COUNT);
    app->channel_view_dataset_active = false;
    app->channel_view_has_data = true;
    app->channel_view_dirty = true;
}

static void simple_app_channel_view_start(SimpleApp* app) {
    if(!app) return;
    simple_app_send_stop_if_needed(app);
    simple_app_channel_view_reset(app);
    app->channel_view_active = true;
    simple_app_send_command(app, CHANNEL_VIEW_COMMAND, false);
    simple_app_channel_view_show_status(app, "Scanning...");
}

static void simple_app_channel_view_stop(SimpleApp* app) {
    if(!app) return;
    if(app->channel_view_active) {
        app->channel_view_active = false;
    }
    app->channel_view_dataset_active = false;
    simple_app_send_stop_if_needed(app);
    app->channel_view_dirty = true;
}

static void simple_app_channel_view_enter(SimpleApp* app) {
    if(!app) return;
    if(!simple_app_alloc_channel_view_buffers(app)) {
        simple_app_show_status_message(app, "OOM: channels", 1500, true);
        if(app->viewport) view_port_update(app->viewport);
        return;
    }
    if(app->channel_view_band != ChannelViewBand24 && app->channel_view_band != ChannelViewBand5) {
        app->channel_view_band = ChannelViewBand24;
    }
    uint8_t* offset_ptr = simple_app_channel_view_offset_ptr(app, app->channel_view_band);
    uint8_t max_offset = simple_app_channel_view_max_offset(app->channel_view_band);
    if(*offset_ptr > max_offset) {
        *offset_ptr = max_offset;
    }
    app->screen = ScreenChannelView;
    simple_app_channel_view_start(app);
    if(app->viewport) {
        view_port_update(app->viewport);
    }
}

static void simple_app_channel_view_process_line(SimpleApp* app, const char* line) {
    if(!app || !line) return;

    while(*line == '>' || *line == ' ') {
        line++;
    }
    if(*line == '\0') return;

    if(strncmp(line, "channel_view_start", 18) == 0) {
        simple_app_channel_view_begin_dataset(app);
        simple_app_channel_view_show_status(app, "Scanning...");
        return;
    }

    if(strncmp(line, "channel_view_end", 16) == 0) {
        simple_app_channel_view_commit_dataset(app);
        simple_app_channel_view_show_status(app, "");
        return;
    }

    if(strncmp(line, "channel_view_error:", 19) == 0) {
        simple_app_channel_view_show_status(app, line + 19);
        return;
    }

    if(!app->channel_view_working_counts_24 || !app->channel_view_working_counts_5) {
        return;
    }

    if(strncmp(line, "band:", 5) == 0) {
        unsigned long band_value = strtoul(line + 5, NULL, 10);
        if(band_value >= 10) {
            app->channel_view_section = ChannelViewSection24;
        } else {
            app->channel_view_section = ChannelViewSection5;
        }
        return;
    }

    if(strncmp(line, "ch", 2) == 0) {
        const char* colon = strchr(line, ':');
        if(!colon) return;
        unsigned long channel = strtoul(line + 2, NULL, 10);
        unsigned long count = strtoul(colon + 1, NULL, 10);
        if(count > UINT16_MAX) {
            count = UINT16_MAX;
        }
        if(app->channel_view_section == ChannelViewSection24) {
            int idx = simple_app_channel_view_find_channel_index(
                channel_view_channels_24ghz, CHANNEL_VIEW_24GHZ_CHANNEL_COUNT, (uint8_t)channel);
            if(idx >= 0) {
                app->channel_view_working_counts_24[idx] = (uint16_t)count;
            }
        } else if(app->channel_view_section == ChannelViewSection5) {
            int idx = simple_app_channel_view_find_channel_index(
                channel_view_channels_5ghz, CHANNEL_VIEW_5GHZ_CHANNEL_COUNT, (uint8_t)channel);
            if(idx >= 0) {
                app->channel_view_working_counts_5[idx] = (uint16_t)count;
            }
        }
        return;
    }
}

static void simple_app_channel_view_feed(SimpleApp* app, char ch) {
    if(!app) return;

    if(ch == '\r') return;

    if(ch == '\n') {
        if(app->channel_view_line_length > 0) {
            app->channel_view_line_buffer[app->channel_view_line_length] = '\0';
            simple_app_channel_view_process_line(app, app->channel_view_line_buffer);
        }
        app->channel_view_line_length = 0;
        return;
    }

    if(app->channel_view_line_length + 1 >= sizeof(app->channel_view_line_buffer)) {
        app->channel_view_line_length = 0;
        return;
    }

    app->channel_view_line_buffer[app->channel_view_line_length++] = ch;
}

static void simple_app_draw_channel_view(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 4, 12, "Channel View");

    if(!app->channel_view_counts_24 || !app->channel_view_counts_5) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, DISPLAY_WIDTH / 2, 40, AlignCenter, AlignCenter, "OOM: channels");
        return;
    }

    const bool showing_5ghz = (app->channel_view_band == ChannelViewBand5);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, DISPLAY_WIDTH - 4, 12, AlignRight, AlignBottom, showing_5ghz ? "5G" : "2.4G");

    const int16_t frame_x = 2;
    const int16_t frame_y = 18;
    const int16_t frame_w = DISPLAY_WIDTH - 4;
    const int16_t frame_h = DISPLAY_HEIGHT - frame_y - 4;
    const int16_t bar_top = frame_y + 10;
    const int16_t bar_bottom = frame_y + frame_h - 12;

    canvas_draw_frame(canvas, frame_x, frame_y, frame_w, frame_h);

    const char* overlay_text = NULL;
    if(app->channel_view_has_status && app->channel_view_status_text[0] != '\0') {
        overlay_text = app->channel_view_status_text;
    } else if(!app->channel_view_has_data) {
        overlay_text = app->channel_view_active ? "Scanning..." : "Press OK to restart";
    }

    if(!app->channel_view_has_data) {
        const char* message = overlay_text;
        if(!message) {
            message = app->channel_view_active ? "Waiting for data..." : "Press OK to restart";
        }
        canvas_draw_str_aligned(
            canvas,
            frame_x + frame_w / 2,
            frame_y + frame_h / 2,
            AlignCenter,
            AlignCenter,
            message);
        app->channel_view_dirty = false;
        return;
    }

    const uint16_t* counts = showing_5ghz ? app->channel_view_counts_5 :
                                            app->channel_view_counts_24;
    const uint8_t* channel_map = showing_5ghz ? channel_view_channels_5ghz :
                                                channel_view_channels_24ghz;
    const size_t channel_count = showing_5ghz ? CHANNEL_VIEW_5GHZ_CHANNEL_COUNT :
                                                CHANNEL_VIEW_24GHZ_CHANNEL_COUNT;
    const size_t visible_columns = simple_app_channel_view_visible_columns(app->channel_view_band);
    const uint8_t offset = *simple_app_channel_view_offset_ptr(app, app->channel_view_band);

    uint16_t max_count = 0;
    for(size_t i = 0; i < channel_count; i++) {
        if(counts[i] > max_count) {
            max_count = counts[i];
        }
    }
    if(max_count == 0) {
        max_count = 1;
    }

    const int16_t column_width = (frame_w - 8) / (int16_t)visible_columns;
    const int16_t column_gap = 2;
    size_t drawn_columns = 0;
    const int16_t label_padding = 8;
    int16_t max_bar_height = bar_bottom - (bar_top + label_padding);
    if(max_bar_height < 2) {
        max_bar_height = 2;
    }
    for(size_t column = 0; column < visible_columns; column++) {
        size_t index = offset + column;
        if(index >= channel_count) break;
        drawn_columns++;
        uint16_t value = counts[index];
        uint8_t channel = channel_map[index];

        int16_t x0 = frame_x + 4 + (int16_t)column * column_width;
        int16_t x_center = x0 + column_width / 2;
        int16_t column_inner_width = column_width - column_gap;
        if(column_inner_width < 2) {
            column_inner_width = 2;
        }

        int16_t bar_height = 0;
        if(value > 0) {
            bar_height = (int16_t)((value * (uint32_t)max_bar_height) / max_count);
            if(bar_height == 0) {
                bar_height = 1;
            }
        }
        int16_t column_bar_top = bar_bottom - bar_height;
        if(bar_height > 0) {
            canvas_draw_box(
                canvas,
                x_center - column_inner_width / 2,
                column_bar_top,
                column_inner_width,
                bar_height);
        }

        char value_label[6];
        snprintf(value_label, sizeof(value_label), "%u", (unsigned)value);
        int16_t value_y = column_bar_top - 2;
        int16_t min_value_y = frame_y + 12;
        if(value_y < min_value_y) {
            value_y = min_value_y;
        }
        canvas_draw_str_aligned(canvas, x_center, value_y, AlignCenter, AlignBottom, value_label);

        char channel_label[6];
        snprintf(channel_label, sizeof(channel_label), "%u", (unsigned)channel);
        canvas_draw_str_aligned(
            canvas, x_center, frame_y + frame_h - 2, AlignCenter, AlignBottom, channel_label);
    }

    if(offset > 0) {
        canvas_draw_str(canvas, frame_x + 1, frame_y + frame_h / 2, "<");
    }
    size_t max_offset = simple_app_channel_view_max_offset(app->channel_view_band);
    if(offset < max_offset) {
        canvas_draw_str(canvas, frame_x + frame_w - 4, frame_y + frame_h / 2, ">");
    }

    app->channel_view_dirty = false;
}

static void simple_app_handle_channel_view_input(SimpleApp* app, InputKey key) {
    if(!app) return;

    if(key == InputKeyBack) {
        simple_app_channel_view_stop(app);
        simple_app_free_channel_view_buffers(app);
        app->menu_state = MenuStateItems;
        app->section_index = MENU_SECTION_MONITORING;
        app->item_index = 0;
        app->item_offset = 0;
        app->screen = ScreenMenu;
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(key == InputKeyUp || key == InputKeyDown) {
        ChannelViewBand new_band =
            (app->channel_view_band == ChannelViewBand24) ? ChannelViewBand5 : ChannelViewBand24;
        app->channel_view_band = new_band;
        uint8_t* offset_ptr = simple_app_channel_view_offset_ptr(app, app->channel_view_band);
        uint8_t max_offset = simple_app_channel_view_max_offset(app->channel_view_band);
        if(*offset_ptr > max_offset) {
            *offset_ptr = max_offset;
        }
        app->channel_view_dirty = true;
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(key == InputKeyOk) {
        simple_app_channel_view_stop(app);
        simple_app_channel_view_start(app);
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(key == InputKeyLeft) {
        if(simple_app_channel_view_adjust_offset(app, app->channel_view_band, -1) &&
           app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(key == InputKeyRight) {
        if(simple_app_channel_view_adjust_offset(app, app->channel_view_band, 1) &&
           app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }
}

static void simple_app_draw_portal_menu(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 4, 14, "Portal");

    canvas_set_font(canvas, FontSecondary);
    simple_app_portal_sync_offset(app);

    size_t offset = app->portal_menu_offset;
    size_t visible = PORTAL_VISIBLE_COUNT;
    if(visible == 0) visible = 1;
    if(visible > PORTAL_MENU_OPTION_COUNT) {
        visible = PORTAL_MENU_OPTION_COUNT;
    }

    uint8_t base_y = 30;
    uint8_t y = base_y;
    uint8_t list_bottom_y = base_y;

    for(size_t pos = 0; pos < visible; pos++) {
        size_t idx = offset + pos;
        if(idx >= PORTAL_MENU_OPTION_COUNT) break;

        const char* label = "Start Portal";
        char detail[48];
        char detail_extra[48];
        detail[0] = '\0';
        detail_extra[0] = '\0';
        bool show_detail_line = false;
        bool show_detail_extra = false;

        switch(idx) {
        case 0:
            label = "SSID";
            snprintf(detail, sizeof(detail), "Current:");
            show_detail_line = true;
            if(app->portal_ssid[0] != '\0') {
                strncpy(detail_extra, app->portal_ssid, sizeof(detail_extra) - 1);
                detail_extra[sizeof(detail_extra) - 1] = '\0';
            } else {
                snprintf(detail_extra, sizeof(detail_extra), "<none>");
            }
            simple_app_truncate_text(detail_extra, 28);
            show_detail_extra = true;
            break;
        case 1:
            label = "Select HTML";
            if(app->karma_selected_html_id != 0 && app->karma_selected_html_name[0] != '\0') {
                snprintf(detail, sizeof(detail), "Current: %s", app->karma_selected_html_name);
            } else {
                snprintf(detail, sizeof(detail), "Current: <none>");
            }
            simple_app_truncate_text(detail, 26);
            show_detail_line = true;
            break;
        case 2:
            label = "Start Portal";
            if(app->portal_ssid[0] == '\0') {
                snprintf(detail, sizeof(detail), "Need SSID");
            } else if(app->karma_selected_html_id == 0) {
                snprintf(detail, sizeof(detail), "Need HTML");
            } else {
                detail[0] = '\0';
            }
            simple_app_truncate_text(detail, 20);
            break;
        default:
            label = "Show Passwords";
            snprintf(detail, sizeof(detail), "portals.txt");
            show_detail_line = true;
            break;
        }

        if(app->portal_menu_index == idx) {
            canvas_draw_str(canvas, 2, y, ">");
        }
        canvas_draw_str(canvas, 14, y, label);

        uint8_t item_height = 12;
        if(show_detail_line || detail[0] != '\0') {
            canvas_draw_str(canvas, 14, (uint8_t)(y + 10), detail);
            item_height += 10;
        }
        if(show_detail_extra) {
            canvas_draw_str(canvas, 14, (uint8_t)(y + item_height), detail_extra);
            item_height += 10;
        }
        y = (uint8_t)(y + item_height);
        list_bottom_y = y;
    }

    uint8_t arrow_x = DISPLAY_WIDTH - 6;
    if(offset > 0) {
        int16_t arrow_y = (int16_t)(base_y - 6);
        if(arrow_y < 12) arrow_y = 12;
        simple_app_draw_scroll_arrow(canvas, arrow_x, arrow_y, true);
    }
    if(offset + visible < PORTAL_MENU_OPTION_COUNT) {
        int16_t arrow_y = (int16_t)(list_bottom_y - 6);
        if(arrow_y > 60) arrow_y = 60;
        if(arrow_y < 16) arrow_y = 16;
        simple_app_draw_scroll_arrow(canvas, arrow_x, arrow_y, false);
    }
}

static void simple_app_handle_portal_menu_input(SimpleApp* app, InputKey key) {
    if(!app) return;

    if(key == InputKeyBack || key == InputKeyLeft) {
        if(app->portal_ssid_listing_active) {
            simple_app_reset_portal_ssid_listing(app);
        }
        app->portal_ssid_popup_active = false;
        if(app->karma_html_listing_active) {
            simple_app_reset_karma_html_listing(app);
        }
        app->karma_html_popup_active = false;
        simple_app_clear_status_message(app);
        app->karma_status_active = false;
        app->portal_menu_offset = 0;
        simple_app_focus_attacks_menu(app);
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(key == InputKeyUp) {
        if(app->portal_menu_index > 0) {
            app->portal_menu_index--;
        } else {
            app->portal_menu_index = PORTAL_MENU_OPTION_COUNT - 1;
        }
        simple_app_portal_sync_offset(app);
        if(app->viewport) {
            view_port_update(app->viewport);
        }
    } else if(key == InputKeyDown) {
        if(app->portal_menu_index + 1 < PORTAL_MENU_OPTION_COUNT) {
            app->portal_menu_index++;
        } else {
            app->portal_menu_index = 0;
        }
        simple_app_portal_sync_offset(app);
        if(app->viewport) {
            view_port_update(app->viewport);
        }
    } else if(key == InputKeyOk) {
        if(app->portal_menu_index == 0) {
            if(app->portal_ssid_listing_active) {
                simple_app_show_status_message(app, "Listing already\nin progress", 1200, true);
                if(app->viewport) {
                    view_port_update(app->viewport);
                }
                return;
            }
            if(app->portal_ssid_count > 0 && !app->portal_ssid_missing) {
                simple_app_open_portal_ssid_popup(app);
            } else if(app->portal_ssid_missing) {
                simple_app_portal_prompt_ssid(app);
            } else {
                simple_app_request_portal_ssid_list(app);
            }
        } else if(app->portal_menu_index == 1) {
            if(app->karma_html_listing_active || app->karma_html_count == 0) {
                simple_app_request_karma_html_list(app);
            } else {
                simple_app_clear_status_message(app);
                app->karma_status_active = false;
                if(app->karma_html_popup_index >= app->karma_html_count) {
                    app->karma_html_popup_index = 0;
                }
                if(app->karma_html_popup_offset >= app->karma_html_count) {
                    app->karma_html_popup_offset = 0;
                }
                app->karma_html_popup_active = true;
                if(app->viewport) {
                    view_port_update(app->viewport);
                }
            }
        } else if(app->portal_menu_index == 2) {
            simple_app_start_portal(app);
        } else {
            app->passwords_select_return_screen = ScreenPortalMenu;
            app->passwords_select_index = 0;
            app->screen = ScreenPasswordsSelect;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
    }
}

static void simple_app_reset_portal_ssid_listing(SimpleApp* app) {
    if(!app) return;
    simple_app_portal_free_ssid_entries(app);
    app->portal_ssid_listing_active = false;
    app->portal_ssid_list_header_seen = false;
    app->portal_ssid_list_length = 0;
    app->portal_ssid_count = 0;
    app->portal_ssid_popup_index = 0;
    app->portal_ssid_popup_offset = 0;
    app->portal_ssid_popup_active = false;
    app->portal_ssid_missing = false;
}

static void simple_app_open_portal_ssid_popup(SimpleApp* app) {
    if(!app || app->portal_ssid_count == 0 || !app->portal_ssid_entries) return;

    size_t total_options = app->portal_ssid_count + 1; // +1 for manual entry
    size_t target_index = 0; // manual by default
    if(app->portal_ssid[0] != '\0') {
        for(size_t i = 0; i < app->portal_ssid_count; i++) {
            if(strncmp(app->portal_ssid_entries[i].ssid, app->portal_ssid, SCAN_SSID_MAX_LEN - 1) ==
               0) {
                target_index = i + 1;
                break;
            }
        }
    }

    app->portal_ssid_popup_index = target_index;
    size_t visible = total_options;
    if(visible > PORTAL_SSID_POPUP_VISIBLE_LINES) {
        visible = PORTAL_SSID_POPUP_VISIBLE_LINES;
    }
    if(visible == 0) visible = 1;

    if(total_options > visible && target_index >= visible) {
        app->portal_ssid_popup_offset = target_index - visible + 1;
    } else {
        app->portal_ssid_popup_offset = 0;
    }
    size_t max_offset = (total_options > visible) ? (total_options - visible) : 0;
    if(app->portal_ssid_popup_offset > max_offset) {
        app->portal_ssid_popup_offset = max_offset;
    }

    app->portal_ssid_popup_active = true;
    simple_app_clear_status_message(app);
    if(app->viewport) {
        view_port_update(app->viewport);
    }
}

static void simple_app_finish_portal_ssid_listing(SimpleApp* app) {
    if(!app || !app->portal_ssid_listing_active) return;
    app->portal_ssid_listing_active = false;
    app->portal_ssid_list_length = 0;
    app->portal_ssid_list_header_seen = false;
    app->last_command_sent = false;
    simple_app_clear_status_message(app);

    if(app->portal_ssid_count == 0) {
        app->portal_ssid_popup_active = false;
        if(app->screen == ScreenPortalMenu) {
            simple_app_show_status_message(app, "SSID presets\nnot available", 1500, true);
            simple_app_portal_prompt_ssid(app);
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
        return;
    }

    app->portal_ssid_missing = false;
    if(app->screen == ScreenPortalMenu) {
        simple_app_open_portal_ssid_popup(app);
    }
}

static void simple_app_process_portal_ssid_line(SimpleApp* app, const char* line) {
    if(!app || !line || !app->portal_ssid_listing_active) return;
    if(!app->portal_ssid_entries) return;

    const char* cursor = line;
    while(*cursor == ' ' || *cursor == '\t') {
        cursor++;
    }

    if(*cursor == '\0') {
        if(app->portal_ssid_count > 0) {
            simple_app_finish_portal_ssid_listing(app);
        }
        return;
    }

    if(strncmp(cursor, "ssid.txt not found", 19) == 0 ||
       strncmp(cursor, "ssid.txt is empty", 18) == 0 || strncmp(cursor, "No SSID", 7) == 0) {
        app->portal_ssid_missing = true;
        app->portal_ssid_count = 0;
        simple_app_finish_portal_ssid_listing(app);
        return;
    }

    if(strncmp(cursor, "SSID presets", 12) == 0) {
        app->portal_ssid_list_header_seen = true;
        return;
    }

    if(!isdigit((unsigned char)cursor[0])) {
        if(app->portal_ssid_count > 0) {
            simple_app_finish_portal_ssid_listing(app);
        }
        return;
    }

    char* endptr = NULL;
    long id = strtol(cursor, &endptr, 10);
    if(id <= 0 || id > 255 || !endptr) {
        return;
    }
    while(*endptr == ' ' || *endptr == '\t') {
        endptr++;
    }
    if(*endptr == '\0') {
        return;
    }

    if(app->portal_ssid_count >= PORTAL_MAX_SSID_PRESETS) {
        return;
    }

    PortalSsidEntry* entry = &app->portal_ssid_entries[app->portal_ssid_count++];
    entry->id = (uint8_t)id;
    strncpy(entry->ssid, endptr, SCAN_SSID_MAX_LEN - 1);
    entry->ssid[SCAN_SSID_MAX_LEN - 1] = '\0';

    size_t len = strlen(entry->ssid);
    while(len > 0 && (entry->ssid[len - 1] == '\r' || entry->ssid[len - 1] == '\n' ||
                      entry->ssid[len - 1] == ' ' || entry->ssid[len - 1] == '\t')) {
        entry->ssid[--len] = '\0';
    }

    app->portal_ssid_list_header_seen = true;
}

static void simple_app_portal_ssid_feed(SimpleApp* app, char ch) {
    if(!app || !app->portal_ssid_listing_active) return;
    if(ch == '\r') return;

    if(ch == '>') {
        if(app->portal_ssid_list_length > 0) {
            app->portal_ssid_list_buffer[app->portal_ssid_list_length] = '\0';
            simple_app_process_portal_ssid_line(app, app->portal_ssid_list_buffer);
            app->portal_ssid_list_length = 0;
        }
        simple_app_finish_portal_ssid_listing(app);
        return;
    }

    if(ch == '\n') {
        if(app->portal_ssid_list_length > 0) {
            app->portal_ssid_list_buffer[app->portal_ssid_list_length] = '\0';
            simple_app_process_portal_ssid_line(app, app->portal_ssid_list_buffer);
        } else if(app->portal_ssid_list_header_seen || app->portal_ssid_missing) {
            simple_app_finish_portal_ssid_listing(app);
        }
        app->portal_ssid_list_length = 0;
        return;
    }

    if(app->portal_ssid_list_length + 1 >= sizeof(app->portal_ssid_list_buffer)) {
        app->portal_ssid_list_length = 0;
        return;
    }

    app->portal_ssid_list_buffer[app->portal_ssid_list_length++] = ch;
}

static bool simple_app_portal_status_ensure(SimpleApp* app) {
    if(!app) return false;
    if(app->portal_status) return true;
    app->portal_status = calloc(1, sizeof(PortalStatus));
    return app->portal_status != NULL;
}

static void simple_app_portal_status_free(SimpleApp* app) {
    if(!app || !app->portal_status) return;
    free(app->portal_status);
    app->portal_status = NULL;
}

static void simple_app_reset_portal_status(SimpleApp* app) {
    if(!app || !app->portal_status) return;
    app->portal_status->status_ssid[0] = '\0';
    app->portal_status->client_count = -1;
    app->portal_status->password_count = 0;
    app->portal_status->last_password[0] = '\0';
    app->portal_status->username_count = 0;
    app->portal_status->last_username[0] = '\0';
    app->portal_status->line_length = 0;
}

static bool simple_app_evil_twin_status_ensure(SimpleApp* app) {
    if(!app) return false;
    if(app->evil_twin_status) return true;
    app->evil_twin_status = calloc(1, sizeof(EvilTwinStatus));
    return app->evil_twin_status != NULL;
}

static void simple_app_evil_twin_status_free(SimpleApp* app) {
    if(!app || !app->evil_twin_status) return;
    free(app->evil_twin_status);
    app->evil_twin_status = NULL;
}

static void simple_app_reset_evil_twin_status(SimpleApp* app) {
    if(!app || !app->evil_twin_status) return;
    app->evil_twin_status->status_ssid[0] = '\0';
    app->evil_twin_status->client_count = -1;
    app->evil_twin_status->password_count = 0;
    app->evil_twin_status->last_password[0] = '\0';
    app->evil_twin_status->status_note[0] = '\0';
    app->evil_twin_status->line_length = 0;
}

static bool
    simple_app_extract_query_param(const char* query, const char* key, char* out, size_t out_size) {
    if(!query || !key || !out || out_size == 0) return false;

    size_t key_len = strlen(key);
    const char* cursor = query;
    while((cursor = strstr(cursor, key)) != NULL) {
        bool boundary_ok = (cursor == query) || (*(cursor - 1) == '&');
        const char* after = cursor + key_len;
        if(boundary_ok && *after == '=') {
            after++;
            size_t written = 0;
            while(*after != '\0' && *after != '&') {
                if(written + 1 < out_size) {
                    char ch = *after;
                    out[written++] = (ch == '+') ? ' ' : ch;
                }
                after++;
            }
            out[written] = '\0';
            simple_app_trim(out);
            return out[0] != '\0';
        }
        cursor += key_len;
    }

    return false;
}

static void simple_app_process_portal_status_line(SimpleApp* app, const char* line) {
    if(!app || !line || !app->portal_status) return;
    PortalStatus* status = app->portal_status;

    const char* cursor = line;
    while(*cursor == ' ' || *cursor == '\t' || *cursor == '>') {
        cursor++;
    }
    if(*cursor == '\0') return;

    const char* ssid_marker = "Starting captive portal with SSID:";
    const char* ap_name_marker = "AP Name:";
    const char* client_marker = "Portal: Client count";
    const char* password_marker = "Password:";
    const char* portal_password_marker = "Portal password received:";
    const char* post_marker = "Received POST data:";

    const char* ssid_ptr = strstr(cursor, ssid_marker);
    if(ssid_ptr) {
        ssid_ptr += strlen(ssid_marker);
        while(*ssid_ptr == ' ' || *ssid_ptr == '\t') {
            ssid_ptr++;
        }
        if(*ssid_ptr != '\0') {
            char ascii_ssid[SCAN_SSID_MAX_LEN];
            simple_app_utf8_to_ascii_pl(ssid_ptr, ascii_ssid, sizeof(ascii_ssid));
            strncpy(status->status_ssid, ascii_ssid, sizeof(status->status_ssid) - 1);
            status->status_ssid[sizeof(status->status_ssid) - 1] = '\0';
            simple_app_trim(status->status_ssid);
        }
        return;
    }

    const char* ap_name_ptr = strstr(cursor, ap_name_marker);
    if(ap_name_ptr) {
        ap_name_ptr += strlen(ap_name_marker);
        while(*ap_name_ptr == ' ' || *ap_name_ptr == '\t') {
            ap_name_ptr++;
        }
        if(*ap_name_ptr != '\0') {
            char ascii_ssid[SCAN_SSID_MAX_LEN];
            simple_app_utf8_to_ascii_pl(ap_name_ptr, ascii_ssid, sizeof(ascii_ssid));
            strncpy(status->status_ssid, ascii_ssid, sizeof(status->status_ssid) - 1);
            status->status_ssid[sizeof(status->status_ssid) - 1] = '\0';
            simple_app_trim(status->status_ssid);
        }
        return;
    }

    const char* client_ptr = strstr(cursor, client_marker);
    if(client_ptr) {
        const char* eq = strchr(client_ptr, '=');
        if(eq) {
            eq++;
            while(*eq == ' ' || *eq == '\t') {
                eq++;
            }
            char* endptr = NULL;
            long count = strtol(eq, &endptr, 10);
            if(endptr && count >= 0) {
                status->client_count = (int32_t)count;
            }
        }
        return;
    }

    const char* password_ptr = strstr(cursor, password_marker);
    const char* portal_password_ptr = strstr(cursor, portal_password_marker);
    if(password_ptr || portal_password_ptr) {
        const char* pwd_source = portal_password_ptr ? portal_password_ptr : password_ptr;
        pwd_source += portal_password_ptr ? strlen(portal_password_marker) :
                                            strlen(password_marker);
        while(*pwd_source == ' ' || *pwd_source == '\t') {
            pwd_source++;
        }
        if(*pwd_source != '\0') {
            char ascii_pw[PORTAL_PASSWORD_MAX_LEN];
            simple_app_utf8_to_ascii_pl(pwd_source, ascii_pw, sizeof(ascii_pw));
            strncpy(status->last_password, ascii_pw, sizeof(status->last_password) - 1);
            status->last_password[sizeof(status->last_password) - 1] = '\0';
            simple_app_trim(status->last_password);
            status->password_count++;
        }
        return;
    }

    const char* post_ptr = strstr(cursor, post_marker);
    if(post_ptr) {
        post_ptr += strlen(post_marker);
        while(*post_ptr == ' ' || *post_ptr == '\t') {
            post_ptr++;
        }
        if(*post_ptr != '\0') {
            char value[PORTAL_PASSWORD_MAX_LEN];
            if(simple_app_extract_query_param(post_ptr, "google_user", value, sizeof(value)) ||
               simple_app_extract_query_param(post_ptr, "username", value, sizeof(value)) ||
               simple_app_extract_query_param(post_ptr, "user", value, sizeof(value)) ||
               simple_app_extract_query_param(post_ptr, "login", value, sizeof(value)) ||
               simple_app_extract_query_param(post_ptr, "email", value, sizeof(value))) {
                strncpy(status->last_username, value, sizeof(status->last_username) - 1);
                status->last_username[sizeof(status->last_username) - 1] = '\0';
                status->username_count++;
            }
        }
        return;
    }
}

static void simple_app_process_evil_twin_status_line(SimpleApp* app, const char* line) {
    if(!app || !line || !app->evil_twin_status) return;
    EvilTwinStatus* status = app->evil_twin_status;

    const char* cursor = line;
    while(*cursor == ' ' || *cursor == '\t' || *cursor == '>') {
        cursor++;
    }
    if(*cursor == '\0') return;

    const char* start_marker = "Starting captive portal for Evil Twin attack on:";
    const char* portal_started_marker = "Captive portal started successfully";
    const char* deauth_started_marker = "Deauth attack started.";
    const char* ap_name_marker = "AP Name:";
    const char* client_marker = "Portal: Client count";
    const char* short_password_marker = "Password:";
    const char* portal_password_marker = "Portal password received:";
    const char* stop_deauth_marker = "Stopping deauth attack to attempt connection";
    const char* resume_deauth_marker = "Resuming deauth attack - password was incorrect.";
    const char* resume_ok_marker = "Deauth attack resumed successfully.";
    const char* attempt_marker = "Attempting to connect to SSID=";
    const char* attempt_count_marker = "Attempting to connect, connectAttemptCount=";
    const char* verified_marker = "Password verified!";
    const char* connected_marker = "Wi-Fi: connected to SSID=";
    const char* wrong_marker = "Evil twin: connection failed";
    const char* shutdown_marker = "Evil Twin portal shut down successfully!";
    const char* saved_marker = "Password saved to eviltwin.txt";

    const char* start_ptr = strstr(cursor, start_marker);
    if(start_ptr) {
        start_ptr += strlen(start_marker);
        while(*start_ptr == ' ' || *start_ptr == '\t') {
            start_ptr++;
        }
        if(*start_ptr != '\0') {
            char ascii_ssid[SCAN_SSID_MAX_LEN];
            simple_app_utf8_to_ascii_pl(start_ptr, ascii_ssid, sizeof(ascii_ssid));
            strncpy(status->status_ssid, ascii_ssid, sizeof(status->status_ssid) - 1);
            status->status_ssid[sizeof(status->status_ssid) - 1] = '\0';
            simple_app_trim(status->status_ssid);
        }
        strncpy(status->status_note, "Portal", sizeof(status->status_note) - 1);
        status->status_note[sizeof(status->status_note) - 1] = '\0';
        return;
    }

    const char* ap_name_ptr = strstr(cursor, ap_name_marker);
    if(ap_name_ptr) {
        ap_name_ptr += strlen(ap_name_marker);
        while(*ap_name_ptr == ' ' || *ap_name_ptr == '\t') {
            ap_name_ptr++;
        }
        if(*ap_name_ptr != '\0') {
            char ascii_ssid[SCAN_SSID_MAX_LEN];
            simple_app_utf8_to_ascii_pl(ap_name_ptr, ascii_ssid, sizeof(ascii_ssid));
            strncpy(status->status_ssid, ascii_ssid, sizeof(status->status_ssid) - 1);
            status->status_ssid[sizeof(status->status_ssid) - 1] = '\0';
            simple_app_trim(status->status_ssid);
        }
        return;
    }

    if(strstr(cursor, portal_started_marker)) {
        strncpy(status->status_note, "Portal", sizeof(status->status_note) - 1);
        status->status_note[sizeof(status->status_note) - 1] = '\0';
        return;
    }

    if(strstr(cursor, deauth_started_marker)) {
        strncpy(status->status_note, "Deauth", sizeof(status->status_note) - 1);
        status->status_note[sizeof(status->status_note) - 1] = '\0';
        return;
    }

    const char* client_ptr = strstr(cursor, client_marker);
    if(client_ptr) {
        const char* eq = strchr(client_ptr, '=');
        if(eq) {
            eq++;
            while(*eq == ' ' || *eq == '\t') {
                eq++;
            }
            char* endptr = NULL;
            long count = strtol(eq, &endptr, 10);
            if(endptr && count >= 0) {
                status->client_count = (int32_t)count;
            }
        }
        return;
    }

    const char* short_password_ptr = strstr(cursor, short_password_marker);
    const char* portal_password_ptr = strstr(cursor, portal_password_marker);
    if(short_password_ptr || portal_password_ptr) {
        const char* pwd_source = short_password_ptr ? short_password_ptr : portal_password_ptr;
        pwd_source += short_password_ptr ? strlen(short_password_marker) :
                                           strlen(portal_password_marker);
        while(*pwd_source == ' ' || *pwd_source == '\t') {
            pwd_source++;
        }
        if(*pwd_source != '\0') {
            char ascii_pw[PORTAL_PASSWORD_MAX_LEN];
            simple_app_utf8_to_ascii_pl(pwd_source, ascii_pw, sizeof(ascii_pw));
            strncpy(status->last_password, ascii_pw, sizeof(status->last_password) - 1);
            status->last_password[sizeof(status->last_password) - 1] = '\0';
            simple_app_trim(status->last_password);
            status->password_count++;
            strncpy(status->status_note, "Received", sizeof(status->status_note) - 1);
            status->status_note[sizeof(status->status_note) - 1] = '\0';
        }
        return;
    }

    if(strstr(cursor, stop_deauth_marker)) {
        strncpy(status->status_note, "Checking", sizeof(status->status_note) - 1);
        status->status_note[sizeof(status->status_note) - 1] = '\0';
        return;
    }

    if(strstr(cursor, resume_deauth_marker)) {
        strncpy(status->status_note, "Wrong", sizeof(status->status_note) - 1);
        status->status_note[sizeof(status->status_note) - 1] = '\0';
        status->last_password[0] = '\0';
        status->password_count = 0;
        simple_app_show_status_message(app, "Bad Password\nEvil ongoing", 1500, true);
        return;
    }

    if(strstr(cursor, resume_ok_marker)) {
        strncpy(status->status_note, "Deauth", sizeof(status->status_note) - 1);
        status->status_note[sizeof(status->status_note) - 1] = '\0';
        return;
    }

    const char* attempt_ptr = strstr(cursor, attempt_marker);
    if(attempt_ptr) {
        const char* ssid_ptr = strstr(attempt_ptr, "SSID='");
        if(ssid_ptr) {
            ssid_ptr += strlen("SSID='");
            const char* end = strchr(ssid_ptr, '\'');
            if(end && end > ssid_ptr) {
                char ascii_ssid[SCAN_SSID_MAX_LEN];
                size_t len = (size_t)(end - ssid_ptr);
                if(len >= sizeof(ascii_ssid)) len = sizeof(ascii_ssid) - 1;
                memcpy(ascii_ssid, ssid_ptr, len);
                ascii_ssid[len] = '\0';
                char ascii_clean[SCAN_SSID_MAX_LEN];
                simple_app_utf8_to_ascii_pl(ascii_ssid, ascii_clean, sizeof(ascii_clean));
                strncpy(status->status_ssid, ascii_clean, sizeof(status->status_ssid) - 1);
                status->status_ssid[sizeof(status->status_ssid) - 1] = '\0';
                simple_app_trim(status->status_ssid);
            }
        }
        strncpy(status->status_note, "Checking", sizeof(status->status_note) - 1);
        status->status_note[sizeof(status->status_note) - 1] = '\0';
        return;
    }

    if(strstr(cursor, attempt_count_marker)) {
        strncpy(status->status_note, "Checking", sizeof(status->status_note) - 1);
        status->status_note[sizeof(status->status_note) - 1] = '\0';
        return;
    }

    const char* connected_ptr = strstr(cursor, connected_marker);
    if(connected_ptr) {
        const char* ssid_ptr = strstr(connected_ptr, "SSID='");
        if(ssid_ptr) {
            ssid_ptr += strlen("SSID='");
            const char* end = strchr(ssid_ptr, '\'');
            if(end && end > ssid_ptr) {
                char ascii_ssid[SCAN_SSID_MAX_LEN];
                size_t len = (size_t)(end - ssid_ptr);
                if(len >= sizeof(ascii_ssid)) len = sizeof(ascii_ssid) - 1;
                memcpy(ascii_ssid, ssid_ptr, len);
                ascii_ssid[len] = '\0';
                char ascii_clean[SCAN_SSID_MAX_LEN];
                simple_app_utf8_to_ascii_pl(ascii_ssid, ascii_clean, sizeof(ascii_clean));
                strncpy(status->status_ssid, ascii_clean, sizeof(status->status_ssid) - 1);
                status->status_ssid[sizeof(status->status_ssid) - 1] = '\0';
                simple_app_trim(status->status_ssid);
            }
        }
        strncpy(status->status_note, "Verified", sizeof(status->status_note) - 1);
        status->status_note[sizeof(status->status_note) - 1] = '\0';
        return;
    }

    if(strstr(cursor, verified_marker)) {
        strncpy(status->status_note, "Verified", sizeof(status->status_note) - 1);
        status->status_note[sizeof(status->status_note) - 1] = '\0';
        return;
    }

    if(strstr(cursor, wrong_marker)) {
        strncpy(status->status_note, "Wrong", sizeof(status->status_note) - 1);
        status->status_note[sizeof(status->status_note) - 1] = '\0';
        status->last_password[0] = '\0';
        status->password_count = 0;
        return;
    }

    if(strstr(cursor, shutdown_marker)) {
        strncpy(status->status_note, "Stopped", sizeof(status->status_note) - 1);
        status->status_note[sizeof(status->status_note) - 1] = '\0';
        return;
    }

    if(strstr(cursor, saved_marker)) {
        strncpy(status->status_note, "Verified", sizeof(status->status_note) - 1);
        status->status_note[sizeof(status->status_note) - 1] = '\0';
        simple_app_show_status_message(app, "Password OK\nClosing Evil", 2000, true);
        return;
    }
}

static void simple_app_portal_status_feed(SimpleApp* app, char ch) {
    if(!app || !app->portal_status) return;
    if(ch == '\r') return;

    if(ch == '\n') {
        if(app->portal_status->line_length > 0) {
            app->portal_status->line_buffer[app->portal_status->line_length] = '\0';
            simple_app_process_portal_status_line(app, app->portal_status->line_buffer);
            app->portal_status->line_length = 0;
        }
        return;
    }

    if(app->portal_status->line_length + 1 >= sizeof(app->portal_status->line_buffer)) {
        app->portal_status->line_length = 0;
        return;
    }

    app->portal_status->line_buffer[app->portal_status->line_length++] = ch;
}

static void simple_app_evil_twin_status_feed(SimpleApp* app, char ch) {
    if(!app || !app->evil_twin_status) return;
    if(ch == '\r') return;

    if(ch == '\n') {
        if(app->evil_twin_status->line_length > 0) {
            app->evil_twin_status->line_buffer[app->evil_twin_status->line_length] = '\0';
            simple_app_process_evil_twin_status_line(app, app->evil_twin_status->line_buffer);
            app->evil_twin_status->line_length = 0;
        }
        return;
    }

    if(app->evil_twin_status->line_length + 1 >= sizeof(app->evil_twin_status->line_buffer)) {
        app->evil_twin_status->line_length = 0;
        return;
    }

    app->evil_twin_status->line_buffer[app->evil_twin_status->line_length++] = ch;
}

static bool simple_app_evil_twin_pass_alloc_entries(SimpleApp* app) {
    if(!app) return false;
    if(app->evil_twin_pass_entries) return true;
    app->evil_twin_pass_entries = calloc(EVIL_TWIN_PASS_MAX_ENTRIES, sizeof(EvilTwinPassEntry));
    return app->evil_twin_pass_entries != NULL;
}

static void simple_app_evil_twin_pass_free_entries(SimpleApp* app) {
    if(!app || !app->evil_twin_pass_entries) return;
    free(app->evil_twin_pass_entries);
    app->evil_twin_pass_entries = NULL;
}

static void simple_app_reset_evil_twin_pass_listing(SimpleApp* app) {
    if(!app) return;
    simple_app_evil_twin_pass_free_entries(app);
    app->evil_twin_pass_listing_active = false;
    app->evil_twin_pass_count = 0;
    app->evil_twin_pass_index = 0;
    app->evil_twin_pass_offset = 0;
    app->evil_twin_pass_list_length = 0;
    app->evil_twin_qr_valid = false;
    app->evil_twin_qr_error[0] = '\0';
    app->evil_twin_qr_ssid[0] = '\0';
    app->evil_twin_qr_pass[0] = '\0';
}

static void simple_app_request_evil_twin_pass_list(SimpleApp* app) {
    if(!app) return;
    simple_app_reset_evil_twin_pass_listing(app);
    simple_app_prepare_password_buffers(app);
    if(!simple_app_evil_twin_pass_alloc_entries(app)) {
        simple_app_show_status_message(app, "OOM: WiFi list", 2000, true);
        return;
    }
    app->evil_twin_pass_listing_active = true;
    app->screen = ScreenEvilTwinPassList;
    simple_app_send_command_quiet(app, "show_pass evil");
    if(app->viewport) {
        view_port_update(app->viewport);
    }
}

static void simple_app_process_evil_twin_pass_line(SimpleApp* app, const char* line) {
    if(!app || !line || !app->evil_twin_pass_entries) return;
    if(app->evil_twin_pass_count >= EVIL_TWIN_PASS_MAX_ENTRIES) return;

    const char* cursor = line;
    while(*cursor == ' ' || *cursor == '\t') {
        cursor++;
    }
    if(*cursor != '"') return;

    const char* ssid_start = strchr(cursor, '"');
    if(!ssid_start) return;
    ssid_start++;
    const char* ssid_end = strchr(ssid_start, '"');
    if(!ssid_end) return;
    const char* pass_start = strchr(ssid_end + 1, '"');
    if(!pass_start) return;
    pass_start++;
    const char* pass_end = strchr(pass_start, '"');
    if(!pass_end) return;

    char raw_ssid[SCAN_SSID_MAX_LEN];
    char raw_pass[EVIL_TWIN_PASS_MAX_LEN];
    size_t ssid_len = (size_t)(ssid_end - ssid_start);
    if(ssid_len >= sizeof(raw_ssid)) ssid_len = sizeof(raw_ssid) - 1;
    memcpy(raw_ssid, ssid_start, ssid_len);
    raw_ssid[ssid_len] = '\0';
    size_t pass_len = (size_t)(pass_end - pass_start);
    if(pass_len >= sizeof(raw_pass)) pass_len = sizeof(raw_pass) - 1;
    memcpy(raw_pass, pass_start, pass_len);
    raw_pass[pass_len] = '\0';

    char ascii_ssid[SCAN_SSID_MAX_LEN];
    char ascii_pass[EVIL_TWIN_PASS_MAX_LEN];
    simple_app_utf8_to_ascii_pl(raw_ssid, ascii_ssid, sizeof(ascii_ssid));
    simple_app_utf8_to_ascii_pl(raw_pass, ascii_pass, sizeof(ascii_pass));
    simple_app_trim(ascii_ssid);
    simple_app_trim(ascii_pass);

    EvilTwinPassEntry* entry = &app->evil_twin_pass_entries[app->evil_twin_pass_count];
    entry->id = (uint16_t)(app->evil_twin_pass_count + 1);
    simple_app_copy_field(entry->ssid, sizeof(entry->ssid), ascii_ssid, "<hidden>");
    simple_app_copy_field(entry->pass, sizeof(entry->pass), ascii_pass, "");
    app->evil_twin_pass_count++;
}

static void simple_app_evil_twin_pass_feed(SimpleApp* app, char ch) {
    if(!app || !app->evil_twin_pass_listing_active) return;
    if(ch == '\r') return;

    bool updated = false;

    if(ch == '>') {
        if(app->evil_twin_pass_list_length > 0) {
            app->evil_twin_pass_list_buffer[app->evil_twin_pass_list_length] = '\0';
            simple_app_process_evil_twin_pass_line(app, app->evil_twin_pass_list_buffer);
            app->evil_twin_pass_list_length = 0;
            updated = true;
        }
        app->evil_twin_pass_listing_active = false;
        app->last_command_sent = false;
        updated = true;
        if(updated && app->screen == ScreenEvilTwinPassList && app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(ch == '\n') {
        if(app->evil_twin_pass_list_length > 0) {
            app->evil_twin_pass_list_buffer[app->evil_twin_pass_list_length] = '\0';
            simple_app_process_evil_twin_pass_line(app, app->evil_twin_pass_list_buffer);
            updated = true;
        }
        app->evil_twin_pass_list_length = 0;
        if(updated && app->screen == ScreenEvilTwinPassList && app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(app->evil_twin_pass_list_length + 1 >= sizeof(app->evil_twin_pass_list_buffer)) {
        app->evil_twin_pass_list_length = 0;
        return;
    }

    app->evil_twin_pass_list_buffer[app->evil_twin_pass_list_length++] = ch;
}

static void simple_app_evil_twin_pass_sync_offset(SimpleApp* app) {
    if(!app) return;
    if(app->evil_twin_pass_count == 0) {
        app->evil_twin_pass_index = 0;
        app->evil_twin_pass_offset = 0;
        return;
    }
    if(app->evil_twin_pass_index >= app->evil_twin_pass_count) {
        app->evil_twin_pass_index = app->evil_twin_pass_count - 1;
    }
    size_t visible = EVIL_TWIN_PASS_VISIBLE_LINES;
    if(visible == 0) visible = 1;
    if(visible > app->evil_twin_pass_count) {
        visible = app->evil_twin_pass_count;
    }
    size_t max_offset =
        (app->evil_twin_pass_count > visible) ? (app->evil_twin_pass_count - visible) : 0;
    if(app->evil_twin_pass_offset > max_offset) {
        app->evil_twin_pass_offset = max_offset;
    }
    if(app->evil_twin_pass_index < app->evil_twin_pass_offset) {
        app->evil_twin_pass_offset = app->evil_twin_pass_index;
    } else if(app->evil_twin_pass_index >= app->evil_twin_pass_offset + visible) {
        app->evil_twin_pass_offset = app->evil_twin_pass_index - visible + 1;
    }
    if(app->evil_twin_pass_offset > max_offset) {
        app->evil_twin_pass_offset = max_offset;
    }
}

static size_t simple_app_escape_wifi_field(const char* src, char* dst, size_t dst_size) {
    if(!dst || dst_size == 0) return 0;
    size_t out = 0;
    if(!src) {
        dst[0] = '\0';
        return 0;
    }
    for(size_t i = 0; src[i] != '\0' && out + 1 < dst_size; i++) {
        char ch = src[i];
        bool needs_escape = (ch == '\\' || ch == ';' || ch == ',' || ch == ':');
        if(needs_escape) {
            if(out + 2 >= dst_size) break;
            dst[out++] = '\\';
        }
        dst[out++] = ch;
    }
    dst[out] = '\0';
    return out;
}

static bool simple_app_build_evil_twin_qr(SimpleApp* app, const char* ssid, const char* pass) {
    if(!app || !ssid) return false;
    if(!simple_app_alloc_evil_twin_qr_buffers(app)) {
        app->evil_twin_qr_valid = false;
        strncpy(app->evil_twin_qr_error, "QR OOM", sizeof(app->evil_twin_qr_error) - 1);
        app->evil_twin_qr_error[sizeof(app->evil_twin_qr_error) - 1] = '\0';
        return false;
    }

    simple_app_copy_field(app->evil_twin_qr_ssid, sizeof(app->evil_twin_qr_ssid), ssid, "");
    simple_app_copy_field(app->evil_twin_qr_pass, sizeof(app->evil_twin_qr_pass), pass, "");

    char ssid_esc[SCAN_SSID_MAX_LEN * 2];
    char pass_esc[EVIL_TWIN_PASS_MAX_LEN * 2];
    simple_app_escape_wifi_field(ssid, ssid_esc, sizeof(ssid_esc));
    simple_app_escape_wifi_field(pass ? pass : "", pass_esc, sizeof(pass_esc));

    char qr_text[EVIL_TWIN_QR_TEXT_MAX];
    int needed = 0;
    if(pass_esc[0] == '\0') {
        needed = snprintf(qr_text, sizeof(qr_text), "WIFI:T:nopass;S:%s;;", ssid_esc);
    } else {
        needed = snprintf(qr_text, sizeof(qr_text), "WIFI:T:WPA;S:%s;P:%s;;", ssid_esc, pass_esc);
    }
    if(needed < 0 || (size_t)needed >= sizeof(qr_text)) {
        app->evil_twin_qr_valid = false;
        strncpy(
            app->evil_twin_qr_error, "SSID/pass too long", sizeof(app->evil_twin_qr_error) - 1);
        app->evil_twin_qr_error[sizeof(app->evil_twin_qr_error) - 1] = '\0';
        return false;
    }

    bool ok = qrcodegen_encodeText(
        qr_text,
        app->evil_twin_qr_temp,
        app->evil_twin_qr,
        qrcodegen_Ecc_MEDIUM,
        1,
        EVIL_TWIN_QR_MAX_VERSION,
        -1,
        true);

    if(!ok) {
        app->evil_twin_qr_valid = false;
        strncpy(app->evil_twin_qr_error, "QR too long", sizeof(app->evil_twin_qr_error) - 1);
        app->evil_twin_qr_error[sizeof(app->evil_twin_qr_error) - 1] = '\0';
        return false;
    }

    app->evil_twin_qr_valid = true;
    app->evil_twin_qr_error[0] = '\0';
    return true;
}

static void simple_app_draw_evil_twin_pass_list(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "Evil Twin QR");
    canvas_set_font(canvas, FontSecondary);

    if(app->evil_twin_pass_listing_active && app->evil_twin_pass_count == 0) {
        canvas_draw_str_aligned(
            canvas, DISPLAY_WIDTH / 2, 38, AlignCenter, AlignCenter, "Loading...");
        return;
    }

    if(app->evil_twin_pass_count == 0) {
        canvas_draw_str_aligned(
            canvas, DISPLAY_WIDTH / 2, 38, AlignCenter, AlignCenter, "No entries");
        return;
    }

    simple_app_evil_twin_pass_sync_offset(app);

    uint8_t y = 24;
    size_t visible = 4;
    if(visible == 0) visible = 1;
    if(visible > app->evil_twin_pass_count) {
        visible = app->evil_twin_pass_count;
    }

    for(size_t i = 0; i < visible; i++) {
        size_t idx = app->evil_twin_pass_offset + i;
        if(idx >= app->evil_twin_pass_count) break;
        EvilTwinPassEntry* entry = &app->evil_twin_pass_entries[idx];
        char line[48];
        snprintf(line, sizeof(line), "%u %s", (unsigned)entry->id, entry->ssid);
        simple_app_truncate_text(line, 24);
        if(idx == app->evil_twin_pass_index) {
            canvas_draw_str(canvas, 2, y, ">");
        }
        canvas_draw_str(canvas, 12, y, line);
        y += HINT_LINE_HEIGHT;
    }

    if(app->evil_twin_pass_count > visible) {
        bool show_up = (app->evil_twin_pass_offset > 0);
        bool show_down = (app->evil_twin_pass_offset + visible < app->evil_twin_pass_count);
        if(show_up || show_down) {
            uint8_t arrow_x = DISPLAY_WIDTH - 6;
            int16_t content_top = 24;
            int16_t content_bottom =
                26 + (int16_t)((visible > 0 ? (visible - 1) : 0) * HINT_LINE_HEIGHT);
            simple_app_draw_scroll_hints(
                canvas, arrow_x, content_top, content_bottom, show_up, show_down);
        }
    }
}

static void simple_app_handle_evil_twin_pass_list_input(SimpleApp* app, InputKey key) {
    if(!app) return;

    if(key == InputKeyBack || key == InputKeyLeft) {
        simple_app_reset_evil_twin_pass_listing(app);
        app->screen = ScreenEvilTwinMenu;
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(app->evil_twin_pass_count == 0) {
        if(key == InputKeyOk && app->evil_twin_pass_listing_active) {
            simple_app_show_status_message(app, "Wait for list\ncompletion", 1200, true);
        }
        return;
    }

    if(key == InputKeyUp) {
        if(app->evil_twin_pass_index > 0) {
            app->evil_twin_pass_index--;
        } else {
            app->evil_twin_pass_index = app->evil_twin_pass_count - 1;
        }
        simple_app_evil_twin_pass_sync_offset(app);
    } else if(key == InputKeyDown) {
        if(app->evil_twin_pass_index + 1 < app->evil_twin_pass_count) {
            app->evil_twin_pass_index++;
        } else {
            app->evil_twin_pass_index = 0;
        }
        simple_app_evil_twin_pass_sync_offset(app);
    } else if(key == InputKeyOk) {
        if(app->evil_twin_pass_listing_active) {
            simple_app_show_status_message(app, "Wait for list\ncompletion", 1200, true);
            return;
        }
        if(app->evil_twin_pass_index < app->evil_twin_pass_count) {
            EvilTwinPassEntry* entry = &app->evil_twin_pass_entries[app->evil_twin_pass_index];
            simple_app_build_evil_twin_qr(app, entry->ssid, entry->pass);
            app->screen = ScreenEvilTwinPassQr;
        }
    } else {
        return;
    }

    if(app->viewport) {
        view_port_update(app->viewport);
    }
}

static void simple_app_draw_evil_twin_pass_qr(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;

    const uint8_t qr_x = 0;
    const uint8_t qr_y = 8;
    const uint8_t qr_w = 32;
    const uint8_t qr_h = 32;

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, qr_x, qr_y, qr_w, qr_h);
    canvas_set_color(canvas, ColorBlack);

    if(app->evil_twin_qr_valid) {
        if(!app->evil_twin_qr) {
            app->evil_twin_qr_valid = false;
            strncpy(app->evil_twin_qr_error, "QR OOM", sizeof(app->evil_twin_qr_error) - 1);
            app->evil_twin_qr_error[sizeof(app->evil_twin_qr_error) - 1] = '\0';
        }
    }

    if(app->evil_twin_qr_valid) {
        int size = qrcodegen_getSize(app->evil_twin_qr);
        int scale_x = qr_w / size;
        int scale_y = qr_h / size;
        int scale = (scale_x < scale_y) ? scale_x : scale_y;
        if(scale < 1) {
            app->evil_twin_qr_valid = false;
            strncpy(app->evil_twin_qr_error, "QR too big", sizeof(app->evil_twin_qr_error) - 1);
            app->evil_twin_qr_error[sizeof(app->evil_twin_qr_error) - 1] = '\0';
        } else {
            int qr_size = size * scale;
            int offset_x = qr_x + (qr_w - qr_size) / 2;
            int offset_y = qr_y + (qr_h - qr_size) / 2;
            for(int y = 0; y < size; y++) {
                for(int x = 0; x < size; x++) {
                    if(qrcodegen_getModule(app->evil_twin_qr, x, y)) {
                        canvas_draw_box(
                            canvas, offset_x + x * scale, offset_y + y * scale, scale, scale);
                    }
                }
            }
        }
    }

    if(!app->evil_twin_qr_valid) {
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, qr_w / 2, 28, AlignCenter, AlignCenter, "QR error");
        if(app->evil_twin_qr_error[0] != '\0') {
            canvas_draw_str_aligned(
                canvas, qr_w / 2, 40, AlignCenter, AlignCenter, app->evil_twin_qr_error);
        }
    }

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    char ssid_line1[20];
    char ssid_line2[20];
    char pass_line1[20];
    char pass_line2[20];
    ssid_line1[0] = '\0';
    ssid_line2[0] = '\0';
    pass_line1[0] = '\0';
    pass_line2[0] = '\0';

    const size_t wrap_len = 18;
    size_t ssid_len = strlen(app->evil_twin_qr_ssid);
    size_t pass_len = strlen(app->evil_twin_qr_pass);
    size_t ssid_copy = (ssid_len > wrap_len) ? wrap_len : ssid_len;
    size_t pass_copy = (pass_len > wrap_len) ? wrap_len : pass_len;
    memcpy(ssid_line1, app->evil_twin_qr_ssid, ssid_copy);
    ssid_line1[ssid_copy] = '\0';
    if(ssid_len > wrap_len) {
        size_t ssid_copy2 = ssid_len - wrap_len;
        if(ssid_copy2 >= sizeof(ssid_line2)) ssid_copy2 = sizeof(ssid_line2) - 1;
        memcpy(ssid_line2, app->evil_twin_qr_ssid + wrap_len, ssid_copy2);
        ssid_line2[ssid_copy2] = '\0';
    }

    memcpy(pass_line1, app->evil_twin_qr_pass, pass_copy);
    pass_line1[pass_copy] = '\0';
    if(pass_len > wrap_len) {
        size_t pass_copy2 = pass_len - wrap_len;
        if(pass_copy2 >= sizeof(pass_line2)) pass_copy2 = sizeof(pass_line2) - 1;
        memcpy(pass_line2, app->evil_twin_qr_pass + wrap_len, pass_copy2);
        pass_line2[pass_copy2] = '\0';
    }

    canvas_draw_str(canvas, 36, 10, "SSID");
    canvas_draw_str(canvas, 36, 20, ssid_line1);
    if(ssid_line2[0] != '\0') {
        canvas_draw_str(canvas, 36, 30, ssid_line2);
    }
    canvas_draw_str(canvas, 36, 42, "PASS");
    canvas_draw_str(canvas, 36, 52, pass_line1);
    if(pass_line2[0] != '\0') {
        canvas_draw_str(canvas, 36, 62, pass_line2);
    }
}

static void simple_app_handle_evil_twin_pass_qr_input(SimpleApp* app, InputKey key) {
    if(!app) return;
    if(key == InputKeyBack || key == InputKeyLeft || key == InputKeyOk) {
        simple_app_free_evil_twin_qr_buffers(app);
        app->screen = ScreenEvilTwinPassList;
        if(app->viewport) {
            view_port_update(app->viewport);
        }
    }
}

static void simple_app_reset_passwords_listing(SimpleApp* app) {
    if(!app) return;
    app->passwords_listing_active = false;
    app->passwords_line_count = 0;
    app->passwords_scroll = 0;
    app->passwords_scroll_x = 0;
    app->passwords_max_line_len = 0;
    app->passwords_line_length = 0;
    app->passwords_title[0] = '\0';
    app->passwords_source = PasswordsSourcePortal;
    app->passwords_select_index = 0;
    app->passwords_select_return_screen = ScreenMenu;
    simple_app_passwords_free_lines(app);
}

static bool simple_app_passwords_alloc_lines(SimpleApp* app) {
    if(!app) return false;
    if(app->passwords_lines) return true;
    app->passwords_lines = calloc(PASSWORDS_MAX_LINES, sizeof(app->passwords_lines[0]));
    return app->passwords_lines != NULL;
}

static void simple_app_passwords_free_lines(SimpleApp* app) {
    if(!app || !app->passwords_lines) return;
    free(app->passwords_lines);
    app->passwords_lines = NULL;
}

static void
    simple_app_request_passwords(SimpleApp* app, AppScreen return_screen, PasswordsSource source) {
    if(!app) return;
    simple_app_reset_passwords_listing(app);
    simple_app_prepare_password_buffers(app);
    if(!simple_app_passwords_alloc_lines(app)) {
        simple_app_show_status_message(app, "OOM: passwords", 1500, true);
        return;
    }
    app->passwords_listing_active = true;
    app->passwords_return_screen = return_screen;
    app->passwords_source = source;
    const char* title = (source == PasswordsSourceEvilTwin) ? "Evil Twin" : "Portal";
    strncpy(app->passwords_title, title, sizeof(app->passwords_title) - 1);
    app->passwords_title[sizeof(app->passwords_title) - 1] = '\0';
    app->screen = ScreenPasswords;
    if(source == PasswordsSourceEvilTwin) {
        simple_app_send_command_quiet(app, "show_pass evil");
    } else {
        simple_app_send_command_quiet(app, "show_pass portal");
    }
    if(app->viewport) {
        view_port_update(app->viewport);
    }
}

static void simple_app_process_passwords_line(SimpleApp* app, const char* line) {
    if(!app || !line || !app->passwords_lines) return;
    if(app->passwords_line_count >= PASSWORDS_MAX_LINES) return;

    char ascii_line[PASSWORDS_LINE_CHAR_LIMIT];
    simple_app_utf8_to_ascii_pl(line, ascii_line, sizeof(ascii_line));
    simple_app_trim(ascii_line);
    if(strncmp(ascii_line, "TX:", 3) == 0) return;
    if(ascii_line[0] == '\0') {
        if(app->passwords_line_count < PASSWORDS_MAX_LINES) {
            app->passwords_lines[app->passwords_line_count][0] = '\0';
            app->passwords_line_count++;
        }
        return;
    }

    strncpy(
        app->passwords_lines[app->passwords_line_count],
        ascii_line,
        PASSWORDS_LINE_CHAR_LIMIT - 1);
    app->passwords_lines[app->passwords_line_count][PASSWORDS_LINE_CHAR_LIMIT - 1] = '\0';
    app->passwords_line_count++;
    size_t line_len = strlen(app->passwords_lines[app->passwords_line_count - 1]);
    if(line_len > app->passwords_max_line_len) {
        app->passwords_max_line_len = line_len;
    }
}

static void simple_app_passwords_feed(SimpleApp* app, char ch) {
    if(!app || !app->passwords_listing_active) return;
    if(ch == '\r') return;

    bool updated = false;

    if(ch == '>') {
        if(app->passwords_line_length > 0) {
            app->passwords_line_buffer[app->passwords_line_length] = '\0';
            simple_app_process_passwords_line(app, app->passwords_line_buffer);
            app->passwords_line_length = 0;
            updated = true;
        }
        app->passwords_listing_active = false;
        updated = true;
        if(updated && app->screen == ScreenPasswords && app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(ch == '\n') {
        if(app->passwords_line_length > 0) {
            app->passwords_line_buffer[app->passwords_line_length] = '\0';
            simple_app_process_passwords_line(app, app->passwords_line_buffer);
            updated = true;
        }
        app->passwords_line_length = 0;
        if(updated && app->screen == ScreenPasswords && app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(app->passwords_line_length + 1 >= sizeof(app->passwords_line_buffer)) {
        app->passwords_line_length = 0;
        return;
    }

    app->passwords_line_buffer[app->passwords_line_length++] = ch;
}

static void simple_app_draw_passwords(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;
    if(!app->passwords_lines) return;

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    char header[32];
    if(app->passwords_title[0] != '\0') {
        snprintf(header, sizeof(header), "%s Pass", app->passwords_title);
    } else {
        snprintf(header, sizeof(header), "Passwords");
    }
    canvas_draw_str(canvas, 4, 12, header);
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH - 2, 12, AlignRight, AlignBottom, "Back");

    if(app->passwords_listing_active && app->passwords_line_count == 0) {
        canvas_draw_str_aligned(
            canvas, DISPLAY_WIDTH / 2, 36, AlignCenter, AlignCenter, "Loading...");
        return;
    }

    if(app->passwords_line_count == 0) {
        canvas_draw_str_aligned(
            canvas, DISPLAY_WIDTH / 2, 36, AlignCenter, AlignCenter, "No passwords");
        return;
    }

    size_t max_scroll = (app->passwords_line_count > PASSWORDS_VISIBLE_LINES) ?
                            (app->passwords_line_count - PASSWORDS_VISIBLE_LINES) :
                            0;
    if(app->passwords_scroll > max_scroll) {
        app->passwords_scroll = max_scroll;
    }
    size_t max_scroll_x = (app->passwords_max_line_len > PASSWORDS_LINE_VISIBLE_CHARS) ?
                              (app->passwords_max_line_len - PASSWORDS_LINE_VISIBLE_CHARS) :
                              0;
    if(app->passwords_scroll_x > max_scroll_x) {
        app->passwords_scroll_x = max_scroll_x;
    }

    uint8_t y = 26;
    for(size_t i = 0; i < PASSWORDS_VISIBLE_LINES; i++) {
        size_t idx = app->passwords_scroll + i;
        if(idx >= app->passwords_line_count) break;
        const char* line = app->passwords_lines[idx];
        size_t line_len = strlen(line);
        size_t offset = app->passwords_scroll_x;
        if(offset > line_len) offset = line_len;
        char window[PASSWORDS_LINE_VISIBLE_CHARS + 1];
        size_t copy_len = line_len - offset;
        if(copy_len > PASSWORDS_LINE_VISIBLE_CHARS) {
            copy_len = PASSWORDS_LINE_VISIBLE_CHARS;
        }
        memcpy(window, line + offset, copy_len);
        window[copy_len] = '\0';
        canvas_draw_str(canvas, 2, y, window);
        y += HINT_LINE_HEIGHT;
    }

    if(app->passwords_line_count > PASSWORDS_VISIBLE_LINES) {
        bool show_up = (app->passwords_scroll > 0);
        bool show_down = (app->passwords_scroll < max_scroll);
        if(show_up || show_down) {
            uint8_t arrow_x = DISPLAY_WIDTH - 6;
            int16_t content_top = 26;
            int16_t content_bottom =
                26 + (int16_t)((PASSWORDS_VISIBLE_LINES - 1) * HINT_LINE_HEIGHT);
            simple_app_draw_scroll_hints(
                canvas, arrow_x, content_top, content_bottom, show_up, show_down);
        }
    }
}

static void simple_app_handle_passwords_input(SimpleApp* app, InputKey key) {
    if(!app) return;
    if(key == InputKeyBack) {
        app->passwords_listing_active = false;
        simple_app_passwords_free_lines(app);
        app->passwords_line_count = 0;
        app->passwords_scroll = 0;
        app->passwords_scroll_x = 0;
        app->passwords_max_line_len = 0;
        app->screen = app->passwords_return_screen;
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(app->passwords_line_count == 0 || !app->passwords_lines) return;

    size_t max_scroll = (app->passwords_line_count > PASSWORDS_VISIBLE_LINES) ?
                            (app->passwords_line_count - PASSWORDS_VISIBLE_LINES) :
                            0;
    size_t max_scroll_x = (app->passwords_max_line_len > PASSWORDS_LINE_VISIBLE_CHARS) ?
                              (app->passwords_max_line_len - PASSWORDS_LINE_VISIBLE_CHARS) :
                              0;

    if(key == InputKeyUp) {
        if(app->passwords_scroll > 0) {
            app->passwords_scroll--;
        }
    } else if(key == InputKeyDown) {
        if(app->passwords_scroll < max_scroll) {
            app->passwords_scroll++;
        }
    } else if(key == InputKeyLeft) {
        if(app->passwords_scroll_x > 0) {
            app->passwords_scroll_x--;
        }
    } else if(key == InputKeyRight) {
        if(app->passwords_scroll_x < max_scroll_x) {
            app->passwords_scroll_x++;
        }
    } else {
        return;
    }

    if(app->viewport) {
        view_port_update(app->viewport);
    }
}

static void simple_app_draw_passwords_select(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 4, 14, "Show Passwords");

    canvas_set_font(canvas, FontSecondary);
    const char* options[2] = {"Portal (portals.txt)", "Evil Twin (eviltwin.txt)"};
    uint8_t y = 34;
    for(size_t idx = 0; idx < 2; idx++) {
        if(app->passwords_select_index == idx) {
            canvas_draw_str(canvas, 2, y, ">");
        }
        canvas_draw_str(canvas, 14, y, options[idx]);
        y += 12;
    }
    canvas_draw_str(canvas, 2, 62, "OK select, Back");
}

static void simple_app_handle_passwords_select_input(SimpleApp* app, InputKey key) {
    if(!app) return;

    if(key == InputKeyBack) {
        app->screen = app->passwords_select_return_screen;
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(key == InputKeyUp || key == InputKeyDown) {
        app->passwords_select_index = (app->passwords_select_index == 0) ? 1 : 0;
    } else if(key == InputKeyOk) {
        PasswordsSource source = (app->passwords_select_index == 1) ? PasswordsSourceEvilTwin :
                                                                      PasswordsSourcePortal;
        simple_app_request_passwords(app, app->passwords_select_return_screen, source);
        return;
    } else {
        return;
    }

    if(app->viewport) {
        view_port_update(app->viewport);
    }
}

static void simple_app_request_portal_ssid_list(SimpleApp* app) {
    if(!app) return;
    simple_app_reset_portal_ssid_listing(app);
    if(!simple_app_portal_alloc_ssid_entries(app)) {
        simple_app_show_status_message(app, "OOM: SSID list", 2000, true);
        return;
    }
    app->portal_ssid_listing_active = true;
    simple_app_show_status_message(app, "Listing SSID...", 0, false);
    simple_app_send_command(app, "list_ssid", false);
    if(app->viewport) {
        view_port_update(app->viewport);
    }
}

static void simple_app_copy_portal_ssid(SimpleApp* app, const char* source) {
    if(!app) return;
    if(!source) {
        app->portal_ssid[0] = '\0';
        return;
    }

    size_t dst = 0;
    size_t max_len = sizeof(app->portal_ssid);
    for(size_t i = 0; source[i] != '\0' && dst + 1 < max_len; i++) {
        char ch = source[i];
        if((unsigned char)ch < 32) {
            continue;
        }
        if(ch == '"') {
            ch = '\'';
        }
        app->portal_ssid[dst++] = ch;
    }
    app->portal_ssid[dst] = '\0';
    simple_app_trim(app->portal_ssid);
}

static void simple_app_portal_prompt_ssid(SimpleApp* app) {
    if(!app) return;
    if(app->portal_input_active) return;
    app->portal_input_requested = true;
}

static void simple_app_portal_sync_offset(SimpleApp* app) {
    if(!app) return;
    size_t total = PORTAL_MENU_OPTION_COUNT;
    size_t visible = PORTAL_VISIBLE_COUNT;
    if(visible == 0) visible = 1;
    if(visible > total) visible = total;
    if(total == 0) {
        app->portal_menu_index = 0;
        app->portal_menu_offset = 0;
        return;
    }
    if(app->portal_menu_index >= total) {
        app->portal_menu_index = total - 1;
    }
    size_t max_offset = (total > visible) ? (total - visible) : 0;
    if(app->portal_menu_offset > max_offset) {
        app->portal_menu_offset = max_offset;
    }
    if(app->portal_menu_index < app->portal_menu_offset) {
        app->portal_menu_offset = app->portal_menu_index;
    } else if(app->portal_menu_index >= app->portal_menu_offset + visible) {
        app->portal_menu_offset = app->portal_menu_index - visible + 1;
    }
    if(app->portal_menu_offset > max_offset) {
        app->portal_menu_offset = max_offset;
    }
}

typedef struct {
    ViewDispatcher* dispatcher;
    bool accepted;
} SimpleAppPortalInputContext;

static void simple_app_portal_text_input_result(void* context) {
    SimpleAppPortalInputContext* ctx = context;
    if(!ctx || !ctx->dispatcher) return;
    ctx->accepted = true;
    view_dispatcher_stop(ctx->dispatcher);
}

static bool simple_app_portal_text_input_navigation(void* context) {
    SimpleAppPortalInputContext* ctx = context;
    if(!ctx || !ctx->dispatcher) return false;
    ctx->accepted = false;
    view_dispatcher_stop(ctx->dispatcher);
    return true;
}

static bool simple_app_portal_run_text_input(SimpleApp* app) {
    if(!app || !app->gui || !app->viewport) return false;

    bool accepted = false;
    bool viewport_detached = false;

    char buffer[SCAN_SSID_MAX_LEN];
    strncpy(buffer, app->portal_ssid, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    char previous_ssid[SCAN_SSID_MAX_LEN];
    strncpy(previous_ssid, app->portal_ssid, sizeof(previous_ssid));
    previous_ssid[sizeof(previous_ssid) - 1] = '\0';

    ViewDispatcher* dispatcher = view_dispatcher_alloc();
    if(!dispatcher) return false;

    TextInput* text_input = text_input_alloc();
    if(!text_input) {
        view_dispatcher_free(dispatcher);
        return false;
    }

    SimpleAppPortalInputContext ctx = {
        .dispatcher = dispatcher,
        .accepted = false,
    };

    view_dispatcher_set_event_callback_context(dispatcher, &ctx);
    view_dispatcher_set_navigation_event_callback(
        dispatcher, simple_app_portal_text_input_navigation);

    text_input_set_header_text(text_input, "Portal SSID");
    text_input_set_result_callback(
        text_input, simple_app_portal_text_input_result, &ctx, buffer, sizeof(buffer), false);
    text_input_set_minimum_length(text_input, 1);

    view_dispatcher_add_view(dispatcher, 0, text_input_get_view(text_input));

    gui_remove_view_port(app->gui, app->viewport);
    viewport_detached = true;
    app->portal_input_active = true;

    view_dispatcher_attach_to_gui(dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(dispatcher, 0);
    view_dispatcher_run(dispatcher);

    view_dispatcher_remove_view(dispatcher, 0);
    view_dispatcher_free(dispatcher);
    text_input_free(text_input);

    if(viewport_detached) {
        gui_add_view_port(app->gui, app->viewport, GuiLayerFullscreen);
        view_port_update(app->viewport);
    }
    app->portal_input_active = false;

    if(ctx.accepted) {
        simple_app_copy_portal_ssid(app, buffer);
        if(strlen(app->portal_ssid) > 32) {
            strncpy(app->portal_ssid, previous_ssid, sizeof(app->portal_ssid) - 1);
            app->portal_ssid[sizeof(app->portal_ssid) - 1] = '\0';
            simple_app_show_status_message(app, "SSID too long\nMax 32 chars", 1500, true);
            return false;
        }
        accepted = true;
    }

    return accepted;
}

static void simple_app_draw_portal_ssid_popup(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas || !app->portal_ssid_popup_active) return;
    if(!app->portal_ssid_entries) return;

    const uint8_t bubble_x = 4;
    const uint8_t bubble_y = 4;
    const uint8_t bubble_w = DISPLAY_WIDTH - (bubble_x * 2);
    const uint8_t bubble_h = 56;
    const uint8_t radius = 9;

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_rbox(canvas, bubble_x, bubble_y, bubble_w, bubble_h, radius);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, bubble_x, bubble_y, bubble_w, bubble_h, radius);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, bubble_x + 8, bubble_y + 16, "Select SSID");

    canvas_set_font(canvas, FontSecondary);
    uint8_t list_y = bubble_y + 28;

    if(app->portal_ssid_count == 0) {
        canvas_draw_str(canvas, bubble_x + 10, list_y, "No presets found");
        canvas_draw_str(
            canvas, bubble_x + 10, (uint8_t)(list_y + HINT_LINE_HEIGHT), "Back returns");
        return;
    }

    size_t total_options = app->portal_ssid_count + 1; // manual option first
    size_t visible = total_options;
    if(visible > PORTAL_SSID_POPUP_VISIBLE_LINES) {
        visible = PORTAL_SSID_POPUP_VISIBLE_LINES;
    }

    if(app->portal_ssid_popup_offset >= total_options) {
        app->portal_ssid_popup_offset = (total_options > visible) ? (total_options - visible) : 0;
    }

    for(size_t i = 0; i < visible; i++) {
        size_t idx = app->portal_ssid_popup_offset + i;
        if(idx >= total_options) break;
        char line[48];
        if(idx == 0) {
            strncpy(line, "Manual select", sizeof(line) - 1);
            line[sizeof(line) - 1] = '\0';
        } else {
            const PortalSsidEntry* entry = &app->portal_ssid_entries[idx - 1];
            snprintf(line, sizeof(line), "%u %s", (unsigned)entry->id, entry->ssid);
        }
        simple_app_truncate_text(line, 28);
        uint8_t line_y = (uint8_t)(list_y + i * HINT_LINE_HEIGHT);
        if(idx == app->portal_ssid_popup_index) {
            canvas_draw_str(canvas, bubble_x + 4, line_y, ">");
        }
        canvas_draw_str(canvas, bubble_x + 14, line_y, line);
    }

    if(total_options > visible) {
        bool show_up = (app->portal_ssid_popup_offset > 0);
        bool show_down = (app->portal_ssid_popup_offset + visible < total_options);
        if(show_up || show_down) {
            uint8_t arrow_x = (uint8_t)(bubble_x + bubble_w - 10);
            int16_t content_top = list_y;
            int16_t content_bottom =
                list_y + (int16_t)((visible > 0 ? (visible - 1) : 0) * HINT_LINE_HEIGHT);
            int16_t min_base = bubble_y + 12;
            int16_t max_base = bubble_y + bubble_h - 12;
            simple_app_draw_scroll_hints_clamped(
                canvas,
                arrow_x,
                content_top,
                content_bottom,
                show_up,
                show_down,
                min_base,
                max_base);
        }
    }
}

static void simple_app_handle_portal_ssid_popup_event(SimpleApp* app, const InputEvent* event) {
    if(!app || !event || !app->portal_ssid_popup_active) return;
    if(!app->portal_ssid_entries) return;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat &&
       event->type != InputTypeLong)
        return;

    InputKey key = event->key;
    bool is_short = (event->type == InputTypeShort);

    size_t total_options = app->portal_ssid_count + 1;
    size_t visible = total_options;
    if(visible > PORTAL_SSID_POPUP_VISIBLE_LINES) {
        visible = PORTAL_SSID_POPUP_VISIBLE_LINES;
    }
    if(visible == 0) visible = 1;

    if(is_short && key == InputKeyBack) {
        app->portal_ssid_popup_active = false;
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(total_options == 0) {
        return;
    }

    if(key == InputKeyUp) {
        if(app->portal_ssid_popup_index > 0) {
            app->portal_ssid_popup_index--;
            if(app->portal_ssid_popup_index < app->portal_ssid_popup_offset) {
                app->portal_ssid_popup_offset = app->portal_ssid_popup_index;
            }
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
    } else if(key == InputKeyDown) {
        if(app->portal_ssid_popup_index + 1 < total_options) {
            app->portal_ssid_popup_index++;
            if(total_options > visible &&
               app->portal_ssid_popup_index >= app->portal_ssid_popup_offset + visible) {
                app->portal_ssid_popup_offset = app->portal_ssid_popup_index - visible + 1;
            }
            size_t max_offset = (total_options > visible) ? (total_options - visible) : 0;
            if(app->portal_ssid_popup_offset > max_offset) {
                app->portal_ssid_popup_offset = max_offset;
            }
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
    } else if(is_short && key == InputKeyOk) {
        size_t idx = app->portal_ssid_popup_index;
        if(idx == 0) {
            app->portal_ssid_popup_active = false;
            simple_app_portal_prompt_ssid(app);
        } else if(idx - 1 < app->portal_ssid_count) {
            const PortalSsidEntry* entry = &app->portal_ssid_entries[idx - 1];
            simple_app_copy_portal_ssid(app, entry->ssid);
            char message[64];
            snprintf(message, sizeof(message), "SSID set:\n%s", entry->ssid);
            simple_app_show_status_message(app, message, 1200, true);
            app->portal_menu_index = 1;
            simple_app_portal_sync_offset(app);
            app->portal_ssid_popup_active = false;
        }
        if(app->viewport) {
            view_port_update(app->viewport);
        }
    }
}

static void simple_app_start_portal(SimpleApp* app) {
    if(!app) return;

    if(app->portal_ssid_listing_active) {
        simple_app_show_status_message(app, "Wait for list\ncompletion", 1500, true);
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(app->karma_html_listing_active) {
        simple_app_show_status_message(app, "Wait for list\ncompletion", 1500, true);
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(app->portal_ssid[0] == '\0') {
        simple_app_show_status_message(app, "Set SSID first", 1200, true);
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(app->karma_selected_html_id == 0) {
        simple_app_show_status_message(app, "Select HTML file\nbefore starting", 1500, true);
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    char select_command[48];
    snprintf(
        select_command,
        sizeof(select_command),
        "select_html %u",
        (unsigned)app->karma_selected_html_id);
    simple_app_send_command(app, select_command, false);
    app->last_command_sent = false;

    char command[128];
    snprintf(command, sizeof(command), "start_portal \"%s\"", app->portal_ssid);
    simple_app_send_command(app, command, true);
}

static void simple_app_evil_twin_sync_offset(SimpleApp* app) {
    if(!app) return;
    size_t visible = EVIL_TWIN_VISIBLE_COUNT;
    if(visible == 0) visible = 1;
    if(visible > EVIL_TWIN_MENU_OPTION_COUNT) {
        visible = EVIL_TWIN_MENU_OPTION_COUNT;
    }
    if(app->evil_twin_menu_index >= EVIL_TWIN_MENU_OPTION_COUNT) {
        app->evil_twin_menu_index = EVIL_TWIN_MENU_OPTION_COUNT - 1;
    }
    size_t max_offset =
        (EVIL_TWIN_MENU_OPTION_COUNT > visible) ? (EVIL_TWIN_MENU_OPTION_COUNT - visible) : 0;
    if(app->evil_twin_menu_offset > max_offset) {
        app->evil_twin_menu_offset = max_offset;
    }
    if(app->evil_twin_menu_index < app->evil_twin_menu_offset) {
        app->evil_twin_menu_offset = app->evil_twin_menu_index;
    } else if(app->evil_twin_menu_index >= app->evil_twin_menu_offset + visible) {
        app->evil_twin_menu_offset = app->evil_twin_menu_index - visible + 1;
    }
    if(app->evil_twin_menu_offset > max_offset) {
        app->evil_twin_menu_offset = max_offset;
    }
}

static void simple_app_draw_evil_twin_menu(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 4, 14, "Evil Twin");

    canvas_set_font(canvas, FontSecondary);
    simple_app_evil_twin_sync_offset(app);

    size_t offset = app->evil_twin_menu_offset;
    size_t visible = EVIL_TWIN_VISIBLE_COUNT;
    if(visible == 0) visible = 1;
    if(visible > EVIL_TWIN_MENU_OPTION_COUNT) {
        visible = EVIL_TWIN_MENU_OPTION_COUNT;
    }

    uint8_t base_y = 30;
    uint8_t y = base_y;
    uint8_t list_bottom_y = base_y;

    for(size_t pos = 0; pos < visible; pos++) {
        size_t idx = offset + pos;
        if(idx >= EVIL_TWIN_MENU_OPTION_COUNT) break;

        const char* label = "Start Evil Twin";
        char detail[48];
        char detail_extra[48];
        detail[0] = '\0';
        detail_extra[0] = '\0';
        bool show_detail_line = false;
        bool show_detail_extra = false;

        switch(idx) {
        case 0: {
            label = "Target";
            if(app->scan_selected_count == 0) {
                snprintf(detail, sizeof(detail), "Select in Scanner");
                show_detail_line = true;
            } else {
                snprintf(
                    detail, sizeof(detail), "Selected: %u", (unsigned)app->scan_selected_count);
                show_detail_line = true;
                if(simple_app_get_first_selected_ssid(app, detail_extra, sizeof(detail_extra))) {
                    simple_app_truncate_text(detail_extra, 28);
                } else {
                    snprintf(detail_extra, sizeof(detail_extra), "<unknown>");
                }
                show_detail_extra = true;
            }
            break;
        }
        case 1:
            label = "Select HTML";
            if(app->evil_twin_selected_html_id > 0 &&
               app->evil_twin_selected_html_name[0] != '\0') {
                snprintf(detail, sizeof(detail), "Current: %s", app->evil_twin_selected_html_name);
            } else {
                snprintf(detail, sizeof(detail), "Current: <none>");
            }
            simple_app_truncate_text(detail, 26);
            show_detail_line = true;
            break;
        case 2:
            label = "Start Evil Twin";
            if(app->scan_selected_count == 0) {
                snprintf(detail, sizeof(detail), "Need target");
            } else if(app->evil_twin_selected_html_id == 0) {
                snprintf(detail, sizeof(detail), "Need HTML");
            } else {
                detail[0] = '\0';
            }
            simple_app_truncate_text(detail, 20);
            break;
        case 3:
            label = "WiFi QR";
            snprintf(detail, sizeof(detail), "eviltwin.txt");
            show_detail_line = true;
            break;
        default:
            label = "Show Passwords";
            snprintf(detail, sizeof(detail), "eviltwin.txt");
            show_detail_line = true;
            break;
        }

        if(app->evil_twin_menu_index == idx) {
            canvas_draw_str(canvas, 2, y, ">");
        }
        canvas_draw_str(canvas, 14, y, label);

        uint8_t item_height = 12;
        if(show_detail_line || detail[0] != '\0') {
            canvas_draw_str(canvas, 14, (uint8_t)(y + 10), detail);
            item_height += 10;
        }
        if(show_detail_extra) {
            canvas_draw_str(canvas, 14, (uint8_t)(y + item_height), detail_extra);
            item_height += 10;
        }
        y = (uint8_t)(y + item_height);
        list_bottom_y = y;
    }

    uint8_t arrow_x = DISPLAY_WIDTH - 6;
    if(offset > 0) {
        int16_t arrow_y = (int16_t)(base_y - 6);
        if(arrow_y < 12) arrow_y = 12;
        simple_app_draw_scroll_arrow(canvas, arrow_x, arrow_y, true);
    }
    if(offset + visible < EVIL_TWIN_MENU_OPTION_COUNT) {
        int16_t arrow_y = (int16_t)(list_bottom_y - 6);
        if(arrow_y > 60) arrow_y = 60;
        if(arrow_y < 16) arrow_y = 16;
        simple_app_draw_scroll_arrow(canvas, arrow_x, arrow_y, false);
    }
}

static void simple_app_draw_evil_twin_popup(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas || !app->evil_twin_popup_active) return;
    if(!app->evil_twin_html_entries) return;

    const uint8_t bubble_x = 4;
    const uint8_t bubble_y = 4;
    const uint8_t bubble_w = DISPLAY_WIDTH - (bubble_x * 2);
    const uint8_t bubble_h = 56;
    const uint8_t radius = 9;

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_rbox(canvas, bubble_x, bubble_y, bubble_w, bubble_h, radius);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, bubble_x, bubble_y, bubble_w, bubble_h, radius);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, bubble_x + 8, bubble_y + 16, "Select HTML");

    canvas_set_font(canvas, FontSecondary);
    uint8_t list_y = bubble_y + 28;

    if(app->evil_twin_html_count == 0) {
        canvas_draw_str(canvas, bubble_x + 10, list_y, "No HTML files");
        canvas_draw_str(
            canvas, bubble_x + 10, (uint8_t)(list_y + HINT_LINE_HEIGHT), "Back returns to menu");
        return;
    }

    size_t visible = app->evil_twin_html_count;
    if(visible > EVIL_TWIN_POPUP_VISIBLE_LINES) {
        visible = EVIL_TWIN_POPUP_VISIBLE_LINES;
    }

    if(app->evil_twin_html_popup_offset >= app->evil_twin_html_count) {
        app->evil_twin_html_popup_offset =
            (app->evil_twin_html_count > visible) ? (app->evil_twin_html_count - visible) : 0;
    }

    for(size_t i = 0; i < visible; i++) {
        size_t idx = app->evil_twin_html_popup_offset + i;
        if(idx >= app->evil_twin_html_count) break;
        const EvilTwinHtmlEntry* entry = &app->evil_twin_html_entries[idx];
        char line[48];
        snprintf(line, sizeof(line), "%u %s", (unsigned)entry->id, entry->name);
        simple_app_truncate_text(line, 28);
        uint8_t line_y = (uint8_t)(list_y + i * HINT_LINE_HEIGHT);
        if(idx == app->evil_twin_html_popup_index) {
            canvas_draw_str(canvas, bubble_x + 4, line_y, ">");
        }
        canvas_draw_str(canvas, bubble_x + 8, line_y, line);
    }

    if(app->evil_twin_html_count > EVIL_TWIN_POPUP_VISIBLE_LINES) {
        bool show_up = (app->evil_twin_html_popup_offset > 0);
        bool show_down = (app->evil_twin_html_popup_offset + visible < app->evil_twin_html_count);
        if(show_up || show_down) {
            uint8_t arrow_x = (uint8_t)(bubble_x + bubble_w - 10);
            int16_t content_top = list_y;
            int16_t content_bottom =
                list_y + (int16_t)((visible > 0 ? (visible - 1) : 0) * HINT_LINE_HEIGHT);
            int16_t min_base = bubble_y + 12;
            int16_t max_base = bubble_y + bubble_h - 12;
            simple_app_draw_scroll_hints_clamped(
                canvas,
                arrow_x,
                content_top,
                content_bottom,
                show_up,
                show_down,
                min_base,
                max_base);
        }
    }
}

static void simple_app_draw_menu(SimpleApp* app, Canvas* canvas) {
    canvas_set_color(canvas, ColorBlack);

    if(app->section_index >= menu_section_count) {
        app->section_index = 0;
    }

    bool show_setup_branding = (app->menu_state == MenuStateItems) &&
                               (app->section_index == MENU_SECTION_SETUP);

    if(show_setup_branding) {
        canvas_set_bitmap_mode(canvas, true);
        canvas_draw_xbm(canvas, 115, 2, 10, 10, image_icon_0_bits);
    }
    canvas_set_bitmap_mode(canvas, false);

    canvas_set_font(canvas, FontSecondary);

    if(simple_app_status_message_is_active(app) && !app->status_message_fullscreen) {
        canvas_draw_str(canvas, 2, 52, app->status_message);
    }
    if(show_setup_branding) {
        char version_text[24];
        snprintf(version_text, sizeof(version_text), "v.%s", LAB_C5_VERSION_TEXT);
        canvas_draw_str_aligned(
            canvas, DISPLAY_WIDTH - 15, 11, AlignRight, AlignBottom, version_text);
    }

    if(app->menu_state == MenuStateSections) {
        size_t visible_sections = simple_app_section_visible_count();
        if(visible_sections == 0) visible_sections = 1;
        simple_app_adjust_section_offset(app);

        canvas_set_font(canvas, FontPrimary);
        for(size_t i = 0; i < visible_sections; i++) {
            size_t idx = app->section_offset + i;
            if(idx >= menu_section_count) break;
            uint8_t y = MENU_SECTION_BASE_Y + (uint8_t)(i * MENU_SECTION_SPACING);
            const char* title = menu_sections[idx].title;
            if(app->section_index == idx) {
                canvas_draw_str(canvas, 0, y, ">");
                canvas_draw_str(canvas, 12, y, title);
            } else {
                canvas_draw_str(canvas, 6, y, title);
            }
        }

        if(menu_section_count > visible_sections) {
            bool show_up = (app->section_offset > 0);
            bool show_down = (app->section_offset + visible_sections < menu_section_count);
            if(show_up || show_down) {
                uint8_t arrow_x = DISPLAY_WIDTH - 6;
                int16_t content_top = MENU_SECTION_BASE_Y;
                int16_t content_bottom =
                    MENU_SECTION_BASE_Y + (int16_t)((visible_sections - 1) * MENU_SECTION_SPACING);
                simple_app_draw_scroll_hints_clamped(
                    canvas, arrow_x, content_top, content_bottom, show_up, show_down, 12, 44);
            }
        }
        return;
    }

    if(app->section_index >= menu_section_count) {
        app->section_index = 0;
    }

    const MenuSection* section = &menu_sections[app->section_index];

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 3, MENU_TITLE_Y, section->title);

    canvas_set_font(canvas, FontSecondary);

    size_t visible_count = simple_app_menu_visible_count(app, app->section_index);
    if(visible_count == 0) {
        visible_count = 1;
    }

    if(section->entry_count == 0) {
        const char* hint = "No options";
        if(app->section_index == MENU_SECTION_SCANNER) {
            hint = "OK: Scan networks";
        }
        canvas_draw_str(canvas, 3, MENU_ITEM_BASE_Y + (MENU_ITEM_SPACING / 2), hint);
        return;
    }

    if(app->item_index >= section->entry_count) {
        app->item_index = section->entry_count - 1;
    }

    size_t max_offset = 0;
    if(section->entry_count > visible_count) {
        max_offset = section->entry_count - visible_count;
    }
    if(app->item_offset > max_offset) {
        app->item_offset = max_offset;
    }
    if(app->item_index < app->item_offset) {
        app->item_offset = app->item_index;
    } else if(app->item_index >= app->item_offset + visible_count) {
        app->item_offset = app->item_index - visible_count + 1;
    }

    for(uint32_t i = 0; i < visible_count; i++) {
        uint32_t idx = app->item_offset + i;
        if(idx >= section->entry_count) break;
        uint8_t y = MENU_ITEM_BASE_Y + i * MENU_ITEM_SPACING;

        if(idx == app->item_index) {
            canvas_draw_str(canvas, 2, y, ">");
            canvas_draw_str(canvas, 12, y, section->entries[idx].label);
        } else {
            canvas_draw_str(canvas, 8, y, section->entries[idx].label);
        }
    }

    if(section->entry_count > visible_count) {
        bool show_up = (app->item_offset > 0);
        bool show_down = (app->item_offset + visible_count < section->entry_count);
        if(show_up || show_down) {
            uint8_t arrow_x = DISPLAY_WIDTH - 6;
            uint8_t max_rows = (section->entry_count < visible_count) ? section->entry_count :
                                                                        visible_count;
            if(max_rows == 0) max_rows = 1;
            int16_t content_top = MENU_ITEM_BASE_Y;
            int16_t content_bottom =
                MENU_ITEM_BASE_Y + (int16_t)((max_rows - 1) * MENU_ITEM_SPACING);
            simple_app_draw_scroll_hints(
                canvas, arrow_x, content_top, content_bottom, show_up, show_down);
        }
    }
}

static size_t simple_app_render_display_lines(
    SimpleApp* app,
    size_t skip_lines,
    char dest[][64],
    size_t max_lines) {
    memset(dest, 0, max_lines * 64);
    if(!app->serial_mutex) return 0;

    furi_mutex_acquire(app->serial_mutex, FuriWaitForever);
    const char* buffer = app->serial_buffer;
    size_t length = app->serial_len;
    size_t line_index = 0;
    size_t col = 0;
    size_t lines_filled = 0;

    for(size_t idx = 0; idx < length; idx++) {
        char ch = buffer[idx];
        if(ch == '\r') continue;

        if(ch == '\n') {
            if(line_index >= skip_lines && lines_filled < max_lines) {
                dest[lines_filled][col] = '\0';
                lines_filled++;
            }
            line_index++;
            col = 0;
            if(line_index >= skip_lines + max_lines) break;
            continue;
        }

        if(col >= SERIAL_LINE_CHAR_LIMIT) {
            if(line_index >= skip_lines && lines_filled < max_lines) {
                dest[lines_filled][col] = '\0';
                lines_filled++;
            }
            line_index++;
            col = 0;
            if(line_index >= skip_lines + max_lines) {
                continue;
            }
        }

        if(line_index >= skip_lines && lines_filled < max_lines) {
            dest[lines_filled][col] = ch;
        }
        col++;
    }

    if(col > 0) {
        if(line_index >= skip_lines && lines_filled < max_lines) {
            dest[lines_filled][col] = '\0';
            lines_filled++;
        }
    }

    furi_mutex_release(app->serial_mutex);
    return lines_filled;
}

static void simple_app_draw_wardrive_serial(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    char display_lines[WARD_DRIVE_CONSOLE_LINES][64];
    size_t lines_filled = simple_app_render_display_lines(
        app, app->serial_scroll, display_lines, WARD_DRIVE_CONSOLE_LINES);

    uint8_t y = 8;
    if(lines_filled == 0) {
        canvas_draw_str(canvas, 2, y, "No UART data");
    } else {
        for(size_t i = 0; i < lines_filled; i++) {
            canvas_draw_str(canvas, 2, y, display_lines[i][0] ? display_lines[i] : " ");
            y += SERIAL_TEXT_LINE_HEIGHT;
        }
    }

    size_t total_lines = simple_app_total_display_lines(app);
    if(total_lines > WARD_DRIVE_CONSOLE_LINES) {
        size_t max_scroll = simple_app_max_scroll(app);
        bool show_up = (app->serial_scroll > 0);
        bool show_down = (app->serial_scroll < max_scroll);
        if(show_up || show_down) {
            uint8_t arrow_x = DISPLAY_WIDTH - 6;
            int16_t content_top = 8;
            int16_t visible_rows =
                (WARD_DRIVE_CONSOLE_LINES > 0) ? (WARD_DRIVE_CONSOLE_LINES - 1) : 0;
            int16_t content_bottom = 8 + (int16_t)(visible_rows * SERIAL_TEXT_LINE_HEIGHT);
            simple_app_draw_scroll_hints(
                canvas, arrow_x, content_top, content_bottom, show_up, show_down);
        }
    }

    int16_t divider_y = 8 + (int16_t)(WARD_DRIVE_CONSOLE_LINES * SERIAL_TEXT_LINE_HEIGHT) + 2;
    if(divider_y >= DISPLAY_HEIGHT) {
        divider_y = DISPLAY_HEIGHT - 2;
    }
    canvas_draw_line(canvas, 0, divider_y, DISPLAY_WIDTH - 1, divider_y);

    const char* status = "0";
    bool numeric = true;
    if(app->wardrive_state) {
        status = app->wardrive_state->status_text[0] ? app->wardrive_state->status_text : "0";
        numeric = app->wardrive_state->status_is_numeric;
    }
    int16_t bottom_center = divider_y + ((DISPLAY_HEIGHT - divider_y) / 2);
    if(bottom_center >= DISPLAY_HEIGHT) {
        bottom_center = DISPLAY_HEIGHT - 2;
    }

    if(!numeric) {
        size_t len = strlen(status);
        if(len > 12) {
            const char* split_ptr = NULL;
            size_t mid = len / 2;
            for(size_t i = mid; i > 0; i--) {
                if(status[i] == ' ') {
                    split_ptr = status + i;
                    break;
                }
            }
            if(!split_ptr) {
                for(size_t i = mid; i < len; i++) {
                    if(status[i] == ' ') {
                        split_ptr = status + i;
                        break;
                    }
                }
            }
            if(split_ptr) {
                size_t first_len = (size_t)(split_ptr - status);
                while(first_len > 0 && status[first_len - 1] == ' ') {
                    first_len--;
                }
                char first_line[32];
                if(first_len >= sizeof(first_line)) {
                    first_len = sizeof(first_line) - 1;
                }
                memcpy(first_line, status, first_len);
                first_line[first_len] = '\0';
                const char* second_start = split_ptr + 1;
                while(*second_start == ' ') {
                    second_start++;
                }
                char second_line[32];
                snprintf(second_line, sizeof(second_line), "%s", second_start);
                if(first_line[0] != '\0' && second_line[0] != '\0') {
                    canvas_set_font(canvas, FontSecondary);
                    int16_t first_y = bottom_center - 4;
                    int16_t second_y = bottom_center + 8;
                    if(second_y >= DISPLAY_HEIGHT) {
                        second_y = DISPLAY_HEIGHT - 2;
                        first_y = second_y - 12;
                    }
                    canvas_draw_str_aligned(
                        canvas, DISPLAY_WIDTH / 2, first_y, AlignCenter, AlignCenter, first_line);
                    canvas_draw_str_aligned(
                        canvas, DISPLAY_WIDTH / 2, second_y, AlignCenter, AlignCenter, second_line);
                    return;
                }
            }
        }
    }

    Font display_font = FontPrimary;
    if(!numeric) {
        size_t len = strlen(status);
        if(len > 10) {
            display_font = FontSecondary;
        }
    }
    canvas_set_font(canvas, display_font);
    canvas_draw_str_aligned(
        canvas, DISPLAY_WIDTH / 2, bottom_center, AlignCenter, AlignCenter, status);
}

static void simple_app_draw_gps(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 4, 12, "GPS");

    canvas_set_font(canvas, FontSecondary);
    if(!app->gps_state || !app->gps_state->has_coords) {
        SimpleAppGpsState* gps = app->gps_state;
        canvas_draw_str_aligned(
            canvas, DISPLAY_WIDTH / 2, 32, AlignCenter, AlignCenter, "Waiting for GPS fix");
        if(gps && gps->has_time) {
            if(simple_app_gps_format_time_display(app, gps->time_text, sizeof(gps->time_text))) {
                simple_app_gps_format_offset(app, gps->offset_text, sizeof(gps->offset_text));
                snprintf(
                    gps->time_line,
                    sizeof(gps->time_line),
                    "Time: %s %s",
                    gps->time_text,
                    gps->offset_text);
            } else {
                snprintf(gps->time_line, sizeof(gps->time_line), "Time: --");
            }
            canvas_draw_str_aligned(
                canvas, DISPLAY_WIDTH / 2, 44, AlignCenter, AlignCenter, gps->time_line);
        }
        if(gps && gps->antenna_status == GpsAntennaOpen) {
            canvas_draw_str_aligned(
                canvas, DISPLAY_WIDTH / 2, 56, AlignCenter, AlignCenter, "Antena not detected");
        } else if(gps && gps->satellites >= 0) {
            snprintf(gps->sats_line, sizeof(gps->sats_line), "Sats: %d", gps->satellites);
            canvas_draw_str_aligned(
                canvas, DISPLAY_WIDTH / 2, 56, AlignCenter, AlignCenter, gps->sats_line);
        }
        return;
    }

    SimpleAppGpsState* gps = app->gps_state;
    snprintf(gps->line, sizeof(gps->line), "Lat: %s", gps->lat);
    canvas_draw_str(canvas, 2, 24, gps->line);
    snprintf(gps->line, sizeof(gps->line), "Lon: %s", gps->lon);
    canvas_draw_str(canvas, 2, 34, gps->line);
    if(app->gps_state->has_time) {
        if(simple_app_gps_format_time_display(app, gps->time_text, sizeof(gps->time_text))) {
            simple_app_gps_format_offset(app, gps->offset_text, sizeof(gps->offset_text));
            snprintf(
                gps->line, sizeof(gps->line), "Time: %s %s", gps->time_text, gps->offset_text);
        } else {
            snprintf(gps->line, sizeof(gps->line), "Time: --");
        }
    } else {
        snprintf(gps->line, sizeof(gps->line), "Time: --");
    }
    canvas_draw_str(canvas, 2, 44, gps->line);
    if(gps->antenna_status == GpsAntennaOpen) {
        snprintf(gps->line, sizeof(gps->line), "Antena not detected");
    } else if(gps->satellites >= 0) {
        snprintf(gps->line, sizeof(gps->line), "Sats: %d", gps->satellites);
    } else {
        snprintf(gps->line, sizeof(gps->line), "Sats: --");
    }
    canvas_draw_str(canvas, 2, 54, gps->line);
}

static void simple_app_draw_deauth_guard(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;
    app->serial_targets_hint = false;

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas, DISPLAY_WIDTH / 2, 14, AlignCenter, AlignCenter, "Deauth Guard");

    canvas_set_font(canvas, FontSecondary);
    if(!app->deauth_guard_has_detection) {
        if(app->deauth_guard_monitoring) {
            canvas_draw_str_aligned(
                canvas, DISPLAY_WIDTH / 2, 32, AlignCenter, AlignCenter, "Monitoring...");
            canvas_draw_str_aligned(
                canvas, DISPLAY_WIDTH / 2, 48, AlignCenter, AlignCenter, "No deauths detected");
        } else {
            canvas_draw_str_aligned(
                canvas, DISPLAY_WIDTH / 2, 32, AlignCenter, AlignCenter, "Warm up...");
            canvas_draw_str_aligned(
                canvas, DISPLAY_WIDTH / 2, 48, AlignCenter, AlignCenter, "Listening for deauths");
        }
        return;
    }

    canvas_draw_str_aligned(
        canvas, DISPLAY_WIDTH / 2, 30, AlignCenter, AlignCenter, "Attack ongoing!");
    char status_line[48];
    if(app->deauth_guard_last_channel > 0) {
        if(app->deauth_guard_has_rssi) {
            snprintf(
                status_line,
                sizeof(status_line),
                "CH %d | RSSI %d",
                app->deauth_guard_last_channel,
                app->deauth_guard_last_rssi);
        } else {
            snprintf(status_line, sizeof(status_line), "CH %d", app->deauth_guard_last_channel);
        }
    } else {
        snprintf(status_line, sizeof(status_line), "Channel: ?");
    }
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 44, AlignCenter, AlignCenter, status_line);

    char ssid_line[SCAN_SSID_MAX_LEN + 8];
    snprintf(
        ssid_line,
        sizeof(ssid_line),
        "%s",
        (app->deauth_guard_last_ssid[0] != '\0') ? app->deauth_guard_last_ssid : "Unknown SSID");
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 58, AlignCenter, AlignCenter, ssid_line);
}

static void simple_app_draw_deauth_overlay(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;
    app->serial_targets_hint = false;

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 12, "Deauth");
    if(app->deauth_running) {
        bool blink_on = ((furi_get_tick() / furi_ms_to_ticks(500)) % 2) == 0;
        if(blink_on) {
            canvas_draw_str_aligned(
                canvas, DISPLAY_WIDTH / 2, 12, AlignCenter, AlignBottom, "Running");
        }
    }

    uint32_t target_count = (app->scan_selected_count > 0) ? (uint32_t)app->scan_selected_count :
                                                             app->deauth_targets;
    char header_right[24];
    if(target_count > 0) {
        snprintf(header_right, sizeof(header_right), "Targets:%lu", (unsigned long)target_count);
    } else {
        snprintf(header_right, sizeof(header_right), "Targets:--");
    }
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH - 2, 12, AlignRight, AlignBottom, header_right);

    if(app->scan_selected_count == 0 || !app->scan_selected_numbers || !app->scan_results) {
        canvas_draw_str_aligned(
            canvas, DISPLAY_WIDTH / 2, 36, AlignCenter, AlignCenter, "No target list");
        return;
    }

    size_t visible = 4;
    size_t max_offset =
        (app->scan_selected_count > visible) ? (app->scan_selected_count - visible) : 0;
    size_t offset = app->deauth_list_offset;
    if(offset > max_offset) offset = max_offset;

    canvas_set_font(canvas, FontSecondary);
    for(size_t i = 0; i < visible; i++) {
        size_t idx = offset + i;
        if(idx >= app->scan_selected_count) break;
        uint16_t number = app->scan_selected_numbers[idx];
        const ScanResult* result = simple_app_find_scan_result_by_number(app, number);
        char line[32];
        if(result) {
            char ssid_ascii[SCAN_SSID_MAX_LEN];
            simple_app_utf8_to_ascii_pl(result->ssid, ssid_ascii, sizeof(ssid_ascii));
            simple_app_trim(ssid_ascii);
            if(ssid_ascii[0] == '\0') {
                strncpy(ssid_ascii, "Hidden", sizeof(ssid_ascii) - 1);
                ssid_ascii[sizeof(ssid_ascii) - 1] = '\0';
            }

            char channel_short[4];
            if(result->channel > 0) {
                snprintf(channel_short, sizeof(channel_short), "%u", (unsigned)result->channel);
            } else {
                strncpy(channel_short, "--", sizeof(channel_short) - 1);
                channel_short[sizeof(channel_short) - 1] = '\0';
            }

            char bssid_text[18];
            simple_app_format_bssid(result->bssid, bssid_text, sizeof(bssid_text));

            char idx_buf[6];
            unsigned idx_value = (unsigned)(idx + 1);
            if(idx_value > 9999U) idx_value = 9999U;
            snprintf(idx_buf, sizeof(idx_buf), "%2u", idx_value);

            char full_line[96];
            size_t pos = 0;
            size_t chunk = strlen(idx_buf);
            if(chunk > sizeof(full_line) - pos - 1) chunk = sizeof(full_line) - pos - 1;
            memcpy(full_line + pos, idx_buf, chunk);
            pos += chunk;
            if(pos < sizeof(full_line) - 1) {
                full_line[pos++] = ' ';
            }
            if(pos < sizeof(full_line) - 1) {
                full_line[pos++] = 'C';
            }
            if(pos < sizeof(full_line) - 1) {
                full_line[pos++] = 'H';
            }
            chunk = strlen(channel_short);
            if(chunk > sizeof(full_line) - pos - 1) chunk = sizeof(full_line) - pos - 1;
            memcpy(full_line + pos, channel_short, chunk);
            pos += chunk;
            if(pos < sizeof(full_line) - 1) {
                full_line[pos++] = ' ';
            }
            chunk = strlen(ssid_ascii);
            if(chunk > sizeof(full_line) - pos - 1) chunk = sizeof(full_line) - pos - 1;
            memcpy(full_line + pos, ssid_ascii, chunk);
            pos += chunk;
            if(pos < sizeof(full_line) - 1) {
                full_line[pos++] = ' ';
            }
            chunk = strlen(bssid_text);
            if(chunk > sizeof(full_line) - pos - 1) chunk = sizeof(full_line) - pos - 1;
            memcpy(full_line + pos, bssid_text, chunk);
            pos += chunk;
            full_line[pos] = '\0';

            char default_line[64];
            size_t dpos = 0;
            chunk = strlen(idx_buf);
            if(chunk > sizeof(default_line) - dpos - 1) chunk = sizeof(default_line) - dpos - 1;
            memcpy(default_line + dpos, idx_buf, chunk);
            dpos += chunk;
            if(dpos < sizeof(default_line) - 1) {
                default_line[dpos++] = ' ';
            }
            if(dpos < sizeof(default_line) - 1) {
                default_line[dpos++] = 'C';
            }
            if(dpos < sizeof(default_line) - 1) {
                default_line[dpos++] = 'H';
            }
            chunk = strlen(channel_short);
            if(chunk > sizeof(default_line) - dpos - 1) chunk = sizeof(default_line) - dpos - 1;
            memcpy(default_line + dpos, channel_short, chunk);
            dpos += chunk;
            if(dpos < sizeof(default_line) - 1) {
                default_line[dpos++] = ' ';
            }

            char bssid_tail[6];
            size_t bssid_len = strlen(bssid_text);
            const char* tail_ptr = (bssid_len > 5) ? (bssid_text + bssid_len - 5) : bssid_text;
            strncpy(bssid_tail, tail_ptr, sizeof(bssid_tail) - 1);
            bssid_tail[sizeof(bssid_tail) - 1] = '\0';

            size_t prefix_len = dpos;
            size_t tail_len = strlen(bssid_tail);
            size_t max_ssid = 0;
            if(SERIAL_LINE_CHAR_LIMIT > prefix_len) {
                max_ssid = SERIAL_LINE_CHAR_LIMIT - prefix_len;
                if(tail_len > 0 && max_ssid > (tail_len + 1)) {
                    max_ssid -= (tail_len + 1);
                } else if(tail_len > 0 && max_ssid <= (tail_len + 1)) {
                    max_ssid = 0;
                }
            }
            chunk = strlen(ssid_ascii);
            if(chunk > max_ssid) chunk = max_ssid;
            if(chunk > 0 && dpos < sizeof(default_line) - 1) {
                if(chunk > sizeof(default_line) - dpos - 1)
                    chunk = sizeof(default_line) - dpos - 1;
                memcpy(default_line + dpos, ssid_ascii, chunk);
                dpos += chunk;
            }
            if(tail_len > 0 && dpos < sizeof(default_line) - 1) {
                default_line[dpos++] = ' ';
                chunk = tail_len;
                if(chunk > sizeof(default_line) - dpos - 1)
                    chunk = sizeof(default_line) - dpos - 1;
                memcpy(default_line + dpos, bssid_tail, chunk);
                dpos += chunk;
            }
            default_line[dpos] = '\0';

            size_t len = strlen(full_line);
            size_t offset_x = app->deauth_list_scroll_x;
            if(offset_x == 0) {
                len = strlen(default_line);
                size_t copy_len = len;
                if(copy_len > SERIAL_LINE_CHAR_LIMIT) copy_len = SERIAL_LINE_CHAR_LIMIT;
                memcpy(line, default_line, copy_len);
                line[copy_len] = '\0';
            } else {
                if(len <= SERIAL_LINE_CHAR_LIMIT) {
                    offset_x = 0;
                } else if(offset_x > len - SERIAL_LINE_CHAR_LIMIT) {
                    offset_x = len - SERIAL_LINE_CHAR_LIMIT;
                }
                size_t copy_len = len - offset_x;
                if(copy_len > SERIAL_LINE_CHAR_LIMIT) copy_len = SERIAL_LINE_CHAR_LIMIT;
                memcpy(line, full_line + offset_x, copy_len);
                line[copy_len] = '\0';
            }
        } else {
            snprintf(line, sizeof(line), "ID %u (no data)", (unsigned)number);
        }

        uint8_t y = (uint8_t)(26 + (i * SERIAL_TEXT_LINE_HEIGHT));
        canvas_draw_str(canvas, 2, y, line);
    }
}

static void simple_app_draw_handshake_overlay(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;
    app->serial_targets_hint = false;

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 12, "Handshaker  ");

    char targets_line[32];
    snprintf(
        targets_line, sizeof(targets_line), "Targets:%lu", (unsigned long)app->handshake_targets);
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH - 2, 12, AlignRight, AlignBottom, targets_line);

    canvas_set_font(canvas, FontPrimary);
    char count_line[32];
    snprintf(
        count_line, sizeof(count_line), "Captured: %lu", (unsigned long)app->handshake_captured);
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 28, AlignCenter, AlignCenter, count_line);

    canvas_set_font(canvas, FontSecondary);
    char ssid_line[32];
    if(app->handshake_ssid[0] != '\0') {
        snprintf(
            ssid_line,
            sizeof(ssid_line),
            "SSID: %.*s",
            (int)(sizeof(ssid_line) - 7),
            app->handshake_ssid);
        simple_app_truncate_text(ssid_line, SERIAL_LINE_CHAR_LIMIT);
        canvas_draw_str_aligned(
            canvas, DISPLAY_WIDTH / 2, 42, AlignCenter, AlignCenter, ssid_line);
    } else {
        ssid_line[0] = '\0';
    }

    char stage_line[32];
    if(app->handshake_note[0] != '\0') {
        snprintf(
            stage_line,
            sizeof(stage_line),
            "Stage: %.*s",
            (int)(sizeof(stage_line) - 8),
            app->handshake_note);
    } else {
        snprintf(stage_line, sizeof(stage_line), "Stage: --");
    }
    simple_app_truncate_text(stage_line, SERIAL_LINE_CHAR_LIMIT);
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 54, AlignCenter, AlignCenter, stage_line);
}

static void simple_app_draw_sae_overlay(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;
    app->serial_targets_hint = false;

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 12, "SAE Overflow");

    canvas_set_font(canvas, FontPrimary);
    char stage_line[24];
    if(app->sae_running) {
        snprintf(stage_line, sizeof(stage_line), "Stage: Running");
    } else {
        snprintf(stage_line, sizeof(stage_line), "Stage: --");
    }
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 28, AlignCenter, AlignCenter, stage_line);

    canvas_set_font(canvas, FontSecondary);
    if(app->sae_ssid[0] != '\0') {
        char ssid_line[32];
        snprintf(
            ssid_line,
            sizeof(ssid_line),
            "SSID: %.*s",
            (int)(sizeof(ssid_line) - 7),
            app->sae_ssid);
        simple_app_truncate_text(ssid_line, SERIAL_LINE_CHAR_LIMIT);
        canvas_draw_str_aligned(
            canvas, DISPLAY_WIDTH / 2, 42, AlignCenter, AlignCenter, ssid_line);
    }

    if(app->sae_has_channel) {
        char ch_line[16];
        snprintf(ch_line, sizeof(ch_line), "CH: %u", (unsigned)app->sae_channel);
        canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 54, AlignCenter, AlignCenter, ch_line);
    }
}

static void simple_app_draw_blackout_overlay(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;
    app->serial_targets_hint = false;

    const char* status = "Idle";
    switch(app->blackout_phase) {
    case BlackoutPhaseStarting:
        status = "Starting";
        break;
    case BlackoutPhaseScanning:
        status = "Scanning";
        break;
    case BlackoutPhaseSorting:
        status = "Sorting";
        break;
    case BlackoutPhaseAttacking:
        status = "Attacking";
        break;
    case BlackoutPhaseStopping:
        status = "Stopping";
        break;
    case BlackoutPhaseFinished:
        status = "Done";
        break;
    case BlackoutPhaseIdle:
    default:
        status = "Idle";
        break;
    }

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 12, "Blackout");
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH - 2, 12, AlignRight, AlignBottom, status);

    canvas_set_font(canvas, FontPrimary);
    char targets_line[32];
    if(app->blackout_networks > 0) {
        snprintf(
            targets_line,
            sizeof(targets_line),
            "Targets: %lu",
            (unsigned long)app->blackout_networks);
    } else {
        snprintf(targets_line, sizeof(targets_line), "Targets: --");
    }
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 28, AlignCenter, AlignCenter, targets_line);

    canvas_set_font(canvas, FontSecondary);
    char mid_line[32];
    if(app->blackout_note[0] != '\0') {
        const int note_limit = 20;
        snprintf(mid_line, sizeof(mid_line), "Note: %.*s", note_limit, app->blackout_note);
    } else if(app->blackout_cycle > 0) {
        snprintf(mid_line, sizeof(mid_line), "Cycle: %lu", (unsigned long)app->blackout_cycle);
    } else {
        snprintf(mid_line, sizeof(mid_line), "Cycle: --");
    }
    simple_app_truncate_text(mid_line, SERIAL_LINE_CHAR_LIMIT);
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 42, AlignCenter, AlignCenter, mid_line);

    char channel_line[32];
    if(app->blackout_has_channel && app->blackout_channel > 0) {
        snprintf(channel_line, sizeof(channel_line), "Channel: %u", app->blackout_channel);
    } else {
        snprintf(channel_line, sizeof(channel_line), "Channel: --");
    }
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 54, AlignCenter, AlignCenter, channel_line);
}

static void simple_app_draw_sniffer_dog_overlay(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;
    app->serial_targets_hint = false;

    const char* status = "Idle";
    switch(app->sniffer_dog_phase) {
    case SnifferDogPhaseStarting:
        status = "Starting";
        break;
    case SnifferDogPhaseHunting:
        status = "Hunting";
        break;
    case SnifferDogPhaseStopping:
        status = "Stopping";
        break;
    case SnifferDogPhaseError:
        status = "Error";
        break;
    case SnifferDogPhaseFinished:
        status = "Done";
        break;
    case SnifferDogPhaseIdle:
    default:
        status = "Idle";
        break;
    }

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 12, "Sniffer Dog");
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH - 2, 12, AlignRight, AlignBottom, status);

    canvas_set_font(canvas, FontPrimary);
    char deauth_line[32];
    if(app->sniffer_dog_deauth_count > 0) {
        snprintf(
            deauth_line,
            sizeof(deauth_line),
            "Deauth: %lu",
            (unsigned long)app->sniffer_dog_deauth_count);
    } else {
        snprintf(deauth_line, sizeof(deauth_line), "Deauth: --");
    }
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 28, AlignCenter, AlignCenter, deauth_line);

    canvas_set_font(canvas, FontSecondary);
    char mid_line[32];
    if(app->sniffer_dog_note[0] != '\0') {
        const int note_limit = 20;
        snprintf(mid_line, sizeof(mid_line), "Note: %.*s", note_limit, app->sniffer_dog_note);
    } else if(app->sniffer_dog_has_channel) {
        if(app->sniffer_dog_has_rssi) {
            snprintf(
                mid_line,
                sizeof(mid_line),
                "CH %u | RSSI %d",
                app->sniffer_dog_channel,
                app->sniffer_dog_rssi);
        } else {
            snprintf(mid_line, sizeof(mid_line), "CH %u", app->sniffer_dog_channel);
        }
    } else {
        snprintf(mid_line, sizeof(mid_line), "Channel: --");
    }
    simple_app_truncate_text(mid_line, SERIAL_LINE_CHAR_LIMIT);
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 42, AlignCenter, AlignCenter, mid_line);

    char phase_line[32];
    if(app->sniffer_dog_running) {
        snprintf(phase_line, sizeof(phase_line), "Ongoing");
    } else if(app->sniffer_dog_phase == SnifferDogPhaseFinished) {
        snprintf(phase_line, sizeof(phase_line), "Stopped");
    } else {
        snprintf(phase_line, sizeof(phase_line), "--");
    }
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 54, AlignCenter, AlignCenter, phase_line);
}

static void simple_app_draw_scanner_status(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    char status_line[48];
    if(app->scan_results_loading || app->scanner_scan_running) {
        if(app->scanner_rescan_hint) {
            snprintf(status_line, sizeof(status_line), "Rescanning...");
        } else {
            snprintf(status_line, sizeof(status_line), "Scanning...");
        }
    } else if(app->scanner_total > 0) {
        snprintf(status_line, sizeof(status_line), "Finished");
    } else if(app->scanner_last_update_tick > 0) {
        snprintf(status_line, sizeof(status_line), "No networks");
    } else {
        snprintf(status_line, sizeof(status_line), "Waiting...");
    }

    canvas_draw_str(canvas, 2, 12, "WiFi Scan");
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH - 2, 12, AlignRight, AlignBottom, status_line);

    canvas_set_font(canvas, FontPrimary);
    char total_line[24];
    snprintf(total_line, sizeof(total_line), "Total: %u", (unsigned)app->scanner_total);
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 28, AlignCenter, AlignCenter, total_line);

    canvas_set_font(canvas, FontSecondary);
    char band_line[32];
    snprintf(
        band_line,
        sizeof(band_line),
        "2.4G:%u  5G:%u",
        (unsigned)app->scanner_band_24,
        (unsigned)app->scanner_band_5);
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 42, AlignCenter, AlignCenter, band_line);

    char open_line[32];
    snprintf(
        open_line,
        sizeof(open_line),
        "Open:%u Hidden:%u",
        (unsigned)app->scanner_open_count,
        (unsigned)app->scanner_hidden_count);
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 54, AlignCenter, AlignCenter, open_line);

    if(!app->scan_results_loading && !app->scanner_scan_running && app->scanner_total > 0) {
        canvas_draw_str_aligned(
            canvas, DISPLAY_WIDTH - 4, DISPLAY_HEIGHT - 2, AlignRight, AlignBottom, "->");
    }
}

static void simple_app_draw_portal_overlay(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;
    const PortalStatus* status = app->portal_status;

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    canvas_draw_str(canvas, 2, 12, "Portal On");

    const char* ssid_value = (status && status->status_ssid[0] != '\0') ? status->status_ssid :
                                                                          app->portal_ssid;
    if(!ssid_value || ssid_value[0] == '\0') {
        ssid_value = "--";
    }

    canvas_set_font(canvas, FontPrimary);
    const char* full_title =
        (app->overlay_title_scroll_text[0] != '\0') ? app->overlay_title_scroll_text : ssid_value;
    size_t title_len = strlen(full_title);
    size_t char_limit = OVERLAY_TITLE_CHAR_LIMIT;
    if(char_limit >= sizeof(app->overlay_title_scroll_text)) {
        char_limit = sizeof(app->overlay_title_scroll_text) - 1;
    }
    if(char_limit == 0) char_limit = 1;
    if(title_len <= char_limit) {
        canvas_draw_str_aligned(
            canvas, DISPLAY_WIDTH / 2, 24, AlignCenter, AlignCenter, full_title);
    } else {
        size_t offset = app->overlay_title_scroll_offset;
        size_t max_offset = title_len - char_limit;
        if(offset > max_offset) offset = max_offset;
        size_t copy_len = char_limit;
        if(offset + copy_len > title_len) copy_len = title_len - offset;
        char title_window[64];
        if(copy_len >= sizeof(title_window)) copy_len = sizeof(title_window) - 1;
        memcpy(title_window, full_title + offset, copy_len);
        title_window[copy_len] = '\0';
        canvas_draw_str_aligned(
            canvas, DISPLAY_WIDTH / 2, 24, AlignCenter, AlignCenter, title_window);
    }

    canvas_set_font(canvas, FontSecondary);
    char clients_line[32];
    if(status && status->client_count >= 0) {
        snprintf(clients_line, sizeof(clients_line), "Clients: %ld", (long)status->client_count);
    } else {
        snprintf(clients_line, sizeof(clients_line), "Clients: --");
    }
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 36, AlignCenter, AlignCenter, clients_line);

    char passwords_line[32];
    snprintf(
        passwords_line,
        sizeof(passwords_line),
        "Passwords: %lu",
        (unsigned long)(status ? status->password_count : 0));
    canvas_draw_str_aligned(
        canvas, DISPLAY_WIDTH / 2, 48, AlignCenter, AlignCenter, passwords_line);

    char users_line[32];
    snprintf(
        users_line,
        sizeof(users_line),
        "Usernames: %lu",
        (unsigned long)(status ? status->username_count : 0));
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 60, AlignCenter, AlignCenter, users_line);
}

static void simple_app_draw_evil_twin_overlay(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;
    const EvilTwinStatus* status = app->evil_twin_status;

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    canvas_draw_str(canvas, 2, 12, "Evil Twin");

    char ssid_buffer[SCAN_SSID_MAX_LEN];
    const char* ssid_value = status ? status->status_ssid : NULL;
    if(!ssid_value || ssid_value[0] == '\0') {
        if(simple_app_get_first_selected_ssid(app, ssid_buffer, sizeof(ssid_buffer))) {
            ssid_value = ssid_buffer;
        } else {
            ssid_value = "--";
        }
    }

    canvas_set_font(canvas, FontPrimary);
    const char* full_title =
        (app->overlay_title_scroll_text[0] != '\0') ? app->overlay_title_scroll_text : ssid_value;
    size_t title_len = strlen(full_title);
    size_t char_limit = OVERLAY_TITLE_CHAR_LIMIT;
    if(char_limit >= sizeof(app->overlay_title_scroll_text)) {
        char_limit = sizeof(app->overlay_title_scroll_text) - 1;
    }
    if(char_limit == 0) char_limit = 1;
    if(title_len <= char_limit) {
        canvas_draw_str_aligned(
            canvas, DISPLAY_WIDTH / 2, 24, AlignCenter, AlignCenter, full_title);
    } else {
        size_t offset = app->overlay_title_scroll_offset;
        size_t max_offset = title_len - char_limit;
        if(offset > max_offset) offset = max_offset;
        size_t copy_len = char_limit;
        if(offset + copy_len > title_len) copy_len = title_len - offset;
        char title_window[64];
        if(copy_len >= sizeof(title_window)) copy_len = sizeof(title_window) - 1;
        memcpy(title_window, full_title + offset, copy_len);
        title_window[copy_len] = '\0';
        canvas_draw_str_aligned(
            canvas, DISPLAY_WIDTH / 2, 24, AlignCenter, AlignCenter, title_window);
    }

    canvas_set_font(canvas, FontSecondary);
    char clients_line[32];
    if(status && status->client_count >= 0) {
        snprintf(clients_line, sizeof(clients_line), "Clients: %ld", (long)status->client_count);
    } else {
        snprintf(clients_line, sizeof(clients_line), "Clients: --");
    }
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 36, AlignCenter, AlignCenter, clients_line);

    const char* note_value = (status && status->status_note[0] != '\0') ? status->status_note :
                                                                          "--";
    char stage_line[32];
    snprintf(stage_line, sizeof(stage_line), "Stage: %s", note_value);
    simple_app_truncate_text(stage_line, 21);
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 48, AlignCenter, AlignCenter, stage_line);

    char passwords_line[64];
    const int pass_limit = 16;
    if(!status || status->password_count == 0 || status->last_password[0] == '\0') {
        snprintf(passwords_line, sizeof(passwords_line), "Pass: --");
    } else if(status->password_count == 1) {
        snprintf(
            passwords_line,
            sizeof(passwords_line),
            "Pass: %.*s",
            pass_limit,
            status->last_password);
    } else {
        snprintf(
            passwords_line,
            sizeof(passwords_line),
            "Pass: %.*s (%lu)",
            pass_limit,
            status->last_password,
            (unsigned long)status->password_count);
    }
    simple_app_truncate_text(passwords_line, 21);
    canvas_draw_str_aligned(
        canvas, DISPLAY_WIDTH / 2, 60, AlignCenter, AlignCenter, passwords_line);
}

static void simple_app_draw_sniffer_overlay(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    char status_line[32];
    if(app->sniffer_running) {
        snprintf(
            status_line,
            sizeof(status_line),
            app->sniffer_scan_done ? "Monitoring" : "Scanning...");
    } else if(app->sniffer_has_data) {
        snprintf(status_line, sizeof(status_line), "Done");
    } else {
        snprintf(status_line, sizeof(status_line), "Waiting...");
    }

    canvas_draw_str(canvas, 2, 12, "Sniffer");
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH - 2, 12, AlignRight, AlignBottom, status_line);

    canvas_set_font(canvas, FontPrimary);
    char packets_line[32];
    snprintf(
        packets_line,
        sizeof(packets_line),
        "Packets: %lu",
        (unsigned long)app->sniffer_packet_count);
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 28, AlignCenter, AlignCenter, packets_line);

    canvas_set_font(canvas, FontSecondary);
    char networks_line[32];
    if(app->sniffer_networks > 0) {
        snprintf(
            networks_line,
            sizeof(networks_line),
            "Networks: %lu",
            (unsigned long)app->sniffer_networks);
    } else {
        snprintf(networks_line, sizeof(networks_line), "Networks: --");
    }
    canvas_draw_str_aligned(
        canvas, DISPLAY_WIDTH / 2, 42, AlignCenter, AlignCenter, networks_line);

    char mode_line[32];
    if(app->sniffer_mode[0] != '\0') {
        snprintf(mode_line, sizeof(mode_line), "Mode: %s", app->sniffer_mode);
    } else {
        snprintf(mode_line, sizeof(mode_line), "Mode: ?");
    }
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 54, AlignCenter, AlignCenter, mode_line);

    if(app->sniffer_has_data && app->sniffer_packet_count >= SNIFFER_RESULTS_ARROW_THRESHOLD) {
        canvas_draw_str_aligned(
            canvas, DISPLAY_WIDTH - 4, DISPLAY_HEIGHT - 2, AlignRight, AlignBottom, "->");
    }
}

static void simple_app_draw_sniffer_results_overlay(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;
    if(!app->sniffer_aps || !app->sniffer_clients) {
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, DISPLAY_WIDTH / 2, 36, AlignCenter, AlignCenter, "OOM: sniffer");
        return;
    }

    size_t ap_count = app->sniffer_ap_count;
    size_t client_count = app->sniffer_client_count;
    if(app->sniffer_results_ap_index >= ap_count && ap_count > 0) {
        app->sniffer_results_ap_index = ap_count - 1;
    }

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    char header[32];
    snprintf(
        header,
        sizeof(header),
        "APs:%u Cli:%u Sel:%u",
        (unsigned)ap_count,
        (unsigned)client_count,
        (unsigned)app->sniffer_selected_count);
    canvas_draw_str(canvas, 2, 12, "Sniffer");
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH - 2, 12, AlignRight, AlignBottom, header);

    if(app->sniffer_results_loading && ap_count == 0 && client_count == 0) {
        canvas_draw_str_aligned(
            canvas, DISPLAY_WIDTH / 2, 36, AlignCenter, AlignCenter, "Loading...");
        return;
    }

    if(ap_count == 0) {
        canvas_draw_str_aligned(
            canvas, DISPLAY_WIDTH / 2, 36, AlignCenter, AlignCenter, "No clients yet");
        return;
    }

    const SnifferApEntry* ap = &app->sniffer_aps[app->sniffer_results_ap_index];
    canvas_set_font(canvas, FontPrimary);
    char computed_title[64];
    snprintf(computed_title, sizeof(computed_title), "%s", ap->ssid[0] ? ap->ssid : "<hidden>");
    char computed_title_ascii[sizeof(computed_title)];
    simple_app_utf8_to_ascii_pl(
        computed_title, computed_title_ascii, sizeof(computed_title_ascii));
    strncpy(computed_title, computed_title_ascii, sizeof(computed_title) - 1);
    computed_title[sizeof(computed_title) - 1] = '\0';

    const char* full_title = (app->overlay_title_scroll_text[0] != '\0') ?
                                 app->overlay_title_scroll_text :
                                 computed_title;
    size_t title_len = strlen(full_title);
    size_t char_limit = OVERLAY_TITLE_CHAR_LIMIT;
    if(char_limit >= sizeof(computed_title)) char_limit = sizeof(computed_title) - 1;
    if(char_limit == 0) char_limit = 1;
    if(title_len <= char_limit) {
        canvas_draw_str(canvas, 2, 30, full_title);
    } else {
        size_t offset = app->overlay_title_scroll_offset;
        size_t max_offset = title_len - char_limit;
        if(offset > max_offset) offset = max_offset;
        size_t copy_len = char_limit;
        if(offset + copy_len > title_len) copy_len = title_len - offset;
        char title_window[64];
        if(copy_len >= sizeof(title_window)) copy_len = sizeof(title_window) - 1;
        memcpy(title_window, full_title + offset, copy_len);
        title_window[copy_len] = '\0';
        canvas_draw_str(canvas, 2, 30, title_window);
    }

    canvas_set_font(canvas, FontSecondary);
    uint16_t current_client =
        (ap->client_count == 0) ? 0 : (uint16_t)(app->sniffer_results_client_offset + 1);
    if(current_client > ap->client_count) current_client = ap->client_count;

    const char* vendor_name = NULL;
    bool vendor_visible = false;
    if(app->scanner_show_vendor && ap->client_count > 0) {
        size_t idx = ap->client_start + app->sniffer_results_client_offset;
        if(idx < ap->client_start + ap->client_count && idx < SNIFFER_MAX_CLIENTS) {
            const SnifferClientEntry* client = &app->sniffer_clients[idx];
            uint8_t mac_bytes[6];
            if(simple_app_parse_mac_bytes(client->mac, mac_bytes)) {
                vendor_name = simple_app_vendor_cache_lookup(app, mac_bytes);
                vendor_visible = (vendor_name && vendor_name[0] != '\0');
            }
        }
    }

    char info[48];
    int info_written = snprintf(
        info,
        sizeof(info),
        "CH%u  Client: %u/%u",
        (unsigned)ap->channel,
        (unsigned)current_client,
        (unsigned)ap->client_count);
    if(vendor_visible && info_written > 0 && (size_t)info_written < sizeof(info) - 2) {
        size_t offset = (size_t)info_written;
        info[offset++] = ' ';
        info[offset] = '\0';
        snprintf(info + offset, sizeof(info) - offset, "%s", vendor_name);
    }
    simple_app_truncate_text(info, 21);
    canvas_draw_str(canvas, 2, 44, info);

    uint8_t list_top = 54;
    uint8_t list_bottom = DISPLAY_HEIGHT - 8;
    uint8_t max_lines =
        (uint8_t)((list_bottom - list_top + SERIAL_TEXT_LINE_HEIGHT) / SERIAL_TEXT_LINE_HEIGHT);
    if(max_lines == 0) max_lines = 1;

    size_t first = ap->client_start + app->sniffer_results_client_offset;
    size_t last_possible = ap->client_start + ap->client_count;
    if(first > last_possible) {
        app->sniffer_results_client_offset = 0;
        first = ap->client_start;
    }

    size_t lines_drawn = 0;
    uint8_t y = list_top;
    for(size_t idx = first; idx < last_possible && lines_drawn < max_lines; idx++) {
        if(idx >= SNIFFER_MAX_CLIENTS) break;
        const SnifferClientEntry* client = &app->sniffer_clients[idx];
        char line[24];
        bool selected = simple_app_sniffer_station_is_selected(app, client->mac);
        snprintf(line, sizeof(line), "%c %s", selected ? '*' : ' ', client->mac);
        canvas_draw_str(canvas, 2, y, line);
        y += SERIAL_TEXT_LINE_HEIGHT;
        lines_drawn++;
    }

    bool show_up = (app->sniffer_results_client_offset > 0) || (app->sniffer_results_ap_index > 0);
    bool show_down = ((first + lines_drawn) < last_possible) ||
                     (app->sniffer_results_ap_index + 1 < ap_count);
    if(show_up || show_down) {
        uint8_t arrow_x = DISPLAY_WIDTH - 6;
        simple_app_draw_scroll_hints(canvas, arrow_x, list_top, y - 2, show_up, show_down);
    }

    if(app->sniffer_selected_count > 0 && app->scan_selected_count > 0) {
        canvas_draw_str(canvas, 2, DISPLAY_HEIGHT - 2, "->");
    }
}

static void simple_app_draw_bt_scan_overlay(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    char status_line[48];
    if(app->bt_scan_running) {
        snprintf(status_line, sizeof(status_line), "Scanning...");
    } else if(app->bt_scan_has_data) {
        snprintf(status_line, sizeof(status_line), "Done");
    } else {
        snprintf(status_line, sizeof(status_line), "Waiting...");
    }

    canvas_draw_str(canvas, 2, 12, "BT Scan");
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH - 2, 12, AlignRight, AlignBottom, status_line);

    canvas_set_font(canvas, FontPrimary);
    char total_line[24];
    snprintf(total_line, sizeof(total_line), "Total: %d", app->bt_scan_total);
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 28, AlignCenter, AlignCenter, total_line);

    canvas_set_font(canvas, FontSecondary);
    int other = app->bt_scan_total - app->bt_scan_airtags - app->bt_scan_smarttags;
    if(other < 0) other = 0;
    char line1[32];
    char line2[32];
    snprintf(
        line1, sizeof(line1), "AirTags:%d Smart:%d", app->bt_scan_airtags, app->bt_scan_smarttags);
    snprintf(line2, sizeof(line2), "Others:%d", other);
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 42, AlignCenter, AlignCenter, line1);
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 54, AlignCenter, AlignCenter, line2);

    if(!app->bt_scan_running && app->bt_scan_has_data && app->bt_scan_total > 0) {
        canvas_draw_str_aligned(
            canvas, DISPLAY_WIDTH - 4, DISPLAY_HEIGHT - 2, AlignRight, AlignBottom, "->");
    }
}

static void simple_app_draw_probe_results_overlay(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;
    if(!app->probe_ssids || !app->probe_clients) {
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, DISPLAY_WIDTH / 2, 36, AlignCenter, AlignCenter, "OOM: probes");
        return;
    }

    size_t ssid_count = app->probe_ssid_count;
    if(app->probe_ssid_index >= ssid_count && ssid_count > 0) {
        app->probe_ssid_index = ssid_count - 1;
        app->probe_client_offset = 0;
    }

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    char header[32];
    snprintf(
        header,
        sizeof(header),
        "SSIDs:%u Req:%u",
        (unsigned)ssid_count,
        (unsigned)app->probe_total);
    canvas_draw_str(canvas, 2, 12, "Probes");
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH - 2, 12, AlignRight, AlignBottom, header);

    if(ssid_count == 0) {
        const char* msg1 = "No probes found";
        const char* msg2 = "Run a scan again";
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 32, AlignCenter, AlignCenter, msg1);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 48, AlignCenter, AlignCenter, msg2);
        return;
    }

    const ProbeSsidEntry* ssid = &app->probe_ssids[app->probe_ssid_index];
    canvas_set_font(canvas, FontPrimary);
    char computed_title[64];
    snprintf(
        computed_title, sizeof(computed_title), "%s", ssid->ssid[0] ? ssid->ssid : "<hidden>");
    char computed_title_ascii[sizeof(computed_title)];
    simple_app_utf8_to_ascii_pl(
        computed_title, computed_title_ascii, sizeof(computed_title_ascii));
    strncpy(computed_title, computed_title_ascii, sizeof(computed_title) - 1);
    computed_title[sizeof(computed_title) - 1] = '\0';

    const char* full_title = (app->overlay_title_scroll_text[0] != '\0') ?
                                 app->overlay_title_scroll_text :
                                 computed_title;
    size_t title_len = strlen(full_title);
    size_t char_limit = OVERLAY_TITLE_CHAR_LIMIT;
    if(char_limit >= sizeof(computed_title)) char_limit = sizeof(computed_title) - 1;
    if(char_limit == 0) char_limit = 1;
    if(title_len <= char_limit) {
        canvas_draw_str(canvas, 2, 30, full_title);
    } else {
        size_t offset = app->overlay_title_scroll_offset;
        size_t max_offset = title_len - char_limit;
        if(offset > max_offset) offset = max_offset;
        size_t copy_len = char_limit;
        if(offset + copy_len > title_len) copy_len = title_len - offset;
        char title_window[64];
        if(copy_len >= sizeof(title_window)) copy_len = sizeof(title_window) - 1;
        memcpy(title_window, full_title + offset, copy_len);
        title_window[copy_len] = '\0';
        canvas_draw_str(canvas, 2, 30, title_window);
    }

    canvas_set_font(canvas, FontSecondary);
    uint16_t current_client = (ssid->client_count == 0) ? 0 :
                                                          (uint16_t)(app->probe_client_offset + 1);
    if(current_client > ssid->client_count) current_client = ssid->client_count;

    char info[32];
    snprintf(
        info,
        sizeof(info),
        "Client: %u/%u",
        (unsigned)current_client,
        (unsigned)ssid->client_count);
    simple_app_truncate_text(info, 21);
    canvas_draw_str(canvas, 2, 44, info);

    uint8_t list_top = 54;
    uint8_t list_bottom = DISPLAY_HEIGHT - 8;
    uint8_t max_lines =
        (uint8_t)((list_bottom - list_top + SERIAL_TEXT_LINE_HEIGHT) / SERIAL_TEXT_LINE_HEIGHT);
    if(max_lines == 0) max_lines = 1;

    size_t first = ssid->client_start + app->probe_client_offset;
    size_t last_possible = ssid->client_start + ssid->client_count;
    if(first > last_possible) {
        app->probe_client_offset = 0;
        first = ssid->client_start;
    }

    size_t lines_drawn = 0;
    uint8_t y = list_top;
    for(size_t idx = first; idx < last_possible && lines_drawn < max_lines; idx++) {
        if(idx >= PROBE_MAX_CLIENTS) break;
        const SnifferClientEntry* client = &app->probe_clients[idx];
        canvas_draw_str(canvas, 6, y, client->mac);
        y += SERIAL_TEXT_LINE_HEIGHT;
        lines_drawn++;
    }

    bool show_up = (app->probe_client_offset > 0) || (app->probe_ssid_index > 0);
    bool show_down = ((first + lines_drawn) < last_possible) ||
                     (app->probe_ssid_index + 1 < ssid_count);
    if(show_up || show_down) {
        uint8_t arrow_x = DISPLAY_WIDTH - 6;
        simple_app_draw_scroll_hints(canvas, arrow_x, list_top, y - 2, show_up, show_down);
    }

    canvas_draw_str(canvas, 2, DISPLAY_HEIGHT - 2, "<-");
}

static void simple_app_draw_bt_scan_list(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;
    canvas_set_color(canvas, ColorBlack);
    if(!app->bt_scan_preview) {
        canvas_draw_str_aligned(
            canvas, DISPLAY_WIDTH / 2, 36, AlignCenter, AlignCenter, "BT buffers OOM");
        return;
    }

    if(app->bt_scan_preview_count == 0) {
        canvas_draw_str_aligned(
            canvas, DISPLAY_WIDTH / 2, 36, AlignCenter, AlignCenter, "No parsed devices yet");
        return;
    }

    canvas_set_font(canvas, FontSecondary);
    const size_t char_limit = 21;
    const uint8_t list_top = 10;
    const uint8_t list_bottom = DISPLAY_HEIGHT - 2;
    uint8_t y = list_top;
    uint8_t max_lines =
        (uint8_t)((list_bottom - list_top + SERIAL_TEXT_LINE_HEIGHT) / SERIAL_TEXT_LINE_HEIGHT);
    if(max_lines > 6) max_lines = 6;
    if(max_lines == 0) max_lines = 1;

    if(app->bt_scan_list_offset >= app->bt_scan_preview_count) {
        app->bt_scan_list_offset =
            (app->bt_scan_preview_count > 0) ? (app->bt_scan_preview_count - 1) : 0;
    }

    size_t lines_used = 0;
    size_t last_drawn = (size_t)-1;
    bool drew_any = false;
    for(size_t i = app->bt_scan_list_offset; i < app->bt_scan_preview_count; i++) {
        BtScanPreview* p = &app->bt_scan_preview[i];
        uint8_t item_h = simple_app_bt_scan_item_height(p);
        if(lines_used + item_h > max_lines) break;

        char line[64];
        snprintf(line, sizeof(line), "%s %d", p->mac, p->rssi);

        const char* to_draw = line;
        char scroll_buf[64];
        size_t len = strlen(line);
        if(len > char_limit && app->bt_locator_scroll_text) {
            if(strncmp(app->bt_locator_scroll_text, line, BT_LOCATOR_SCROLL_TEXT_LEN) != 0) {
                simple_app_bt_locator_set_scroll_text(app, line);
            }
            simple_app_bt_locator_tick_scroll(app, char_limit);
            size_t offset = app->bt_locator_scroll_offset;
            if(offset >= len) offset = 0;
            size_t copy_len = (len - offset > char_limit) ? char_limit : (len - offset);
            memcpy(scroll_buf, line + offset, copy_len);
            scroll_buf[copy_len] = '\0';
            to_draw = scroll_buf;
        } else {
            simple_app_bt_locator_reset_scroll(app);
        }
        canvas_draw_str(canvas, 4, y, to_draw);
        y += SERIAL_TEXT_LINE_HEIGHT;
        lines_used++;
        drew_any = true;

        if(p->has_name && p->name[0] != '\0' && lines_used < max_lines) {
            char name_buf[48];
            strncpy(name_buf, p->name, sizeof(name_buf) - 1);
            name_buf[sizeof(name_buf) - 1] = '\0';
            simple_app_truncate_text(name_buf, char_limit);
            canvas_draw_str(canvas, 12, y, name_buf);
            y += SERIAL_TEXT_LINE_HEIGHT;
            lines_used++;
        }
        last_drawn = i;
    }

    if(app->bt_scan_preview_count > 0 && drew_any) {
        bool show_up = app->bt_scan_list_offset > 0;
        bool show_down = (last_drawn + 1) < app->bt_scan_preview_count;
        if(show_up || show_down) {
            uint8_t arrow_x = DISPLAY_WIDTH - 6;
            int16_t content_top = list_top;
            int16_t content_bottom = y - 2;
            simple_app_draw_scroll_hints(
                canvas, arrow_x, content_top, content_bottom, show_up, show_down);
        }
    }
}

static void simple_app_draw_airtag_overlay(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    char status_line[48];
    if(app->bt_scan_running) {
        snprintf(status_line, sizeof(status_line), "Scanning...");
    } else if(app->bt_scan_has_data) {
        snprintf(status_line, sizeof(status_line), "Done");
    } else {
        snprintf(status_line, sizeof(status_line), "Waiting...");
    }

    canvas_draw_str(canvas, 2, 12, "AirTag Scan");
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH - 2, 12, AlignRight, AlignBottom, status_line);

    canvas_set_font(canvas, FontPrimary);
    char total_line[24];
    snprintf(total_line, sizeof(total_line), "Total: %d", app->bt_scan_total);
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 28, AlignCenter, AlignCenter, total_line);

    canvas_set_font(canvas, FontSecondary);
    int other = app->bt_scan_total - app->bt_scan_airtags - app->bt_scan_smarttags;
    if(other < 0) other = 0;
    char line1[32];
    char line2[32];
    snprintf(line1, sizeof(line1), "AirTags: %d", app->bt_scan_airtags);
    snprintf(line2, sizeof(line2), "Smart:%d Other:%d", app->bt_scan_smarttags, other);
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 42, AlignCenter, AlignCenter, line1);
    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, 54, AlignCenter, AlignCenter, line2);

    if(!app->bt_scan_running && app->bt_scan_has_data && app->bt_scan_total > 0) {
        canvas_draw_str_aligned(
            canvas, DISPLAY_WIDTH - 4, DISPLAY_HEIGHT - 2, AlignRight, AlignBottom, "->");
    }
}

static void simple_app_draw_bt_locator(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;
    if(!app->bt_locator_devices) {
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas, DISPLAY_WIDTH / 2, 32, AlignCenter, AlignCenter, "BT buffers OOM");
        return;
    }

    canvas_set_color(canvas, ColorBlack);
    uint32_t now_ticks = furi_get_tick();
    uint32_t ticks_per_second = furi_ms_to_ticks(1000);
    if(ticks_per_second == 0) ticks_per_second = 1;
    uint32_t warmup_ticks = furi_ms_to_ticks(BT_LOCATOR_WARMUP_MS);
    if(warmup_ticks == 0) warmup_ticks = 1;
    uint32_t warmup_elapsed =
        (app->bt_locator_start_tick > 0) ? (now_ticks - app->bt_locator_start_tick) : 0;
    uint32_t console_elapsed = (app->bt_locator_last_console_tick > 0) ?
                                   (now_ticks - app->bt_locator_last_console_tick) :
                                   warmup_elapsed;
    bool warmup_active =
        !app->bt_locator_has_rssi &&
        ((app->bt_locator_start_tick > 0 && warmup_elapsed < warmup_ticks) ||
         (app->bt_locator_last_console_tick > 0 && console_elapsed < warmup_ticks));

    const char* mac = NULL;
    if(app->bt_locator_mac[0] != '\0') {
        mac = app->bt_locator_mac;
    } else if(app->bt_locator_saved_mac[0] != '\0') {
        mac = app->bt_locator_saved_mac;
    } else if(
        app->bt_locator_selected >= 0 &&
        app->bt_locator_selected < (int32_t)app->bt_locator_count) {
        mac = app->bt_locator_devices[app->bt_locator_selected].mac;
    }
    if(!mac || mac[0] == '\0' || mac[0] == '>' || (mac[0] == 'U' && mac[1] == 's')) {
        mac = (app->bt_locator_saved_mac[0] != '\0') ? app->bt_locator_saved_mac : "No MAC";
    }
    canvas_set_font(canvas, FontSecondary);
    const uint8_t mac_y = 12;
    const uint8_t name_y = 22;
    const uint8_t rssi_y = 36;
    const uint8_t quality_y = 50;
    const uint8_t status_y = 62;

    canvas_draw_str_aligned(canvas, DISPLAY_WIDTH / 2, mac_y, AlignCenter, AlignCenter, mac);

    canvas_set_font(canvas, FontPrimary);
    char rssi_line[32];
    if(app->bt_locator_has_rssi) {
        snprintf(rssi_line, sizeof(rssi_line), "RSSI: %d dBm", app->bt_locator_rssi);
    } else {
        snprintf(rssi_line, sizeof(rssi_line), "RSSI: N/A");
    }
    canvas_draw_str_aligned(
        canvas, DISPLAY_WIDTH / 2, rssi_y, AlignCenter, AlignCenter, rssi_line);

    canvas_set_font(canvas, FontSecondary);
    const char* quality = warmup_active ? "Warm up" : "No signal";
    if(app->bt_locator_has_rssi) {
        if(app->bt_locator_rssi >= -60) {
            quality = "Great";
        } else if(app->bt_locator_rssi >= -75) {
            quality = "Good";
        } else if(app->bt_locator_rssi >= -90) {
            quality = "Poor";
        } else {
            quality = "Very weak";
        }
    }
    canvas_draw_str_aligned(
        canvas, DISPLAY_WIDTH / 2, quality_y, AlignCenter, AlignCenter, quality);

    // Use free space to show the device name (if known)
    const char* name_ptr = NULL;
    if(app->bt_locator_current_name[0] != '\0') {
        name_ptr = app->bt_locator_current_name;
    } else if(
        app->bt_locator_selected >= 0 &&
        (size_t)app->bt_locator_selected < app->bt_locator_count) {
        const char* n = app->bt_locator_devices[app->bt_locator_selected].name;
        if(n && n[0] != '\0') name_ptr = n;
    } else {
        for(size_t i = 0; i < app->bt_locator_count; i++) {
            if(strncmp(
                   app->bt_locator_devices[i].mac, mac, sizeof(app->bt_locator_devices[i].mac)) ==
               0) {
                if(app->bt_locator_devices[i].name[0] != '\0') {
                    name_ptr = app->bt_locator_devices[i].name;
                }
                break;
            }
        }
    }
    if(name_ptr) {
        char name_line[40];
        strncpy(name_line, name_ptr, sizeof(name_line) - 1);
        name_line[sizeof(name_line) - 1] = '\0';
        simple_app_truncate_text(name_line, 19);
        canvas_draw_str_aligned(
            canvas, DISPLAY_WIDTH / 2, name_y, AlignCenter, AlignCenter, name_line);
    }

    char status_line[48];
    if(app->bt_scan_running) {
        snprintf(status_line, sizeof(status_line), "Scanning...");
    } else if(app->bt_locator_list_ready) {
        snprintf(status_line, sizeof(status_line), "Done");
    } else if(app->bt_scan_last_update_tick > 0) {
        uint32_t elapsed = (now_ticks - app->bt_scan_last_update_tick) / ticks_per_second;
        snprintf(status_line, sizeof(status_line), "Updated: %lus ago", (unsigned long)elapsed);
    } else if(warmup_active) {
        uint32_t basis = warmup_elapsed;
        if(app->bt_locator_last_console_tick > 0) {
            uint32_t console_elapsed_local = (app->bt_locator_last_console_tick <= now_ticks) ?
                                                 (now_ticks - app->bt_locator_last_console_tick) :
                                                 0;
            if(console_elapsed_local < basis || app->bt_locator_start_tick == 0) {
                basis = console_elapsed_local;
            }
        }
        uint32_t remaining_ticks = (basis < warmup_ticks) ? (warmup_ticks - basis) : 0;
        uint32_t remaining_seconds = (remaining_ticks + ticks_per_second - 1) / ticks_per_second;
        snprintf(
            status_line, sizeof(status_line), "Warm up... %lus", (unsigned long)remaining_seconds);
    } else {
        snprintf(status_line, sizeof(status_line), "No data");
    }
    canvas_draw_str_aligned(
        canvas, DISPLAY_WIDTH / 2, status_y, AlignCenter, AlignBottom, status_line);

    if(app->bt_locator_list_ready) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas, DISPLAY_WIDTH - 4, DISPLAY_HEIGHT - 2, AlignRight, AlignBottom, "->");
    }
}

static void simple_app_draw_bt_locator_list(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;
    if(!app->bt_locator_devices) {
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas, DISPLAY_WIDTH / 2, 32, AlignCenter, AlignCenter, "BT buffers OOM");
        return;
    }

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);

    uint8_t list_top = 10;
    uint8_t list_bottom = DISPLAY_HEIGHT - 2;

    if(app->bt_locator_list_loading) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas,
            DISPLAY_WIDTH / 2,
            (list_top + list_bottom) / 2 - 6,
            AlignCenter,
            AlignCenter,
            "Scanning...");
        char count_line[32];
        snprintf(count_line, sizeof(count_line), "Found: %u", (unsigned)app->bt_locator_count);
        canvas_draw_str_aligned(
            canvas,
            DISPLAY_WIDTH / 2,
            (list_top + list_bottom) / 2 + 6,
            AlignCenter,
            AlignCenter,
            count_line);
        return;
    }

    if(app->bt_locator_count == 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas,
            DISPLAY_WIDTH / 2,
            (list_top + list_bottom) / 2 - 6,
            AlignCenter,
            AlignCenter,
            "No devices");
        return;
    }

    canvas_set_font(canvas, FontSecondary);
    size_t max_lines =
        (size_t)((list_bottom - list_top + SERIAL_TEXT_LINE_HEIGHT) / SERIAL_TEXT_LINE_HEIGHT);
    if(max_lines > 6) max_lines = 6;
    if(max_lines == 0) max_lines = 1;
    const size_t char_limit = 21;
    uint8_t y = list_top;
    size_t lines_used = 0;
    for(size_t i = app->bt_locator_offset; i < app->bt_locator_count && lines_used < max_lines;
        i++) {
        const bool selected = (i == app->bt_locator_index);
        const bool picked = ((int32_t)i == app->bt_locator_selected);
        const char* mac = app->bt_locator_devices[i].mac;
        int rssi = app->bt_locator_devices[i].rssi;
        bool has_rssi = app->bt_locator_devices[i].has_rssi;
        const char* name = app->bt_locator_devices[i].name;
        char line[48];
        if(has_rssi) {
            snprintf(line, sizeof(line), "%s %d", mac, rssi);
        } else {
            snprintf(line, sizeof(line), "%s ?", mac);
        }
        const char* to_draw = line;
        char scroll_buf[48];
        size_t len = strlen(line);
        if(selected && len > char_limit && app->bt_locator_scroll_text) {
            if(strncmp(app->bt_locator_scroll_text, line, BT_LOCATOR_SCROLL_TEXT_LEN) != 0) {
                simple_app_bt_locator_set_scroll_text(app, line);
            }
            simple_app_bt_locator_tick_scroll(app, char_limit);
            size_t offset = app->bt_locator_scroll_offset;
            if(offset >= len) offset = 0;
            size_t copy_len = (len - offset > char_limit) ? char_limit : (len - offset);
            memcpy(scroll_buf, line + offset, copy_len);
            scroll_buf[copy_len] = '\0';
            to_draw = scroll_buf;
        } else {
            simple_app_bt_locator_reset_scroll(app);
            simple_app_truncate_text(line, char_limit);
            to_draw = line;
        }
        if(selected) {
            canvas_draw_str(canvas, 2, y, ">");
        }
        if(picked) {
            canvas_draw_str(canvas, 10, y, "*");
        }
        canvas_draw_str(canvas, picked ? 18 : 12, y, to_draw);
        y += SERIAL_TEXT_LINE_HEIGHT;
        lines_used++;

        if(name && name[0] != '\0' && lines_used < max_lines) {
            char name_buf[48];
            strncpy(name_buf, name, sizeof(name_buf) - 1);
            name_buf[sizeof(name_buf) - 1] = '\0';
            simple_app_truncate_text(name_buf, char_limit);
            canvas_draw_str(canvas, picked ? 18 : 12, y, name_buf);
            y += SERIAL_TEXT_LINE_HEIGHT;
            lines_used++;
        }
    }

    // Hint action only when a target is selected
    if(app->bt_locator_selected >= 0 &&
       app->bt_locator_selected < (int32_t)app->bt_locator_count) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas, DISPLAY_WIDTH - 2, DISPLAY_HEIGHT - 2, AlignRight, AlignBottom, "->");
    }
}

static void __attribute__((unused))
simple_app_draw_bt_scan_serial(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;
    canvas_set_color(canvas, ColorBlack);
    const int16_t console_top = 8;
    const int16_t line_height = SERIAL_TEXT_LINE_HEIGHT;
    if(app->bt_locator_mode) {
        simple_app_draw_bt_locator(app, canvas);
        return;
    }
    if(app->bt_locator_console_mode) {
        canvas_set_font(canvas, FontSecondary);
        char display_lines[BT_SCAN_CONSOLE_LINES][64];
        size_t lines_filled = simple_app_render_display_lines(
            app, app->serial_scroll, display_lines, BT_SCAN_CONSOLE_LINES);

        int16_t y = console_top;
        if(lines_filled == 0) {
            canvas_draw_str(canvas, 2, y, "Scanning...");
            y += SERIAL_TEXT_LINE_HEIGHT;
        } else {
            for(size_t i = 0; i < lines_filled; i++) {
                canvas_draw_str(canvas, 2, y, display_lines[i][0] ? display_lines[i] : " ");
                y += SERIAL_TEXT_LINE_HEIGHT;
            }
        }

        size_t total_lines = simple_app_total_display_lines(app);
        if(total_lines > BT_SCAN_CONSOLE_LINES) {
            size_t max_scroll = simple_app_max_scroll(app);
            bool show_up = (app->serial_scroll > 0);
            bool show_down = (app->serial_scroll < max_scroll);
            if(show_up || show_down) {
                uint8_t arrow_x = DISPLAY_WIDTH - 6;
                int16_t content_top = console_top;
                int16_t visible_rows = (BT_SCAN_CONSOLE_LINES > 0) ? (BT_SCAN_CONSOLE_LINES - 1) :
                                                                     0;
                int16_t content_bottom = console_top + (int16_t)(visible_rows * line_height);
                simple_app_draw_scroll_hints(
                    canvas, arrow_x, content_top, content_bottom, show_up, show_down);
            }
        }

        if(app->bt_locator_list_ready) {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(
                canvas, DISPLAY_WIDTH - 2, DISPLAY_HEIGHT - 2, AlignRight, AlignBottom, "->");
        }
        return;
    }
    canvas_set_font(canvas, FontSecondary);
    char display_lines[BT_SCAN_CONSOLE_LINES][64];
    size_t lines_filled = simple_app_render_display_lines(
        app, app->serial_scroll, display_lines, BT_SCAN_CONSOLE_LINES);

    int16_t y = console_top;
    if(lines_filled == 0) {
        canvas_draw_str(canvas, 2, y, "No UART data");
    } else {
        for(size_t i = 0; i < lines_filled; i++) {
            canvas_draw_str(canvas, 2, y, display_lines[i][0] ? display_lines[i] : " ");
            y += SERIAL_TEXT_LINE_HEIGHT;
        }
    }

    size_t total_lines = simple_app_total_display_lines(app);
    if(total_lines > BT_SCAN_CONSOLE_LINES) {
        size_t max_scroll = simple_app_max_scroll(app);
        bool show_up = (app->serial_scroll > 0);
        bool show_down = (app->serial_scroll < max_scroll);
        if(show_up || show_down) {
            uint8_t arrow_x = DISPLAY_WIDTH - 6;
            int16_t visible_rows = (BT_SCAN_CONSOLE_LINES > 0) ? (BT_SCAN_CONSOLE_LINES - 1) : 0;
            int16_t content_bottom = console_top + (int16_t)(visible_rows * line_height);
            /* Custom arrow placement: top hugs screen edge, bottom lifted off divider */
            if(show_up) {
                int16_t up_base = 2; // apex ends at y=0
                if(up_base < 0) up_base = 0;
                simple_app_draw_scroll_arrow(canvas, arrow_x, up_base, true);
            }
            if(show_down) {
                int16_t down_base = content_bottom - 2; // raise 4px higher than before
                if(down_base > 63) down_base = 63;
                simple_app_draw_scroll_arrow(canvas, arrow_x, down_base, false);
            }
        }
    }

    size_t used_lines = (lines_filled > 0) ? lines_filled : 1;
    if(used_lines > BT_SCAN_CONSOLE_LINES) used_lines = BT_SCAN_CONSOLE_LINES;
    int16_t divider_y = (int16_t)(console_top + (int16_t)(used_lines * line_height) - 2);
    if(divider_y < console_top) divider_y = console_top;
    if(divider_y > DISPLAY_HEIGHT - 6) {
        divider_y = DISPLAY_HEIGHT - 6;
    }
    canvas_draw_line(canvas, 0, divider_y, DISPLAY_WIDTH - 1, divider_y);

    char line1[32];
    char line2[32];
    char total_line[32];
    if(app->bt_locator_mode) {
        // Locator view is drawn separately
        return;
    }
    snprintf(line1, sizeof(line1), "AirTags: %d", app->bt_scan_airtags);
    snprintf(line2, sizeof(line2), "SmartTags: %d", app->bt_scan_smarttags);
    snprintf(total_line, sizeof(total_line), "Total: %d", app->bt_scan_total);

    canvas_set_font(canvas, FontPrimary);
    int16_t bottom_y = DISPLAY_HEIGHT - 2;
    int16_t top_y = bottom_y - SERIAL_TEXT_LINE_HEIGHT;
    if(top_y <= divider_y + 2) {
        top_y = divider_y + 2;
        bottom_y = top_y + SERIAL_TEXT_LINE_HEIGHT;
        if(bottom_y > DISPLAY_HEIGHT - 2) {
            bottom_y = DISPLAY_HEIGHT - 2;
            top_y = bottom_y - SERIAL_TEXT_LINE_HEIGHT;
        }
    }
    canvas_draw_str_aligned(canvas, 2, top_y, AlignLeft, AlignBottom, line1);
    canvas_draw_str_aligned(canvas, 2, bottom_y, AlignLeft, AlignBottom, line2);

    // Place total in bottom-right corner to avoid overlapping the counts
    canvas_draw_str_aligned(
        canvas, DISPLAY_WIDTH - 2, DISPLAY_HEIGHT - 2, AlignRight, AlignBottom, total_line);
}

static void simple_app_draw_serial(SimpleApp* app, Canvas* canvas) {
    if(app && app->deauth_guard_view_active && !app->deauth_guard_full_console) {
        simple_app_draw_deauth_guard(app, canvas);
        return;
    }

    if(app && app->handshake_view_active && !app->handshake_full_console) {
        simple_app_draw_handshake_overlay(app, canvas);
        return;
    }

    if(app && app->sae_view_active && !app->sae_full_console) {
        simple_app_draw_sae_overlay(app, canvas);
        return;
    }

    if(app && app->deauth_view_active && !app->deauth_full_console) {
        simple_app_draw_deauth_overlay(app, canvas);
        return;
    }

    if(app && app->blackout_view_active && !app->blackout_full_console) {
        simple_app_draw_blackout_overlay(app, canvas);
        return;
    }

    if(app && app->sniffer_dog_view_active && !app->sniffer_dog_full_console) {
        simple_app_draw_sniffer_dog_overlay(app, canvas);
        return;
    }

    if(app && app->scanner_view_active && !app->scanner_full_console) {
        simple_app_draw_scanner_status(app, canvas);
        return;
    }

    if(app && app->evil_twin_running && !app->evil_twin_full_console) {
        simple_app_draw_evil_twin_overlay(app, canvas);
        return;
    }

    if(app && app->portal_running && !app->portal_full_console) {
        simple_app_draw_portal_overlay(app, canvas);
        return;
    }

    if(app && app->probe_results_active && !app->probe_full_console) {
        simple_app_draw_probe_results_overlay(app, canvas);
        return;
    }

    if(app && app->sniffer_results_active && !app->sniffer_full_console) {
        simple_app_draw_sniffer_results_overlay(app, canvas);
        return;
    }

    if(app && app->sniffer_view_active && !app->sniffer_full_console) {
        simple_app_draw_sniffer_overlay(app, canvas);
        return;
    }

    if(app && app->bt_scan_view_active && !app->bt_scan_full_console) {
        if(app->airtag_scan_mode) {
            if(app->bt_scan_show_list) {
                simple_app_draw_bt_scan_list(app, canvas);
            } else {
                simple_app_draw_airtag_overlay(app, canvas);
            }
            return;
        }
        if(app->bt_locator_mode) {
            simple_app_draw_bt_locator(app, canvas);
            return;
        }
        if(app->bt_scan_show_list) {
            simple_app_draw_bt_scan_list(app, canvas);
        } else {
            simple_app_draw_bt_scan_overlay(app, canvas);
        }
        return;
    }

    if(app && app->wardrive_view_active) {
        simple_app_draw_wardrive_serial(app, canvas);
        return;
    }

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    char display_lines[SERIAL_VISIBLE_LINES][64];
    size_t lines_filled = simple_app_render_display_lines(
        app, app->serial_scroll, display_lines, SERIAL_VISIBLE_LINES);

    uint8_t y = 8;
    if(lines_filled == 0) {
        canvas_draw_str(canvas, 2, y, "No UART data");
    } else {
        for(size_t i = 0; i < lines_filled; i++) {
            canvas_draw_str(canvas, 2, y, display_lines[i][0] ? display_lines[i] : " ");
            y += SERIAL_TEXT_LINE_HEIGHT;
        }
    }

    size_t total_lines = simple_app_total_display_lines(app);
    if(total_lines > SERIAL_VISIBLE_LINES) {
        size_t max_scroll = simple_app_max_scroll(app);
        bool show_up = (app->serial_scroll > 0);
        bool show_down = (app->serial_scroll < max_scroll);
        if(show_up || show_down) {
            uint8_t arrow_x = DISPLAY_WIDTH - 6;
            int16_t content_top = 8;
            int16_t content_bottom =
                8 + (int16_t)((SERIAL_VISIBLE_LINES > 0 ? (SERIAL_VISIBLE_LINES - 1) : 0) *
                              SERIAL_TEXT_LINE_HEIGHT);
            simple_app_draw_scroll_hints(
                canvas, arrow_x, content_top, content_bottom, show_up, show_down);
        }
    }

    if(app->serial_targets_hint && !app->blackout_view_active) {
        canvas_draw_str(canvas, DISPLAY_WIDTH - 14, 62, "->");
    }
}

static void simple_app_draw_results(SimpleApp* app, Canvas* canvas) {
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, app->result_font);

    if(app->scan_results_loading && app->visible_result_count == 0) {
        canvas_draw_str(canvas, 2, 20, "Loading...");
        canvas_draw_str(canvas, 2, 62, "[Results] Selected: 0");
        return;
    }

    if(app->visible_result_count == 0) {
        canvas_draw_str(canvas, 2, 20, "No results");
        canvas_draw_str(canvas, 2, 62, "[Results] Selected: 0");
        return;
    }

    simple_app_adjust_result_offset(app);

    uint8_t y = RESULT_START_Y;
    size_t visible_line_budget = (app->result_max_lines > 0) ? app->result_max_lines : 1;
    size_t lines_left = visible_line_budget;
    size_t entry_line_capacity = (app->result_max_lines > 0) ? app->result_max_lines :
                                                               RESULT_DEFAULT_MAX_LINES;
    if(entry_line_capacity == 0) entry_line_capacity = RESULT_DEFAULT_MAX_LINES;
    if(entry_line_capacity > RESULT_DEFAULT_MAX_LINES) {
        entry_line_capacity = RESULT_DEFAULT_MAX_LINES;
    }
    size_t char_limit = (app->result_char_limit > 0) ? app->result_char_limit :
                                                       RESULT_DEFAULT_CHAR_LIMIT;
    if(char_limit == 0) char_limit = RESULT_DEFAULT_CHAR_LIMIT;
    if(char_limit == 0) char_limit = 1;
    if(char_limit > 63) char_limit = 63;

    for(size_t idx = app->scan_result_offset; idx < app->visible_result_count && lines_left > 0;
        idx++) {
        const ScanResult* result = simple_app_visible_result_const(app, idx);
        if(!result) continue;

        char segments[RESULT_DEFAULT_MAX_LINES][64];
        memset(segments, 0, sizeof(segments));
        size_t segments_available =
            simple_app_build_result_lines(app, result, segments, entry_line_capacity);
        if(segments_available == 0) {
            strncpy(segments[0], "-", sizeof(segments[0]) - 1);
            segments[0][sizeof(segments[0]) - 1] = '\0';
            segments_available = 1;
        }
        if(segments_available > lines_left) {
            segments_available = lines_left;
        }

        for(size_t segment = 0; segment < segments_available && lines_left > 0; segment++) {
            if(segment == 0) {
                if(idx == app->scan_result_index) {
                    canvas_draw_str(canvas, RESULT_PREFIX_X, y, ">");
                } else {
                    canvas_draw_str(canvas, RESULT_PREFIX_X, y, " ");
                }

                char display_line[64];
                memset(display_line, 0, sizeof(display_line));
                const char* source_line = segments[0];
                size_t source_len = strlen(source_line);
                size_t local_limit = char_limit;
                if(local_limit >= sizeof(display_line)) {
                    local_limit = sizeof(display_line) - 1;
                }
                if(local_limit == 0) {
                    local_limit = 1;
                }

                if(idx == app->scan_result_index) {
                    if(source_len <= local_limit) {
                        strncpy(display_line, source_line, sizeof(display_line) - 1);
                        display_line[sizeof(display_line) - 1] = '\0';
                    } else {
                        size_t offset = app->result_scroll_offset;
                        size_t max_offset = source_len - local_limit;
                        if(offset > max_offset) {
                            offset = max_offset;
                        }
                        size_t copy_len = local_limit;
                        if(offset + copy_len > source_len) {
                            copy_len = source_len - offset;
                        }
                        if(copy_len > sizeof(display_line) - 1) {
                            copy_len = sizeof(display_line) - 1;
                        }
                        memcpy(display_line, source_line + offset, copy_len);
                        display_line[copy_len] = '\0';
                    }
                } else {
                    strncpy(display_line, source_line, sizeof(display_line) - 1);
                    display_line[sizeof(display_line) - 1] = '\0';
                    if(source_len > local_limit) {
                        size_t truncate_limit = local_limit;
                        if(truncate_limit >= sizeof(display_line)) {
                            truncate_limit = sizeof(display_line) - 1;
                        }
                        if(truncate_limit == 0) {
                            truncate_limit = 1;
                        }
                        simple_app_truncate_text(display_line, truncate_limit);
                    }
                }

                const char* render_line = (display_line[0] != '\0') ? display_line : " ";
                canvas_draw_str(canvas, RESULT_TEXT_X, y, render_line);
            } else {
                const char* render_line = (segments[segment][0] != '\0') ? segments[segment] : " ";
                canvas_draw_str(canvas, RESULT_TEXT_X, y, render_line);
            }
            y += app->result_line_height;
            lines_left--;
            if(lines_left == 0) break;
        }

        if(lines_left > 0) {
            bool more_entries = false;
            for(size_t next = idx + 1; next < app->visible_result_count; next++) {
                const ScanResult* next_result = simple_app_visible_result_const(app, next);
                if(!next_result) continue;
                uint8_t next_lines = simple_app_result_line_count(app, next_result);
                if(next_lines == 0) next_lines = 1;
                if(next_lines <= lines_left) {
                    more_entries = true;
                    break;
                }
            }
            if(more_entries) {
                y += RESULT_ENTRY_SPACING;
            }
        }
    }

    size_t total_lines = simple_app_total_result_lines(app);
    size_t offset_lines = simple_app_result_offset_lines(app);
    if(total_lines > visible_line_budget) {
        size_t max_scroll = total_lines - visible_line_budget;
        if(offset_lines > max_scroll) offset_lines = max_scroll;
        bool show_up = (offset_lines > 0);
        bool show_down = (offset_lines < max_scroll);
        if(show_up || show_down) {
            uint8_t arrow_x = DISPLAY_WIDTH - 6;
            int16_t content_top = RESULT_START_Y;
            int16_t visible_rows = (visible_line_budget > 0) ? (int16_t)(visible_line_budget - 1) :
                                                               0;
            int16_t content_bottom =
                RESULT_START_Y + (int16_t)(visible_rows * app->result_line_height);
            simple_app_draw_scroll_hints(
                canvas, arrow_x, content_top, content_bottom, show_up, show_down);
        }
    }

    char footer[48];
    snprintf(footer, sizeof(footer), "[Results] Selected: %u", (unsigned)app->scan_selected_count);
    simple_app_truncate_text(footer, SERIAL_LINE_CHAR_LIMIT);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 62, footer);
    if(app->scan_selected_count > 0) {
        canvas_draw_str(canvas, DISPLAY_WIDTH - 10, 62, "->");
    }
}

static void simple_app_draw_setup_led(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 16, 14, "LED Settings");

    canvas_set_font(canvas, FontSecondary);
    uint8_t y = 32;
    const char* state = app->led_enabled ? "On" : "Off";
    char line[32];
    snprintf(line, sizeof(line), "State: %s", state);
    if(app->led_setup_index == 0) {
        canvas_draw_str(canvas, 2, y, ">");
    }
    canvas_draw_str(canvas, 16, y, line);

    y += 14;
    uint32_t level = app->led_level;
    if(level < 1) level = 1;
    if(level > 100) level = 100;
    snprintf(line, sizeof(line), "Brightness: %lu", (unsigned long)level);
    if(app->led_setup_index == 1) {
        canvas_draw_str(canvas, 2, y, ">");
    }
    canvas_draw_str(canvas, 16, y, line);

    const char* footer = (app->led_setup_index == 0) ? "Left/Right toggle, Back exit" :
                                                       "Left/Right adjust, Back exit";
    canvas_draw_str(canvas, 2, 62, footer);
}

static void simple_app_draw_setup_boot(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 10, 14, app->boot_setup_long ? "Boot Long" : "Boot Short");

    canvas_set_font(canvas, FontSecondary);
    uint8_t y = 32;
    const char* status = app->boot_setup_long ? (app->boot_long_enabled ? "On" : "Off") :
                                                (app->boot_short_enabled ? "On" : "Off");
    char line[48];
    snprintf(line, sizeof(line), "Status: %s", status);
    if(app->boot_setup_index == 0) {
        canvas_draw_str(canvas, 2, y, ">");
    }
    canvas_draw_str(canvas, 16, y, line);

    y += 14;
    uint8_t cmd_index = app->boot_setup_long ? app->boot_long_command_index :
                                               app->boot_short_command_index;
    if(cmd_index >= BOOT_COMMAND_OPTION_COUNT) {
        cmd_index = 0;
    }
    snprintf(line, sizeof(line), "Command:");
    if(app->boot_setup_index == 1) {
        canvas_draw_str(canvas, 2, y, ">");
    }
    canvas_draw_str(canvas, 16, y, line);

    // Draw command value on its own line to keep it within screen width
    y += 12;
    canvas_draw_str(canvas, 16, y, boot_command_options[cmd_index]);
}

static void simple_app_reset_sd_listing(SimpleApp* app) {
    if(!app) return;
    simple_app_sd_free_files(app);
    app->sd_listing_active = false;
    app->sd_list_header_seen = false;
    app->sd_list_length = 0;
    app->sd_file_count = 0;
    app->sd_file_popup_index = 0;
    app->sd_file_popup_offset = 0;
    app->sd_file_popup_active = false;
    app->sd_delete_confirm_active = false;
    app->sd_delete_confirm_yes = false;
    app->sd_delete_target_name[0] = '\0';
    app->sd_delete_target_path[0] = '\0';
    simple_app_reset_sd_scroll(app);
}

static void simple_app_copy_sd_folder(SimpleApp* app, const SdFolderOption* folder) {
    if(!app) return;
    if(folder) {
        strncpy(app->sd_current_folder_label, folder->label, SD_MANAGER_FOLDER_LABEL_MAX - 1);
        app->sd_current_folder_label[SD_MANAGER_FOLDER_LABEL_MAX - 1] = '\0';
        strncpy(app->sd_current_folder_path, folder->path, SD_MANAGER_PATH_MAX - 1);
        app->sd_current_folder_path[SD_MANAGER_PATH_MAX - 1] = '\0';
    } else {
        app->sd_current_folder_label[0] = '\0';
        app->sd_current_folder_path[0] = '\0';
    }

    size_t len = strlen(app->sd_current_folder_path);
    while(len > 0 && app->sd_current_folder_path[len - 1] == '/') {
        app->sd_current_folder_path[--len] = '\0';
    }
}

static void simple_app_open_sd_manager(SimpleApp* app) {
    if(!app) return;
    app->screen = ScreenSetupSdManager;
    simple_app_reset_sd_scroll(app);
    if(app->sd_folder_index >= sd_folder_option_count) {
        app->sd_folder_index = 0;
    }
    app->sd_folder_offset = 0;
    simple_app_reset_sd_listing(app);
    app->sd_refresh_pending = false;

    if(sd_folder_option_count > 0) {
        simple_app_copy_sd_folder(app, &sd_folder_options[app->sd_folder_index]);
    } else {
        simple_app_copy_sd_folder(app, NULL);
    }

    app->sd_folder_popup_active = (sd_folder_option_count > 0);
    if(app->viewport) {
        view_port_update(app->viewport);
    }
}

static void simple_app_request_sd_listing(SimpleApp* app, const SdFolderOption* folder) {
    if(!app || !folder) return;
    app->sd_folder_popup_active = false;
    simple_app_reset_sd_listing(app);
    if(!simple_app_sd_alloc_files(app)) {
        simple_app_show_status_message(app, "OOM: SD list", 2000, true);
        return;
    }
    simple_app_copy_sd_folder(app, folder);
    app->sd_listing_active = true;
    app->sd_refresh_pending = false;

    simple_app_show_status_message(app, "Listing files...", 0, false);

    char command[200];
    if(app->sd_current_folder_path[0] != '\0') {
        snprintf(command, sizeof(command), "list_dir \"%s\"", app->sd_current_folder_path);
    } else {
        snprintf(command, sizeof(command), "list_dir");
    }
    simple_app_send_command(app, command, false);
    if(app->viewport) {
        view_port_update(app->viewport);
    }
}

static void simple_app_finish_sd_listing(SimpleApp* app) {
    if(!app || !app->sd_listing_active) return;

    app->sd_listing_active = false;
    app->sd_list_length = 0;
    app->sd_list_header_seen = false;
    app->last_command_sent = false;
    simple_app_clear_status_message(app);

    if(app->sd_file_count == 0) {
        simple_app_show_status_message(app, "No files in\nselected folder", 1500, true);
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    app->sd_file_popup_index = 0;
    size_t visible = app->sd_file_count;
    if(visible > SD_MANAGER_POPUP_VISIBLE_LINES) {
        visible = SD_MANAGER_POPUP_VISIBLE_LINES;
    }
    app->sd_file_popup_offset = 0;
    if(app->sd_file_popup_index >= app->sd_file_count) {
        app->sd_file_popup_index = app->sd_file_count - 1;
    }

    app->sd_file_popup_active = true;
    simple_app_reset_sd_scroll(app);
    if(app->viewport) {
        view_port_update(app->viewport);
    }
}

static void simple_app_sd_scroll_buffer(
    SimpleApp* app,
    const char* text,
    size_t char_limit,
    char* out,
    size_t out_size) {
    if(!app || !text || !out || out_size == 0) return;
    if(char_limit == 0) char_limit = 1;
    if(char_limit >= out_size) {
        char_limit = out_size - 1;
    }
    size_t len = strlen(text);
    if(len <= char_limit) {
        simple_app_reset_sd_scroll(app);
        strncpy(out, text, out_size - 1);
        out[out_size - 1] = '\0';
        return;
    }

    if(strncmp(text, app->sd_scroll_text, SD_MANAGER_FILE_NAME_MAX) != 0 ||
       app->sd_scroll_char_limit != char_limit) {
        strncpy(app->sd_scroll_text, text, SD_MANAGER_FILE_NAME_MAX - 1);
        app->sd_scroll_text[SD_MANAGER_FILE_NAME_MAX - 1] = '\0';
        app->sd_scroll_char_limit = (uint8_t)char_limit;
        app->sd_scroll_offset = 0;
        app->sd_scroll_direction = 1;
        app->sd_scroll_hold = RESULT_SCROLL_EDGE_PAUSE_STEPS;
        app->sd_scroll_last_tick = furi_get_tick();
    }

    size_t max_offset = len - char_limit;
    if(app->sd_scroll_offset > max_offset) {
        app->sd_scroll_offset = max_offset;
    }

    const char* start = text + app->sd_scroll_offset;
    strncpy(out, start, char_limit);
    out[char_limit] = '\0';
}

static void simple_app_process_sd_line(SimpleApp* app, const char* line) {
    if(!app || !line || !app->sd_listing_active) return;
    if(!app->sd_files) return;

    const char* cursor = line;
    while(*cursor == ' ' || *cursor == '\t') {
        cursor++;
    }

    if(*cursor == '\0') {
        if(app->sd_list_header_seen || app->sd_file_count > 0) {
            simple_app_finish_sd_listing(app);
        }
        return;
    }

    if(strncmp(cursor, "Files in", 8) == 0 || strncmp(cursor, "Found", 5) == 0) {
        app->sd_list_header_seen = true;
        return;
    }

    if(strncmp(cursor, "No files", 8) == 0) {
        app->sd_file_count = 0;
        simple_app_finish_sd_listing(app);
        return;
    }

    if(strncmp(cursor, "Failed", 6) == 0 || strncmp(cursor, "Invalid", 7) == 0) {
        simple_app_reset_sd_listing(app);
        app->last_command_sent = false;
        simple_app_show_status_message(app, "SD listing failed", 1500, true);
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(!isdigit((unsigned char)cursor[0])) {
        return;
    }

    char* endptr = NULL;
    long id = strtol(cursor, &endptr, 10);
    if(id <= 0 || !endptr) {
        return;
    }
    while(*endptr == ' ' || *endptr == '\t') {
        endptr++;
    }
    if(*endptr == '\0') {
        return;
    }
    if(app->sd_file_count >= SD_MANAGER_MAX_FILES) {
        return;
    }

    SdFileEntry* entry = &app->sd_files[app->sd_file_count++];
    strncpy(entry->name, endptr, SD_MANAGER_FILE_NAME_MAX - 1);
    entry->name[SD_MANAGER_FILE_NAME_MAX - 1] = '\0';

    size_t len = strlen(entry->name);
    while(len > 0 && (entry->name[len - 1] == '\r' || entry->name[len - 1] == '\n' ||
                      entry->name[len - 1] == ' ')) {
        entry->name[--len] = '\0';
    }

    app->sd_list_header_seen = true;
}

static void simple_app_sd_feed(SimpleApp* app, char ch) {
    if(!app || !app->sd_listing_active) return;
    if(ch == '\r') return;

    if(ch == '>') {
        if(app->sd_list_length > 0) {
            app->sd_list_buffer[app->sd_list_length] = '\0';
            simple_app_process_sd_line(app, app->sd_list_buffer);
            app->sd_list_length = 0;
        }
        if(app->sd_file_count > 0 || app->sd_list_header_seen) {
            simple_app_finish_sd_listing(app);
        }
        return;
    }

    if(ch == '\n') {
        if(app->sd_list_length > 0) {
            app->sd_list_buffer[app->sd_list_length] = '\0';
            simple_app_process_sd_line(app, app->sd_list_buffer);
        } else if(app->sd_list_header_seen) {
            simple_app_finish_sd_listing(app);
        }
        app->sd_list_length = 0;
        return;
    }

    if(app->sd_list_length + 1 >= sizeof(app->sd_list_buffer)) {
        app->sd_list_length = 0;
        return;
    }

    app->sd_list_buffer[app->sd_list_length++] = ch;
}

static void simple_app_draw_sd_folder_popup(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas || !app->sd_folder_popup_active) return;

    const uint8_t bubble_x = 4;
    const uint8_t bubble_y = 4;
    const uint8_t bubble_w = DISPLAY_WIDTH - (bubble_x * 2);
    const uint8_t bubble_h = 56;
    const uint8_t radius = 9;

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_rbox(canvas, bubble_x, bubble_y, bubble_w, bubble_h, radius);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, bubble_x, bubble_y, bubble_w, bubble_h, radius);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, bubble_x + 8, bubble_y + 16, "Select folder");

    canvas_set_font(canvas, FontSecondary);
    uint8_t list_y = bubble_y + 28;

    if(sd_folder_option_count == 0) {
        canvas_draw_str(canvas, bubble_x + 8, list_y, "No folders configured");
        return;
    }

    size_t visible = sd_folder_option_count;
    if(visible > SD_MANAGER_POPUP_VISIBLE_LINES) {
        visible = SD_MANAGER_POPUP_VISIBLE_LINES;
    }

    if(app->sd_folder_index >= sd_folder_option_count) {
        app->sd_folder_index = sd_folder_option_count - 1;
    }
    if(app->sd_folder_offset + visible <= app->sd_folder_index) {
        app->sd_folder_offset = app->sd_folder_index - visible + 1;
    }
    if(app->sd_folder_offset >= sd_folder_option_count) {
        app->sd_folder_offset = 0;
    }

    for(size_t i = 0; i < visible; i++) {
        size_t idx = app->sd_folder_offset + i;
        if(idx >= sd_folder_option_count) break;
        const SdFolderOption* option = &sd_folder_options[idx];
        char full_line[64];
        snprintf(full_line, sizeof(full_line), "%s (%s)", option->label, option->path);

        char line[32];
        if(idx == app->sd_folder_index) {
            simple_app_sd_scroll_buffer(app, full_line, 23, line, sizeof(line));
        } else {
            strncpy(line, full_line, sizeof(line) - 1);
            line[sizeof(line) - 1] = '\0';
            simple_app_truncate_text(line, 23);
        }

        uint8_t line_y = (uint8_t)(list_y + i * HINT_LINE_HEIGHT);
        if(idx == app->sd_folder_index) {
            canvas_draw_str(canvas, bubble_x + 4, line_y, ">");
        }
        canvas_draw_str(canvas, bubble_x + 8, line_y, line);
    }

    if(sd_folder_option_count > SD_MANAGER_POPUP_VISIBLE_LINES) {
        bool show_up = (app->sd_folder_offset > 0);
        bool show_down = (app->sd_folder_offset + visible < sd_folder_option_count);
        if(show_up || show_down) {
            uint8_t arrow_x = (uint8_t)(bubble_x + bubble_w - 10);
            int16_t content_top = list_y;
            int16_t content_bottom =
                list_y + (int16_t)((visible > 0 ? (visible - 1) : 0) * HINT_LINE_HEIGHT);
            int16_t min_base = bubble_y + 12;
            int16_t max_base = bubble_y + bubble_h - 12;
            simple_app_draw_scroll_hints_clamped(
                canvas,
                arrow_x,
                content_top,
                content_bottom,
                show_up,
                show_down,
                min_base,
                max_base);
        }
    }
}

static void simple_app_draw_sd_file_popup(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas || !app->sd_file_popup_active) return;
    if(!app->sd_files) return;

    const uint8_t bubble_x = 4;
    const uint8_t bubble_y = 4;
    const uint8_t bubble_w = DISPLAY_WIDTH - (bubble_x * 2);
    const uint8_t bubble_h = 56;
    const uint8_t radius = 9;

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_rbox(canvas, bubble_x, bubble_y, bubble_w, bubble_h, radius);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, bubble_x, bubble_y, bubble_w, bubble_h, radius);

    canvas_set_font(canvas, FontPrimary);
    char title[48];
    snprintf(
        title,
        sizeof(title),
        "Files: %s",
        (app->sd_current_folder_label[0] != '\0') ? app->sd_current_folder_label : "Unknown");
    simple_app_truncate_text(title, 28);
    canvas_draw_str(canvas, bubble_x + 8, bubble_y + 16, title);

    canvas_set_font(canvas, FontSecondary);
    uint8_t list_y = bubble_y + 28;

    if(app->sd_file_count == 0) {
        canvas_draw_str(canvas, bubble_x + 8, list_y, "No files found");
        return;
    }

    size_t visible = app->sd_file_count;
    if(visible > SD_MANAGER_POPUP_VISIBLE_LINES) {
        visible = SD_MANAGER_POPUP_VISIBLE_LINES;
    }

    if(app->sd_file_popup_index >= app->sd_file_count) {
        app->sd_file_popup_index = (app->sd_file_count > 0) ? (app->sd_file_count - 1) : 0;
    }

    if(app->sd_file_count > visible &&
       app->sd_file_popup_index >= app->sd_file_popup_offset + visible) {
        app->sd_file_popup_offset = app->sd_file_popup_index - visible + 1;
    }

    size_t max_offset = (app->sd_file_count > visible) ? (app->sd_file_count - visible) : 0;
    if(app->sd_file_popup_offset > max_offset) {
        app->sd_file_popup_offset = max_offset;
    }

    for(size_t i = 0; i < visible; i++) {
        size_t idx = app->sd_file_popup_offset + i;
        if(idx >= app->sd_file_count) break;
        const SdFileEntry* entry = &app->sd_files[idx];
        char line[48];
        if(idx == app->sd_file_popup_index) {
            simple_app_sd_scroll_buffer(app, entry->name, 20, line, sizeof(line));
        } else {
            strncpy(line, entry->name, sizeof(line) - 1);
            line[sizeof(line) - 1] = '\0';
            simple_app_truncate_text(line, 20);
        }
        uint8_t line_y = (uint8_t)(list_y + i * HINT_LINE_HEIGHT);
        if(idx == app->sd_file_popup_index) {
            canvas_draw_str(canvas, bubble_x + 4, line_y, ">");
        }
        canvas_draw_str(canvas, bubble_x + 8, line_y, line);
    }

    if(app->sd_file_count > SD_MANAGER_POPUP_VISIBLE_LINES) {
        bool show_up = (app->sd_file_popup_offset > 0);
        bool show_down = (app->sd_file_popup_offset + visible < app->sd_file_count);
        if(show_up || show_down) {
            uint8_t arrow_x = (uint8_t)(bubble_x + bubble_w - 10);
            int16_t content_top = list_y;
            int16_t content_bottom =
                list_y + (int16_t)((visible > 0 ? (visible - 1) : 0) * HINT_LINE_HEIGHT);
            int16_t min_base = bubble_y + 12;
            int16_t max_base = bubble_y + bubble_h - 12;
            simple_app_draw_scroll_hints_clamped(
                canvas,
                arrow_x,
                content_top,
                content_bottom,
                show_up,
                show_down,
                min_base,
                max_base);
        }
    }
}

static void simple_app_draw_sd_delete_confirm(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas || !app->sd_delete_confirm_active) return;

    const uint8_t bubble_x = 4;
    const uint8_t bubble_y = 8;
    const uint8_t bubble_w = DISPLAY_WIDTH - (bubble_x * 2);
    const uint8_t bubble_h = 48;
    const uint8_t radius = 9;

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_rbox(canvas, bubble_x, bubble_y, bubble_w, bubble_h, radius);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, bubble_x, bubble_y, bubble_w, bubble_h, radius);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, bubble_x + 10, bubble_y + 14, "Delete file?");

    canvas_set_font(canvas, FontSecondary);
    char name_line[48];
    simple_app_sd_scroll_buffer(app, app->sd_delete_target_name, 18, name_line, sizeof(name_line));
    canvas_draw_str(canvas, bubble_x + 10, bubble_y + 28, name_line);

    const char* options = app->sd_delete_confirm_yes ? "No        > Yes" : "> No        Yes";
    canvas_draw_str(canvas, bubble_x + 18, bubble_y + 44, options);
}

static void simple_app_draw_setup_sd_manager(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 10, 14, "SD Manager");

    canvas_set_font(canvas, FontSecondary);
    char line[64];
    if(app->sd_current_folder_label[0] != '\0') {
        snprintf(line, sizeof(line), "Folder: %s", app->sd_current_folder_label);
    } else {
        snprintf(line, sizeof(line), "Folder: <select>");
    }
    simple_app_truncate_text(line, 30);
    canvas_draw_str(canvas, 8, 30, line);

    if(app->sd_current_folder_path[0] != '\0') {
        snprintf(line, sizeof(line), "Path: %.54s", app->sd_current_folder_path);
    } else {
        snprintf(line, sizeof(line), "Path: <not set>");
    }
    simple_app_truncate_text(line, 30);
    canvas_draw_str(canvas, 8, 42, line);

    if(app->sd_listing_active) {
        canvas_draw_str(canvas, 8, 54, "Listing files...");
    } else {
        snprintf(
            line,
            sizeof(line),
            "Cached files: %u",
            (unsigned)((app->sd_file_count > 0) ? app->sd_file_count : 0));
        simple_app_truncate_text(line, 28);
        canvas_draw_str(canvas, 8, 54, line);
    }

    canvas_draw_str(canvas, 8, 62, "OK=folders  Right=refresh");
}

static void simple_app_handle_sd_folder_popup_event(SimpleApp* app, const InputEvent* event) {
    if(!app || !event || !app->sd_folder_popup_active) return;

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return;

    InputKey key = event->key;
    if(event->type == InputTypeShort && key == InputKeyBack) {
        app->sd_folder_popup_active = false;
        simple_app_reset_sd_scroll(app);
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(sd_folder_option_count == 0) {
        if(event->type == InputTypeShort && key == InputKeyOk) {
            app->sd_folder_popup_active = false;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
        return;
    }

    size_t visible = sd_folder_option_count;
    if(visible > SD_MANAGER_POPUP_VISIBLE_LINES) {
        visible = SD_MANAGER_POPUP_VISIBLE_LINES;
    }

    if(key == InputKeyUp) {
        if(app->sd_folder_index > 0) {
            app->sd_folder_index--;
            simple_app_reset_sd_scroll(app);
            if(app->sd_folder_index < app->sd_folder_offset) {
                app->sd_folder_offset = app->sd_folder_index;
            }
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
    } else if(key == InputKeyDown) {
        if(app->sd_folder_index + 1 < sd_folder_option_count) {
            app->sd_folder_index++;
            simple_app_reset_sd_scroll(app);
            if(sd_folder_option_count > visible &&
               app->sd_folder_index >= app->sd_folder_offset + visible) {
                app->sd_folder_offset = app->sd_folder_index - visible + 1;
            }
            size_t max_offset =
                (sd_folder_option_count > visible) ? (sd_folder_option_count - visible) : 0;
            if(app->sd_folder_offset > max_offset) {
                app->sd_folder_offset = max_offset;
            }
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
    } else if(event->type == InputTypeShort && key == InputKeyOk) {
        if(app->sd_folder_index < sd_folder_option_count) {
            const SdFolderOption* folder = &sd_folder_options[app->sd_folder_index];
            simple_app_request_sd_listing(app, folder);
        }
    }
}

static void simple_app_handle_sd_file_popup_event(SimpleApp* app, const InputEvent* event) {
    if(!app || !event || !app->sd_file_popup_active) return;
    if(!app->sd_files) return;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return;

    InputKey key = event->key;
    if(event->type == InputTypeShort && key == InputKeyBack) {
        app->sd_file_popup_active = false;
        simple_app_reset_sd_scroll(app);
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(app->sd_file_count == 0) {
        if(event->type == InputTypeShort && key == InputKeyOk) {
            app->sd_file_popup_active = false;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
        return;
    }

    size_t visible = app->sd_file_count;
    if(visible > SD_MANAGER_POPUP_VISIBLE_LINES) {
        visible = SD_MANAGER_POPUP_VISIBLE_LINES;
    }

    if(key == InputKeyUp) {
        if(app->sd_file_popup_index > 0) {
            app->sd_file_popup_index--;
            simple_app_reset_sd_scroll(app);
            if(app->sd_file_popup_index < app->sd_file_popup_offset) {
                app->sd_file_popup_offset = app->sd_file_popup_index;
            }
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
    } else if(key == InputKeyDown) {
        if(app->sd_file_popup_index + 1 < app->sd_file_count) {
            app->sd_file_popup_index++;
            simple_app_reset_sd_scroll(app);
            if(app->sd_file_count > visible &&
               app->sd_file_popup_index >= app->sd_file_popup_offset + visible) {
                app->sd_file_popup_offset = app->sd_file_popup_index - visible + 1;
            }
            size_t max_offset = (app->sd_file_count > visible) ? (app->sd_file_count - visible) :
                                                                 0;
            if(app->sd_file_popup_offset > max_offset) {
                app->sd_file_popup_offset = max_offset;
            }
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
    } else if(event->type == InputTypeShort && key == InputKeyOk) {
        if(app->sd_file_popup_index < app->sd_file_count) {
            const SdFileEntry* entry = &app->sd_files[app->sd_file_popup_index];
            simple_app_reset_sd_scroll(app);
            strncpy(app->sd_delete_target_name, entry->name, SD_MANAGER_FILE_NAME_MAX - 1);
            app->sd_delete_target_name[SD_MANAGER_FILE_NAME_MAX - 1] = '\0';

            if(app->sd_current_folder_path[0] != '\0') {
                size_t max_total = sizeof(app->sd_delete_target_path) - 1;
                size_t folder_len =
                    simple_app_bounded_strlen(app->sd_current_folder_path, max_total);
                size_t remaining = (folder_len < max_total) ? (max_total - folder_len - 1) :
                                                              0; // minus slash
                size_t name_len = simple_app_bounded_strlen(entry->name, remaining);
                snprintf(
                    app->sd_delete_target_path,
                    sizeof(app->sd_delete_target_path),
                    "%.*s/%.*s",
                    (int)folder_len,
                    app->sd_current_folder_path,
                    (int)name_len,
                    entry->name);
            } else {
                strncpy(
                    app->sd_delete_target_path,
                    entry->name,
                    sizeof(app->sd_delete_target_path) - 1);
                app->sd_delete_target_path[sizeof(app->sd_delete_target_path) - 1] = '\0';
            }

            app->sd_delete_confirm_yes = false;
            app->sd_delete_confirm_active = true;
            app->sd_file_popup_active = false;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
    }
}

static void simple_app_handle_sd_delete_confirm_event(SimpleApp* app, const InputEvent* event) {
    if(!app || !event || !app->sd_delete_confirm_active) return;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return;

    InputKey key = event->key;
    if(event->type == InputTypeShort && key == InputKeyBack) {
        app->sd_delete_confirm_active = false;
        app->sd_delete_confirm_yes = false;
        app->sd_file_popup_active = true;
        simple_app_reset_sd_scroll(app);
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(key == InputKeyLeft || key == InputKeyRight) {
        app->sd_delete_confirm_yes = !app->sd_delete_confirm_yes;
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(event->type == InputTypeShort && key == InputKeyOk) {
        if(app->sd_delete_confirm_yes) {
            char command[220];
            snprintf(command, sizeof(command), "file_delete \"%s\"", app->sd_delete_target_path);
            simple_app_send_command(app, command, false);
            simple_app_show_status_message(app, "Deleting...", 800, false);
            app->sd_refresh_pending = true;
            app->sd_refresh_tick = furi_get_tick() + furi_ms_to_ticks(400);
            simple_app_reset_sd_listing(app);
        } else {
            app->sd_file_popup_active = true;
        }
        app->sd_delete_confirm_active = false;
        app->sd_delete_confirm_yes = false;
        simple_app_reset_sd_scroll(app);
        if(app->viewport) {
            view_port_update(app->viewport);
        }
    }
}

static void simple_app_handle_setup_sd_manager_input(SimpleApp* app, InputKey key) {
    if(!app) return;

    if(key == InputKeyBack) {
        app->sd_folder_popup_active = false;
        app->sd_file_popup_active = false;
        app->sd_delete_confirm_active = false;
        app->sd_refresh_pending = false;
        simple_app_reset_sd_listing(app);
        simple_app_reset_sd_scroll(app);
        app->screen = ScreenMenu;
        app->menu_state = MenuStateItems;
        app->section_index = MENU_SECTION_SETUP;
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(key == InputKeyOk) {
        app->sd_folder_popup_active = true;
        app->sd_file_popup_active = false;
        app->sd_delete_confirm_active = false;
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(key == InputKeyDown) {
        if(app->sd_file_count > 0 && !app->sd_listing_active) {
            app->sd_file_popup_active = true;
            app->sd_file_popup_index = 0;
            app->sd_file_popup_offset = 0;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
        return;
    }

    if(key == InputKeyRight) {
        if(!app->sd_listing_active && sd_folder_option_count > 0 &&
           app->sd_folder_index < sd_folder_option_count) {
            simple_app_request_sd_listing(app, &sd_folder_options[app->sd_folder_index]);
        }
        return;
    }
}

static void simple_app_draw_setup_scanner(SimpleApp* app, Canvas* canvas) {
    if(!app) return;

    static const char* option_labels[] = {
        "SSID", "BSSID", "Channel", "Security", "Power", "Band", "Vendor"};

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 4, 12, "Scanner Filters");

    canvas_set_font(canvas, FontSecondary);
    size_t option_count = ScannerOptionCount;
    if(app->scanner_view_offset >= option_count) {
        app->scanner_view_offset =
            (option_count > 0 && option_count > SCANNER_FILTER_VISIBLE_COUNT) ?
                option_count - SCANNER_FILTER_VISIBLE_COUNT :
                0;
    }

    uint8_t y = 26;
    for(size_t i = 0; i < SCANNER_FILTER_VISIBLE_COUNT; i++) {
        size_t option_index = app->scanner_view_offset + i;
        if(option_index >= option_count) break;

        char line[48];
        memset(line, 0, sizeof(line));

        if(option_index == ScannerOptionMinPower) {
            const char* fmt = (app->scanner_adjusting_power &&
                               app->scanner_setup_index == ScannerOptionMinPower) ?
                                  "Min power*: %d dBm" :
                                  "Min power: %d dBm";
            snprintf(line, sizeof(line), fmt, (int)app->scanner_min_power);
        } else {
            bool enabled = false;
            switch(option_index) {
            case ScannerOptionShowSSID:
                enabled = app->scanner_show_ssid;
                break;
            case ScannerOptionShowBSSID:
                enabled = app->scanner_show_bssid;
                break;
            case ScannerOptionShowChannel:
                enabled = app->scanner_show_channel;
                break;
            case ScannerOptionShowSecurity:
                enabled = app->scanner_show_security;
                break;
            case ScannerOptionShowPower:
                enabled = app->scanner_show_power;
                break;
            case ScannerOptionShowBand:
                enabled = app->scanner_show_band;
                break;
            case ScannerOptionShowVendor:
                enabled = app->scanner_show_vendor;
                break;
            default:
                break;
            }
            snprintf(
                line, sizeof(line), "[%c] %s", enabled ? 'x' : ' ', option_labels[option_index]);
        }

        simple_app_truncate_text(line, 20);

        if(app->scanner_setup_index == option_index) {
            canvas_draw_str(canvas, 2, y, ">");
        }
        canvas_draw_str(canvas, 12, y, line);
        y += 10;
    }

    if(option_count > SCANNER_FILTER_VISIBLE_COUNT) {
        size_t max_offset = option_count - SCANNER_FILTER_VISIBLE_COUNT;
        if(app->scanner_view_offset > max_offset) {
            app->scanner_view_offset = max_offset;
        }
        bool show_up = (app->scanner_view_offset > 0);
        bool show_down = (app->scanner_view_offset < max_offset);
        if(show_up || show_down) {
            uint8_t arrow_x = DISPLAY_WIDTH - 6;
            int16_t content_top = 26;
            int16_t content_bottom = 26 + (int16_t)((SCANNER_FILTER_VISIBLE_COUNT > 0 ?
                                                         (SCANNER_FILTER_VISIBLE_COUNT - 1) :
                                                         0) *
                                                    10);
            simple_app_draw_scroll_hints(
                canvas, arrow_x, content_top, content_bottom, show_up, show_down);
        }
    }

    const char* footer = "OK toggle, Back exit";
    if(app->scanner_setup_index == ScannerOptionMinPower) {
        footer = app->scanner_adjusting_power ? "Up/Down adjust, OK" : "OK edit, Back exit";
    }
    canvas_draw_str(canvas, 2, 62, footer);
}

static void simple_app_draw_setup_scanner_timing(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 6, 14, "Scanner Timing");

    canvas_set_font(canvas, FontSecondary);
    uint8_t y = 32;
    char line[48];

    snprintf(
        line, sizeof(line), "Min Channel Scan: %ums", (unsigned)app->scanner_min_channel_time);
    simple_app_truncate_text(line, 26);
    if(app->scanner_timing_index == 0) {
        canvas_draw_str(canvas, 2, y, ">");
    }
    canvas_draw_str(canvas, 12, y, line);

    y += 14;
    snprintf(
        line, sizeof(line), "Max Channel Scan: %ums", (unsigned)app->scanner_max_channel_time);
    simple_app_truncate_text(line, 26);
    if(app->scanner_timing_index == 1) {
        canvas_draw_str(canvas, 2, y, ">");
    }
    canvas_draw_str(canvas, 12, y, line);

    canvas_draw_str(canvas, 2, 62, "Left/Right adjust, Back exit");
}

static void simple_app_draw(Canvas* canvas, void* context) {
    SimpleApp* app = context;
    canvas_clear(canvas);
    bool status_active = simple_app_status_message_is_active(app);
    if(status_active && app->status_message_fullscreen) {
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontPrimary);
        const char* message = app->status_message;
        size_t line_count = 1;
        for(const char* ptr = message; *ptr; ptr++) {
            if(*ptr == '\n') {
                line_count++;
            }
        }
        const uint8_t line_height = 16;
        int16_t first_line_y = 32;
        if(line_count > 1) {
            first_line_y -= (int16_t)((line_count - 1) * line_height) / 2;
        }
        const char* line_ptr = message;
        char line_buffer[64];
        for(size_t line_index = 0; line_index < line_count; line_index++) {
            const char* next_break = strchr(line_ptr, '\n');
            size_t line_length = next_break ? (size_t)(next_break - line_ptr) : strlen(line_ptr);
            if(line_length >= sizeof(line_buffer)) {
                line_length = sizeof(line_buffer) - 1;
            }
            memcpy(line_buffer, line_ptr, line_length);
            line_buffer[line_length] = '\0';
            canvas_draw_str_aligned(
                canvas,
                DISPLAY_WIDTH / 2,
                first_line_y + (int16_t)(line_index * line_height),
                AlignCenter,
                AlignCenter,
                line_buffer);
            if(!next_break) break;
            line_ptr = next_break + 1;
        }
        return;
    }

    if(app->screen == ScreenMenu && app->menu_state == MenuStateSections) {
        simple_app_draw_sync_status(app, canvas);
    }

    switch(app->screen) {
    case ScreenMenu:
        simple_app_draw_menu(app, canvas);
        break;
    case ScreenSerial:
        simple_app_draw_serial(app, canvas);
        break;
    case ScreenResults:
        simple_app_draw_results(app, canvas);
        break;
    case ScreenSetupScanner:
        simple_app_draw_setup_scanner(app, canvas);
        break;
    case ScreenSetupScannerTiming:
        simple_app_draw_setup_scanner_timing(app, canvas);
        break;
    case ScreenSetupLed:
        simple_app_draw_setup_led(app, canvas);
        break;
    case ScreenSetupBoot:
        simple_app_draw_setup_boot(app, canvas);
        break;
    case ScreenSetupSdManager:
        simple_app_draw_setup_sd_manager(app, canvas);
        break;
    case ScreenHelpQr:
        simple_app_draw_help_qr(app, canvas);
        break;
    case ScreenSetupKarma:
        simple_app_draw_setup_karma(app, canvas);
        break;
    case ScreenConsole:
        simple_app_draw_console(app, canvas);
        break;
    case ScreenGps:
        simple_app_draw_gps(app, canvas);
        break;
    case ScreenPackageMonitor:
        simple_app_draw_package_monitor(app, canvas);
        break;
    case ScreenChannelView:
        simple_app_draw_channel_view(app, canvas);
        break;
    case ScreenConfirmBlackout:
        simple_app_draw_confirm_blackout(app, canvas);
        break;
    case ScreenConfirmSnifferDos:
        simple_app_draw_confirm_sniffer_dos(app, canvas);
        break;
    case ScreenConfirmExit:
        simple_app_draw_confirm_exit(app, canvas);
        break;
    case ScreenKarmaMenu:
        simple_app_draw_karma_menu(app, canvas);
        break;
    case ScreenEvilTwinMenu:
        simple_app_draw_evil_twin_menu(app, canvas);
        break;
    case ScreenEvilTwinPassList:
        simple_app_draw_evil_twin_pass_list(app, canvas);
        break;
    case ScreenEvilTwinPassQr:
        simple_app_draw_evil_twin_pass_qr(app, canvas);
        break;
    case ScreenPortalMenu:
        simple_app_draw_portal_menu(app, canvas);
        break;
    case ScreenBtLocatorList:
        simple_app_draw_bt_locator_list(app, canvas);
        break;
    case ScreenPasswords:
        simple_app_draw_passwords(app, canvas);
        break;
    case ScreenPasswordsSelect:
        simple_app_draw_passwords_select(app, canvas);
        break;
    default:
        simple_app_draw_results(app, canvas);
        break;
    }

    if(app->sd_delete_confirm_active) {
        simple_app_draw_sd_delete_confirm(app, canvas);
    } else if(app->sd_file_popup_active) {
        simple_app_draw_sd_file_popup(app, canvas);
    } else if(app->sd_folder_popup_active) {
        simple_app_draw_sd_folder_popup(app, canvas);
    } else if(app->portal_ssid_popup_active) {
        simple_app_draw_portal_ssid_popup(app, canvas);
    } else if(app->karma_probe_popup_active) {
        simple_app_draw_karma_probe_popup(app, canvas);
    } else if(app->karma_html_popup_active) {
        simple_app_draw_karma_html_popup(app, canvas);
    } else if(app->evil_twin_popup_active) {
        simple_app_draw_evil_twin_popup(app, canvas);
    }

    simple_app_draw_ram_overlay(app, canvas);
}

static void simple_app_handle_menu_input(SimpleApp* app, InputKey key) {
    if(key == InputKeyUp) {
        if(app->menu_state == MenuStateSections) {
            size_t section_count = menu_section_count;
            if(section_count == 0) return;
            if(app->section_index > 0) {
                app->section_index--;
            } else {
                app->section_index = section_count - 1;
            }
            simple_app_adjust_section_offset(app);
            app->exit_confirm_active = false;
            view_port_update(app->viewport);
        } else {
            const MenuSection* section = &menu_sections[app->section_index];
            size_t entry_count = section->entry_count;
            if(entry_count == 0) return;
            if(app->item_index > 0) {
                app->item_index--;
                if(app->item_index < app->item_offset) {
                    app->item_offset = app->item_index;
                }
            } else {
                app->item_index = entry_count - 1;
                size_t visible_count = simple_app_menu_visible_count(app, app->section_index);
                if(visible_count == 0) visible_count = 1;
                if(app->item_index >= visible_count) {
                    app->item_offset = app->item_index - visible_count + 1;
                } else {
                    app->item_offset = 0;
                }
            }
            if(app->section_index == MENU_SECTION_ATTACKS) {
                app->last_attack_index = app->item_index;
            }
            view_port_update(app->viewport);
        }
    } else if(key == InputKeyDown) {
        if(app->menu_state == MenuStateSections) {
            size_t section_count = menu_section_count;
            if(section_count == 0) return;
            if(app->section_index + 1 < section_count) {
                app->section_index++;
            } else {
                app->section_index = 0;
            }
            simple_app_adjust_section_offset(app);
            app->exit_confirm_active = false;
            view_port_update(app->viewport);
        } else {
            const MenuSection* section = &menu_sections[app->section_index];
            size_t entry_count = section->entry_count;
            if(entry_count == 0) return;
            if(app->item_index + 1 < entry_count) {
                app->item_index++;
                size_t visible_count = simple_app_menu_visible_count(app, app->section_index);
                if(visible_count == 0) visible_count = 1;
                if(app->item_index >= app->item_offset + visible_count) {
                    app->item_offset = app->item_index - visible_count + 1;
                }
            } else {
                app->item_index = 0;
                app->item_offset = 0;
            }
            if(app->section_index == MENU_SECTION_ATTACKS) {
                app->last_attack_index = app->item_index;
            }
            view_port_update(app->viewport);
        }
    } else if(key == InputKeyOk) {
        if(app->menu_state == MenuStateSections) {
            if(app->section_index == MENU_SECTION_SCANNER) {
                simple_app_send_command(app, SCANNER_SCAN_COMMAND, true);
                view_port_update(app->viewport);
                return;
            }
            app->exit_confirm_active = false;
            app->menu_state = MenuStateItems;
            if(app->section_index == MENU_SECTION_ATTACKS) {
                simple_app_focus_attacks_menu(app);
                app->last_attack_index = app->item_index;
                view_port_update(app->viewport);
                return;
            }
            app->item_index = 0;
            app->item_offset = 0;
            size_t visible_count = simple_app_menu_visible_count(app, app->section_index);
            if(visible_count == 0) visible_count = 1;
            if(menu_sections[app->section_index].entry_count > visible_count) {
                size_t max_offset = menu_sections[app->section_index].entry_count - visible_count;
                if(app->item_offset > max_offset) {
                    app->item_offset = max_offset;
                }
            }
            if(app->section_index == MENU_SECTION_SETUP) {
                simple_app_request_led_status(app);
                simple_app_request_boot_status(app);
                simple_app_request_vendor_status(app);
                simple_app_request_scanner_timing(app);
            }
            view_port_update(app->viewport);
            return;
        }

        const MenuSection* section = &menu_sections[app->section_index];
        if(section->entry_count == 0) return;
        if(app->item_index >= section->entry_count) {
            app->item_index = section->entry_count - 1;
        }
        if(app->section_index == MENU_SECTION_ATTACKS) {
            app->last_attack_index = app->item_index;
        }

        const MenuEntry* entry = &section->entries[app->item_index];
        if(entry->action == MenuActionResults) {
            simple_app_request_scan_results(app, entry->command);
        } else if(entry->action == MenuActionCommandWithTargets) {
            simple_app_send_command_with_targets(app, entry->command);
        } else if(entry->action == MenuActionCommand) {
            if(entry->command && entry->command[0] != '\0') {
                if(strcmp(entry->command, "start_sniffer") == 0) {
                    simple_app_send_start_sniffer(app);
                } else if(strcmp(entry->command, "start_sniffer_noscan") == 0) {
                    simple_app_send_resume_sniffer(app);
                } else if(strcmp(entry->command, "show_sniffer_results") == 0) {
                    if(!app->sniffer_has_data) {
                        simple_app_show_status_message(app, "No sniffer data", 1200, true);
                        return;
                    }
                    simple_app_send_command(app, entry->command, true);
                } else {
                    simple_app_send_command(app, entry->command, true);
                }
            }
        } else if(entry->action == MenuActionOpenKarmaMenu) {
            app->screen = ScreenKarmaMenu;
            app->karma_menu_index = 0;
            app->karma_menu_offset = 0;
            view_port_update(app->viewport);
        } else if(entry->action == MenuActionOpenEvilTwinMenu) {
            app->screen = ScreenEvilTwinMenu;
            app->evil_twin_menu_index = 0;
            app->evil_twin_menu_offset = 0;
            simple_app_evil_twin_sync_offset(app);
            view_port_update(app->viewport);
        } else if(entry->action == MenuActionOpenPortalMenu) {
            app->screen = ScreenPortalMenu;
            app->portal_menu_index = 0;
            app->portal_menu_offset = 0;
            simple_app_portal_sync_offset(app);
            view_port_update(app->viewport);
        } else if(entry->action == MenuActionToggleBacklight) {
            simple_app_toggle_backlight(app);
        } else if(entry->action == MenuActionToggleOtgPower) {
            simple_app_toggle_otg_power(app);
        } else if(entry->action == MenuActionToggleShowRam) {
            simple_app_toggle_show_ram(app);
        } else if(entry->action == MenuActionToggleDebugMark) {
            simple_app_toggle_debug_mark(app);
        } else if(entry->action == MenuActionOpenLedSetup) {
            app->screen = ScreenSetupLed;
            app->led_setup_index = 0;
            simple_app_request_led_status(app);
        } else if(entry->action == MenuActionOpenBootSetup) {
            app->screen = ScreenSetupBoot;
            app->boot_setup_index = 0;
            app->boot_setup_long = (entry->label == menu_label_boot_long);
            simple_app_request_boot_status(app);
        } else if(entry->action == MenuActionOpenSdManager) {
            simple_app_open_sd_manager(app);
            return;
        } else if(entry->action == MenuActionOpenHelp) {
            simple_app_prepare_help_qr(app);
            app->screen = ScreenHelpQr;
        } else if(entry->action == MenuActionOpenDownload) {
            simple_app_send_download(app);
        } else if(entry->action == MenuActionOpenScannerTiming) {
            app->screen = ScreenSetupScannerTiming;
            app->scanner_timing_index = 0;
            simple_app_request_scanner_timing(app);
        } else if(entry->action == MenuActionOpenScannerSetup) {
            app->screen = ScreenSetupScanner;
            app->scanner_setup_index = 0;
            app->scanner_adjusting_power = false;
            app->scanner_view_offset = 0;
            simple_app_request_vendor_status(app);
        } else if(entry->action == MenuActionOpenPackageMonitor) {
            simple_app_package_monitor_enter(app);
            return;
        } else if(entry->action == MenuActionOpenChannelView) {
            simple_app_channel_view_enter(app);
            return;
        } else if(entry->action == MenuActionOpenConsole) {
            simple_app_console_enter(app);
        } else if(entry->action == MenuActionOpenGps) {
            if(!simple_app_gps_state_ensure(app)) {
                simple_app_show_status_message(app, "OOM: GPS", 1500, true);
                return;
            }
            app->screen = ScreenGps;
            app->gps_view_active = true;
            app->gps_console_mode = false;
            simple_app_reset_gps_status(app);
            simple_app_send_command(app, "start_gps_raw", false);
        } else if(entry->action == MenuActionOpenBtLocator) {
            simple_app_bt_locator_begin_scan(app);
        } else if(entry->action == MenuActionConfirmBlackout) {
            app->confirm_blackout_yes = false;
            app->screen = ScreenConfirmBlackout;
        } else if(entry->action == MenuActionConfirmSnifferDos) {
            app->confirm_sniffer_dos_yes = false;
            app->screen = ScreenConfirmSnifferDos;
        }

        view_port_update(app->viewport);
    } else if(key == InputKeyBack) {
        simple_app_send_stop_if_needed(app);
        if(app->menu_state == MenuStateItems) {
            if(app->section_index == MENU_SECTION_ATTACKS) {
                simple_app_send_command_quiet(app, "unselect_networks");
            }
            app->menu_state = MenuStateSections;
            simple_app_adjust_section_offset(app);
            view_port_update(app->viewport);
        } else {
            // Brief haptic to highlight exit confirmation
            furi_hal_vibro_on(true);
            furi_delay_ms(40);
            furi_hal_vibro_on(false);
            app->confirm_exit_yes = false;
            app->screen = ScreenConfirmExit;
            view_port_update(app->viewport);
        }
    }
}

static void simple_app_handle_evil_twin_menu_input(SimpleApp* app, InputKey key) {
    if(!app) return;

    if(key == InputKeyBack || key == InputKeyLeft) {
        if(app->evil_twin_listing_active) {
            simple_app_send_stop_if_needed(app);
            simple_app_reset_evil_twin_listing(app);
            simple_app_clear_status_message(app);
        }
        simple_app_close_evil_twin_popup(app);
        app->evil_twin_menu_offset = 0;
        simple_app_focus_attacks_menu(app);
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(key == InputKeyUp) {
        if(app->evil_twin_menu_index > 0) {
            app->evil_twin_menu_index--;
        } else {
            app->evil_twin_menu_index = EVIL_TWIN_MENU_OPTION_COUNT - 1;
        }
        simple_app_evil_twin_sync_offset(app);
        if(app->viewport) {
            view_port_update(app->viewport);
        }
    } else if(key == InputKeyDown) {
        if(app->evil_twin_menu_index + 1 < EVIL_TWIN_MENU_OPTION_COUNT) {
            app->evil_twin_menu_index++;
        } else {
            app->evil_twin_menu_index = 0;
        }
        simple_app_evil_twin_sync_offset(app);
        if(app->viewport) {
            view_port_update(app->viewport);
        }
    } else if(key == InputKeyOk) {
        if(app->evil_twin_menu_index == 0) {
            if(app->scan_selected_count == 0) {
                simple_app_show_status_message(app, "Select in Scanner", 1200, true);
            }
            return;
        } else if(app->evil_twin_menu_index == 1) {
            if(app->evil_twin_listing_active) {
                simple_app_show_status_message(app, "Listing already\nin progress", 1200, true);
                return;
            }
            simple_app_request_evil_twin_html_list(app);
        } else if(app->evil_twin_menu_index == 2) {
            simple_app_start_evil_portal(app);
        } else if(app->evil_twin_menu_index == 3) {
            simple_app_request_evil_twin_pass_list(app);
        } else {
            app->passwords_select_return_screen = ScreenEvilTwinMenu;
            app->passwords_select_index = 1;
            app->screen = ScreenPasswordsSelect;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
    }
}

static void simple_app_request_evil_twin_html_list(SimpleApp* app) {
    if(!app) return;
    simple_app_close_evil_twin_popup(app);
    simple_app_reset_evil_twin_listing(app);
    if(!simple_app_evil_twin_alloc_html_entries(app)) {
        simple_app_show_status_message(app, "OOM: HTML list", 2000, true);
        return;
    }
    app->evil_twin_listing_active = true;
    simple_app_show_status_message(app, "Listing HTML...", 0, false);
    simple_app_send_command(app, "list_sd", true);
}

static void simple_app_start_evil_portal(SimpleApp* app) {
    if(!app) return;
    if(app->evil_twin_listing_active) {
        simple_app_show_status_message(app, "Wait for list\ncompletion", 1200, true);
        return;
    }
    if(app->evil_twin_selected_html_id == 0) {
        simple_app_show_status_message(app, "Select HTML file\nbefore starting", 1500, true);
        return;
    }
    char select_command[48];
    snprintf(
        select_command,
        sizeof(select_command),
        "select_html %u",
        (unsigned)app->evil_twin_selected_html_id);
    simple_app_send_command(app, select_command, false);
    app->last_command_sent = false;
    simple_app_send_command_with_targets(app, "start_evil_twin");
    if(app->viewport) {
        view_port_update(app->viewport);
    }
}

static void simple_app_prepare_help_qr(SimpleApp* app) {
    if(!app) return;
    if(app->help_qr_ready) return;

    const char* url = "https://github.com/C5Lab/projectZero/wiki";
    if(!simple_app_alloc_help_qr_buffers(app)) {
        app->help_qr_valid = false;
        strncpy(app->help_qr_error, "QR OOM", sizeof(app->help_qr_error) - 1);
        app->help_qr_error[sizeof(app->help_qr_error) - 1] = '\0';
        app->help_qr_ready = true;
        return;
    }
    bool ok = qrcodegen_encodeText(
        url,
        app->help_qr_temp,
        app->help_qr,
        qrcodegen_Ecc_MEDIUM,
        1,
        EVIL_TWIN_QR_MAX_VERSION,
        -1,
        true);

    app->help_qr_valid = ok;
    if(!ok) {
        strncpy(app->help_qr_error, "QR too long", sizeof(app->help_qr_error) - 1);
        app->help_qr_error[sizeof(app->help_qr_error) - 1] = '\0';
    } else {
        app->help_qr_error[0] = '\0';
    }
    app->help_qr_ready = true;
}

static void simple_app_draw_help_qr(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;
    if(!app->help_qr_ready) {
        simple_app_prepare_help_qr(app);
    }

    const uint8_t qr_x = 0;
    const uint8_t qr_y = 0;
    const uint8_t qr_w = 64;
    const uint8_t qr_h = 64;

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, qr_x, qr_y, qr_w, qr_h);
    canvas_set_color(canvas, ColorBlack);

    bool ok = app->help_qr_valid;
    if(ok) {
        int size = qrcodegen_getSize(app->help_qr);
        int scale_x = qr_w / size;
        int scale_y = qr_h / size;
        int scale = (scale_x < scale_y) ? scale_x : scale_y;
        if(scale >= 1) {
            int qr_size = size * scale;
            int offset_x = qr_x + (qr_w - qr_size) / 2;
            int offset_y = qr_y + (qr_h - qr_size) / 2;
            for(int y = 0; y < size; y++) {
                for(int x = 0; x < size; x++) {
                    if(qrcodegen_getModule(app->help_qr, x, y)) {
                        canvas_draw_box(
                            canvas, offset_x + x * scale, offset_y + y * scale, scale, scale);
                    }
                }
            }
        } else {
            ok = false;
        }
    }

    canvas_set_font(canvas, FontSecondary);
    if(!ok) {
        canvas_draw_str_aligned(canvas, qr_w / 2, 32, AlignCenter, AlignCenter, "QR error");
        if(app->help_qr_error[0] != '\0') {
            canvas_draw_str_aligned(
                canvas, qr_w / 2, 44, AlignCenter, AlignCenter, app->help_qr_error);
        }
    }

    canvas_draw_str(canvas, 68, 20, "Scan to");
    canvas_draw_str(canvas, 68, 32, "open wiki");
    canvas_draw_str(canvas, 68, 62, "Back");
}

static void simple_app_handle_help_qr_input(SimpleApp* app, InputKey key) {
    if(!app) return;
    if(key == InputKeyBack || key == InputKeyLeft || key == InputKeyOk) {
        simple_app_free_help_qr_buffers(app);
        app->screen = ScreenMenu;
        if(app->viewport) {
            view_port_update(app->viewport);
        }
    }
}

static void simple_app_close_evil_twin_popup(SimpleApp* app) {
    if(!app) return;
    app->evil_twin_popup_active = false;
}

static void simple_app_reset_evil_twin_listing(SimpleApp* app) {
    if(!app) return;
    simple_app_evil_twin_free_html_entries(app);
    app->evil_twin_listing_active = false;
    app->evil_twin_list_header_seen = false;
    app->evil_twin_list_length = 0;
    app->evil_twin_html_count = 0;
    app->evil_twin_html_popup_index = 0;
    app->evil_twin_html_popup_offset = 0;
}

static void simple_app_finish_evil_twin_listing(SimpleApp* app) {
    if(!app || !app->evil_twin_listing_active) return;
    app->evil_twin_listing_active = false;
    app->evil_twin_list_length = 0;
    app->evil_twin_list_header_seen = false;
    app->evil_twin_popup_active = false;
    app->last_command_sent = false;
    app->screen = ScreenEvilTwinMenu;
    simple_app_clear_status_message(app);

    if(app->evil_twin_html_count == 0) {
        simple_app_show_status_message(app, "No HTML files\nfound on SD", 1500, true);
        return;
    }

    size_t target_index = 0;
    if(app->evil_twin_selected_html_id != 0) {
        for(size_t i = 0; i < app->evil_twin_html_count; i++) {
            if(app->evil_twin_html_entries[i].id == app->evil_twin_selected_html_id) {
                target_index = i;
                break;
            }
        }
        if(target_index >= app->evil_twin_html_count) {
            target_index = 0;
        }
    }

    app->evil_twin_html_popup_index = target_index;
    size_t visible = (app->evil_twin_html_count > EVIL_TWIN_POPUP_VISIBLE_LINES) ?
                         EVIL_TWIN_POPUP_VISIBLE_LINES :
                         app->evil_twin_html_count;
    if(visible == 0) visible = 1;
    if(app->evil_twin_html_count <= visible || app->evil_twin_html_popup_index < visible) {
        app->evil_twin_html_popup_offset = 0;
    } else {
        app->evil_twin_html_popup_offset = app->evil_twin_html_popup_index - visible + 1;
    }
    size_t max_offset =
        (app->evil_twin_html_count > visible) ? (app->evil_twin_html_count - visible) : 0;
    if(app->evil_twin_html_popup_offset > max_offset) {
        app->evil_twin_html_popup_offset = max_offset;
    }

    app->evil_twin_popup_active = true;
    if(app->viewport) {
        view_port_update(app->viewport);
    }
}

static void simple_app_process_evil_twin_line(SimpleApp* app, const char* line) {
    if(!app || !line || !app->evil_twin_listing_active) return;
    if(!app->evil_twin_html_entries) return;

    const char* cursor = line;
    while(*cursor == ' ' || *cursor == '\t') {
        cursor++;
    }
    if(*cursor == '\0') {
        if(app->evil_twin_html_count > 0) {
            simple_app_finish_evil_twin_listing(app);
        }
        return;
    }

    if(strncmp(cursor, "HTML files", 10) == 0) {
        app->evil_twin_list_header_seen = true;
        return;
    }

    if(strncmp(cursor, "No HTML", 7) == 0 || strncmp(cursor, "No html", 7) == 0) {
        app->evil_twin_html_count = 0;
        simple_app_finish_evil_twin_listing(app);
        return;
    }

    if(!isdigit((unsigned char)cursor[0])) {
        if(app->evil_twin_html_count > 0) {
            simple_app_finish_evil_twin_listing(app);
        }
        return;
    }

    char* endptr = NULL;
    long id = strtol(cursor, &endptr, 10);
    if(id <= 0 || id > 255 || !endptr) {
        return;
    }
    while(*endptr == ' ' || *endptr == '\t') {
        endptr++;
    }
    if(*endptr == '\0') {
        return;
    }

    if(app->evil_twin_html_count >= EVIL_TWIN_MAX_HTML_FILES) {
        return;
    }

    EvilTwinHtmlEntry* entry = &app->evil_twin_html_entries[app->evil_twin_html_count++];
    entry->id = (uint8_t)id;
    strncpy(entry->name, endptr, EVIL_TWIN_HTML_NAME_MAX - 1);
    entry->name[EVIL_TWIN_HTML_NAME_MAX - 1] = '\0';
    app->evil_twin_list_header_seen = true;

    size_t len = strlen(entry->name);
    while(len > 0 && (entry->name[len - 1] == '\r' || entry->name[len - 1] == '\n' ||
                      entry->name[len - 1] == ' ')) {
        entry->name[--len] = '\0';
    }
}

static void simple_app_evil_twin_feed(SimpleApp* app, char ch) {
    if(!app || !app->evil_twin_listing_active) return;
    if(ch == '\r') return;

    if(ch == '>') {
        if(app->evil_twin_list_length > 0) {
            app->evil_twin_list_buffer[app->evil_twin_list_length] = '\0';
            simple_app_process_evil_twin_line(app, app->evil_twin_list_buffer);
            app->evil_twin_list_length = 0;
        }
        if(app->evil_twin_html_count > 0 || app->evil_twin_list_header_seen) {
            simple_app_finish_evil_twin_listing(app);
        }
        return;
    }

    if(ch == '\n') {
        if(app->evil_twin_list_length > 0) {
            app->evil_twin_list_buffer[app->evil_twin_list_length] = '\0';
            simple_app_process_evil_twin_line(app, app->evil_twin_list_buffer);
        } else if(app->evil_twin_list_header_seen) {
            simple_app_finish_evil_twin_listing(app);
        }
        app->evil_twin_list_length = 0;
        return;
    }

    if(app->evil_twin_list_length + 1 >= sizeof(app->evil_twin_list_buffer)) {
        app->evil_twin_list_length = 0;
        return;
    }

    app->evil_twin_list_buffer[app->evil_twin_list_length++] = ch;
}

static void simple_app_handle_evil_twin_popup_event(SimpleApp* app, const InputEvent* event) {
    if(!app || !event || !app->evil_twin_popup_active) return;
    if(!app->evil_twin_html_entries) return;

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return;

    InputKey key = event->key;
    if(event->type == InputTypeShort && key == InputKeyBack) {
        simple_app_close_evil_twin_popup(app);
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(app->evil_twin_html_count == 0) {
        if(event->type == InputTypeShort && key == InputKeyOk) {
            simple_app_close_evil_twin_popup(app);
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
        return;
    }

    size_t visible = (app->evil_twin_html_count > EVIL_TWIN_POPUP_VISIBLE_LINES) ?
                         EVIL_TWIN_POPUP_VISIBLE_LINES :
                         app->evil_twin_html_count;
    if(visible == 0) visible = 1;

    if(key == InputKeyUp) {
        if(app->evil_twin_html_popup_index > 0) {
            app->evil_twin_html_popup_index--;
            if(app->evil_twin_html_popup_index < app->evil_twin_html_popup_offset) {
                app->evil_twin_html_popup_offset = app->evil_twin_html_popup_index;
            }
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
    } else if(key == InputKeyDown) {
        if(app->evil_twin_html_popup_index + 1 < app->evil_twin_html_count) {
            app->evil_twin_html_popup_index++;
            if(app->evil_twin_html_count > visible &&
               app->evil_twin_html_popup_index >= app->evil_twin_html_popup_offset + visible) {
                app->evil_twin_html_popup_offset = app->evil_twin_html_popup_index - visible + 1;
            }
            size_t max_offset =
                (app->evil_twin_html_count > visible) ? (app->evil_twin_html_count - visible) : 0;
            if(app->evil_twin_html_popup_offset > max_offset) {
                app->evil_twin_html_popup_offset = max_offset;
            }
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
    } else if(event->type == InputTypeShort && key == InputKeyOk) {
        if(app->evil_twin_html_popup_index < app->evil_twin_html_count) {
            const EvilTwinHtmlEntry* entry =
                &app->evil_twin_html_entries[app->evil_twin_html_popup_index];
            app->evil_twin_selected_html_id = entry->id;
            strncpy(app->evil_twin_selected_html_name, entry->name, EVIL_TWIN_HTML_NAME_MAX - 1);
            app->evil_twin_selected_html_name[EVIL_TWIN_HTML_NAME_MAX - 1] = '\0';

            app->karma_selected_html_id = entry->id;
            strncpy(app->karma_selected_html_name, entry->name, KARMA_HTML_NAME_MAX - 1);
            app->karma_selected_html_name[KARMA_HTML_NAME_MAX - 1] = '\0';

            char command[48];
            snprintf(command, sizeof(command), "select_html %u", (unsigned)entry->id);
            simple_app_send_command(app, command, false);
            app->last_command_sent = false;

            char message[64];
            snprintf(message, sizeof(message), "HTML set:\n%s", entry->name);
            simple_app_show_status_message(app, message, 1500, true);

            simple_app_close_evil_twin_popup(app);
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
    }
}

static void simple_app_update_karma_duration_label(SimpleApp* app) {
    if(!app) return;
    if(app->karma_sniffer_duration_sec < KARMA_SNIFFER_DURATION_MIN_SEC) {
        app->karma_sniffer_duration_sec = KARMA_SNIFFER_DURATION_MIN_SEC;
    } else if(app->karma_sniffer_duration_sec > KARMA_SNIFFER_DURATION_MAX_SEC) {
        app->karma_sniffer_duration_sec = KARMA_SNIFFER_DURATION_MAX_SEC;
    }
}

static void simple_app_modify_karma_duration(SimpleApp* app, int32_t delta) {
    if(!app || delta == 0) return;
    int32_t value = (int32_t)app->karma_sniffer_duration_sec + delta;
    if(value < (int32_t)KARMA_SNIFFER_DURATION_MIN_SEC) {
        value = (int32_t)KARMA_SNIFFER_DURATION_MIN_SEC;
    }
    if(value > (int32_t)KARMA_SNIFFER_DURATION_MAX_SEC) {
        value = (int32_t)KARMA_SNIFFER_DURATION_MAX_SEC;
    }
    if((uint32_t)value != app->karma_sniffer_duration_sec) {
        app->karma_sniffer_duration_sec = (uint32_t)value;
        simple_app_update_karma_duration_label(app);
        simple_app_mark_config_dirty(app);
    }
}

static void simple_app_reset_karma_probe_listing(SimpleApp* app) {
    if(!app) return;
    app->karma_probe_listing_active = false;
    app->karma_probe_list_header_seen = false;
    app->karma_probe_list_length = 0;
    app->karma_probe_popup_index = 0;
    app->karma_probe_popup_offset = 0;
    app->karma_probe_popup_active = false;
}

static int simple_app_find_karma_probe_by_name(const SimpleApp* app, const char* name) {
    if(!app || !name || !app->karma_probes) return -1;
    for(size_t i = 0; i < app->karma_probe_count; i++) {
        if(strncmp(app->karma_probes[i].name, name, sizeof(app->karma_probes[i].name)) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static void simple_app_finish_karma_probe_listing(SimpleApp* app) {
    if(!app || !app->karma_probe_listing_active) return;
    app->karma_probe_listing_active = false;
    app->karma_probe_list_length = 0;
    app->karma_probe_list_header_seen = false;
    app->last_command_sent = false;
    bool on_karma_screen = (app->screen == ScreenKarmaMenu);
    if(app->karma_status_active) {
        simple_app_clear_status_message(app);
        app->karma_status_active = false;
    }

    if(app->karma_probe_count == 0) {
        if(on_karma_screen) {
            simple_app_show_status_message(app, "No probes found", 1500, true);
            app->karma_probe_popup_active = false;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
        return;
    }

    if(!on_karma_screen) {
        app->karma_probe_popup_active = false;
        return;
    }

    size_t target_index = 0;
    if(app->karma_selected_probe_id != 0) {
        for(size_t i = 0; i < app->karma_probe_count; i++) {
            if(app->karma_probes[i].id == app->karma_selected_probe_id) {
                target_index = i;
                break;
            }
        }
        if(target_index >= app->karma_probe_count) {
            target_index = 0;
        }
    }

    app->karma_probe_popup_index = target_index;
    size_t visible = (app->karma_probe_count > KARMA_POPUP_VISIBLE_LINES) ?
                         KARMA_POPUP_VISIBLE_LINES :
                         app->karma_probe_count;
    if(visible == 0) visible = 1;

    if(app->karma_probe_count <= visible || app->karma_probe_popup_index < visible) {
        app->karma_probe_popup_offset = 0;
    } else {
        app->karma_probe_popup_offset = app->karma_probe_popup_index - visible + 1;
    }
    size_t max_offset = (app->karma_probe_count > visible) ? (app->karma_probe_count - visible) :
                                                             0;
    if(app->karma_probe_popup_offset > max_offset) {
        app->karma_probe_popup_offset = max_offset;
    }

    app->karma_probe_popup_active = true;
    if(app->viewport) {
        view_port_update(app->viewport);
    }
}

static void simple_app_process_karma_probe_line(SimpleApp* app, const char* line) {
    if(!app || !line || !app->karma_probe_listing_active) return;
    if(!app->karma_probes) return;

    const char* cursor = line;
    while(*cursor == ' ' || *cursor == '\t') {
        cursor++;
    }

    if(*cursor == '\0') {
        if(app->karma_probe_list_header_seen) {
            simple_app_finish_karma_probe_listing(app);
        }
        return;
    }

    if(strncmp(cursor, "Probes", 6) == 0 || strncmp(cursor, "Probe", 5) == 0) {
        app->karma_probe_list_header_seen = true;
        return;
    }

    if(strncmp(cursor, "No probes", 9) == 0 || strncmp(cursor, "No Probes", 9) == 0) {
        simple_app_finish_karma_probe_listing(app);
        return;
    }

    if(!isdigit((unsigned char)cursor[0])) {
        if(app->karma_probe_list_header_seen) {
            simple_app_finish_karma_probe_listing(app);
        }
        return;
    }

    char* endptr = NULL;
    long id = strtol(cursor, &endptr, 10);
    if(id <= 0 || id > 255 || !endptr) {
        return;
    }
    while(*endptr == ' ' || *endptr == '\t') {
        endptr++;
    }
    if(*endptr == '\0') {
        return;
    }

    char name_buf[KARMA_PROBE_NAME_MAX];
    strncpy(name_buf, endptr, sizeof(name_buf) - 1);
    name_buf[sizeof(name_buf) - 1] = '\0';

    size_t len = strlen(name_buf);
    while(len > 0 && (name_buf[len - 1] == '\r' || name_buf[len - 1] == '\n' ||
                      name_buf[len - 1] == ' ' || name_buf[len - 1] == '\t')) {
        name_buf[--len] = '\0';
    }
    if(name_buf[0] == '\0') {
        return;
    }

    int existing = simple_app_find_karma_probe_by_name(app, name_buf);
    if(existing >= 0) {
        app->karma_probes[existing].id = (uint8_t)id;
        return;
    }

    if(app->karma_probe_count >= KARMA_MAX_PROBES) {
        return;
    }

    KarmaProbeEntry* entry = &app->karma_probes[app->karma_probe_count++];
    entry->id = (uint8_t)id;
    strncpy(entry->name, name_buf, KARMA_PROBE_NAME_MAX - 1);
    entry->name[KARMA_PROBE_NAME_MAX - 1] = '\0';

    app->karma_probe_list_header_seen = true;
}

static void simple_app_karma_probe_feed(SimpleApp* app, char ch) {
    if(!app || !app->karma_probe_listing_active) return;
    if(ch == '\r') return;

    if(ch == '>') {
        if(app->karma_probe_list_length > 0) {
            app->karma_probe_list_buffer[app->karma_probe_list_length] = '\0';
            simple_app_process_karma_probe_line(app, app->karma_probe_list_buffer);
            app->karma_probe_list_length = 0;
        }
        if(app->karma_probe_count > 0 || app->karma_probe_list_header_seen) {
            simple_app_finish_karma_probe_listing(app);
        }
        return;
    }

    if(ch == '\n') {
        if(app->karma_probe_list_length > 0) {
            app->karma_probe_list_buffer[app->karma_probe_list_length] = '\0';
            simple_app_process_karma_probe_line(app, app->karma_probe_list_buffer);
        } else if(app->karma_probe_list_header_seen) {
            simple_app_finish_karma_probe_listing(app);
        }
        app->karma_probe_list_length = 0;
        return;
    }

    if(app->karma_probe_list_length + 1 >= sizeof(app->karma_probe_list_buffer)) {
        app->karma_probe_list_length = 0;
        return;
    }

    app->karma_probe_list_buffer[app->karma_probe_list_length++] = ch;
}

static void simple_app_draw_karma_probe_popup(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas || !app->karma_probe_popup_active) return;
    if(!app->karma_probes) return;

    const uint8_t bubble_x = 4;
    const uint8_t bubble_y = 4;
    const uint8_t bubble_w = DISPLAY_WIDTH - (bubble_x * 2);
    const uint8_t bubble_h = 56;
    const uint8_t radius = 9;

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_rbox(canvas, bubble_x, bubble_y, bubble_w, bubble_h, radius);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, bubble_x, bubble_y, bubble_w, bubble_h, radius);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, bubble_x + 8, bubble_y + 16, "Select Probe");

    canvas_set_font(canvas, FontSecondary);
    uint8_t list_y = bubble_y + 28;

    if(app->karma_probe_count == 0) {
        canvas_draw_str(canvas, bubble_x + 10, list_y, "No probes found");
        canvas_draw_str(
            canvas, bubble_x + 10, (uint8_t)(list_y + HINT_LINE_HEIGHT), "Back returns");
        return;
    }

    size_t visible = app->karma_probe_count;
    if(visible > KARMA_POPUP_VISIBLE_LINES) {
        visible = KARMA_POPUP_VISIBLE_LINES;
    }

    if(app->karma_probe_popup_offset >= app->karma_probe_count) {
        app->karma_probe_popup_offset =
            (app->karma_probe_count > visible) ? (app->karma_probe_count - visible) : 0;
    }

    for(size_t i = 0; i < visible; i++) {
        size_t idx = app->karma_probe_popup_offset + i;
        if(idx >= app->karma_probe_count) break;
        const KarmaProbeEntry* entry = &app->karma_probes[idx];
        char line[64];
        snprintf(line, sizeof(line), "%u %s", (unsigned)entry->id, entry->name);
        simple_app_truncate_text(line, 28);
        uint8_t line_y = (uint8_t)(list_y + i * HINT_LINE_HEIGHT);
        if(idx == app->karma_probe_popup_index) {
            canvas_draw_str(canvas, bubble_x + 4, line_y, ">");
        }
        canvas_draw_str(canvas, bubble_x + 8, line_y, line);
    }

    if(app->karma_probe_count > KARMA_POPUP_VISIBLE_LINES) {
        bool show_up = (app->karma_probe_popup_offset > 0);
        bool show_down = (app->karma_probe_popup_offset + visible < app->karma_probe_count);
        if(show_up || show_down) {
            uint8_t arrow_x = (uint8_t)(bubble_x + bubble_w - 10);
            int16_t content_top = list_y;
            int16_t content_bottom =
                list_y + (int16_t)((visible > 0 ? (visible - 1) : 0) * HINT_LINE_HEIGHT);
            int16_t min_base = bubble_y + 12;
            int16_t max_base = bubble_y + bubble_h - 12;
            simple_app_draw_scroll_hints_clamped(
                canvas,
                arrow_x,
                content_top,
                content_bottom,
                show_up,
                show_down,
                min_base,
                max_base);
        }
    }
}

static void simple_app_handle_karma_probe_popup_event(SimpleApp* app, const InputEvent* event) {
    if(!app || !event || !app->karma_probe_popup_active) return;
    if(!app->karma_probes) return;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return;

    InputKey key = event->key;
    if(event->type == InputTypeShort && key == InputKeyBack) {
        app->karma_probe_popup_active = false;
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(app->karma_probe_count == 0) {
        if(event->type == InputTypeShort && key == InputKeyOk) {
            app->karma_probe_popup_active = false;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
        return;
    }

    size_t visible = (app->karma_probe_count > KARMA_POPUP_VISIBLE_LINES) ?
                         KARMA_POPUP_VISIBLE_LINES :
                         app->karma_probe_count;
    if(visible == 0) visible = 1;

    if(key == InputKeyUp) {
        if(app->karma_probe_popup_index > 0) {
            app->karma_probe_popup_index--;
            if(app->karma_probe_popup_index < app->karma_probe_popup_offset) {
                app->karma_probe_popup_offset = app->karma_probe_popup_index;
            }
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
    } else if(key == InputKeyDown) {
        if(app->karma_probe_popup_index + 1 < app->karma_probe_count) {
            app->karma_probe_popup_index++;
            if(app->karma_probe_count > visible &&
               app->karma_probe_popup_index >= app->karma_probe_popup_offset + visible) {
                app->karma_probe_popup_offset = app->karma_probe_popup_index - visible + 1;
            }
            size_t max_offset =
                (app->karma_probe_count > visible) ? (app->karma_probe_count - visible) : 0;
            if(app->karma_probe_popup_offset > max_offset) {
                app->karma_probe_popup_offset = max_offset;
            }
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
    } else if(event->type == InputTypeShort && key == InputKeyOk) {
        if(app->karma_probe_popup_index < app->karma_probe_count) {
            const KarmaProbeEntry* entry = &app->karma_probes[app->karma_probe_popup_index];
            app->karma_selected_probe_id = entry->id;
            strncpy(app->karma_selected_probe_name, entry->name, KARMA_PROBE_NAME_MAX - 1);
            app->karma_selected_probe_name[KARMA_PROBE_NAME_MAX - 1] = '\0';

            char message[64];
            snprintf(message, sizeof(message), "Probe selected:\n%s", entry->name);
            simple_app_show_status_message(app, message, 1500, true);

            app->karma_probe_popup_active = false;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
    }
}

static void simple_app_request_karma_probe_list(SimpleApp* app) {
    if(!app) return;
    simple_app_reset_karma_probe_listing(app);
    if(!simple_app_karma_alloc_probes(app)) {
        simple_app_show_status_message(app, "OOM: probe list", 2000, true);
        return;
    }
    app->karma_probe_listing_active = true;
    app->karma_pending_probe_refresh = false;
    bool show_status = (app->screen == ScreenKarmaMenu);
    if(show_status) {
        simple_app_show_status_message(app, "Listing probes...", 0, false);
        app->karma_status_active = true;
    } else {
        app->karma_status_active = false;
    }
    simple_app_send_command(app, "list_probes", false);
    if(show_status && app->viewport) {
        view_port_update(app->viewport);
    }
}

static void simple_app_reset_karma_html_listing(SimpleApp* app) {
    if(!app) return;
    simple_app_karma_free_html_entries(app);
    app->karma_html_listing_active = false;
    app->karma_html_list_header_seen = false;
    app->karma_html_list_length = 0;
    app->karma_html_count = 0;
    app->karma_html_popup_index = 0;
    app->karma_html_popup_offset = 0;
    app->karma_html_popup_active = false;
}

static void simple_app_finish_karma_html_listing(SimpleApp* app) {
    if(!app || !app->karma_html_listing_active) return;
    app->karma_html_listing_active = false;
    app->karma_html_list_length = 0;
    app->karma_html_list_header_seen = false;
    app->last_command_sent = false;
    bool on_html_screen = (app->screen == ScreenKarmaMenu) || (app->screen == ScreenPortalMenu);
    if(app->karma_status_active) {
        simple_app_clear_status_message(app);
        app->karma_status_active = false;
    }

    if(app->karma_html_count == 0) {
        if(on_html_screen) {
            simple_app_show_status_message(app, "No HTML files\nfound on SD", 1500, true);
            app->karma_html_popup_active = false;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
        return;
    }

    if(!on_html_screen) {
        app->karma_html_popup_active = false;
        return;
    }

    size_t target_index = 0;
    if(app->karma_selected_html_id != 0) {
        for(size_t i = 0; i < app->karma_html_count; i++) {
            if(app->karma_html_entries[i].id == app->karma_selected_html_id) {
                target_index = i;
                break;
            }
        }
        if(target_index >= app->karma_html_count) {
            target_index = 0;
        }
    }

    app->karma_html_popup_index = target_index;
    size_t visible = (app->karma_html_count > KARMA_POPUP_VISIBLE_LINES) ?
                         KARMA_POPUP_VISIBLE_LINES :
                         app->karma_html_count;
    if(visible == 0) visible = 1;

    if(app->karma_html_count <= visible || app->karma_html_popup_index < visible) {
        app->karma_html_popup_offset = 0;
    } else {
        app->karma_html_popup_offset = app->karma_html_popup_index - visible + 1;
    }
    size_t max_offset = (app->karma_html_count > visible) ? (app->karma_html_count - visible) : 0;
    if(app->karma_html_popup_offset > max_offset) {
        app->karma_html_popup_offset = max_offset;
    }

    app->karma_html_popup_active = true;
    if(app->viewport) {
        view_port_update(app->viewport);
    }
}

static void simple_app_process_karma_html_line(SimpleApp* app, const char* line) {
    if(!app || !line || !app->karma_html_listing_active) return;
    if(!app->karma_html_entries) return;

    const char* cursor = line;
    while(*cursor == ' ' || *cursor == '\t') {
        cursor++;
    }
    if(*cursor == '\0') {
        if(app->karma_html_count > 0) {
            simple_app_finish_karma_html_listing(app);
        }
        return;
    }

    if(strncmp(cursor, "HTML files", 10) == 0) {
        app->karma_html_list_header_seen = true;
        return;
    }

    if(strncmp(cursor, "No HTML", 7) == 0 || strncmp(cursor, "No html", 7) == 0) {
        app->karma_html_count = 0;
        simple_app_finish_karma_html_listing(app);
        return;
    }

    if(!isdigit((unsigned char)cursor[0])) {
        if(app->karma_html_count > 0) {
            simple_app_finish_karma_html_listing(app);
        }
        return;
    }

    char* endptr = NULL;
    long id = strtol(cursor, &endptr, 10);
    if(id <= 0 || id > 255 || !endptr) {
        return;
    }
    while(*endptr == ' ' || *endptr == '\t') {
        endptr++;
    }
    if(*endptr == '\0') {
        return;
    }

    if(app->karma_html_count >= KARMA_MAX_HTML_FILES) {
        return;
    }

    KarmaHtmlEntry* entry = &app->karma_html_entries[app->karma_html_count++];
    entry->id = (uint8_t)id;
    strncpy(entry->name, endptr, KARMA_HTML_NAME_MAX - 1);
    entry->name[KARMA_HTML_NAME_MAX - 1] = '\0';

    size_t len = strlen(entry->name);
    while(len > 0 && (entry->name[len - 1] == '\r' || entry->name[len - 1] == '\n' ||
                      entry->name[len - 1] == ' ' || entry->name[len - 1] == '\t')) {
        entry->name[--len] = '\0';
    }

    app->karma_html_list_header_seen = true;
}

static void simple_app_karma_html_feed(SimpleApp* app, char ch) {
    if(!app || !app->karma_html_listing_active) return;
    if(ch == '\r') return;

    if(ch == '>') {
        if(app->karma_html_list_length > 0) {
            app->karma_html_list_buffer[app->karma_html_list_length] = '\0';
            simple_app_process_karma_html_line(app, app->karma_html_list_buffer);
            app->karma_html_list_length = 0;
        }
        if(app->karma_html_count > 0 || app->karma_html_list_header_seen) {
            simple_app_finish_karma_html_listing(app);
        }
        return;
    }

    if(ch == '\n') {
        if(app->karma_html_list_length > 0) {
            app->karma_html_list_buffer[app->karma_html_list_length] = '\0';
            simple_app_process_karma_html_line(app, app->karma_html_list_buffer);
        } else if(app->karma_html_list_header_seen) {
            simple_app_finish_karma_html_listing(app);
        }
        app->karma_html_list_length = 0;
        return;
    }

    if(app->karma_html_list_length + 1 >= sizeof(app->karma_html_list_buffer)) {
        app->karma_html_list_length = 0;
        return;
    }

    app->karma_html_list_buffer[app->karma_html_list_length++] = ch;
}

static void simple_app_draw_karma_html_popup(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas || !app->karma_html_popup_active) return;
    if(!app->karma_html_entries) return;

    const uint8_t bubble_x = 4;
    const uint8_t bubble_y = 4;
    const uint8_t bubble_w = DISPLAY_WIDTH - (bubble_x * 2);
    const uint8_t bubble_h = 56;
    const uint8_t radius = 9;

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_rbox(canvas, bubble_x, bubble_y, bubble_w, bubble_h, radius);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, bubble_x, bubble_y, bubble_w, bubble_h, radius);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, bubble_x + 8, bubble_y + 16, "Select HTML");

    canvas_set_font(canvas, FontSecondary);
    uint8_t list_y = bubble_y + 28;

    if(app->karma_html_count == 0) {
        canvas_draw_str(canvas, bubble_x + 10, list_y, "No HTML files");
        canvas_draw_str(
            canvas, bubble_x + 10, (uint8_t)(list_y + HINT_LINE_HEIGHT), "Back returns");
        return;
    }

    size_t visible = app->karma_html_count;
    if(visible > KARMA_POPUP_VISIBLE_LINES) {
        visible = KARMA_POPUP_VISIBLE_LINES;
    }

    if(app->karma_html_popup_offset >= app->karma_html_count) {
        app->karma_html_popup_offset =
            (app->karma_html_count > visible) ? (app->karma_html_count - visible) : 0;
    }

    for(size_t i = 0; i < visible; i++) {
        size_t idx = app->karma_html_popup_offset + i;
        if(idx >= app->karma_html_count) break;
        const KarmaHtmlEntry* entry = &app->karma_html_entries[idx];
        char line[48];
        snprintf(line, sizeof(line), "%u %s", (unsigned)entry->id, entry->name);
        simple_app_truncate_text(line, 28);
        uint8_t line_y = (uint8_t)(list_y + i * HINT_LINE_HEIGHT);
        if(idx == app->karma_html_popup_index) {
            canvas_draw_str(canvas, bubble_x + 4, line_y, ">");
        }
        canvas_draw_str(canvas, bubble_x + 8, line_y, line);
    }

    if(app->karma_html_count > KARMA_POPUP_VISIBLE_LINES) {
        bool show_up = (app->karma_html_popup_offset > 0);
        bool show_down = (app->karma_html_popup_offset + visible < app->karma_html_count);
        if(show_up || show_down) {
            uint8_t arrow_x = (uint8_t)(bubble_x + bubble_w - 10);
            int16_t content_top = list_y;
            int16_t content_bottom =
                list_y + (int16_t)((visible > 0 ? (visible - 1) : 0) * HINT_LINE_HEIGHT);
            int16_t min_base = bubble_y + 12;
            int16_t max_base = bubble_y + bubble_h - 12;
            simple_app_draw_scroll_hints_clamped(
                canvas,
                arrow_x,
                content_top,
                content_bottom,
                show_up,
                show_down,
                min_base,
                max_base);
        }
    }
}

static void simple_app_handle_karma_html_popup_event(SimpleApp* app, const InputEvent* event) {
    if(!app || !event || !app->karma_html_popup_active) return;
    if(!app->karma_html_entries) return;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat &&
       event->type != InputTypeLong)
        return;

    InputKey key = event->key;
    bool is_short = (event->type == InputTypeShort);

    if(is_short && key == InputKeyBack) {
        app->karma_html_popup_active = false;
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(app->karma_html_count == 0) {
        if(is_short && key == InputKeyOk) {
            app->karma_html_popup_active = false;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
        return;
    }

    size_t visible = (app->karma_html_count > KARMA_POPUP_VISIBLE_LINES) ?
                         KARMA_POPUP_VISIBLE_LINES :
                         app->karma_html_count;
    if(visible == 0) visible = 1;

    if(key == InputKeyUp) {
        if(app->karma_html_popup_index > 0) {
            app->karma_html_popup_index--;
            if(app->karma_html_popup_index < app->karma_html_popup_offset) {
                app->karma_html_popup_offset = app->karma_html_popup_index;
            }
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
    } else if(key == InputKeyDown) {
        if(app->karma_html_popup_index + 1 < app->karma_html_count) {
            app->karma_html_popup_index++;
            if(app->karma_html_count > visible &&
               app->karma_html_popup_index >= app->karma_html_popup_offset + visible) {
                app->karma_html_popup_offset = app->karma_html_popup_index - visible + 1;
            }
            size_t max_offset =
                (app->karma_html_count > visible) ? (app->karma_html_count - visible) : 0;
            if(app->karma_html_popup_offset > max_offset) {
                app->karma_html_popup_offset = max_offset;
            }
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
    } else if(is_short && key == InputKeyOk) {
        if(app->karma_html_popup_index < app->karma_html_count) {
            const KarmaHtmlEntry* entry = &app->karma_html_entries[app->karma_html_popup_index];
            app->karma_selected_html_id = entry->id;
            strncpy(app->karma_selected_html_name, entry->name, KARMA_HTML_NAME_MAX - 1);
            app->karma_selected_html_name[KARMA_HTML_NAME_MAX - 1] = '\0';

            app->evil_twin_selected_html_id = entry->id;
            strncpy(app->evil_twin_selected_html_name, entry->name, EVIL_TWIN_HTML_NAME_MAX - 1);
            app->evil_twin_selected_html_name[EVIL_TWIN_HTML_NAME_MAX - 1] = '\0';

            char command[48];
            snprintf(command, sizeof(command), "select_html %u", (unsigned)entry->id);
            simple_app_send_command(app, command, false);
            app->last_command_sent = false;

            char message[64];
            snprintf(message, sizeof(message), "HTML set:\n%s", entry->name);
            simple_app_show_status_message(app, message, 1500, true);

            if(app->screen == ScreenPortalMenu) {
                app->portal_menu_index = 2;
                simple_app_portal_sync_offset(app);
            }

            app->karma_html_popup_active = false;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
    }
}

static void simple_app_request_karma_html_list(SimpleApp* app) {
    if(!app) return;
    simple_app_reset_karma_html_listing(app);
    if(!simple_app_karma_alloc_html_entries(app)) {
        simple_app_show_status_message(app, "OOM: HTML list", 2000, true);
        return;
    }
    app->karma_html_listing_active = true;
    bool show_status = (app->screen == ScreenKarmaMenu) || (app->screen == ScreenPortalMenu);
    if(show_status) {
        simple_app_show_status_message(app, "Listing HTML...", 0, false);
        app->karma_status_active = true;
    } else {
        app->karma_status_active = false;
    }
    simple_app_send_command(app, "list_sd", false);
    if(show_status && app->viewport) {
        view_port_update(app->viewport);
    }
}

static void simple_app_start_karma_sniffer(SimpleApp* app) {
    if(!app) return;
    if(app->karma_probe_listing_active || app->karma_html_listing_active ||
       app->evil_twin_listing_active) {
        simple_app_send_stop_if_needed(app);
        simple_app_reset_karma_probe_listing(app);
        simple_app_reset_karma_html_listing(app);
        if(app->evil_twin_listing_active) {
            simple_app_reset_evil_twin_listing(app);
        }
    }
    if(app->karma_sniffer_running) {
        simple_app_send_stop_if_needed(app);
        app->karma_sniffer_running = false;
        app->karma_pending_probe_refresh = false;
    }
    simple_app_update_karma_duration_label(app);
    uint32_t duration_ms = app->karma_sniffer_duration_sec * 1000;
    app->karma_sniffer_stop_tick = furi_get_tick() + furi_ms_to_ticks(duration_ms);
    app->karma_sniffer_running = true;
    app->karma_pending_probe_refresh = false;

    simple_app_show_status_message(app, "Collecting probes...", 0, false);
    app->karma_status_active = true;
    simple_app_send_command(app, "start_sniffer", false);
    if(app->viewport) {
        view_port_update(app->viewport);
    }
}

static void simple_app_start_karma_attack(SimpleApp* app) {
    if(!app) return;
    if(app->karma_sniffer_running) {
        simple_app_show_status_message(app, "Sniffer running\nwait for idle", 1500, true);
        return;
    }
    if(app->karma_probe_listing_active || app->karma_html_listing_active ||
       app->evil_twin_listing_active) {
        simple_app_show_status_message(app, "Wait for list\ncompletion", 1500, true);
        return;
    }
    if(app->karma_selected_probe_id == 0) {
        simple_app_show_status_message(app, "Select probe\nbefore starting", 1500, true);
        return;
    }
    if(app->karma_selected_html_id == 0) {
        simple_app_show_status_message(app, "Select HTML file\nbefore starting", 1500, true);
        return;
    }

    char select_command[48];
    snprintf(
        select_command,
        sizeof(select_command),
        "select_html %u",
        (unsigned)app->karma_selected_html_id);
    simple_app_send_command(app, select_command, false);
    app->last_command_sent = false;

    char start_command[48];
    snprintf(
        start_command,
        sizeof(start_command),
        "start_karma %u",
        (unsigned)app->karma_selected_probe_id);
    simple_app_send_command(app, start_command, true);
}

static void simple_app_draw_karma_menu(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 4, 8, "Karma");

    canvas_set_font(canvas, FontSecondary);

    const size_t visible_count = 3;
    const size_t total_count = KARMA_MENU_OPTION_COUNT;
    size_t offset = app->karma_menu_offset;

    if(app->karma_menu_index < offset) {
        offset = app->karma_menu_index;
    } else if(app->karma_menu_index >= offset + visible_count) {
        offset = (app->karma_menu_index >= visible_count) ?
                     (app->karma_menu_index - visible_count + 1) :
                     0;
    }

    if(total_count <= visible_count) {
        offset = 0;
    } else if(offset + visible_count > total_count) {
        offset = total_count - visible_count;
    }
    app->karma_menu_offset = offset;

    const uint8_t base_y = 18;
    uint8_t current_y = base_y;
    uint8_t list_bottom_y = base_y;

    size_t draw_count = 0;
    for(size_t idx = offset; idx < total_count && draw_count < visible_count;
        idx++, draw_count++) {
        bool is_selected = (app->karma_menu_index == idx);
        bool detail_block = (idx == 2 || idx == 3);

        char label[20];
        char info[48];
        info[0] = '\0';

        switch(idx) {
        case 0:
            strncpy(label, "Collect Probes", sizeof(label) - 1);
            label[sizeof(label) - 1] = '\0';
            if(app->karma_sniffer_running) {
                strncpy(info, "running", sizeof(info) - 1);
            } else if(app->karma_pending_probe_refresh) {
                strncpy(info, "updating", sizeof(info) - 1);
            } else {
                strncpy(info, "idle", sizeof(info) - 1);
            }
            break;
        case 1:
            strncpy(label, "Duration", sizeof(label) - 1);
            label[sizeof(label) - 1] = '\0';
            snprintf(info, sizeof(info), "%lus", (unsigned long)app->karma_sniffer_duration_sec);
            break;
        case 2:
            strncpy(label, "Select Probe", sizeof(label) - 1);
            label[sizeof(label) - 1] = '\0';
            if(app->karma_selected_probe_id != 0 && app->karma_selected_probe_name[0] != '\0') {
                strncpy(info, app->karma_selected_probe_name, sizeof(info) - 1);
                info[sizeof(info) - 1] = '\0';
                simple_app_truncate_text(info, 20);
            } else {
                strncpy(info, "<none>", sizeof(info) - 1);
            }
            break;
        case 3:
            strncpy(label, "Select HTML", sizeof(label) - 1);
            label[sizeof(label) - 1] = '\0';
            if(app->karma_selected_html_id != 0 && app->karma_selected_html_name[0] != '\0') {
                strncpy(info, app->karma_selected_html_name, sizeof(info) - 1);
                info[sizeof(info) - 1] = '\0';
                simple_app_truncate_text(info, 20);
            } else {
                strncpy(info, "<none>", sizeof(info) - 1);
            }
            break;
        default:
            strncpy(label, "Start Karma", sizeof(label) - 1);
            label[sizeof(label) - 1] = '\0';
            if(app->karma_selected_probe_id == 0) {
                strncpy(info, "need probe", sizeof(info) - 1);
            } else if(app->karma_selected_html_id == 0) {
                strncpy(info, "need HTML", sizeof(info) - 1);
            } else if(app->karma_sniffer_running) {
                strncpy(info, "sniffer", sizeof(info) - 1);
            } else {
                info[0] = '\0';
            }
            break;
        }
        label[sizeof(label) - 1] = '\0';
        info[sizeof(info) - 1] = '\0';

        if(is_selected) {
            canvas_draw_str(canvas, 2, current_y, ">");
        }
        canvas_draw_str(canvas, 12, current_y, label);

        if(detail_block) {
            canvas_draw_str(canvas, 14, (uint8_t)(current_y + 10), info);
        } else if(info[0] != '\0') {
            canvas_draw_str_aligned(canvas, 124, current_y, AlignRight, AlignCenter, info);
        }

        uint8_t block_height = detail_block ? 18 : 12;
        current_y = (uint8_t)(current_y + block_height);
        list_bottom_y = current_y;
    }

    uint8_t arrow_x = DISPLAY_WIDTH - 6;
    if(offset > 0) {
        int16_t top_arrow_y = (base_y > 4) ? (int16_t)(base_y - 4) : 12;
        simple_app_draw_scroll_arrow(canvas, arrow_x, top_arrow_y, true);
    }
    if(offset + visible_count < total_count) {
        int16_t raw_y = (int16_t)list_bottom_y - 8;
        if(raw_y > 60) raw_y = 60;
        if(raw_y < 16) raw_y = 16;
        simple_app_draw_scroll_arrow(canvas, arrow_x, raw_y, false);
    }
}

static void simple_app_handle_karma_menu_input(SimpleApp* app, InputKey key) {
    if(!app) return;

    if(key == InputKeyBack || key == InputKeyLeft) {
        simple_app_send_stop_if_needed(app);
        simple_app_reset_karma_probe_listing(app);
        simple_app_reset_karma_html_listing(app);
        simple_app_clear_status_message(app);
        app->karma_status_active = false;
        app->karma_probe_popup_active = false;
        app->karma_html_popup_active = false;
        app->karma_menu_offset = 0;
        simple_app_focus_attacks_menu(app);
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(key == InputKeyUp) {
        if(app->karma_menu_index > 0) {
            app->karma_menu_index--;
        } else {
            app->karma_menu_index = KARMA_MENU_OPTION_COUNT - 1;
        }
        if(app->karma_menu_index < app->karma_menu_offset) {
            app->karma_menu_offset = app->karma_menu_index;
        }
        if(app->viewport) {
            view_port_update(app->viewport);
        }
    } else if(key == InputKeyDown) {
        if(app->karma_menu_index + 1 < KARMA_MENU_OPTION_COUNT) {
            app->karma_menu_index++;
        } else {
            app->karma_menu_index = 0;
        }
        size_t visible = 3;
        if(app->karma_menu_index < app->karma_menu_offset) {
            app->karma_menu_offset = app->karma_menu_index;
        } else if(app->karma_menu_index >= app->karma_menu_offset + visible) {
            app->karma_menu_offset = app->karma_menu_index - visible + 1;
        }
        if(app->viewport) {
            view_port_update(app->viewport);
        }
    } else if(key == InputKeyOk) {
        switch(app->karma_menu_index) {
        case 0:
            simple_app_start_karma_sniffer(app);
            break;
        case 1:
            simple_app_clear_status_message(app);
            app->karma_status_active = false;
            app->screen = ScreenSetupKarma;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
            break;
        case 2:
            if(app->karma_sniffer_running) {
                simple_app_show_status_message(app, "Sniffer running\nwait for idle", 1500, true);
                break;
            }
            if(app->karma_probe_listing_active || app->karma_probe_count == 0) {
                simple_app_request_karma_probe_list(app);
            } else {
                simple_app_clear_status_message(app);
                app->karma_status_active = false;
                if(app->karma_probe_popup_index >= app->karma_probe_count) {
                    app->karma_probe_popup_index = 0;
                }
                if(app->karma_probe_popup_offset >= app->karma_probe_count) {
                    app->karma_probe_popup_offset = 0;
                }
                app->karma_probe_popup_active = true;
                if(app->viewport) {
                    view_port_update(app->viewport);
                }
            }
            break;
        case 3:
            if(app->karma_html_listing_active || app->karma_html_count == 0) {
                simple_app_request_karma_html_list(app);
            } else {
                simple_app_clear_status_message(app);
                app->karma_status_active = false;
                if(app->karma_html_popup_index >= app->karma_html_count) {
                    app->karma_html_popup_index = 0;
                }
                if(app->karma_html_popup_offset >= app->karma_html_count) {
                    app->karma_html_popup_offset = 0;
                }
                app->karma_html_popup_active = true;
                if(app->viewport) {
                    view_port_update(app->viewport);
                }
            }
            break;
        default:
            if(app->karma_sniffer_running) {
                simple_app_show_status_message(app, "Sniffer running\nwait for idle", 1500, true);
                break;
            }
            simple_app_start_karma_attack(app);
            break;
        }
    }
}

static void simple_app_update_karma_sniffer(SimpleApp* app) {
    if(!app) return;

    uint32_t now = furi_get_tick();
    if(app->karma_sniffer_running && now >= app->karma_sniffer_stop_tick) {
        app->karma_sniffer_running = false;
        simple_app_send_command(app, "stop", false);
        app->last_command_sent = false;
        if(app->screen == ScreenKarmaMenu) {
            simple_app_show_status_message(app, "Sniffer stopped", 1000, true);
            app->karma_status_active = false;
        } else {
            simple_app_clear_status_message(app);
            app->karma_status_active = false;
        }
        app->karma_pending_probe_refresh = true;
        app->karma_pending_probe_tick = now + furi_ms_to_ticks(KARMA_AUTO_LIST_DELAY_MS);
        if(app->screen == ScreenKarmaMenu && app->viewport) {
            view_port_update(app->viewport);
        }
    }

    if(app->karma_pending_probe_refresh && now >= app->karma_pending_probe_tick) {
        if(app->karma_probe_listing_active) {
            app->karma_pending_probe_tick = now + furi_ms_to_ticks(KARMA_AUTO_LIST_DELAY_MS);
        } else {
            app->karma_pending_probe_refresh = false;
            simple_app_request_karma_probe_list(app);
            if(app->screen == ScreenKarmaMenu && app->viewport) {
                view_port_update(app->viewport);
            }
        }
    }
}

static void simple_app_draw_setup_karma(SimpleApp* app, Canvas* canvas) {
    if(!app || !canvas) return;

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 6, 16, "Karma Sniffer");

    canvas_set_font(canvas, FontSecondary);
    char line[32];
    snprintf(line, sizeof(line), "Duration: %lus", (unsigned long)app->karma_sniffer_duration_sec);
    canvas_draw_str(canvas, 6, 32, line);
    canvas_draw_str(canvas, 6, 46, "Adjust with arrows");
    canvas_draw_str(canvas, 6, 58, "OK to exit");
}

static void simple_app_handle_setup_karma_input(SimpleApp* app, InputKey key) {
    if(!app) return;

    if(key == InputKeyBack || key == InputKeyOk) {
        simple_app_save_config_if_dirty(app, "Config saved", true);
        app->screen = ScreenKarmaMenu;
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    uint32_t before = app->karma_sniffer_duration_sec;

    if(key == InputKeyUp) {
        simple_app_modify_karma_duration(app, KARMA_SNIFFER_DURATION_STEP);
    } else if(key == InputKeyDown) {
        simple_app_modify_karma_duration(app, -(int32_t)KARMA_SNIFFER_DURATION_STEP);
    } else if(key == InputKeyRight) {
        simple_app_modify_karma_duration(app, 1);
    } else if(key == InputKeyLeft) {
        simple_app_modify_karma_duration(app, -1);
    }

    if(before != app->karma_sniffer_duration_sec && app->viewport) {
        view_port_update(app->viewport);
    }
}

static void simple_app_handle_setup_scanner_input(SimpleApp* app, InputKey key) {
    if(!app) return;

    if(key == InputKeyBack) {
        simple_app_send_stop_if_needed(app);
        if(app->scanner_adjusting_power) {
            app->scanner_adjusting_power = false;
            view_port_update(app->viewport);
        } else {
            simple_app_save_config_if_dirty(app, "Config saved", true);
            app->screen = ScreenMenu;
            view_port_update(app->viewport);
        }
        return;
    }

    if(app->scanner_adjusting_power && app->scanner_setup_index == ScannerOptionMinPower) {
        if(key == InputKeyUp || key == InputKeyRight) {
            simple_app_modify_min_power(app, SCAN_POWER_STEP);
            view_port_update(app->viewport);
            return;
        } else if(key == InputKeyDown || key == InputKeyLeft) {
            simple_app_modify_min_power(app, -SCAN_POWER_STEP);
            view_port_update(app->viewport);
            return;
        }
    }

    if(key == InputKeyUp) {
        if(app->scanner_setup_index > 0) {
            app->scanner_setup_index--;
            if(app->scanner_setup_index != ScannerOptionMinPower) {
                app->scanner_adjusting_power = false;
            }
            if(app->scanner_setup_index < app->scanner_view_offset) {
                app->scanner_view_offset = app->scanner_setup_index;
            }
            view_port_update(app->viewport);
        }
    } else if(key == InputKeyDown) {
        if(app->scanner_setup_index + 1 < ScannerOptionCount) {
            app->scanner_setup_index++;
            if(app->scanner_setup_index != ScannerOptionMinPower) {
                app->scanner_adjusting_power = false;
            }
            if(app->scanner_setup_index >=
               app->scanner_view_offset + SCANNER_FILTER_VISIBLE_COUNT) {
                app->scanner_view_offset =
                    app->scanner_setup_index - SCANNER_FILTER_VISIBLE_COUNT + 1;
            }
            view_port_update(app->viewport);
        }
    } else if(key == InputKeyOk) {
        if(app->scanner_setup_index == ScannerOptionMinPower) {
            app->scanner_adjusting_power = !app->scanner_adjusting_power;
            view_port_update(app->viewport);
        } else {
            if(app->scanner_setup_index == ScannerOptionShowVendor) {
                if(app->scanner_show_vendor && simple_app_enabled_field_count(app) <= 1) {
                    if(app->viewport) {
                        view_port_update(app->viewport);
                    }
                } else {
                    bool new_state = !app->scanner_show_vendor;
                    app->scanner_show_vendor = new_state;
                    app->vendor_scan_enabled = new_state;
                    simple_app_mark_config_dirty(app);
                    simple_app_update_result_layout(app);
                    simple_app_rebuild_visible_results(app);
                    simple_app_adjust_result_offset(app);
                    simple_app_send_vendor_command(app, new_state);
                    if(app->viewport) {
                        view_port_update(app->viewport);
                    }
                }
                return;
            }
            bool* flag =
                simple_app_scanner_option_flag(app, (ScannerOption)app->scanner_setup_index);
            if(flag) {
                if(*flag && simple_app_enabled_field_count(app) <= 1) {
                    view_port_update(app->viewport);
                } else {
                    *flag = !(*flag);
                    simple_app_mark_config_dirty(app);
                    simple_app_update_result_layout(app);
                    simple_app_rebuild_visible_results(app);
                    simple_app_adjust_result_offset(app);
                    view_port_update(app->viewport);
                }
            }
        }
    }
}

static void simple_app_handle_setup_scanner_timing_input(SimpleApp* app, InputKey key) {
    if(!app) return;

    if(key == InputKeyBack) {
        simple_app_save_config_if_dirty(app, "Config saved", true);
        app->screen = ScreenMenu;
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(key == InputKeyUp) {
        if(app->scanner_timing_index > 0) {
            app->scanner_timing_index = 0;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
        return;
    }

    if(key == InputKeyDown) {
        if(app->scanner_timing_index < 1) {
            app->scanner_timing_index = 1;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
        return;
    }

    if(key == InputKeyLeft) {
        simple_app_modify_channel_time(
            app, app->scanner_timing_index == 0, -SCANNER_CHANNEL_TIME_STEP_MS);
    } else if(key == InputKeyRight) {
        simple_app_modify_channel_time(
            app, app->scanner_timing_index == 0, SCANNER_CHANNEL_TIME_STEP_MS);
    } else if(key == InputKeyOk) {
        simple_app_request_channel_time(app, app->scanner_timing_index == 0);
    }
}

static void simple_app_handle_setup_led_input(SimpleApp* app, InputKey key) {
    if(!app) return;

    if(key == InputKeyBack) {
        simple_app_save_config_if_dirty(app, "Config saved", true);
        app->screen = ScreenMenu;
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(key == InputKeyUp) {
        if(app->led_setup_index > 0) {
            app->led_setup_index = 0;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
        return;
    }

    if(key == InputKeyDown) {
        if(app->led_setup_index < 1) {
            app->led_setup_index = 1;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
        return;
    }

    if(app->led_setup_index == 0) {
        bool previous = app->led_enabled;
        if(key == InputKeyLeft) {
            app->led_enabled = false;
        } else if(key == InputKeyRight) {
            app->led_enabled = true;
        } else if(key == InputKeyOk) {
            app->led_enabled = !app->led_enabled;
        } else {
            return;
        }
        if(previous != app->led_enabled) {
            simple_app_mark_config_dirty(app);
            simple_app_update_led_label(app);
            simple_app_send_led_power_command(app);
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
        return;
    }

    if(app->led_setup_index == 1) {
        uint32_t before = app->led_level;
        if(key == InputKeyLeft) {
            if(app->led_level > 1) {
                app->led_level--;
            }
        } else if(key == InputKeyRight) {
            if(app->led_level < 100) {
                app->led_level++;
            }
        } else if(key == InputKeyOk) {
            return;
        } else {
            return;
        }
        if(before != app->led_level) {
            simple_app_mark_config_dirty(app);
            simple_app_update_led_label(app);
            simple_app_send_led_level_command(app);
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
    }
}

static void simple_app_handle_setup_boot_input(SimpleApp* app, InputKey key) {
    if(!app) return;

    if(key == InputKeyBack) {
        simple_app_save_config_if_dirty(app, "Config saved", true);
        app->screen = ScreenMenu;
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(key == InputKeyUp) {
        if(app->boot_setup_index > 0) {
            app->boot_setup_index = 0;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
        return;
    }

    if(key == InputKeyDown) {
        if(app->boot_setup_index < 1) {
            app->boot_setup_index = 1;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
        return;
    }

    bool is_long = app->boot_setup_long;
    bool* enabled_ptr = is_long ? &app->boot_long_enabled : &app->boot_short_enabled;
    uint8_t* cmd_index_ptr = is_long ? &app->boot_long_command_index :
                                       &app->boot_short_command_index;

    if(app->boot_setup_index == 0) {
        bool previous = *enabled_ptr;
        if(key == InputKeyLeft) {
            *enabled_ptr = false;
        } else if(key == InputKeyRight) {
            *enabled_ptr = true;
        } else if(key == InputKeyOk) {
            *enabled_ptr = !(*enabled_ptr);
        } else {
            return;
        }
        if(previous != *enabled_ptr) {
            simple_app_mark_config_dirty(app);
            simple_app_update_boot_labels(app);
            simple_app_send_boot_status(app, !is_long, *enabled_ptr);
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
        return;
    }

    if(app->boot_setup_index == 1) {
        uint8_t before = *cmd_index_ptr;
        if(key == InputKeyLeft) {
            if(*cmd_index_ptr == 0) {
                *cmd_index_ptr = BOOT_COMMAND_OPTION_COUNT - 1;
            } else {
                (*cmd_index_ptr)--;
            }
        } else if(key == InputKeyRight || key == InputKeyOk) {
            *cmd_index_ptr = (uint8_t)((*cmd_index_ptr + 1) % BOOT_COMMAND_OPTION_COUNT);
        } else {
            return;
        }
        if(before != *cmd_index_ptr) {
            simple_app_mark_config_dirty(app);
            simple_app_update_boot_labels(app);
            simple_app_send_boot_command(app, !is_long, *cmd_index_ptr);
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
    }
}

static void simple_app_handle_serial_input(SimpleApp* app, InputKey key) {
    if(!app) return;
    if(app->gps_console_mode && key == InputKeyBack) {
        app->screen = ScreenGps;
        app->gps_console_mode = false;
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }
    if(app->blackout_view_active && !app->blackout_full_console && key != InputKeyBack &&
       key != InputKeyUp && key != InputKeyDown) {
        return;
    }
    if(app->sniffer_dog_view_active && !app->sniffer_dog_full_console && key != InputKeyBack &&
       key != InputKeyUp && key != InputKeyDown) {
        return;
    }
    if(app->deauth_view_active && !app->deauth_full_console && key != InputKeyBack &&
       key != InputKeyUp && key != InputKeyDown && key != InputKeyLeft && key != InputKeyRight &&
       key != InputKeyOk) {
        return;
    }
    if(app->handshake_view_active && !app->handshake_full_console && key != InputKeyBack &&
       key != InputKeyOk) {
        return;
    }
    if(app->sae_view_active && !app->sae_full_console && key != InputKeyBack &&
       key != InputKeyOk) {
        return;
    }
    if(app->deauth_guard_view_active && !app->deauth_guard_full_console) {
        // In Deauth Guard compact mode ignore navigation; only Back stops and exits.
        if(key == InputKeyBack) {
            simple_app_send_stop_if_needed(app);
            app->serial_targets_hint = false;
            app->screen = ScreenMenu;
            app->serial_follow_tail = true;
            simple_app_update_scroll(app);
            view_port_update(app->viewport);
        }
        return;
    }

    if(app->deauth_view_active && !app->deauth_full_console &&
       (key == InputKeyUp || key == InputKeyDown || key == InputKeyLeft || key == InputKeyRight)) {
        if(app->scan_selected_count > 0) {
            size_t visible = 4;
            size_t max_offset =
                (app->scan_selected_count > visible) ? (app->scan_selected_count - visible) : 0;
            if(key == InputKeyUp) {
                if(app->deauth_list_offset > 0) {
                    app->deauth_list_offset--;
                }
            } else if(key == InputKeyDown) {
                if(app->deauth_list_offset < max_offset) {
                    app->deauth_list_offset++;
                }
            } else if(key == InputKeyLeft) {
                if(app->deauth_list_scroll_x >= 4) {
                    app->deauth_list_scroll_x -= 4;
                } else {
                    app->deauth_list_scroll_x = 0;
                }
            } else if(key == InputKeyRight) {
                app->deauth_list_scroll_x += 4;
            }
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }
        return;
    }

    if(app->evil_twin_running && !app->evil_twin_full_console && key != InputKeyBack) {
        return;
    }

    if(app->portal_running && !app->portal_full_console && key != InputKeyBack) {
        return;
    }

    if(app->probe_results_active) {
        if(!app->probe_ssids || !app->probe_clients) {
            return;
        }
        if(key == InputKeyBack) {
            simple_app_send_stop_preserve_results(app);
            app->probe_results_active = false;
            app->probe_full_console = false;
            app->screen = ScreenMenu;
            app->serial_follow_tail = true;
            simple_app_free_probe_buffers(app);
            if(app->viewport) {
                view_port_update(app->viewport);
            }
            return;
        }
        if(key == InputKeyRight) {
            // Boundary hint: no further screen to the right from Probes.
            furi_hal_vibro_on(true);
            furi_delay_ms(40);
            furi_hal_vibro_on(false);
            return;
        }
        if(key == InputKeyLeft) {
            app->probe_results_active = false;
            app->probe_full_console = false;
            app->sniffer_results_active = true;
            app->sniffer_full_console = false;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
            return;
        }
        if(!app->probe_full_console && app->probe_ssid_count > 0) {
            ProbeSsidEntry* ssid = &app->probe_ssids[app->probe_ssid_index];
            uint8_t list_top = 54;
            uint8_t list_bottom = DISPLAY_HEIGHT - 8;
            uint8_t max_lines = (uint8_t)((list_bottom - list_top + SERIAL_TEXT_LINE_HEIGHT) /
                                          SERIAL_TEXT_LINE_HEIGHT);
            if(max_lines == 0) max_lines = 1;

            if(key == InputKeyUp) {
                if(app->probe_client_offset > 0) {
                    app->probe_client_offset--;
                } else if(app->probe_ssid_index > 0) {
                    app->probe_ssid_index--;
                    app->probe_client_offset = 0;
                }
            } else if(key == InputKeyDown) {
                if(ssid->client_count > 0 &&
                   app->probe_client_offset + max_lines < ssid->client_count) {
                    app->probe_client_offset++;
                } else if(app->probe_ssid_index + 1 < app->probe_ssid_count) {
                    app->probe_ssid_index++;
                    app->probe_client_offset = 0;
                }
            } else {
                // ignore other keys in probe overlay
                return;
            }
            if(app->viewport) {
                view_port_update(app->viewport);
            }
            return;
        }
    }

    if(app->sniffer_results_active && !app->sniffer_full_console) {
        if(!app->sniffer_aps || !app->sniffer_clients) {
            return;
        }
        size_t ap_count = app->sniffer_ap_count;
        if(ap_count == 0) {
            return;
        }
        SnifferApEntry* ap = &app->sniffer_aps[app->sniffer_results_ap_index];
        uint8_t list_top = 54;
        uint8_t list_bottom = DISPLAY_HEIGHT - 8;
        uint8_t max_lines = (uint8_t)((list_bottom - list_top + SERIAL_TEXT_LINE_HEIGHT) /
                                      SERIAL_TEXT_LINE_HEIGHT);
        if(max_lines == 0) max_lines = 1;
        const char* current_mac = NULL;
        if(ap->client_count > 0) {
            size_t idx = ap->client_start + app->sniffer_results_client_offset;
            if(idx < ap->client_start + ap->client_count && idx < SNIFFER_MAX_CLIENTS) {
                current_mac = app->sniffer_clients[idx].mac;
            }
        }

        if(key == InputKeyBack) {
            simple_app_send_stop_preserve_results(app);
            simple_app_clear_station_selection(app);
            simple_app_send_command_quiet(app, "clear_sniffer_results");
            app->sniffer_results_active = false;
            app->sniffer_full_console = false;
            app->sniffer_view_active = false;
            app->screen = ScreenMenu;
            app->serial_follow_tail = true;
            simple_app_free_sniffer_buffers(app);
            simple_app_free_probe_buffers(app);
            if(app->viewport) {
                view_port_update(app->viewport);
            }
            return;
        } else if(key == InputKeyLeft) {
            simple_app_send_start_sniffer(app);
            return;
        } else if(key == InputKeyRight) {
            if(app->scan_selected_count == 0) {
                simple_app_show_status_message(app, "Select network\nfirst", 1200, true);
                return;
            }
            if(app->sniffer_selected_count == 0) {
                simple_app_show_status_message(app, "Pick a client\nfirst", 1200, true);
                return;
            }
            simple_app_send_select_stations(app);
            simple_app_clear_sniffer_data(app);
            return;
        } else if(key == InputKeyOk) {
            if(current_mac) {
                simple_app_toggle_sniffer_selected(app, current_mac);
                if(app->viewport) {
                    view_port_update(app->viewport);
                }
            }
            return;
        } else if(key == InputKeyUp) {
            if(app->sniffer_results_client_offset > 0) {
                app->sniffer_results_client_offset--;
            } else if(app->sniffer_results_ap_index > 0) {
                app->sniffer_results_ap_index--;
                app->sniffer_results_client_offset = 0;
            }
        } else if(key == InputKeyDown) {
            if(ap->client_count > 0 &&
               app->sniffer_results_client_offset + max_lines < ap->client_count) {
                app->sniffer_results_client_offset++;
            } else if(app->sniffer_results_ap_index + 1 < ap_count) {
                app->sniffer_results_ap_index++;
                app->sniffer_results_client_offset = 0;
            }
        } else {
            return;
        }
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(app->sniffer_view_active && !app->sniffer_full_console && key == InputKeyRight &&
       app->sniffer_has_data && app->sniffer_packet_count >= SNIFFER_RESULTS_ARROW_THRESHOLD) {
        // Stop current sniffer session before requesting results to avoid it running in background.
        simple_app_send_command(app, "stop", false);
        simple_app_send_command(app, "show_sniffer_results", true);
        return;
    }
    if(app->sniffer_view_active && !app->sniffer_full_console && key == InputKeyLeft) {
        // Boundary hint: nothing to the left of Sniffer overlay.
        furi_hal_vibro_on(true);
        furi_delay_ms(40);
        furi_hal_vibro_on(false);
        return;
    }

    if(app->scanner_view_active && !app->scanner_full_console && key == InputKeyOk) {
        simple_app_send_command(app, SCANNER_SCAN_COMMAND, true);
        return;
    }

    if(key == InputKeyBack) {
        bool had_sniffer =
            (app->sniffer_view_active || app->sniffer_results_active || app->probe_results_active);
        bool return_to_portal = app->portal_running;
        bool return_to_evil_twin = app->evil_twin_running;
        simple_app_send_stop_if_needed(app);
        simple_app_clear_station_selection(app);
        if(had_sniffer) {
            simple_app_send_command_quiet(app, "clear_sniffer_results");
        }
        app->serial_targets_hint = false;
        if(app->blackout_view_active) {
            app->blackout_view_active = false;
            simple_app_reset_blackout_status(app);
            simple_app_focus_attacks_menu(app);
        } else if(app->handshake_view_active) {
            simple_app_reset_handshake_status(app);
            simple_app_focus_attacks_menu(app);
        } else if(app->sae_view_active) {
            simple_app_reset_sae_status(app);
            simple_app_focus_attacks_menu(app);
        } else if(app->deauth_view_active) {
            app->deauth_view_active = false;
            simple_app_reset_deauth_status(app);
            simple_app_focus_attacks_menu(app);
        } else if(app->sniffer_dog_view_active) {
            app->sniffer_dog_view_active = false;
            simple_app_reset_sniffer_dog_status(app);
            simple_app_focus_attacks_menu(app);
        } else if(return_to_evil_twin) {
            app->evil_twin_popup_active = false;
            app->screen = ScreenEvilTwinMenu;
        } else if(return_to_portal) {
            app->portal_ssid_popup_active = false;
            app->portal_full_console = false;
            app->screen = ScreenPortalMenu;
        } else {
            app->screen = ScreenMenu;
        }
        if(had_sniffer) {
            simple_app_free_sniffer_buffers(app);
            simple_app_free_probe_buffers(app);
        }
        app->serial_follow_tail = true;
        simple_app_update_scroll(app);
        view_port_update(app->viewport);
    } else if(key == InputKeyUp) {
        if(app->bt_scan_view_active && app->bt_scan_show_list && !app->bt_scan_full_console) {
            if(app->bt_scan_list_offset > 0) {
                app->bt_scan_list_offset--;
                if(app->viewport) view_port_update(app->viewport);
            }
        } else if(app->serial_scroll > 0) {
            app->serial_scroll--;
            app->serial_follow_tail = false;
            view_port_update(app->viewport);
        }
    } else if(key == InputKeyDown) {
        if(app->bt_scan_view_active && app->bt_scan_show_list && !app->bt_scan_full_console) {
            if(simple_app_bt_scan_can_scroll_down(app)) {
                app->bt_scan_list_offset++;
                if(app->viewport) view_port_update(app->viewport);
            }
        } else {
            size_t max_scroll = simple_app_max_scroll(app);
            if(app->serial_scroll < max_scroll) {
                app->serial_scroll++;
                app->serial_follow_tail = (app->serial_scroll == max_scroll);
                view_port_update(app->viewport);
            } else {
                app->serial_follow_tail = true;
                simple_app_update_scroll(app);
                view_port_update(app->viewport);
            }
        }
    } else if(key == InputKeyLeft) {
        size_t step = SERIAL_VISIBLE_LINES;
        if(app->serial_scroll > 0) {
            if(app->serial_scroll > step) {
                app->serial_scroll -= step;
            } else {
                app->serial_scroll = 0;
            }
            app->serial_follow_tail = false;
            view_port_update(app->viewport);
        }
    } else if(key == InputKeyRight) {
        if(app->serial_targets_hint) {
            app->serial_targets_hint = false;
            simple_app_request_scan_results(app, SCAN_RESULTS_COMMAND);
            return;
        }

        if(app->bt_locator_console_mode && app->bt_locator_list_ready) {
            app->bt_locator_console_mode = false;
            app->screen = ScreenBtLocatorList;
            view_port_update(app->viewport);
            return;
        }

        if(app->bt_scan_view_active && !app->bt_scan_full_console && app->bt_scan_has_data &&
           !app->bt_scan_running) {
            if(!app->bt_scan_show_list) {
                app->bt_scan_show_list = true;
                app->bt_scan_list_offset = 0;
                if(app->viewport) view_port_update(app->viewport);
                return;
            }
        }

        size_t step = SERIAL_VISIBLE_LINES;
        size_t max_scroll = simple_app_max_scroll(app);
        if(app->serial_scroll < max_scroll) {
            app->serial_scroll =
                (app->serial_scroll + step < max_scroll) ? app->serial_scroll + step : max_scroll;
            app->serial_follow_tail = (app->serial_scroll == max_scroll);
            view_port_update(app->viewport);
        } else {
            app->serial_follow_tail = true;
            simple_app_update_scroll(app);
            view_port_update(app->viewport);
        }
    } else if(key == InputKeyOk) {
        app->serial_follow_tail = true;
        simple_app_update_scroll(app);
        view_port_update(app->viewport);
    }
}

static void simple_app_handle_results_input(SimpleApp* app, InputKey key) {
    if(key == InputKeyBack || key == InputKeyLeft) {
        if(app->visible_result_count == 0 && !app->scan_results_loading) {
            simple_app_clear_status_message(app);
            app->screen = ScreenMenu;
            app->menu_state = MenuStateSections;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
            return;
        }
        // Scanner results flow is one-way; no return to the scanner.
        furi_hal_vibro_on(true);
        furi_delay_ms(40);
        furi_hal_vibro_on(false);
        return;
    }

    if(app->visible_result_count == 0) return;

    if(key == InputKeyUp) {
        if(app->scan_result_index > 0) {
            app->scan_result_index--;
            simple_app_reset_result_scroll(app);
            simple_app_adjust_result_offset(app);
            view_port_update(app->viewport);
        }
    } else if(key == InputKeyDown) {
        if(app->scan_result_index + 1 < app->visible_result_count) {
            app->scan_result_index++;
            simple_app_reset_result_scroll(app);
            simple_app_adjust_result_offset(app);
            view_port_update(app->viewport);
        }
    } else if(key == InputKeyOk) {
        if(app->scan_result_index < app->visible_result_count) {
            ScanResult* result = simple_app_visible_result(app, app->scan_result_index);
            if(result) {
                simple_app_toggle_scan_selection(app, result);
                simple_app_adjust_result_offset(app);
                view_port_update(app->viewport);
            }
        }
    } else if(key == InputKeyRight) {
        if(app->scan_selected_count > 0) {
            simple_app_compact_scan_results(app);
            app->screen = ScreenMenu;
            app->menu_state = MenuStateItems;
            app->section_index = MENU_SECTION_ATTACKS;
            app->item_index = 0;
            app->item_offset = 0;
            app->last_attack_index = 0;
            app->serial_targets_hint = false;
            view_port_update(app->viewport);
        }
    }
}

static void simple_app_handle_bt_locator_input(SimpleApp* app, InputKey key) {
    if(!app || !app->bt_locator_devices) return;
    if(key == InputKeyBack || key == InputKeyLeft) {
        simple_app_send_stop_if_needed(app);
        app->screen = ScreenMenu;
        view_port_update(app->viewport);
        return;
    }

    if(app->bt_locator_list_loading || app->bt_locator_count == 0) {
        return;
    }

    if(key == InputKeyUp) {
        if(app->bt_locator_index > 0) {
            app->bt_locator_index--;
            simple_app_bt_locator_ensure_visible(app);
            simple_app_bt_locator_reset_scroll(app);
            if(app->viewport) view_port_update(app->viewport);
        }
    } else if(key == InputKeyDown) {
        if(app->bt_locator_index + 1 < app->bt_locator_count) {
            app->bt_locator_index++;
            simple_app_bt_locator_ensure_visible(app);
            simple_app_bt_locator_reset_scroll(app);
            if(app->viewport) view_port_update(app->viewport);
        }
    } else if(key == InputKeyRight) {
        if(app->bt_locator_selected < 0) {
            return; // require explicit selection
        }
        size_t target_idx = (size_t)app->bt_locator_selected;
        if(target_idx >= app->bt_locator_count) return;
        const char* mac = app->bt_locator_devices[target_idx].mac;
        if(mac[0] != '\0' && strchr(mac, ':')) {
            // capture name for overlay
            const char* selected_name = app->bt_locator_devices[target_idx].name;
            if(selected_name && selected_name[0] != '\0') {
                strncpy(
                    app->bt_locator_current_name,
                    selected_name,
                    sizeof(app->bt_locator_current_name) - 1);
                app->bt_locator_current_name[sizeof(app->bt_locator_current_name) - 1] = '\0';
            } else {
                app->bt_locator_current_name[0] = '\0';
            }
            char command[64];
            snprintf(command, sizeof(command), "scan_bt %s", mac);
            app->bt_locator_console_mode = false;
            app->bt_locator_mode = true;
            app->bt_locator_has_rssi = false;
            app->bt_locator_start_tick = furi_get_tick();
            app->bt_locator_last_console_tick = app->bt_locator_start_tick;
            strncpy(app->bt_locator_mac, mac, sizeof(app->bt_locator_mac) - 1);
            app->bt_locator_mac[sizeof(app->bt_locator_mac) - 1] = '\0';
            strncpy(app->bt_locator_saved_mac, mac, sizeof(app->bt_locator_saved_mac) - 1);
            app->bt_locator_saved_mac[sizeof(app->bt_locator_saved_mac) - 1] = '\0';
            app->bt_locator_preserve_mac = true;
            simple_app_send_command(app, command, true);
        }
    } else if(key == InputKeyOk) {
        if((int32_t)app->bt_locator_index == app->bt_locator_selected) {
            app->bt_locator_selected = -1;
        } else {
            app->bt_locator_selected = (int32_t)app->bt_locator_index;
            const char* mac_pick = app->bt_locator_devices[app->bt_locator_index].mac;
            if(mac_pick && mac_pick[0] != '\0') {
                strncpy(
                    app->bt_locator_saved_mac, mac_pick, sizeof(app->bt_locator_saved_mac) - 1);
                app->bt_locator_saved_mac[sizeof(app->bt_locator_saved_mac) - 1] = '\0';
                strncpy(app->bt_locator_mac, mac_pick, sizeof(app->bt_locator_mac) - 1);
                app->bt_locator_mac[sizeof(app->bt_locator_mac) - 1] = '\0';
                app->bt_locator_preserve_mac = true;
                const char* selected_name = app->bt_locator_devices[app->bt_locator_index].name;
                if(selected_name && selected_name[0] != '\0') {
                    strncpy(
                        app->bt_locator_current_name,
                        selected_name,
                        sizeof(app->bt_locator_current_name) - 1);
                    app->bt_locator_current_name[sizeof(app->bt_locator_current_name) - 1] = '\0';
                } else {
                    app->bt_locator_current_name[0] = '\0';
                }
                app->bt_locator_start_tick = furi_get_tick();
                app->bt_locator_last_console_tick = app->bt_locator_start_tick;
            }
        }
        simple_app_bt_locator_reset_scroll(app);
        if(app->viewport) view_port_update(app->viewport);
    }
}

static bool simple_app_is_direction_key(InputKey key) {
    return (
        key == InputKeyUp || key == InputKeyDown || key == InputKeyLeft || key == InputKeyRight);
}

static void simple_app_input(InputEvent* event, void* context) {
    SimpleApp* app = context;
    if(!app || !event) return;

    // Global long Back: stop and return to main menu regardless of current state.
    if(event->type == InputTypeLong && event->key == InputKeyBack) {
        simple_app_send_stop_if_needed(app);
        simple_app_clear_station_selection(app);
        app->serial_targets_hint = false;
        app->scanner_view_active = false;
        app->sniffer_view_active = false;
        app->sniffer_results_active = false;
        app->probe_results_active = false;
        app->bt_scan_view_active = false;
        app->bt_locator_mode = false;
        app->blackout_view_active = false;
        simple_app_reset_blackout_status(app);
        app->deauth_view_active = false;
        simple_app_reset_deauth_status(app);
        simple_app_reset_handshake_status(app);
        simple_app_reset_sae_status(app);
        app->sniffer_dog_view_active = false;
        simple_app_reset_sniffer_dog_status(app);
        app->deauth_guard_view_active = false;
        app->screen = ScreenMenu;
        app->menu_state = MenuStateSections;
        simple_app_adjust_section_offset(app);
        app->serial_follow_tail = true;
        app->last_back_tick = 0;
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    uint32_t now = furi_get_tick();

    // Global double short Back: show exit confirmation (after stopping tasks).
    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        uint32_t last = app->last_back_tick;
        app->last_back_tick = now;
        if(last != 0 && (now - last) <= furi_ms_to_ticks(DOUBLE_BACK_EXIT_MS)) {
            simple_app_send_stop_if_needed(app);
            simple_app_clear_station_selection(app);
            app->serial_targets_hint = false;
            app->confirm_exit_yes = false;
            app->exit_confirm_active = true;
            app->exit_confirm_tick = now;
            app->screen = ScreenConfirmExit;
            app->menu_state = MenuStateSections;
            app->last_back_tick = 0;
            app->last_input_tick = now;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
            return;
        }
    }

    app->last_input_tick = now;

    if(simple_app_status_message_is_active(app) && app->status_message_fullscreen) {
        if(event->type == InputTypeShort &&
           (event->key == InputKeyOk || event->key == InputKeyBack)) {
            if(event->key == InputKeyBack) {
                simple_app_send_stop_if_needed(app);
            }
            simple_app_clear_status_message(app);
        }
        return;
    }

    if(app->sd_delete_confirm_active) {
        simple_app_handle_sd_delete_confirm_event(app, event);
        return;
    }

    if(app->sd_file_popup_active) {
        simple_app_handle_sd_file_popup_event(app, event);
        return;
    }

    if(app->sd_folder_popup_active) {
        simple_app_handle_sd_folder_popup_event(app, event);
        return;
    }

    if(app->portal_ssid_popup_active) {
        simple_app_handle_portal_ssid_popup_event(app, event);
        return;
    }

    if(app->karma_probe_popup_active) {
        simple_app_handle_karma_probe_popup_event(app, event);
        return;
    }

    if(app->karma_html_popup_active) {
        simple_app_handle_karma_html_popup_event(app, event);
        return;
    }

    if(app->evil_twin_popup_active) {
        simple_app_handle_evil_twin_popup_event(app, event);
        return;
    }

    if((event->type == InputTypeLong || event->type == InputTypeShort) &&
       event->key == InputKeyOk && app->handshake_view_active && app->screen == ScreenSerial) {
        app->handshake_full_console = !app->handshake_full_console;
        app->serial_follow_tail = true;
        app->serial_targets_hint = false;
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if((event->type == InputTypeLong || event->type == InputTypeShort) &&
       event->key == InputKeyOk && app->sae_view_active && app->screen == ScreenSerial) {
        app->sae_full_console = !app->sae_full_console;
        app->serial_follow_tail = true;
        app->serial_targets_hint = false;
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if((event->type == InputTypeLong || event->type == InputTypeShort) &&
       event->key == InputKeyOk && app->deauth_view_active && app->screen == ScreenSerial) {
        app->deauth_full_console = !app->deauth_full_console;
        app->serial_follow_tail = true;
        app->serial_targets_hint = false;
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if((event->type == InputTypeLong || event->type == InputTypeShort) &&
       event->key == InputKeyOk && app->deauth_guard_view_active) {
        app->deauth_guard_full_console = !app->deauth_guard_full_console;
        app->serial_follow_tail = true;
        app->serial_targets_hint = false;
        if(app->viewport) {
            view_port_update(app->viewport);
        }
        return;
    }

    if(event->type == InputTypeLong && event->key == InputKeyOk) {
        if(app->screen == ScreenGps) {
            app->screen = ScreenSerial;
            app->gps_console_mode = true;
            app->serial_follow_tail = true;
            simple_app_update_scroll(app);
            if(app->viewport) {
                view_port_update(app->viewport);
            }
            return;
        }
        if(app->screen == ScreenSerial && app->gps_console_mode) {
            app->screen = ScreenGps;
            app->gps_console_mode = false;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
            return;
        }
        if(app->blackout_view_active && app->screen == ScreenSerial) {
            app->blackout_full_console = !app->blackout_full_console;
            app->serial_follow_tail = true;
            app->serial_targets_hint = false;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
            return;
        }
        if(app->sniffer_dog_view_active && app->screen == ScreenSerial) {
            app->sniffer_dog_full_console = !app->sniffer_dog_full_console;
            app->serial_follow_tail = true;
            app->serial_targets_hint = false;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
            return;
        }
        if(app->scanner_view_active) {
            app->scanner_full_console = !app->scanner_full_console;
            app->serial_follow_tail = true;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
            return;
        }
        if(app->sniffer_view_active || app->sniffer_results_active) {
            app->sniffer_full_console = !app->sniffer_full_console;
            app->serial_follow_tail = true;
            app->serial_targets_hint = false;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
            return;
        }
        if(app->probe_results_active) {
            app->probe_full_console = !app->probe_full_console;
            app->serial_follow_tail = true;
            app->serial_targets_hint = false;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
            return;
        }
        if(app->bt_scan_view_active) {
            app->bt_scan_full_console = !app->bt_scan_full_console;
            if(app->bt_scan_full_console) {
                app->bt_scan_show_list = false;
            }
            app->serial_follow_tail = true;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
            return;
        }
        // Debug helper: when in BT locator monitor, long OK shows raw console
        if(app->bt_locator_mode && app->screen == ScreenSerial) {
            app->bt_scan_full_console = !app->bt_scan_full_console;
            if(app->bt_scan_full_console) {
                app->bt_scan_show_list = false;
            }
            app->serial_follow_tail = true;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
            return;
        }
        if(app->evil_twin_running && app->screen == ScreenSerial) {
            app->evil_twin_full_console = !app->evil_twin_full_console;
            app->serial_follow_tail = true;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
            return;
        }
        if(app->portal_running && app->screen == ScreenSerial) {
            app->portal_full_console = !app->portal_full_console;
            app->serial_follow_tail = true;
            if(app->viewport) {
                view_port_update(app->viewport);
            }
            return;
        }
    }

    bool allow_event = false;
    if(event->type == InputTypeShort) {
        allow_event = true;
    } else if(
        (event->type == InputTypeRepeat || event->type == InputTypeLong) &&
        simple_app_is_direction_key(event->key)) {
        allow_event = true;
    }

    if(!allow_event) return;

    if(event->key != InputKeyBack && app->exit_confirm_active) {
        app->exit_confirm_active = false;
    }

    switch(app->screen) {
    case ScreenMenu:
        simple_app_handle_menu_input(app, event->key);
        break;
    case ScreenSerial:
        simple_app_handle_serial_input(app, event->key);
        break;
    case ScreenResults:
        simple_app_handle_results_input(app, event->key);
        break;
    case ScreenSetupScanner:
        simple_app_handle_setup_scanner_input(app, event->key);
        break;
    case ScreenSetupScannerTiming:
        simple_app_handle_setup_scanner_timing_input(app, event->key);
        break;
    case ScreenSetupLed:
        simple_app_handle_setup_led_input(app, event->key);
        break;
    case ScreenSetupBoot:
        simple_app_handle_setup_boot_input(app, event->key);
        break;
    case ScreenSetupSdManager:
        simple_app_handle_setup_sd_manager_input(app, event->key);
        break;
    case ScreenHelpQr:
        simple_app_handle_help_qr_input(app, event->key);
        break;
    case ScreenSetupKarma:
        simple_app_handle_setup_karma_input(app, event->key);
        break;
    case ScreenConsole:
        simple_app_handle_console_input(app, event->key);
        break;
    case ScreenGps:
        simple_app_handle_gps_input(app, event->key);
        break;
    case ScreenPackageMonitor:
        simple_app_handle_package_monitor_input(app, event->key);
        break;
    case ScreenChannelView:
        simple_app_handle_channel_view_input(app, event->key);
        break;
    case ScreenConfirmExit:
        simple_app_handle_confirm_exit_input(app, event->key);
        break;
    case ScreenConfirmBlackout:
        simple_app_handle_confirm_blackout_input(app, event->key);
        break;
    case ScreenConfirmSnifferDos:
        simple_app_handle_confirm_sniffer_dos_input(app, event->key);
        break;
    case ScreenEvilTwinMenu:
        simple_app_handle_evil_twin_menu_input(app, event->key);
        break;
    case ScreenEvilTwinPassList:
        simple_app_handle_evil_twin_pass_list_input(app, event->key);
        break;
    case ScreenEvilTwinPassQr:
        simple_app_handle_evil_twin_pass_qr_input(app, event->key);
        break;
    case ScreenKarmaMenu:
        simple_app_handle_karma_menu_input(app, event->key);
        break;
    case ScreenPortalMenu:
        simple_app_handle_portal_menu_input(app, event->key);
        break;
    case ScreenBtLocatorList:
        simple_app_handle_bt_locator_input(app, event->key);
        break;
    case ScreenPasswords:
        simple_app_handle_passwords_input(app, event->key);
        break;
    case ScreenPasswordsSelect:
        simple_app_handle_passwords_select_input(app, event->key);
        break;
    default:
        simple_app_handle_results_input(app, event->key);
        break;
    }
}

static void simple_app_process_stream(SimpleApp* app) {
    if(!app || !app->rx_stream) return;

    uint8_t chunk[64];
    bool updated = false;

    while(true) {
        size_t received = furi_stream_buffer_receive(app->rx_stream, chunk, sizeof(chunk), 0);
        if(received == 0) break;

        // Detect pong in incoming chunk
        for(size_t i = 0; i + 3 < received; i++) {
            if(chunk[i] == 'p' && chunk[i + 1] == 'o' && chunk[i + 2] == 'n' &&
               chunk[i + 3] == 'g') {
                simple_app_handle_pong(app);
                break;
            }
        }

        simple_app_append_serial_data(app, chunk, received);
        updated = true;
    }

    if(updated && (app->screen == ScreenSerial || app->screen == ScreenConsole)) {
        view_port_update(app->viewport);
    }
    if(app->package_monitor_dirty && app->screen == ScreenPackageMonitor && app->viewport) {
        view_port_update(app->viewport);
    }
    if(app->channel_view_dirty && app->screen == ScreenChannelView && app->viewport) {
        view_port_update(app->viewport);
    }
}

static void
    simple_app_serial_irq(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    SimpleApp* app = context;
    if(!app || !app->rx_stream || !(event & FuriHalSerialRxEventData)) return;

    do {
        uint8_t byte = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(app->rx_stream, &byte, 1, 0);
        app->board_last_rx_tick = furi_get_tick();
    } while(furi_hal_serial_async_rx_available(handle));
}

int32_t Lab_C5_app(void* p) {
    UNUSED(p);

    if(!simple_app_usb_profile_ok()) {
        simple_app_show_usb_blocker();
        return 0;
    }

    // If SD is busy (e.g., mounted via qFlipper), exit immediately
    Storage* early_storage = furi_record_open(RECORD_STORAGE);
    if(early_storage) {
        if(storage_sd_status(early_storage) != FSE_OK) {
            furi_record_close(RECORD_STORAGE);
            return 0;
        }
        furi_record_close(RECORD_STORAGE);
    }

    const size_t free_heap = memmgr_get_free_heap();
    const size_t needed_heap = sizeof(SimpleApp) + 4096;
    if(free_heap < needed_heap) {
        simple_app_show_low_memory(free_heap, needed_heap);
        return 0;
    }

    SimpleApp* app = malloc(sizeof(SimpleApp));
    if(!app) {
        simple_app_show_low_memory(free_heap, needed_heap);
        return 0;
    }
    memset(app, 0, sizeof(SimpleApp));
    app->heap_start_free = memmgr_get_free_heap();
    FURI_LOG_I(
        TAG,
        "Heap after app malloc:%lu min:%lu",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_get_minimum_free_heap());
    app->deauth_guard_last_channel = -1;
    app->package_monitor_channel = PACKAGE_MONITOR_DEFAULT_CHANNEL;
    app->channel_view_band = ChannelViewBand24;
    simple_app_channel_view_reset(app);
    app->scanner_show_ssid = true;
    app->scanner_show_bssid = true;
    app->scanner_show_channel = true;
    app->scanner_show_security = true;
    app->scanner_show_power = true;
    app->scanner_show_band = true;
    app->scanner_show_vendor = false;
    app->vendor_scan_enabled = false;
    app->vendor_read_pending = false;
    app->vendor_read_length = 0;
    app->vendor_read_buffer[0] = '\0';
    app->scanner_min_power = SCAN_POWER_MIN_DBM;
    app->scanner_min_channel_time = SCANNER_CHANNEL_TIME_DEFAULT_MIN;
    app->scanner_max_channel_time = SCANNER_CHANNEL_TIME_DEFAULT_MAX;
    app->scanner_timing_index = 0;
    app->scanner_timing_min_pending = false;
    app->scanner_timing_max_pending = false;
    app->scanner_timing_read_length = 0;
    app->scanner_timing_read_buffer[0] = '\0';
    app->scanner_setup_index = 0;
    app->scanner_adjusting_power = false;
    app->otg_power_initial_state = furi_hal_power_is_otg_enabled();
    app->otg_power_enabled = app->otg_power_initial_state;
    app->backlight_enabled = true;
    app->gps_time_24h = true;
    app->gps_utc_offset_minutes = 0;
    app->gps_dst_enabled = false;
    app->gps_zda_tz_enabled = false;
    app->show_ram_overlay = false;
    app->debug_mark_enabled = false;
    app->debug_log_initialized = false;
    app->led_enabled = true;
    app->led_level = 10;
    app->led_setup_index = 0;
    app->board_ready_seen = false;
    app->board_sync_pending = true;
    app->board_ping_outstanding = false;
    app->board_last_ping_tick = 0;
    app->board_last_rx_tick = furi_get_tick();
    app->board_missing_shown = false;
    app->board_bootstrap_active = true;
    app->board_bootstrap_reboot_sent = false;
    app->board_ping_failures = 0;
    app->exit_confirm_active = false;
    app->exit_confirm_tick = 0;
    app->boot_short_enabled = false;
    app->boot_long_enabled = false;
    app->boot_short_command_index = 1; // start_sniffer_dog
    app->boot_long_command_index = 0; // start_blackout
    app->boot_setup_index = 0;
    app->boot_setup_long = false;
    app->scanner_view_offset = 0;
    app->karma_sniffer_duration_sec = 15;
    simple_app_update_karma_duration_label(app);
    simple_app_update_result_layout(app);
    simple_app_update_backlight_label(app);
    simple_app_update_ram_label(app);
    simple_app_update_debug_label(app);
    simple_app_update_led_label(app);
    simple_app_update_boot_labels(app);
    app->sd_folder_index = 0;
    app->sd_folder_offset = 0;
    app->sd_refresh_pending = false;
    simple_app_reset_sd_listing(app);
    if(sd_folder_option_count > 0) {
        simple_app_copy_sd_folder(app, &sd_folder_options[0]);
    }
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->menu_state = MenuStateSections;
    app->screen = ScreenMenu;
    app->serial_follow_tail = true;
    simple_app_reset_scan_results(app);
    simple_app_reset_blackout_status(app);
    simple_app_reset_deauth_status(app);
    simple_app_reset_handshake_status(app);
    simple_app_reset_sae_status(app);
    simple_app_reset_sniffer_dog_status(app);
    simple_app_reset_sniffer_status(app);
    simple_app_reset_sniffer_results(app);
    simple_app_reset_probe_results(app);
    simple_app_reset_portal_status(app);
    simple_app_reset_evil_twin_status(app);
    simple_app_reset_passwords_listing(app);
    app->last_input_tick = furi_get_tick();

    app->serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    if(!app->serial) {
        free(app);
        return 0;
    }

    furi_hal_serial_init(app->serial, 115200);

    app->serial_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!app->serial_mutex) {
        furi_hal_serial_deinit(app->serial);
        furi_hal_serial_control_release(app->serial);
        free(app);
        return 0;
    }

    app->rx_stream = furi_stream_buffer_alloc(UART_STREAM_SIZE, 1);
    if(!app->rx_stream) {
        furi_mutex_free(app->serial_mutex);
        furi_hal_serial_deinit(app->serial);
        furi_hal_serial_control_release(app->serial);
        free(app);
        return 0;
    }
    FURI_LOG_I(
        TAG,
        "Heap after rx_stream:%lu min:%lu",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_get_minimum_free_heap());

    furi_stream_buffer_reset(app->rx_stream);
    simple_app_reset_serial_log(app, "READY");

    {
        char line[96];
        snprintf(
            line,
            sizeof(line),
            "Heap start free:%lu min:%lu SimpleApp:%lu\n",
            (unsigned long)memmgr_get_free_heap(),
            (unsigned long)memmgr_get_minimum_free_heap(),
            (unsigned long)sizeof(SimpleApp));
        simple_app_append_serial_data(app, (const uint8_t*)line, strlen(line));
    }

    furi_hal_serial_async_rx_start(app->serial, simple_app_serial_irq, app, false);

    app->gui = furi_record_open(RECORD_GUI);
    app->viewport = view_port_alloc();
    view_port_draw_callback_set(app->viewport, simple_app_draw, app);
    view_port_input_callback_set(app->viewport, simple_app_input, app);
    gui_add_view_port(app->gui, app->viewport, GuiLayerFullscreen);
    FURI_LOG_I(
        TAG,
        "Heap after viewport:%lu min:%lu",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_get_minimum_free_heap());

    simple_app_load_config(app);
    app->vendor_scan_enabled = app->scanner_show_vendor;
    app->debug_log_initialized = false;
    if(app->debug_mark_enabled) {
        simple_app_reset_debug_log(app);
        app->debug_log_initialized = true;
    }
    simple_app_update_backlight_label(app);
    simple_app_update_ram_label(app);
    simple_app_update_debug_label(app);
    simple_app_update_led_label(app);
    simple_app_update_boot_labels(app);
    simple_app_apply_backlight(app);
    // Respect config value (loaded by simple_app_load_config) and apply it to HW
    simple_app_apply_otg_power(app);
    if(app->viewport) {
        view_port_update(app->viewport);
    }

    if(!simple_app_status_message_is_active(app)) {
        simple_app_show_status_message(app, "Board discovery\nSending ping...", 0, true);
    }
    simple_app_send_ping(app);
    if(app->viewport) {
        view_port_update(app->viewport);
    }

    while(!app->exit_app) {
        simple_app_process_stream(app);
        simple_app_update_karma_sniffer(app);
        simple_app_update_result_scroll(app);
        simple_app_update_overlay_title_scroll(app);
        simple_app_update_sd_scroll(app);
        simple_app_update_deauth_guard(app);
        simple_app_update_deauth_blink(app);
        simple_app_update_handshake_blink(app);
        simple_app_ping_watchdog(app);
        simple_app_usb_runtime_guard(app);

        if(app->portal_input_requested && !app->portal_input_active) {
            app->portal_input_requested = false;
            bool accepted = simple_app_portal_run_text_input(app);
            if(accepted) {
                app->portal_ssid_missing = false;
                if(app->portal_ssid[0] != '\0') {
                    simple_app_show_status_message(app, "SSID updated", 1000, true);
                    app->portal_menu_index = 1;
                } else {
                    simple_app_show_status_message(app, "SSID cleared", 1000, true);
                }
                simple_app_portal_sync_offset(app);
            }
            if(app->viewport) {
                view_port_update(app->viewport);
            }
        }

        if(app->sd_refresh_pending && !app->sd_listing_active &&
           app->screen == ScreenSetupSdManager) {
            uint32_t now = furi_get_tick();
            if(now >= app->sd_refresh_tick && sd_folder_option_count > 0 &&
               app->sd_folder_index < sd_folder_option_count) {
                simple_app_request_sd_listing(app, &sd_folder_options[app->sd_folder_index]);
            }
        }

        if(app->exit_confirm_active) {
            uint32_t now = furi_get_tick();
            if(now - app->exit_confirm_tick >= furi_ms_to_ticks(2000)) {
                app->exit_confirm_active = false;
            }
        }

        furi_delay_ms(20);
    }

    FURI_LOG_I(
        TAG,
        "Heap before exit prep:%lu min:%lu",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_get_minimum_free_heap());
    simple_app_prepare_exit(app);
    FURI_LOG_I(
        TAG,
        "Heap after exit prep:%lu min:%lu",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_get_minimum_free_heap());

    simple_app_save_config_if_dirty(app, NULL, false);

    gui_remove_view_port(app->gui, app->viewport);
    view_port_free(app->viewport);
    FURI_LOG_I(
        TAG,
        "Heap after viewport free:%lu min:%lu",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_get_minimum_free_heap());
    furi_record_close(RECORD_GUI);

    furi_hal_serial_async_rx_stop(app->serial);
    furi_stream_buffer_free(app->rx_stream);
    FURI_LOG_I(
        TAG,
        "Heap after stream free:%lu min:%lu",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_get_minimum_free_heap());
    furi_hal_serial_deinit(app->serial);
    furi_hal_serial_control_release(app->serial);
    furi_mutex_free(app->serial_mutex);
    FURI_LOG_I(
        TAG,
        "Heap after mutex free:%lu min:%lu",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_get_minimum_free_heap());
    if(app->notifications) {
        if(app->backlight_notification_enforced) {
            notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
            app->backlight_notification_enforced = false;
        }
        furi_record_close(RECORD_NOTIFICATION);
        app->notifications = NULL;
    }
    if(app->backlight_insomnia) {
        furi_hal_power_insomnia_exit();
        app->backlight_insomnia = false;
    }
    // Do not restore OTG state on exit: 5V setting is user-configurable and persisted via config.
    furi_hal_light_set(LightBacklight, BACKLIGHT_ON_LEVEL);
    simple_app_gps_state_free(app);
    simple_app_wardrive_state_free(app);
    free(app);

    return 0;
}
