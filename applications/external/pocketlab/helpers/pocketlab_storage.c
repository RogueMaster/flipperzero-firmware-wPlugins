#include "pocketlab_storage.h"

#include <furi.h>
#include <storage/storage.h>

#define POCKETLAB_STATE_MAGIC   0x504C4231UL /* "PLB1" */
#define POCKETLAB_STATE_VERSION 1

#define POCKETLAB_DATA_DIR   "/ext/apps_data/pocketlab"
#define POCKETLAB_STATE_PATH POCKETLAB_DATA_DIR "/state.bin"

static void pocketlab_storage_set_defaults(PocketLabState* state) {
    state->magic = POCKETLAB_STATE_MAGIC;
    state->version = POCKETLAB_STATE_VERSION;
    state->sound = 1;
    memset(state->reserved, 0, sizeof(state->reserved));
    state->xp = 0;
    state->level = 1;
    state->completed_mask = 0;
}

void pocketlab_storage_load(PocketLabState* state) {
    furi_assert(state);
    pocketlab_storage_set_defaults(state);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, POCKETLAB_STATE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        PocketLabState loaded;
        uint16_t read = storage_file_read(file, &loaded, sizeof(loaded));
        bool valid = read == sizeof(loaded) && loaded.magic == POCKETLAB_STATE_MAGIC &&
                     loaded.version == POCKETLAB_STATE_VERSION;
        if(valid) {
            *state = loaded;
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void pocketlab_storage_save(const PocketLabState* state) {
    furi_assert(state);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, POCKETLAB_DATA_DIR);

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, POCKETLAB_STATE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, state, sizeof(*state));
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}
