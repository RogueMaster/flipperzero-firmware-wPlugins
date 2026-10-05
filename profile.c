#include "profile.h"

#include <furi.h>
#include <furi_hal_random.h>
#include <storage/storage.h>
#include <dolphin/dolphin.h>
#include <string.h>

#define POD_DIR           "/ext/apps_data/pod"
#define POD_PROFILE_PATH  POD_DIR "/profile.bin"
#define POD_PROFILE_MAGIC 0x31444F50UL // "POD1" little-endian

static void pod_profile_default(PodProfile* p) {
    memset(p, 0, sizeof(PodProfile));
    p->magic = POD_PROFILE_MAGIC;
    p->profile_id = furi_hal_random_get();
    if(p->profile_id == 0) p->profile_id = 1;
    // Everyone gets a unique-ish default callsign until they edit it.
    snprintf(p->name, sizeof(p->name), "POD-%04lX", (unsigned long)(p->profile_id & 0xFFFF));
    p->icon = 0;
}

void pod_profile_save(const PodProfile* profile) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, POD_DIR);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, POD_PROFILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, profile, sizeof(PodProfile));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void pod_profile_load(PodProfile* profile) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(file, POD_PROFILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        PodProfile tmp;
        if(storage_file_read(file, &tmp, sizeof(tmp)) == sizeof(tmp) &&
           tmp.magic == POD_PROFILE_MAGIC) {
            tmp.name[POD_NAME_MAX] = '\0';
            *profile = tmp;
            ok = true;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if(!ok) {
        pod_profile_default(profile);
        pod_profile_save(profile);
    }
}

uint8_t pod_profile_dolphin_level(void) {
    Dolphin* dolphin = furi_record_open(RECORD_DOLPHIN);
    DolphinStats stats = dolphin_stats(dolphin);
    furi_record_close(RECORD_DOLPHIN);
    return stats.level;
}
