#include "sr_settings_store.h"

#include <furi.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>

/*
 * Paths go through APP_DATA_PATH, so storage creates the app directory automatically when it
 * resolves /data.
 * A literal /ext/apps_data/... is forbidden (such a path does not trigger auto-creation).
 */
#define SR_SETTINGS_PATH     APP_DATA_PATH("settings.txt")
#define SR_SETTINGS_TMP_PATH APP_DATA_PATH("settings.tmp")

/*
 * The longest file save can produce must still fall within load's read limit, or save would
 * emit a file it cannot load back (judged Unreadable -> settings silently lost).
 * The two constants come from different anonymous enums, so comparing them directly would trip
 * -Werror=enum-compare; hence the explicit casts to int.
 */
_Static_assert(
    (int)SR_SETTINGS_TEXT_MAX <= (int)SR_SETTINGS_STORE_MAX,
    "settings read cap must cover the longest file save can write");

const char* sr_settings_store_status_str(SrSettingsLoadStatus st) {
    switch(st) {
    case SrSettingsLoadOk:
        return "ok";
    case SrSettingsLoadDegraded:
        return "degraded";
    case SrSettingsLoadMissing:
        return "missing";
    case SrSettingsLoadUnreadable:
        return "unreadable";
    case SrSettingsLoadBadHeader:
        return "bad header";
    default:
        return "unknown";
    }
}

SrSettingsLoadStatus sr_settings_store_load(SrSettings* out, SrSettingsParseStats* stats) {
    Storage* storage;
    File* file;
    uint64_t size;
    char* buf;
    size_t nread;
    SrSettingsParseStats parsed;
    SrSettingsLoadStatus status;

    if(out == NULL) {
        return SrSettingsLoadUnreadable;
    }

    sr_settings_defaults(out);
    if(stats != NULL) {
        memset(stats, 0, sizeof(*stats));
    }

    storage = furi_record_open(RECORD_STORAGE);
    file = storage_file_alloc(storage);
    buf = NULL;
    status = SrSettingsLoadMissing;

    /* Any open failure maps to Missing (D5). Even on failure it must close (storage.h:80). */
    if(!storage_file_open(file, SR_SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        status = SrSettingsLoadMissing;
    } else {
        /* size is uint64_t: check the upper bound before casting to size_t, to avoid 32-bit truncation. */
        size = storage_file_size(file);
        if(size == 0 || size > (uint64_t)SR_SETTINGS_STORE_MAX) {
            status = SrSettingsLoadUnreadable;
        } else {
            buf = (char*)malloc((size_t)size);
            if(buf == NULL) {
                status = SrSettingsLoadUnreadable;
            } else {
                nread = storage_file_read(file, buf, (size_t)size);
                if(nread != (size_t)size) {
                    status = SrSettingsLoadUnreadable;
                } else {
                    /* parse reads only [buf, buf+size); do not append a NUL and do not scan it as a C string. */
                    memset(&parsed, 0, sizeof(parsed));
                    if(!sr_settings_parse(buf, (size_t)size, out, &parsed)) {
                        status = SrSettingsLoadBadHeader;
                    } else if(parsed.values_invalid > 0 || parsed.lines_malformed > 0) {
                        status = SrSettingsLoadDegraded;
                    } else {
                        /* keys_unknown > 0 alone does not constitute Degraded (forward compatibility). */
                        status = SrSettingsLoadOk;
                    }
                    if(stats != NULL) {
                        *stats = parsed;
                    }
                }
            }
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    if(buf != NULL) {
        free(buf);
    }
    furi_record_close(RECORD_STORAGE);
    return status;
}

bool sr_settings_store_save(const SrSettings* s) {
    char* buf;
    size_t n;
    Storage* storage;
    File* file;
    bool opened;
    bool ready;
    bool ok;

    if(s == NULL) {
        return false;
    }

    /* The 160 B serialization buffer must not go on the 2048 B main-thread stack. */
    buf = (char*)malloc(SR_SETTINGS_TEXT_MAX);
    if(buf == NULL) {
        return false;
    }

    n = sr_settings_serialize(s, buf, SR_SETTINGS_TEXT_MAX);
    if(n == 0) {
        free(buf);
        return false;
    }

    storage = furi_record_open(RECORD_STORAGE);
    file = storage_file_alloc(storage);
    opened = storage_file_open(file, SR_SETTINGS_TMP_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    ready = false;

    if(opened) {
        /* The write return value must equal n, or a full SD card silently loses data. */
        if(storage_file_write(file, buf, n) == n && storage_file_sync(file)) {
            ready = true;
        }
    }

    /* The file must be closed before rename (storage.h:303). Even a failed open must close. */
    storage_file_close(file);
    storage_file_free(file);

    ok = false;
    if(ready) {
        /* rename overwrites an existing target, so do not remove final first (storage.h:304). */
        if(storage_common_rename(storage, SR_SETTINGS_TMP_PATH, SR_SETTINGS_PATH) == FSE_OK) {
            ok = true;
        }
    }

    if(!ok) {
        storage_common_remove(storage, SR_SETTINGS_TMP_PATH);
    }

    furi_record_close(RECORD_STORAGE);
    free(buf);
    return ok;
}
