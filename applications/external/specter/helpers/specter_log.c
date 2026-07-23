#include "specter_log.h"

#include <datetime/datetime.h>
#include <furi_hal_rtc.h>
#include <stdarg.h>
#include <stdio.h>
#include <storage/storage.h>
#include <string.h>

#define LOG_PATH APP_DATA_PATH("logbook.txt")
#define LINE_MAX 128u

bool specter_log_append(const char* fmt, ...) {
    furi_assert(fmt);

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    char body[LINE_MAX];
    va_list args;
    va_start(args, fmt);
    vsnprintf(body, sizeof(body), fmt, args);
    va_end(args);

    /* Timestamp on its own line, detail indented under it. The on-device text
     * box wraps at about 21 columns, so an entry laid out this way stays
     * readable there and still looks like a log in a text editor. Callers keep
     * each detail line inside that budget. */
    char line[LINE_MAX + 32u];
    int len = snprintf(
        line,
        sizeof(line),
        "%04u-%02u-%02u %02u:%02u:%02u\n  %s\n",
        (unsigned)dt.year,
        (unsigned)dt.month,
        (unsigned)dt.day,
        (unsigned)dt.hour,
        (unsigned)dt.minute,
        (unsigned)dt.second,
        body);
    if(len <= 0) return false;
    if(len > (int)sizeof(line) - 1) len = (int)sizeof(line) - 1; // snprintf truncated

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, STORAGE_APP_DATA_PATH_PREFIX);

    File* file = storage_file_alloc(storage);
    bool ok = storage_file_open(file, LOG_PATH, FSAM_WRITE, FSOM_OPEN_APPEND);
    if(ok) ok = storage_file_write(file, line, (size_t)len) == (size_t)len;

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool specter_log_read_tail(FuriString* out) {
    furi_assert(out);
    furi_string_reset(out);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool ok = false;

    if(storage_file_open(file, LOG_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t size = storage_file_size(file);
        uint64_t start = 0;
        size_t want = (size_t)size;

        /* Only ever hold the tail in RAM - the log is allowed to outgrow it. */
        if(size > SPECTER_LOG_TAIL_BYTES) {
            start = size - SPECTER_LOG_TAIL_BYTES;
            want = SPECTER_LOG_TAIL_BYTES;
        }

        if(want > 0 && storage_file_seek(file, (uint32_t)start, true)) {
            char* buf = malloc(want + 1u);
            size_t got = storage_file_read(file, buf, want);
            buf[got] = '\0';

            /* If we cut into the middle of a line, drop the fragment. */
            const char* text = buf;
            if(start > 0) {
                const char* nl = strchr(buf, '\n');
                if(nl) text = nl + 1;
            }

            if(*text) {
                furi_string_set(out, text);
                ok = true;
            }
            free(buf);
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool specter_log_clear(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    /* Truncate rather than delete: the file staying put makes it obvious the
     * logbook is a real thing that is simply empty. */
    bool ok = storage_file_open(file, LOG_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS);

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

uint32_t specter_log_size(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FileInfo info;
    uint32_t size = 0;
    if(storage_common_stat(storage, LOG_PATH, &info) == FSE_OK) {
        size = (uint32_t)info.size;
    }
    furi_record_close(RECORD_STORAGE);
    return size;
}
