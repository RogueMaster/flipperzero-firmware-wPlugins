#include "session_log.h"

#include <applications/services/storage/storage.h>
#include <stdlib.h>
#include <string.h>

struct SessionLog {
    Storage* storage;
    File* file;
    bool open;
    SessionLogStatus status;
    uint32_t written;
};

SessionLog* session_log_alloc(void) {
    SessionLog* log = malloc(sizeof(SessionLog));
    memset(log, 0, sizeof(SessionLog));
    log->status = SessionLogOk;
    return log;
}

void session_log_free(SessionLog* log) {
    furi_assert(log);
    session_log_end(log);
    free(log);
}

static bool session_log_write_str(SessionLog* log, const char* s) {
    size_t len = strlen(s);
    return storage_file_write(log->file, s, len) == len;
}

SessionLogStatus session_log_begin(SessionLog* log) {
    furi_assert(log);
    if(log->open) return log->status;

    log->written = 0;
    log->storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(log->storage, EXT_PATH("apps_data"));
    storage_common_mkdir(log->storage, EXT_PATH("apps_data/ghosttag"));

    log->file = storage_file_alloc(log->storage);
    if(!storage_file_open(log->file, GHOSTTAG_LOG_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        storage_file_free(log->file);
        log->file = NULL;
        furi_record_close(RECORD_STORAGE);
        log->storage = NULL;
        log->status = SessionLogFailed;
        return log->status;
    }

    if(storage_file_size(log->file) >= GHOSTTAG_LOG_MAX_SIZE) {
        storage_file_close(log->file);
        storage_file_free(log->file);
        log->file = NULL;
        furi_record_close(RECORD_STORAGE);
        log->storage = NULL;
        log->status = SessionLogFull;
        return log->status;
    }

    /* A header per session, so two hunts never read as one. */
    if(storage_file_size(log->file) == 0) {
        session_log_write_str(log, "# GhostTag session log\n");
        session_log_write_str(log, "# uptime_ms,type,mac,rssi,rssi_max,sightings,dwell_s\n");
    }
    session_log_write_str(log, "--- session start ---\n");

    log->open = true;
    log->status = SessionLogOk;
    return log->status;
}

SessionLogStatus session_log_follower(SessionLog* log, const TrackerRecord* rec) {
    furi_assert(log);
    furi_assert(rec);
    if(!log->open) return log->status;

    if(storage_file_size(log->file) >= GHOSTTAG_LOG_MAX_SIZE) {
        session_log_end(log);
        log->status = SessionLogFull;
        return log->status;
    }

    char line[128];
    uint32_t dwell_s = (rec->last_seen - rec->first_seen) / 1000UL;
    if(dwell_s > 99999UL) dwell_s = 99999UL;

    int n = snprintf(
        line,
        sizeof(line),
        "%lu,%s,%02X:%02X:%02X:%02X:%02X:%02X,%d,%d,%u,%lu\n",
        (unsigned long)rec->last_seen,
        tracker_type_short(rec->type),
        rec->mac[0],
        rec->mac[1],
        rec->mac[2],
        rec->mac[3],
        rec->mac[4],
        rec->mac[5],
        (int)rec->rssi,
        (int)rec->rssi_max,
        (unsigned)rec->count,
        (unsigned long)dwell_s);
    if(n <= 0) return log->status;

    if(!session_log_write_str(log, line)) {
        session_log_end(log);
        log->status = SessionLogFailed;
        return log->status;
    }
    log->written++;
    return SessionLogOk;
}

void session_log_end(SessionLog* log) {
    furi_assert(log);
    if(!log->open) return;
    log->open = false;
    if(log->file) {
        storage_file_close(log->file);
        storage_file_free(log->file);
        log->file = NULL;
    }
    if(log->storage) {
        furi_record_close(RECORD_STORAGE);
        log->storage = NULL;
    }
}

bool session_log_is_open(SessionLog* log) {
    furi_assert(log);
    return log->open;
}

SessionLogStatus session_log_status(SessionLog* log) {
    furi_assert(log);
    return log->status;
}

uint32_t session_log_count(SessionLog* log) {
    furi_assert(log);
    return log->written;
}
