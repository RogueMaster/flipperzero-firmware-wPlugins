#include "maze3d.h"
#include <storage/storage.h>
#include <stdlib.h>

#define SAVE_PATH APP_DATA_PATH("maze3d.sav")

typedef struct {
    uint32_t magic;
    int campaign_cleared;
    int endless_floor;
} SaveData;

#define SAVE_MAGIC 0x4D415A33u // "MAZ3"

void storage_load(void) {
    SaveData d = {0};
    Storage* st = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(st);
    bool ok = storage_file_open(f, SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING);
    if(ok && storage_file_read(f, &d, sizeof(d)) == sizeof(d) && d.magic == SAVE_MAGIC) {
        g.campaign_cleared = d.campaign_cleared;
        g.endless_floor = d.endless_floor;
    } else {
        g.campaign_cleared = 0;
        g.endless_floor = 1;
    }
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

void storage_save(void) {
    SaveData d;
    d.magic = SAVE_MAGIC;
    d.campaign_cleared = g.campaign_cleared;
    d.endless_floor = g.endless_floor;
    Storage* st = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(st);
    if(storage_file_open(f, SAVE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(f, &d, sizeof(d));
    }
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}
