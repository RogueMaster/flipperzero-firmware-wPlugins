#include "scheduler_custom_file_types.h"
#include "scheduler_settings_io.h"
#include "src/scheduler_app_i.h"

#include <flipper_format/flipper_format.h>
#include <furi.h>

#define TAG "SchedulerSettingsIO"

// Settings file keys
#define KEY_FILETYPE      "FileType"
#define KEY_VERSION       "FileVersion"
#define KEY_INTERVAL_SEC  "IntervalSec"
#define KEY_TIMING_MODE   "TimingModeIdx"
#define KEY_TXCOUNT_IDX   "TxCountIdx"
#define KEY_TX_MODE_IDX   "TxModeIdx"
#define KEY_TX_DELAY_MS   "TxDelayMs"
#define KEY_RADIO_IDX     "RadioIdx"
#define KEY_SCHEDULE_FILE "TxFile"
#define KEY_SCHEDULE_TYPE "TxFileType"
#define KEY_LIST_COUNT    "ListCount"

typedef struct {
    uint32_t interval_seconds;
    uint16_t tx_delay_ms;
    uint8_t timing_mode;
    uint8_t txcount_idx;
    uint8_t tx_mode_idx;
    uint8_t radio_idx;
} SchedulerSettingsFields;

const char* path_basename(const char* path) {
    const char* slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

bool scheduler_settings_save_to_path(SchedulerApp* app, const char* full_path) {
    furi_assert(app);

    bool ok = false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);

    do {
        // true if created or already exists
        if(!storage_common_mkdir(storage, SCHEDULER_APP_FOLDER)) {
            FURI_LOG_E(TAG, "Failed to create/open dir: %s", SCHEDULER_APP_FOLDER);
            dialog_message_show_storage_error(app->dialogs, "Failed to create/open directory");
            break;
        }

        if(!flipper_format_file_open_always(ff, full_path)) {
            FuriString* message =
                furi_string_alloc_set_str("Failed to open settings file for write: ");
            furi_string_cat_str(message, full_path);
            dialog_message_show_storage_error(app->dialogs, furi_string_get_cstr(message));
            FURI_LOG_E(TAG, furi_string_get_cstr(message));
            furi_string_free(message);
            break;
        }

        // Header
        flipper_format_write_string_cstr(ff, KEY_FILETYPE, SCHEDULER_APP_SETTINGS_FILE_TYPE);
        uint32_t ver = SCHEDULER_APP_SETTINGS_FILE_VERSION;
        flipper_format_write_uint32(ff, KEY_VERSION, &ver, 1);

        // Options
        uint32_t v = 0;
        v = scheduler_get_interval_seconds(app->scheduler);
        flipper_format_write_uint32(ff, KEY_INTERVAL_SEC, &v, 1);

        v = scheduler_get_timing_mode(app->scheduler);
        flipper_format_write_uint32(ff, KEY_TIMING_MODE, &v, 1);

        v = scheduler_get_tx_count(app->scheduler);
        flipper_format_write_uint32(ff, KEY_TXCOUNT_IDX, &v, 1);

        v = scheduler_get_tx_mode(app->scheduler);
        flipper_format_write_uint32(ff, KEY_TX_MODE_IDX, &v, 1);

        v = scheduler_get_tx_delay_ms(app->scheduler);
        flipper_format_write_uint32(ff, KEY_TX_DELAY_MS, &v, 1);

        v = scheduler_get_radio(app->scheduler);
        flipper_format_write_uint32(ff, KEY_RADIO_IDX, &v, 1);

        flipper_format_write_string(ff, KEY_SCHEDULE_FILE, app->tx_file_path);

        v = scheduler_get_file_type(app->scheduler);
        flipper_format_write_uint32(ff, KEY_SCHEDULE_TYPE, &v, 1);

        v = scheduler_get_list_count(app->scheduler);
        flipper_format_write_uint32(ff, KEY_LIST_COUNT, &v, 1);

        ok = true;
    } while(false);

    flipper_format_file_close(ff);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

static bool read_u32(FlipperFormat* ff, const char* key, uint32_t* out) {
    uint32_t tmp = 0;
    if(!flipper_format_read_uint32(ff, key, &tmp, 1)) {
        FURI_LOG_E(TAG, "Missing/invalid %s", key);
        return false;
    }
    *out = tmp;
    return true;
}

static bool read_u16(FlipperFormat* ff, const char* key, uint16_t* out) {
    uint32_t tmp = 0;
    if(!read_u32(ff, key, &tmp)) {
        return false;
    }
    *out = (uint16_t)tmp;
    return true;
}

static bool read_u8(FlipperFormat* ff, const char* key, uint8_t* out) {
    uint32_t tmp = 0;
    if(!read_u32(ff, key, &tmp)) {
        return false;
    }
    *out = (uint8_t)tmp;
    return true;
}

static bool scheduler_settings_read_required(FlipperFormat* ff, SchedulerSettingsFields* s) {
    if(!read_u32(ff, KEY_INTERVAL_SEC, &s->interval_seconds)) return false;
    if(!read_u8(ff, KEY_TIMING_MODE, &s->timing_mode)) return false;
    if(!read_u8(ff, KEY_TXCOUNT_IDX, &s->txcount_idx)) return false;
    if(!read_u8(ff, KEY_TX_MODE_IDX, &s->tx_mode_idx)) return false;
    if(!read_u16(ff, KEY_TX_DELAY_MS, &s->tx_delay_ms)) return false;
    if(!read_u8(ff, KEY_RADIO_IDX, &s->radio_idx)) return false;

    return true;
}

bool scheduler_settings_load_from_path(SchedulerApp* app, const char* full_path) {
    furi_assert(app);
    furi_assert(full_path);

    bool ok = false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);

    FuriString* sched_file = furi_string_alloc();
    FuriString* filetype = furi_string_alloc();

    do {
        if(!flipper_format_file_open_existing(ff, full_path)) {
            FURI_LOG_E(TAG, "Open failed: %s", full_path);
            break;
        }

        if(!flipper_format_read_string(ff, KEY_FILETYPE, filetype)) {
            FURI_LOG_E(TAG, "Missing Filetype");
            break;
        }
        if(strcmp(furi_string_get_cstr(filetype), SCHEDULER_APP_SETTINGS_FILE_TYPE) != 0) {
            FURI_LOG_E(TAG, "Wrong Filetype: %s", furi_string_get_cstr(filetype));
            break;
        }

        uint32_t ver;
        flipper_format_read_uint32(ff, KEY_VERSION, &ver, 1);
        if(ver != SCHEDULER_APP_SETTINGS_FILE_VERSION) {
            FURI_LOG_E(TAG, "Unsupported Version: %lu", (unsigned long)ver);
            break;
        }

        // Always fail on load errors, since user specifically wants certain settings from file
        SchedulerSettingsFields settings = {0};
        if(!scheduler_settings_read_required(ff, &settings)) {
            break;
        }

        scheduler_set_interval_seconds(app->scheduler, settings.interval_seconds);
        scheduler_set_timing_mode(app->scheduler, settings.timing_mode);
        scheduler_set_tx_count(app->scheduler, settings.txcount_idx);
        scheduler_set_tx_mode(app->scheduler, (SchedulerTxMode)settings.tx_mode_idx);
        scheduler_set_tx_delay_ms(app->scheduler, settings.tx_delay_ms);
        scheduler_set_radio(app->scheduler, app->ext_radio_present ? settings.radio_idx : 0);

        if(flipper_format_read_string(ff, KEY_SCHEDULE_FILE, sched_file)) {
            furi_string_set(app->tx_file_path, furi_string_get_cstr(sched_file));

            uint32_t file_type = SchedulerFileTypeSingle;
            flipper_format_read_uint32(ff, KEY_SCHEDULE_TYPE, &file_type, 1);

            uint32_t list_count = 1;
            flipper_format_read_uint32(ff, KEY_LIST_COUNT, &list_count, 1);

            if((FileTxType)file_type == SchedulerFileTypeSingle) {
                scheduler_set_file(app->scheduler, furi_string_get_cstr(sched_file), 0);
            } else {
                // If list_count missing, fallback to count 1
                if(list_count == 0) {
                    list_count = 1;
                }
                scheduler_set_file(
                    app->scheduler, furi_string_get_cstr(sched_file), (int8_t)list_count);
            }
        }
        // TODO: Error here too

        ok = true;
    } while(false);

    flipper_format_file_close(ff);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);

    furi_string_free(filetype);
    furi_string_free(sched_file);

    return ok;
}
