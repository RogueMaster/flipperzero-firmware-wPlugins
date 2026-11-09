#include "meshcore_settings.h"

#include <furi.h>
#include <storage/storage.h>

#define MESHCORE_SETTINGS_DIR  EXT_PATH("apps_data/meshcore_cfg")
#define MESHCORE_SETTINGS_PATH MESHCORE_SETTINGS_DIR "/settings.bin"

void meshcore_settings_load(MeshCoreSettings* out) {
    /* Default: everything on. A field user who wants quiet turns it off; a fresh
     * install should be discoverable, so it announces itself out of the box. */
    out->sound = true;
    out->vibro = true;
    out->led = true;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, MESHCORE_SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint8_t b[3];
        if(storage_file_read(file, b, sizeof(b)) == sizeof(b)) {
            out->sound = b[0] != 0;
            out->vibro = b[1] != 0;
            out->led = b[2] != 0;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void meshcore_settings_save(const MeshCoreSettings* settings) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, EXT_PATH("apps_data"));
    storage_simply_mkdir(storage, MESHCORE_SETTINGS_DIR);

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, MESHCORE_SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint8_t b[3] = {
            settings->sound ? 1u : 0u,
            settings->vibro ? 1u : 0u,
            settings->led ? 1u : 0u,
        };
        storage_file_write(file, b, sizeof(b));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}
