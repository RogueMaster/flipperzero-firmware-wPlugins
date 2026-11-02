#include "scheduler_custom_file_types.h"
#include "scheduler_settings_io.h"
#include "scheduler_playlist.h"
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
#define KEY_STEP_MODE     "StepMode"
#define KEY_SCHEDULE_KIND "ScheduleType"
#define KEY_LINE_WAITS    "LineIntervals"
#define KEY_LINE_TIMES    "LineTimes"
#define KEY_START_LINE    "StartLine"

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

static void scheduler_settings_write_lines(FlipperFormat* ff, Scheduler* scheduler) {
    const uint8_t count = scheduler_get_list_count(scheduler);
    uint32_t* values = malloc(count * sizeof(uint32_t));

    for(uint8_t i = 0; i < count; ++i) {
        values[i] = scheduler_get_line_interval(scheduler, i);
    }
    flipper_format_write_uint32(ff, KEY_LINE_WAITS, values, count);

    for(uint8_t i = 0; i < count; ++i) {
        values[i] = scheduler_get_line_time(scheduler, i);
    }
    flipper_format_write_uint32(ff, KEY_LINE_TIMES, values, count);

    free(values);
}

// stored_count: line count the schedule was saved with, the playlist may have changed since
// version: file version, line times are minutes before version 3 and seconds since
static void scheduler_settings_read_lines(
    FlipperFormat* ff,
    Scheduler* scheduler,
    uint32_t stored_count,
    uint32_t version) {
    if(stored_count == 0 || stored_count > SCHEDULER_MAX_LINES) {
        return;
    }

    const uint8_t count = MIN((uint8_t)stored_count, scheduler_get_list_count(scheduler));
    uint32_t* values = malloc(stored_count * sizeof(uint32_t));

    if(flipper_format_read_uint32(ff, KEY_LINE_WAITS, values, stored_count)) {
        for(uint8_t i = 0; i < count; ++i) {
            scheduler_set_line_interval(scheduler, i, values[i]);
        }
    } else {
        FURI_LOG_W(TAG, "Missing/invalid %s", KEY_LINE_WAITS);
    }

    if(flipper_format_read_uint32(ff, KEY_LINE_TIMES, values, stored_count)) {
        for(uint8_t i = 0; i < count; ++i) {
            scheduler_set_line_time(scheduler, i, version < 3 ? values[i] * 60 : values[i]);
        }
    } else {
        FURI_LOG_W(TAG, "Missing/invalid %s", KEY_LINE_TIMES);
    }

    free(values);
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

        // Version 2: playlist stepping and per-line schedules
        v = scheduler_get_step_mode(app->scheduler);
        flipper_format_write_uint32(ff, KEY_STEP_MODE, &v, 1);

        v = scheduler_get_schedule_type(app->scheduler);
        flipper_format_write_uint32(ff, KEY_SCHEDULE_KIND, &v, 1);

        scheduler_settings_write_lines(ff, app->scheduler);

        // Optional, kept last so files saved without it still load
        v = scheduler_get_start_line(app->scheduler);
        flipper_format_write_uint32(ff, KEY_START_LINE, &v, 1);

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

        uint32_t ver = 0;
        flipper_format_read_uint32(ff, KEY_VERSION, &ver, 1);
        // Version 1 files have no playlist stepping, they load with defaults
        if(ver < 1 || ver > SCHEDULER_APP_SETTINGS_FILE_VERSION) {
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
                scheduler_set_embedded_timing(app->scheduler, false);
            } else {
                // If list_count missing, fallback to count 1
                if(list_count == 0) {
                    list_count = 1;
                }
                // Use the current playlist length in case it was edited after saving
                int actual_count =
                    scheduler_playlist_count(storage, furi_string_get_cstr(sched_file));
                if(actual_count <= 0 || actual_count > SCHEDULER_MAX_LINES) {
                    FURI_LOG_W(TAG, "Can't count playlist, using saved count");
                    actual_count = MIN(list_count, (uint32_t)SCHEDULER_MAX_LINES);
                }
                scheduler_set_file(
                    app->scheduler, furi_string_get_cstr(sched_file), (int8_t)actual_count);
                SchedulerPlaylistTiming playlist_timing;
                scheduler_set_embedded_timing(
                    app->scheduler,
                    scheduler_playlist_read_timing(
                        storage, furi_string_get_cstr(sched_file), &playlist_timing) &&
                        playlist_timing.has_timing);
            }

            scheduler_set_step_mode(app->scheduler, SchedulerStepModeAll);
            scheduler_set_schedule_type(app->scheduler, SchedulerScheduleUniform);
            scheduler_set_start_line(app->scheduler, SchedulerStartLineFirst);
            if(ver >= 2) {
                uint32_t v = 0;
                if(flipper_format_read_uint32(ff, KEY_STEP_MODE, &v, 1)) {
                    scheduler_set_step_mode(app->scheduler, (SchedulerStepMode)v);
                }
                if(flipper_format_read_uint32(ff, KEY_SCHEDULE_KIND, &v, 1)) {
                    scheduler_set_schedule_type(app->scheduler, (SchedulerScheduleType)v);
                }
                scheduler_settings_read_lines(ff, app->scheduler, list_count, ver);
                if(flipper_format_read_uint32(ff, KEY_START_LINE, &v, 1)) {
                    scheduler_set_start_line(app->scheduler, (SchedulerStartLine)v);
                }
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
