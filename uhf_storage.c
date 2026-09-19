#include "uhf_storage.h"

#include <storage/storage.h>

static void uhf_storage_close(Storage* storage, File* file) {
    if(file) {
        storage_file_close(file);
        storage_file_free(file);
    }
    if(storage) furi_record_close(RECORD_STORAGE);
}

bool uhf_storage_read_blob(const char* path, void* data, size_t size) {
    if(!path || !data || size == 0U) return false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;
    File* file = storage_file_alloc(storage);
    const bool ok = file && storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING) &&
                    storage_file_read(file, data, size) == size;
    uhf_storage_close(storage, file);
    return ok;
}

bool uhf_storage_write_blob(const char* directory, const char* path, const void* data, size_t size) {
    if(!directory || !path || !data || size == 0U) return false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;
    bool ok = false;
    const FS_Error mkdir_result = storage_common_mkdir(storage, directory);
    if(mkdir_result == FSE_OK || mkdir_result == FSE_EXIST) {
        File* file = storage_file_alloc(storage);
        if(file && storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            ok = storage_file_write(file, data, size) == size && storage_file_sync(file);
        }
        uhf_storage_close(storage, file);
    } else {
        furi_record_close(RECORD_STORAGE);
    }
    return ok;
}
