#include "nus_stats.h"

#include <furi.h>
#include <storage/storage.h>

#define TAG "NusStats"

#define STATS_PARENT "/ext/apps_data"
#define STATS_DIR    "/ext/apps_data/claude_buddy"
#define STATS_FILE   "/ext/apps_data/claude_buddy/stats.bin"

#define STATS_VERSION 1

void nus_stats_load(NusStats* out) {
    if(!out) return;
    memset(out, 0, sizeof(*out));

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, STATS_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint8_t ver = 0;
        if(storage_file_read(f, &ver, 1) == 1 && ver == STATS_VERSION) {
            NusStats tmp;
            if(storage_file_read(f, &tmp, sizeof(tmp)) == sizeof(tmp)) {
                *out = tmp;
            }
        }
    }
    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

void nus_stats_save(const NusStats* in) {
    if(!in) return;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, STATS_PARENT);
    storage_simply_mkdir(storage, STATS_DIR);

    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, STATS_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint8_t ver = STATS_VERSION;
        if(storage_file_write(f, &ver, 1) != 1 ||
           storage_file_write(f, in, sizeof(*in)) != sizeof(*in)) {
            FURI_LOG_E(TAG, "short write");
        }
    } else {
        FURI_LOG_E(TAG, "open for write failed: %s", STATS_FILE);
    }
    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}
