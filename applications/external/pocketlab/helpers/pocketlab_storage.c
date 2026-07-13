#include "pocketlab_storage.h"

#include <furi.h>
#include <storage/storage.h>

#define POCKETLAB_STATE_MAGIC   0x504C4231UL /* "PLB1" */
#define POCKETLAB_STATE_VERSION 3

#define POCKETLAB_DATA_DIR   "/ext/apps_data/pocketlab"
#define POCKETLAB_STATE_PATH POCKETLAB_DATA_DIR "/state.bin"

// Version 1 on-disk layout, kept only to migrate old saves forward.
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t sound;
    uint8_t reserved[5];
    uint32_t xp;
    uint32_t level;
    uint32_t completed_mask;
} PocketLabStateV1;

// Version 2 on-disk layout (before the LED/vibro toggles were added).
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t sound;
    uint8_t reserved;
    uint32_t xp;
    uint32_t level;
    uint64_t completed_mask;
    uint32_t streak_days;
    uint32_t last_day;
} PocketLabStateV2;

static void pocketlab_storage_set_defaults(PocketLabState* state) {
    state->magic = POCKETLAB_STATE_MAGIC;
    state->version = POCKETLAB_STATE_VERSION;
    state->sound = 1;
    state->led = 1;
    state->vibro = 1;
    state->reserved = 0;
    state->xp = 0;
    state->level = 1;
    state->completed_mask = 0;
    state->streak_days = 0;
    state->last_day = 0;
}

static void pocketlab_storage_migrate_v1(PocketLabState* state, const PocketLabStateV1* old) {
    state->sound = old->sound;
    state->xp = old->xp;
    state->level = old->level;
    state->completed_mask = old->completed_mask;
    // led/vibro and streak fields keep their defaults
}

static void pocketlab_storage_migrate_v2(PocketLabState* state, const PocketLabStateV2* old) {
    state->sound = old->sound;
    state->xp = old->xp;
    state->level = old->level;
    state->completed_mask = old->completed_mask;
    state->streak_days = old->streak_days;
    state->last_day = old->last_day;
    // led/vibro keep their defaults (on)
}

void pocketlab_storage_load(PocketLabState* state) {
    furi_assert(state);
    pocketlab_storage_set_defaults(state);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, POCKETLAB_STATE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint32_t magic = 0;
        uint16_t version = 0;
        storage_file_read(file, &magic, sizeof(magic));
        storage_file_read(file, &version, sizeof(version));
        storage_file_seek(file, 0, true);

        if(magic == POCKETLAB_STATE_MAGIC) {
            if(version == POCKETLAB_STATE_VERSION) {
                PocketLabState loaded;
                if(storage_file_read(file, &loaded, sizeof(loaded)) == sizeof(loaded)) {
                    *state = loaded;
                }
            } else if(version == 2) {
                PocketLabStateV2 old;
                if(storage_file_read(file, &old, sizeof(old)) == sizeof(old)) {
                    pocketlab_storage_migrate_v2(state, &old);
                }
            } else if(version == 1) {
                PocketLabStateV1 old;
                if(storage_file_read(file, &old, sizeof(old)) == sizeof(old)) {
                    pocketlab_storage_migrate_v1(state, &old);
                }
            }
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
