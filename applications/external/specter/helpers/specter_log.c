#include "specter_log.h"

#include <datetime/datetime.h>
#include <furi_hal_rtc.h>
#include <stdarg.h>
#include <stdio.h>
#include <storage/storage.h>
#include <string.h>

#define LOG_PATH   APP_DATA_PATH("logbook.txt")
#define CSV_PATH   APP_DATA_PATH("logbook.csv")
#define CSV_HEADER "timestamp,type,detail\n"
#define DETAIL_MAX 96u
/* Sized for the worst case -Wformat-truncation assumes: the DateTime fields are
 * uint8/uint16 but arrive here as unsigned, so gcc budgets more digits than a
 * real 2000-2099 date ever needs. */
#define STAMP_MAX  32u

static void stamp_now(char* out, size_t n) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    snprintf(
        out,
        n,
        "%04u-%02u-%02u %02u:%02u:%02u",
        (unsigned)dt.year,
        (unsigned)dt.month,
        (unsigned)dt.day,
        (unsigned)dt.hour,
        (unsigned)dt.minute,
        (unsigned)dt.second);
}

/* Append `text` to `path`, seeding it with `header` first if it is new/empty. */
static bool append_to(Storage* storage, const char* path, const char* header, const char* text) {
    File* file = storage_file_alloc(storage);
    bool ok = storage_file_open(file, path, FSAM_WRITE, FSOM_OPEN_APPEND);
    if(ok) {
        if(header && storage_file_size(file) == 0) {
            size_t hn = strlen(header);
            ok = storage_file_write(file, header, hn) == hn;
        }
        if(ok) {
            size_t tn = strlen(text);
            ok = storage_file_write(file, text, tn) == tn;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

bool specter_log_append(const char* type, const char* fmt, ...) {
    furi_assert(type);
    furi_assert(fmt);

    char detail[DETAIL_MAX];
    va_list args;
    va_start(args, fmt);
    vsnprintf(detail, sizeof(detail), fmt, args);
    va_end(args);

    /* A stray comma would shift every CSV column after it; turn it into a
     * space so the row still parses rather than rejecting the entry. */
    for(char* p = detail; *p; p++) {
        if(*p == ',') *p = ' ';
        if(*p == '\n' || *p == '\r') *p = ' ';
    }

    char stamp[STAMP_MAX];
    stamp_now(stamp, sizeof(stamp));

    /* Grouped for the on-device viewer: timestamp, then an indented line. */
    char txt[STAMP_MAX + DETAIL_MAX + 32u];
    snprintf(txt, sizeof(txt), "%s\n  %-6s %s\n", stamp, type, detail);

    /* One flat row for the spreadsheet. */
    char csv[STAMP_MAX + DETAIL_MAX + 32u];
    snprintf(csv, sizeof(csv), "%s,%s,%s\n", stamp, type, detail);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, STORAGE_APP_DATA_PATH_PREFIX);

    bool ok = append_to(storage, LOG_PATH, NULL, txt);
    /* The CSV is a convenience mirror; don't fail the whole write if only it
     * couldn't be updated, but do report a genuine .txt failure. */
    append_to(storage, CSV_PATH, CSV_HEADER, csv);

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

static bool truncate_file(Storage* storage, const char* path) {
    File* file = storage_file_alloc(storage);
    /* Truncate rather than delete: the file staying put makes it obvious the
     * logbook is a real thing that is simply empty. */
    bool ok = storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

bool specter_log_clear(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool ok = truncate_file(storage, LOG_PATH);
    truncate_file(storage, CSV_PATH);
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
