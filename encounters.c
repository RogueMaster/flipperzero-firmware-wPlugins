#include "encounters.h"

#include <furi.h>
#include <storage/storage.h>
#include <string.h>

#define POD_DIR       "/ext/apps_data/pod"
#define POD_ENC_PATH  POD_DIR "/encounters.bin"
#define POD_ENC_MAGIC 0x32434E45UL // "ENC2" (bumped: record layout changed)

void pod_encounters_load(PodEncounters* e) {
    memset(e, 0, sizeof(*e));
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, POD_ENC_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint32_t magic = 0;
        uint16_t count = 0;
        if(storage_file_read(file, &magic, sizeof(magic)) == sizeof(magic) &&
           magic == POD_ENC_MAGIC &&
           storage_file_read(file, &count, sizeof(count)) == sizeof(count)) {
            if(count > POD_MAX_ENCOUNTERS) count = POD_MAX_ENCOUNTERS;
            size_t bytes = (size_t)count * sizeof(PodEncounter);
            if(storage_file_read(file, e->items, bytes) == bytes) {
                e->count = count;
            }
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void pod_encounters_save(const PodEncounters* e) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, POD_DIR);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, POD_ENC_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint32_t magic = POD_ENC_MAGIC;
        storage_file_write(file, &magic, sizeof(magic));
        storage_file_write(file, &e->count, sizeof(e->count));
        storage_file_write(file, e->items, (size_t)e->count * sizeof(PodEncounter));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

int pod_encounters_find(const PodEncounters* e, uint32_t profile_id) {
    for(uint16_t i = 0; i < e->count; i++) {
        if(e->items[i].profile_id == profile_id) return i;
    }
    return -1;
}

static void pod_enc_set_name(PodEncounter* enc, const char* name) {
    strncpy(enc->name, name, POD_NAME_MAX);
    enc->name[POD_NAME_MAX] = '\0';
}

int pod_encounters_update(PodEncounters* e, const PodBeacon* b, uint32_t now, bool* is_new) {
    int idx = pod_encounters_find(e, b->profile_id);
    if(idx >= 0) {
        PodEncounter* enc = &e->items[idx];
        enc->last_seen = now;
        if(enc->times_seen < 0xFFFF) enc->times_seen++;
        enc->dolphin_level = b->dolphin_level;
        pod_enc_set_name(enc, b->name);
        *is_new = false;
        return idx;
    }
    if(e->count >= POD_MAX_ENCOUNTERS) {
        *is_new = false;
        return -1;
    }
    idx = e->count++;
    PodEncounter* enc = &e->items[idx];
    memset(enc, 0, sizeof(*enc));
    enc->profile_id = b->profile_id;
    pod_enc_set_name(enc, b->name);
    enc->icon = b->icon;
    enc->dolphin_level = b->dolphin_level;
    enc->first_seen = now;
    enc->last_seen = now;
    enc->times_seen = 1;
    *is_new = true;
    return idx;
}
