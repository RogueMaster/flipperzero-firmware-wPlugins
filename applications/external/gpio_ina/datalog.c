
/*
 * This file is part of the INA Meter application for Flipper Zero (https://github.com/cepetr/flipper-tina).
  *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <furi/furi.h>
#include <furi_hal.h>
#include <storage/storage.h>

#include "datalog.h"
#include "pipe.h"
#include "app_common.h"

// Path to the datalog files
#define DATALOG_DATA_PATH_PREFIX STORAGE_APP_DATA_PATH_PREFIX "/logs"

struct Datalog {
    Storage* storage;
    File* file;
    FuriString* file_name;
    FuriThread* thread;
    PipeSideBundle pipe;

    // Flag to stop the worker thread
    volatile bool stopped;
    // Unix timestamp of the first record
    uint32_t start_time;
    uint32_t start_ticks;
    // Number of records
    uint32_t record_count;
};

#define DATALOG_WORKER_STOP (1 << 0)

static int32_t datalog_worker(void* context) {
    Datalog* log = (Datalog*)context;
    while(!log->stopped) {
        uint8_t buffer[512];
        size_t nrecved = pipe_receive(log->pipe.bobs_side, buffer, sizeof(buffer));
        if(nrecved > 0) {
            size_t nwritten = storage_file_write(log->file, buffer, nrecved);
            if(nrecved != nwritten) {
                FURI_LOG_D(
                    TAG,
                    "Failed to write %d bytes to %s",
                    nrecved,
                    furi_string_get_cstr(log->file_name));
            }
        }
    }
    return 0;
}

Datalog* datalog_open(Storage* storage, const char* file_name) {
    Datalog* log = malloc(sizeof(Datalog));
    memset(log, 0, sizeof(Datalog));
    log->storage = storage;
    log->start_time = furi_hal_rtc_get_timestamp();

    FURI_LOG_I(TAG, "Opening datalog file...");

    log->file_name = furi_string_alloc_set(file_name);
    furi_check(log->file_name != NULL, "Failed to allocate datalog name");

    storage_common_mkdir(storage, DATALOG_DATA_PATH_PREFIX);

    log->file = storage_file_alloc(log->storage);
    furi_check(log->file != NULL, "Failed to allocate file");

    if(!storage_file_open(
           log->file, furi_string_get_cstr(log->file_name), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_E(TAG, "Failed to open datalog file %s", furi_string_get_cstr(log->file_name));
        storage_file_free(log->file);
        free(log);
        return NULL;
    }

    log->thread = furi_thread_alloc();
    furi_check(log->file != NULL, "Failed to allocate thread");

    log->pipe = pipe_alloc(1024, 1);
    furi_check(log->pipe.alices_side != NULL, "Failed to allocate pipe");
    furi_check(log->pipe.bobs_side != NULL, "Failed to allocate pipe");

    furi_thread_set_name(log->thread, "DATALOG");
    furi_thread_set_stack_size(log->thread, 4096);
    furi_thread_set_context(log->thread, log);
    furi_thread_set_callback(log->thread, datalog_worker);
    furi_thread_start(log->thread);

    FURI_LOG_I(TAG, "Datalog file opened");

    return log;
}

void datalog_close(Datalog* log) {
    if(log != NULL) {
        FURI_LOG_I(TAG, "Closing datalog file...");
        log->stopped = true;
        pipe_free(log->pipe.alices_side);
        furi_thread_join(log->thread);
        furi_thread_free(log->thread);
        storage_file_close(log->file);
        storage_file_free(log->file);
        pipe_free(log->pipe.bobs_side);
        furi_string_free(log->file_name);
        free(log);
    }
}

const FuriString* datalog_get_file_name(Datalog* log) {
    return log->file_name;
}

uint32_t datalog_get_duration(const Datalog* log) {
    return furi_hal_rtc_get_timestamp() - log->start_time;
}

uint32_t datalog_get_record_count(const Datalog* log) {
    return log->record_count;
}

size_t datalog_get_file_size(Datalog* log) {
    return storage_file_size(log->file);
}

static void datalog_append_string(Datalog* log, const char* str) {
    size_t len = strlen(str);
    pipe_send(log->pipe.alices_side, str, len);
}

void datalog_append_record(Datalog* log, const SensorState* state) {
    FuriString* str = furi_string_alloc();
    furi_check(str != NULL, "Failed to allocate string");

    if(log->record_count == 0) {
        datalog_append_string(log, "timestamp; ticks; current; voltage\n");
        log->start_ticks = state->ticks;
    }

    ++log->record_count;

    DateTime time;
    datetime_timestamp_to_datetime(state->timestamp, &time);

    furi_string_printf(
        str,
        "%04d-%02d-%02d %02d:%02d:%02d; %ld; %f; %f\n",
        time.year,
        time.month,
        time.day,
        time.hour,
        time.minute,
        time.second,
        state->ticks - log->start_ticks,
        state->current,
        state->voltage);

    datalog_append_string(log, furi_string_get_cstr(str));

    furi_string_free(str);
}

FuriString* datalog_build_unique_file_name(void) {
    FuriString* file_name = furi_string_alloc();
    furi_check(file_name != NULL, "Failed to allocate string");

    DateTime time;
    furi_hal_rtc_get_datetime(&time);

    furi_string_printf(
        file_name,
        DATALOG_DATA_PATH_PREFIX "/%04d%02d%02d_%02d%02d%02d.csv",
        time.year,
        time.month,
        time.day,
        time.hour,
        time.minute,
        time.second);

    return file_name;
}
