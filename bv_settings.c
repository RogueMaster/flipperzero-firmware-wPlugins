#include "bv_settings.h"

#include <furi.h>
#include <storage/storage.h>
#include <string.h>

#define TAG "BioVaultSettings"

#define SETTINGS_PATH APP_DATA_PATH("settings.bin")
#define SETTINGS_MAGIC "BVS2"
#define SETTINGS_MAGIC_V1 "BVS1"
#define SETTINGS_MAGIC_LEN 4
#define SETTINGS_SIZE_V1 (SETTINGS_MAGIC_LEN + 1) // magic + send_newline
#define SETTINGS_SIZE (SETTINGS_MAGIC_LEN + 4) // + protect_reads + authlim + tag_protected

void bv_settings_load(BvSettings* out) {
    memset(out, 0, sizeof(*out)); // defaults

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    uint8_t buf[SETTINGS_SIZE];
    if(storage_file_open(file, SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t n = storage_file_read(file, buf, sizeof(buf));
        // Accept BVS2 and any later "BVS" revision (first 8 bytes share layout).
        bool v2plus = n >= SETTINGS_SIZE && memcmp(buf, "BVS", 3) == 0 && buf[3] >= '2';
        if(v2plus) {
            out->send_newline = buf[4] != 0;
            out->protect_reads = buf[5] != 0;
            out->authlim = buf[6] <= 7 ? buf[6] : 0;
            out->tag_protected = buf[7] != 0;
        } else if(n >= SETTINGS_SIZE_V1 && memcmp(buf, SETTINGS_MAGIC_V1, SETTINGS_MAGIC_LEN) == 0) {
            out->send_newline = buf[4] != 0; // v1: only field present
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

bool bv_settings_save(const BvSettings* s) {
    uint8_t buf[SETTINGS_SIZE];
    memcpy(buf, SETTINGS_MAGIC, SETTINGS_MAGIC_LEN);
    buf[4] = s->send_newline ? 1 : 0;
    buf[5] = s->protect_reads ? 1 : 0;
    buf[6] = s->authlim <= 7 ? s->authlim : 0;
    buf[7] = s->tag_protected ? 1 : 0;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, APP_DATA_PATH("")); // ensure dir exists
    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(file, SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        ok = storage_file_write(file, buf, sizeof(buf)) == sizeof(buf);
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    FURI_LOG_I(TAG, "save settings: %s", ok ? "OK" : "FAIL");
    return ok;
}
