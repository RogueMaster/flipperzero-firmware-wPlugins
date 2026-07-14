#include "caixianlin_storage.h"
#include <storage/storage.h>

bool caixianlin_storage_load(CaixianlinRemoteApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buf[32];
        uint16_t bytes_read = storage_file_read(file, buf, sizeof(buf) - 1);
        if(bytes_read > 0) {
            buf[bytes_read] = '\0';
            int station_id, channel;
            if(sscanf(buf, "%d,%d", &station_id, &channel) == 2) {
                app->station_id = (uint16_t)station_id;
                app->channel = (uint8_t)channel;
                success = true;
            }
        }
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return success;
}

void caixianlin_storage_save(CaixianlinRemoteApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char buf[32];
        int len = snprintf(buf, sizeof(buf), "%d,%d", app->station_id, app->channel);
        storage_file_write(file, buf, len);
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}
