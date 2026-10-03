#pragma once

#include <furi_hal.h>
#include <stdint.h>
#include <stdbool.h>
#include <storage/storage.h>
#include <lib/subghz/types.h>

// 1 = "AM650"
// "AM270", "AM650", "FM238", "FM12K", "FM476",
#define SUBGHZ_LAST_SETTING_DEFAULT_PRESET    1
#define SUBGHZ_LAST_SETTING_DEFAULT_FREQUENCY 433920000

typedef struct {
    uint32_t frequency;
    uint32_t preset_index; // AKA Modulation
    bool protocol_file_names;
    bool enable_hopping;
    uint32_t ignore_filter;
    uint32_t filter;
    float rssi;
    bool delete_old_signals;
    uint32_t gps_protocol; // SubGhzGpsProtocol
    uint32_t gps_baudrate;
    uint32_t gps_pins; // SubGhzGpsPins
    bool remove_duplicates;
    uint32_t repeater_state;
    bool enable_sound;
    bool autosave;
    float hopping_threshold;
    uint8_t tx_power;
} SubGhzLastSettings;

SubGhzLastSettings* subghz_wardriving_last_settings_alloc(void);

void subghz_wardriving_last_settings_free(SubGhzLastSettings* instance);

void subghz_wardriving_last_settings_load(SubGhzLastSettings* instance, size_t preset_count);

bool subghz_wardriving_last_settings_save(SubGhzLastSettings* instance);
