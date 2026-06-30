// settings_def.h
#pragma once
#include <stdint.h>
#include <stdbool.h>

// Settings keys - add new settings here
typedef enum {
    SETTING_RGB_MODE,
    SETTING_CHANNEL_HOP_DELAY,
    SETTING_ENABLE_CHANNEL_HOPPING,
    SETTING_ENABLE_RANDOM_BLE_MAC,
    SETTING_STOP_ON_BACK,
    SETTING_ENABLE_FILTERING, // Keep for settings file compatibility
    SETTING_VIEW_LOGS_FROM_START,
    SETTING_SHOW_INFO,
    SETTING_REBOOT_ESP,
    SETTING_CLEAR_LOGS,
    SETTING_CLEAR_NVS,
    SETTING_CLEAR_PCAPS,
    SETTING_CLEAR_WARDRIVE,
    SETTING_DISABLE_ESP_CHECK,
    SETTINGS_COUNT
} SettingKey;

// Settings operations result
typedef enum {
    SETTINGS_OK,
    SETTINGS_INVALID_VALUE,
    SETTINGS_FILE_ERROR,
    SETTINGS_PARSE_ERROR
} SettingsResult;

// Setting value definitions
typedef enum {
    RGB_MODE_STEALTH,
    RGB_MODE_NORMAL,
    RGB_MODE_RAINBOW,
    RGB_MODE_COUNT
} RGBMode;

typedef enum {
    CHANNEL_HOP_500MS,
    CHANNEL_HOP_1000MS,
    CHANNEL_HOP_2000MS,
    CHANNEL_HOP_3000MS,
    CHANNEL_HOP_4000MS,
    CHANNEL_HOP_COUNT
} ChannelHopDelay;

typedef struct {
    uint8_t rgb_mode_index;
    uint8_t channel_hop_delay_index;
    uint8_t enable_channel_hopping_index;
    uint8_t enable_random_ble_mac_index;
    uint8_t stop_on_back_index;
    uint8_t enable_filtering_index;
    uint8_t view_logs_from_start_index;
    uint8_t reboot_esp_index;
    uint8_t clear_logs_index;
    uint8_t clear_nvs_index;
    uint8_t disable_esp_check_index;
} Settings;

// Add this to settings_def.h
typedef struct {
    const char* name;
    const char* command;
    void (*callback)(void* context); // Function pointer for the action
} SettingAction;

typedef struct {
    const char* name;
    union {
        struct {
            uint8_t max_value;
            const char* const* value_names;
            const char* uart_command;
        } setting;
        SettingAction action;
    } data;
    bool is_action;
} SettingMetadata;

// Value name arrays
extern const char* const SETTING_VALUE_NAMES_RGB_MODE[];
extern const char* const SETTING_VALUE_NAMES_CHANNEL_HOP[];
extern const char* const SETTING_VALUE_NAMES_BOOL[];
extern const char* const SETTING_VALUE_NAMES_ACTION[];

// Function declarations
const SettingMetadata* settings_get_metadata(SettingKey key);
bool setting_is_visible(SettingKey key);

// Common definitions
#define GHOST_ESP_APP_FOLDER          "/ext/apps_data/ghost_esp"
#define GHOST_ESP_APP_FOLDER_PCAPS    "/ext/apps_data/ghost_esp/pcaps"
#define GHOST_ESP_APP_FOLDER_WARDRIVE "/ext/apps_data/ghost_esp/wardrive"
#define GHOST_ESP_APP_FOLDER_LOGS     "/ext/apps_data/ghost_esp/logs"
#define GHOST_ESP_APP_SETTINGS_FILE   "/ext/apps_data/ghost_esp/settings.ini"

#define SETTINGS_HEADER_MAGIC 0xDEADBEEF
#define SETTINGS_FILE_VERSION 1

// SettingsHeader structure
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t settings_count;
} SettingsHeader;
