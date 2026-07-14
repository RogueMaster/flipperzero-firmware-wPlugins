#pragma once

#include <stdlib.h>
#include <string.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format_i.h>

// Settings
#define VK_THERMO_SETTINGS_FILE_VERSION   1
#define CONFIG_FILE_DIRECTORY_PATH        EXT_PATH("apps_data/vk_thermo")
#define VK_THERMO_SETTINGS_SAVE_PATH      CONFIG_FILE_DIRECTORY_PATH "/vk_thermo.conf"
#define VK_THERMO_SETTINGS_SAVE_PATH_TMP  VK_THERMO_SETTINGS_SAVE_PATH ".tmp"
#define VK_THERMO_SETTINGS_HEADER         "VkThermo Settings"
#define VK_THERMO_SETTINGS_KEY_HAPTIC     "Haptic"
#define VK_THERMO_SETTINGS_KEY_LED        "Led"
#define VK_THERMO_SETTINGS_KEY_SPEAKER    "Speaker"
#define VK_THERMO_SETTINGS_KEY_TEMP_UNIT  "TempUnit"
#define VK_THERMO_SETTINGS_KEY_EH_TIMEOUT "EhTimeout"
#define VK_THERMO_SETTINGS_KEY_DEBUG      "Debug"

// Log storage
#define VK_THERMO_LOG_MAX_ENTRIES 50
#define VK_THERMO_CSV_LEGACY_PATH CONFIG_FILE_DIRECTORY_PATH "/readings.csv"
#define VK_THERMO_UID_LENGTH      8

typedef struct {
    uint8_t uid[VK_THERMO_UID_LENGTH]; // ISO15693 UID is 8 bytes
    uint32_t timestamp;
    float temperature_celsius;
    bool valid;
} VkThermoLogEntry;

typedef struct {
    VkThermoLogEntry entries[VK_THERMO_LOG_MAX_ENTRIES];
    uint8_t count;
    uint8_t head; // Circular buffer head
} VkThermoLog;

// Settings functions
void vk_thermo_save_settings(void* context);
void vk_thermo_read_settings(void* context);

// Log functions
void vk_thermo_log_init(VkThermoLog* log);
void vk_thermo_log_add_entry(VkThermoLog* log, const uint8_t* uid, float temperature);
void vk_thermo_log_clear(VkThermoLog* log);
void vk_thermo_log_save_csv(VkThermoLog* log);
void vk_thermo_log_load_csv(VkThermoLog* log);
void vk_thermo_log_delete_csv(void);

// Stats functions
float vk_thermo_log_get_min(VkThermoLog* log);
float vk_thermo_log_get_max(VkThermoLog* log);
float vk_thermo_log_get_avg(VkThermoLog* log);
float vk_thermo_log_get_min_for_uid(VkThermoLog* log, const uint8_t* uid);
float vk_thermo_log_get_max_for_uid(VkThermoLog* log, const uint8_t* uid);

// UID helper functions
void vk_thermo_uid_to_string(const uint8_t* uid, char* str, size_t str_len);
void vk_thermo_uid_to_short_string(const uint8_t* uid, char* str, size_t str_len);

// Temperature conversion
float vk_thermo_celsius_to_fahrenheit(float celsius);
float vk_thermo_celsius_to_kelvin(float celsius);
