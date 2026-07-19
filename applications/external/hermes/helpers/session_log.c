#include "session_log.h"

#include <storage/storage.h>
#include <furi_hal_rtc.h>
#include <string.h>

struct SessionLog {
    Storage* storage;
    File* file;
    bool open;
    uint32_t bytes;
    char name[SESSION_LOG_PATH_MAX];
};

SessionLog* session_log_alloc(void) {
    SessionLog* log = malloc(sizeof(SessionLog));
    memset(log, 0, sizeof(SessionLog));
    log->storage = furi_record_open(RECORD_STORAGE);
    log->file = storage_file_alloc(log->storage);
    return log;
}

void session_log_free(SessionLog* log) {
    furi_assert(log);
    session_log_close(log);
    storage_file_free(log->file);
    furi_record_close(RECORD_STORAGE);
    free(log);
}

/** Write a NUL-terminated string. Only used for the header and for notes. */
static void session_log_puts(SessionLog* log, const char* s) {
    storage_file_write(log->file, s, strlen(s));
}

bool session_log_open(SessionLog* log, uint32_t baud, HermesFraming framing, HermesPort port) {
    furi_assert(log);
    if(log->open) session_log_close(log);

    if(storage_common_mkdir(log->storage, SESSION_LOG_DIR) == FSE_DENIED) return false;

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    /* Sortable name, one file per capture: hermes_20260716_145530.log */
    snprintf(
        log->name,
        sizeof(log->name),
        "hermes_%04u%02u%02u_%02u%02u%02u.log",
        dt.year,
        dt.month,
        dt.day,
        dt.hour,
        dt.minute,
        dt.second);

    char path[SESSION_LOG_PATH_MAX + sizeof(SESSION_LOG_DIR) + 2];
    snprintf(path, sizeof(path), SESSION_LOG_DIR "/%s", log->name);

    if(!storage_file_open(log->file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        return false;
    }

    char header[192];
    snprintf(
        header,
        sizeof(header),
        "# Hermes capture\r\n"
        "# %04u-%02u-%02u %02u:%02u:%02u\r\n"
        "# %lu baud %s on %s (%s)\r\n"
        "\r\n",
        dt.year,
        dt.month,
        dt.day,
        dt.hour,
        dt.minute,
        dt.second,
        baud,
        hermes_framing_name(framing),
        hermes_port_name(port),
        hermes_port_pins(port));
    session_log_puts(log, header);

    log->bytes = 0;
    log->open = true;
    return true;
}

void session_log_close(SessionLog* log) {
    furi_assert(log);
    if(!log->open) return;

    session_log_puts(log, "\r\n# end of capture\r\n");
    storage_file_close(log->file);
    log->open = false;
    log->name[0] = '\0';
}

bool session_log_is_open(const SessionLog* log) {
    furi_assert(log);
    return log->open;
}

void session_log_write(SessionLog* log, const uint8_t* data, size_t len) {
    furi_assert(log);
    if(!log->open || len == 0) return;

    const size_t written = storage_file_write(log->file, data, len);
    log->bytes += (uint32_t)written;

    /* A card pulled mid-capture, or a full one. Stop rather than pretend. */
    if(written < len) session_log_close(log);
}

void session_log_note(SessionLog* log, const char* what) {
    furi_assert(log);
    if(!log->open) return;

    char line[80];
    snprintf(line, sizeof(line), "\r\n[hermes: %s]\r\n", what);
    session_log_puts(log, line);
}

uint32_t session_log_bytes(const SessionLog* log) {
    furi_assert(log);
    return log->bytes;
}

const char* session_log_name(const SessionLog* log) {
    furi_assert(log);
    return log->name;
}
