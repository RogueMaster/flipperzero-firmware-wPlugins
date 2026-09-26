#include "maze3d.h"
#include <stdio.h>
#include <storage/storage.h>
#include <string.h>

#define SAVE_PATH APP_DATA_PATH("maze3d.sav")

typedef struct {
    uint32_t magic;
    uint8_t lang;
} SaveData;

#define SAVE_MAGIC 0x4D5A3344u // "MZ3D"

void storage_save(void) {
    SaveData sd;
    sd.magic = SAVE_MAGIC;
    sd.lang = g.lang;
    Storage* st = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(st);
    if(storage_file_open(f, SAVE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(f, &sd, sizeof(sd));
    }
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

void storage_load(void) {
    Storage* st = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(st);
    if(storage_file_open(f, SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        SaveData sd;
        if(storage_file_read(f, &sd, sizeof(sd)) == sizeof(sd) && sd.magic == SAVE_MAGIC) {
            g.lang = sd.lang;
        }
    }
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}
