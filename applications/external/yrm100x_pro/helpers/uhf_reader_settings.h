#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <storage/storage.h>

#define UHF_READER_MIN_POWER_DBM 15U
#define UHF_READER_MAX_POWER_DBM 26U

#define UHF_READER_TAG_PROFILE_GENERIC  0U
#define UHF_READER_TAG_PROFILE_MONZA4QT 1U
#define UHF_READER_TAG_PROFILE_COUNT    2U

#define UHF_READER_FULL_DUMP_ATTEMPTS_MIN     1U
#define UHF_READER_FULL_DUMP_ATTEMPTS_DEFAULT 4U
#define UHF_READER_FULL_DUMP_ATTEMPTS_MAX     10U

#define UHF_READER_CLONE_ATTEMPTS_MIN     1U
#define UHF_READER_CLONE_ATTEMPTS_DEFAULT 5U
#define UHF_READER_CLONE_ATTEMPTS_MAX     5U

#define UHF_READER_FOREVER_DELAY_MIN_SEC     1U
#define UHF_READER_FOREVER_DELAY_DEFAULT_SEC 2U
#define UHF_READER_FOREVER_DELAY_MAX_SEC     60U

typedef struct {
    bool reader_profile_initialized;
    uint8_t save_on_write_index;
    uint8_t region_index;
    uint8_t power_dbm;
    uint8_t session_index;
    uint8_t target_index;
    uint32_t default_access_password;
    uint8_t multi_full_dump_index;
    uint8_t auto_save_multi_index;
    uint8_t full_dump_attempts;
    uint8_t clone_attempts;
    uint8_t forever_delay_seconds;
    bool sound_enabled;
    bool vibration_enabled;
    uint8_t tag_profile_index;
} UHFReaderSettings;

void uhf_reader_settings_set_defaults(UHFReaderSettings* settings);
bool uhf_reader_settings_load(Storage* storage, UHFReaderSettings* settings);
bool uhf_reader_settings_save(Storage* storage, const UHFReaderSettings* settings);
