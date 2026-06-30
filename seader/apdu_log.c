#include "apdu_log.h"

#include <storage/storage.h>
#include <flipper_format/flipper_format.h>
#include <toolbox/stream/file_stream.h>
#include <toolbox/stream/buffered_file_stream.h>
#include <toolbox/args.h>

#define TAG "APDULog"

struct APDULog {
    Stream* stream;
    size_t total_lines;
};

static inline void apdu_log_add_ending_new_line(APDULog* instance) {
    if(stream_seek(instance->stream, -1, StreamOffsetFromEnd)) {
        uint8_t last_char = 0;

        // Check if the last char is new line or add a new line
        if(stream_read(instance->stream, &last_char, 1) == 1 && last_char != '\n') {
            FURI_LOG_D(TAG, "Adding new line ending");
            stream_write_char(instance->stream, '\n');
        }

        stream_rewind(instance->stream);
    }
}

static bool apdu_log_read_log_line(APDULog* instance, FuriString* line, bool* is_endfile) {
    if(stream_read_line(instance->stream, line) == false) {
        *is_endfile = true;
    }

    else {
        size_t newline_index = furi_string_search_char(line, '\n', 0);

        if(newline_index != FURI_STRING_FAILURE) {
            furi_string_left(line, newline_index);
        }

        FURI_LOG_T(
            TAG, "Read line: %s, len: %zu", furi_string_get_cstr(line), furi_string_size(line));

        return true;
    }

    return false;
}

bool apdu_log_check_presence(const char* path) {
    furi_check(path);

    Storage* storage = furi_record_open(RECORD_STORAGE);

    bool log_present = storage_common_stat(storage, path, NULL) == FSE_OK;

    furi_record_close(RECORD_STORAGE);

    return log_present;
}

APDULog* apdu_log_alloc(const char* path, APDULogMode mode) {
    furi_check(path);

    APDULog* instance = malloc(sizeof(APDULog));

    Storage* storage = furi_record_open(RECORD_STORAGE);
    instance->stream = buffered_file_stream_alloc(storage);

    FS_OpenMode open_mode = (mode == APDULogModeOpenAlways) ? FSOM_OPEN_ALWAYS :
                                                              FSOM_OPEN_EXISTING;

    instance->total_lines = 0;

    bool file_exists =
        buffered_file_stream_open(instance->stream, path, FSAM_READ_WRITE, open_mode);

    if(!file_exists) {
        buffered_file_stream_close(instance->stream);
    } else {
        // Eventually add new line character in the last line to avoid skipping lines
        apdu_log_add_ending_new_line(instance);
    }

    FuriString* line = furi_string_alloc();

    bool is_endfile = false;

    // In this loop we only count the entries in the file
    // We prefer not to load the whole file in memory for space reasons
    while(file_exists && !is_endfile) {
        bool read_log = apdu_log_read_log_line(instance, line, &is_endfile);
        if(read_log) {
            instance->total_lines++;
        }
    }
    stream_rewind(instance->stream);
    FURI_LOG_I(TAG, "Loaded log with %zu lines", instance->total_lines);

    furi_string_free(line);

    return instance;
}

void apdu_log_free(APDULog* instance) {
    furi_check(instance);
    furi_check(instance->stream);

    buffered_file_stream_close(instance->stream);
    stream_free(instance->stream);
    free(instance);

    furi_record_close(RECORD_STORAGE);
}

size_t apdu_log_get_total_lines(APDULog* instance) {
    furi_check(instance);

    return instance->total_lines;
}

bool apdu_log_rewind(APDULog* instance) {
    furi_check(instance);
    furi_check(instance->stream);

    return stream_rewind(instance->stream);
}

bool apdu_log_get_next_log_str(APDULog* instance, FuriString* log) {
    furi_assert(instance);
    furi_assert(instance->stream);
    furi_assert(log);

    bool log_read = false;
    bool is_endfile = false;

    furi_string_reset(log);

    while(!log_read && !is_endfile)
        log_read = apdu_log_read_log_line(instance, log, &is_endfile);

    return log_read;
}
