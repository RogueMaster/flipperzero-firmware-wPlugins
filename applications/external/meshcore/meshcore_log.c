#include "meshcore_log.h"

#include <stdarg.h>
#include <storage/storage.h>

struct MeshCoreLog {
    FuriString* text;
    FuriMutex* mutex;
    uint32_t revision;
};

MeshCoreLog* meshcore_log_alloc(void) {
    MeshCoreLog* log = malloc(sizeof(MeshCoreLog));
    log->text = furi_string_alloc();
    log->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    log->revision = 0;
    return log;
}

void meshcore_log_free(MeshCoreLog* log) {
    furi_assert(log);
    furi_mutex_free(log->mutex);
    furi_string_free(log->text);
    free(log);
}

/* Caller holds the mutex. Drops oldest text, then the leftover partial line so
 * the view never starts mid-entry. */
static void meshcore_log_trim(MeshCoreLog* log) {
    size_t size = furi_string_size(log->text);
    if(size <= MESHCORE_LOG_CAP) return;

    furi_string_right(log->text, size - MESHCORE_LOG_CAP);

    size_t newline = furi_string_search_char(log->text, '\n', 0);
    if(newline != FURI_STRING_FAILURE) {
        furi_string_right(log->text, newline + 1);
    }
}

void meshcore_log_printf(MeshCoreLog* log, const char* format, ...) {
    furi_assert(log);
    furi_mutex_acquire(log->mutex, FuriWaitForever);

    /* Where this line starts, so the same text can go to the system log after
     * the mutex is released. */
    size_t start = furi_string_size(log->text);

    va_list args;
    va_start(args, format);
    furi_string_cat_vprintf(log->text, format, args);
    va_end(args);

    /* Copied out before trimming, which can move or drop the line entirely. */
    FuriString* line = furi_string_alloc_set(log->text);
    furi_string_right(line, start);

    furi_string_cat_str(log->text, "\n");

    meshcore_log_trim(log);
    log->revision++;
    furi_mutex_release(log->mutex);

    /* Mirrored to the system log so `log` on the Flipper CLI shows it live.
     * The on-screen log needs the device in hand and a free thumb; this is how
     * a running session can be watched from a laptop, which is the only way
     * some of this gets checked at all. Emitted outside the mutex: the logging
     * subsystem takes locks of its own. */
    FURI_LOG_I(MESHCORE_LOG_TAG, "%s", furi_string_get_cstr(line));
    furi_string_free(line);
}

void meshcore_log_frame(MeshCoreLog* log, bool tx, const uint8_t* payload, size_t len) {
    furi_assert(log);
    furi_mutex_acquire(log->mutex, FuriWaitForever);

    /* Deliberately "TX"/"RX" and not '<'/'>'. In this protocol those glyphs are
     * the wire lead bytes and they mean the opposite of the intuitive
     * out/in: we transmit frames led by 0x3C '<' and receive ones led by
     * 0x3E '>'. Printing the arrows here would read backwards to anyone who
     * knows the protocol, which is exactly who uses this view. */
    furi_string_cat_printf(log->text, "%s %02X", tx ? "TX" : "RX", len ? payload[0] : 0);

    /* One frame is at most MC_MAX_PAYLOAD bytes; cap the dump so a burst of
     * traffic cannot flush the whole log in one entry. */
    size_t shown = len < 24 ? len : 24;
    for(size_t i = 1; i < shown; i++) {
        furi_string_cat_printf(log->text, " %02X", payload[i]);
    }
    if(shown < len) {
        furi_string_cat_printf(log->text, " +%u", (unsigned)(len - shown));
    }
    furi_string_cat_str(log->text, "\n");

    meshcore_log_trim(log);
    log->revision++;
    furi_mutex_release(log->mutex);
}

void meshcore_log_snapshot(MeshCoreLog* log, FuriString* out) {
    furi_assert(log);
    furi_mutex_acquire(log->mutex, FuriWaitForever);
    furi_string_set(out, log->text);
    furi_mutex_release(log->mutex);
}

void meshcore_log_clear(MeshCoreLog* log) {
    furi_assert(log);
    furi_mutex_acquire(log->mutex, FuriWaitForever);
    furi_string_reset(log->text);
    log->revision++;
    furi_mutex_release(log->mutex);
}

uint32_t meshcore_log_revision(MeshCoreLog* log) {
    furi_assert(log);
    furi_mutex_acquire(log->mutex, FuriWaitForever);
    uint32_t revision = log->revision;
    furi_mutex_release(log->mutex);
    return revision;
}

void meshcore_log_dump(MeshCoreLog* log) {
    furi_assert(log);

    /* Snapshotted under the lock, written outside it: the write takes tens of
     * milliseconds and nothing else should be waiting on the log for that long. */
    FuriString* text = furi_string_alloc();
    meshcore_log_snapshot(log, text);
    if(furi_string_empty(text)) {
        furi_string_free(text);
        return;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, EXT_PATH("apps_data"));
    storage_simply_mkdir(storage, EXT_PATH("apps_data/meshcore_cfg"));

    File* file = storage_file_alloc(storage);
    /* CREATE_ALWAYS: one run's log, not an ever-growing file nobody prunes. */
    if(storage_file_open(file, MESHCORE_LOG_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        const char* cstr = furi_string_get_cstr(text);
        storage_file_write(file, cstr, (uint16_t)strlen(cstr));
        storage_file_sync(file);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    furi_string_free(text);
}
