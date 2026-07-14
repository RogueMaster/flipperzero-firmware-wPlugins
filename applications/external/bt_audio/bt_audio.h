#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/text_box.h>
#include <gui/modules/variable_item_list.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <stdio.h>
#if __has_include(<cfw/cfw.h>)
#include <cfw/cfw.h>
#endif

#define TAG "BtAudio"

#if __has_include(<cfw/cfw.h>)
#define BT_AUDIO_UART_CH (cfw_settings.uart_esp_channel)
#else
#define BT_AUDIO_UART_CH (FuriHalSerialIdUsart)
#endif
#define BT_AUDIO_BAUDRATE            (115200)
#define BT_AUDIO_RX_BUF_SIZE         (256)
#define BT_AUDIO_MAX_DEVICES         (10)
#define BT_AUDIO_DEVICE_NAME_LEN     (64)
#define BT_AUDIO_MAC_LEN             (18)
#define BT_AUDIO_CMD_BUF_SIZE        (32)
#define BT_AUDIO_FILENAME_LEN        (128)
#define BT_AUDIO_MAX_FAVORITES       (40) // Maximum number of favorite tracks
#define BT_AUDIO_BT_NAME_LEN         (32) // Max Bluetooth device name length
#define BT_AUDIO_DEVICE_HISTORY_SIZE (10) // Maximum number of device history entries
#define BT_AUDIO_WIFI_SSID_LEN       (64) // Max WiFi SSID length
#define BT_AUDIO_WIFI_PASS_LEN       (64) // Max WiFi password length
#define BT_AUDIO_STREAM_URL_LEN      (256) // Max stream URL length

// Config file path
#define BT_AUDIO_CONFIG_DIR          "/ext/apps_data/bt_audio"
#define BT_AUDIO_CONFIG_FILE         "/ext/apps_data/bt_audio/config.txt"
#define BT_AUDIO_FAVORITES_FILE      "/ext/apps_data/bt_audio/favorites.txt"
#define BT_AUDIO_DEVICE_HISTORY_FILE "/ext/apps_data/bt_audio/device_history.txt"
#define BT_AUDIO_WIFI_CONFIG_FILE    "/ext/apps_data/bt_audio/wifi.config"

// Device history entry - stores MAC and friendly name for quick reconnect
typedef struct {
    char mac[BT_AUDIO_MAC_LEN];
    char name[BT_AUDIO_DEVICE_NAME_LEN];
} BtAudioDeviceEntry;

// SD card source options
typedef enum {
    BtAudioSdSourceFlipper = 0,
    BtAudioSdSourceESP32,
    BtAudioSdSourceCount,
} BtAudioSdSource;

// BT TX power levels
typedef enum {
    BtAudioTxPowerLow = 0, // -12 dBm
    BtAudioTxPowerMedium, // 0 dBm
    BtAudioTxPowerHigh, // +6 dBm
    BtAudioTxPowerMax, // +9 dBm (default)
    BtAudioTxPowerCount,
} BtAudioTxPower;

// EQ preset levels (0=Off, 1-5 = adjustment levels)
typedef enum {
    BtAudioEqOff = 0,
    BtAudioEqLevel1,
    BtAudioEqLevel2,
    BtAudioEqLevel3,
    BtAudioEqLevel4,
    BtAudioEqLevel5,
    BtAudioEqLevelCount,
} BtAudioEqLevel;

// WiFi configuration structure (separate from main config)
typedef struct {
    bool wifi_enabled; // Enable WiFi streaming feature
    char ssid[BT_AUDIO_WIFI_SSID_LEN]; // WiFi network SSID
    char password[BT_AUDIO_WIFI_PASS_LEN]; // WiFi network password
    char stream_url[BT_AUDIO_STREAM_URL_LEN]; // Stream URL (http/https)
} BtAudioWifiConfig;

// Configuration structure
typedef struct {
    BtAudioSdSource sd_source;
    BtAudioTxPower tx_power;
    bool enable_5v_gpio;
    bool background_mode; // Keep audio playing when app exits
    bool keep_backlight_on; // Keep screen backlight always on
    bool shuffle_mode; // Shuffle/random play mode
    bool continuous_play; // When playing from file browser: ON = play next files in directory, OFF = repeat selected file
    uint8_t initial_volume; // Initial volume level (0-127), default 96 (~75%)
    int8_t eq_bass; // EQ bass adjustment (-10 to +10 dB)
    int8_t eq_mid; // EQ mid adjustment (-10 to +10 dB)
    int8_t eq_treble; // EQ treble adjustment (-10 to +10 dB)
    char last_connected_mac[BT_AUDIO_MAC_LEN];
    char bt_device_name[BT_AUDIO_BT_NAME_LEN]; // Custom Bluetooth device name
} BtAudioConfig;

typedef struct BtAudio BtAudio;
typedef struct BtAudioUart BtAudioUart;

// Model for player view - holds reference to app
typedef struct {
    BtAudio* app;
} PlayerViewModel;

typedef enum {
    BtAudioViewSubmenu,
    BtAudioViewTextBox,
    BtAudioViewWidget,
    BtAudioViewQRCode,
    BtAudioViewVariableItemList,
    BtAudioViewPlayer,
} BtAudioView;

typedef enum {
    // Start at 100 to avoid collision with submenu indices (0-10)
    BtAudioEventStartScan = 100,
    BtAudioEventConnect,
    BtAudioEventConnectReady, // Audio pipeline ready after connection
    BtAudioEventPlayTone,
    BtAudioEventShowQR,
    BtAudioEventFirmwareOK,
    BtAudioEventFirmwareError,
    BtAudioEventScanTimeout,
    BtAudioEventDiagComplete,
    BtAudioEventDeviceFound,
    BtAudioEventConnectTimeout,
    BtAudioEventConnectFailed,
    BtAudioEventPlayComplete,
    BtAudioEventDisconnected,
    BtAudioEventFileChanged,
    BtAudioEventPaused,
    BtAudioEventResumed,
    BtAudioEventScrollTick,
    BtAudioEventStatusComplete, // STATUS response complete - rebuild menu if needed
    BtAudioEventReconnectSuccess, // Reconnection successful (from Reconnect menu)
    BtAudioEventReconnectFailed, // Reconnection failed (from Reconnect menu)
} BtAudioEvent;

struct BtAudio {
    Gui* gui;
    NotificationApp* notifications;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    Submenu* submenu;
    TextBox* text_box;
    Widget* widget;
    VariableItemList* variable_item_list;
    BtAudioUart* uart;
    FuriString* text_box_store;
    char device_list[BT_AUDIO_MAX_DEVICES][BT_AUDIO_DEVICE_NAME_LEN];
    uint8_t device_count;
    int8_t selected_device;
    bool esp32_present;
    bool firmware_check_done;
    View* qr_view;
    View* player_view; // MP3 player view
    FuriTimer* scan_timer;
    FuriTimer* version_timer;
    FuriTimer* connect_timer; // Timer for connection timeout
    FuriTimer* init_timer; // Timer for audio pipeline initialization delay
    FuriTimer* play_timer; // Timer for play completion timeout
    FuriTimer* scroll_timer; // Timer for filename scrolling
    char firmware_response[BT_AUDIO_DEVICE_NAME_LEN];
    char diag_buffer[512]; // Buffer for diagnostic output
    char current_filename[BT_AUDIO_FILENAME_LEN]; // Current playing filename
    bool uart_rx_active; // Track if UART is receiving data
    uint32_t last_rx_time; // Time of last UART RX
    bool is_connected; // Track if BT device is connected
    bool is_playing; // Track if audio is playing
    bool is_paused; // Track if playback is paused
    bool reconnect_in_progress; // Track if reconnection is in progress (ignore disconnect during this)
    uint8_t connection_retry_count; // Track connection retry attempts (for improved reliability)
    int scroll_offset; // Scroll offset for long filenames
    uint32_t last_left_press; // Time of last left button press (for double-tap detection)
    bool button_held; // Track if a button is being held
    bool playing_favorites; // Track if playing favorites playlist
    bool current_is_favorite; // Track if current track is a favorite
    char favorites[BT_AUDIO_MAX_FAVORITES][BT_AUDIO_FILENAME_LEN]; // Favorite track filenames
    uint8_t favorites_count; // Number of favorites
    BtAudioDeviceEntry device_history[BT_AUDIO_DEVICE_HISTORY_SIZE]; // Last 10 connected devices
    uint8_t device_history_count; // Number of devices in history
    BtAudioConfig config; // Configuration settings
    BtAudioWifiConfig wifi_config; // WiFi configuration settings
};

static inline void bt_audio_set_firmware_response(BtAudio* app, const char* data) {
    if(!app || !data) return;
    int written = snprintf(app->firmware_response, sizeof(app->firmware_response), "%s", data);
    if(written >= (int)sizeof(app->firmware_response)) {
        FURI_LOG_W(TAG, "Firmware response truncated");
    }
}

// UART functions
BtAudioUart* bt_audio_uart_init(BtAudio* app);
void bt_audio_uart_free(BtAudioUart* uart);
void bt_audio_uart_tx(BtAudioUart* uart, const char* data);
void bt_audio_uart_set_rx_callback(
    BtAudioUart* uart,
    void (*callback)(const char* data, size_t len, void* context));

// Config functions
void bt_audio_config_load(BtAudio* app);
void bt_audio_config_save(BtAudio* app);

// WiFi config functions
void bt_audio_wifi_config_load(BtAudio* app);
void bt_audio_wifi_config_save(BtAudio* app);

// Favorites functions
void bt_audio_favorites_load(BtAudio* app);
void bt_audio_favorites_save(BtAudio* app);
bool bt_audio_is_favorite(BtAudio* app, const char* filename);
bool bt_audio_add_favorite(BtAudio* app, const char* filename);
bool bt_audio_remove_favorite(BtAudio* app, const char* filename);
bool bt_audio_toggle_favorite(BtAudio* app, const char* filename);

// Device history functions
void bt_audio_device_history_load(BtAudio* app);
void bt_audio_device_history_save(BtAudio* app);
void bt_audio_device_history_add(BtAudio* app, const char* mac, const char* name);

// Flipper SD streaming functions
// Streams an MP3 file from Flipper's SD card to ESP32 over UART using base64 encoding
bool bt_audio_stream_flipper_sd_file(BtAudio* app, const char* filepath);
// Stops any in-progress Flipper SD streaming
void bt_audio_stop_flipper_stream(BtAudio* app);

// Scene functions
#include "scenes/bt_audio_scene.h"
