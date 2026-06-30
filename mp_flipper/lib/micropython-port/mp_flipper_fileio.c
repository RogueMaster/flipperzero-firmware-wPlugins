#include <furi.h>
#include <storage/storage.h>

#include <mp_flipper_fileio.h>
#include <mp_flipper_runtime.h>

#include "mp_flipper_context.h"
#include "mp_flipper_file_helper.h"

uint8_t MP_FLIPPER_FILE_ACCESS_MODE_READ = FSAM_READ;
uint8_t MP_FLIPPER_FILE_ACCESS_MODE_WRITE = FSAM_WRITE;

uint8_t MP_FLIPPER_FILE_OPEN_MODE_OPEN_EXIST = FSOM_OPEN_EXISTING;
uint8_t MP_FLIPPER_FILE_OPEN_MODE_OPEN_ALWAYS = FSOM_OPEN_ALWAYS;
uint8_t MP_FLIPPER_FILE_OPEN_MODE_OPEN_APPEND = FSOM_OPEN_APPEND;
uint8_t MP_FLIPPER_FILE_OPEN_MODE_CREATE_NEW = FSOM_CREATE_NEW;
uint8_t MP_FLIPPER_FILE_OPEN_MODE_CREATE_ALWAYS = FSOM_CREATE_ALWAYS;

inline void* mp_flipper_file_open(const char* name, uint8_t access_mode, uint8_t open_mode) {
    mp_flipper_context_t* ctx = mp_flipper_context;

    File* file = storage_file_alloc(ctx->storage);
    FuriString* path = furi_string_alloc_set_str(name);

    do {
        if(mp_flipper_try_resolve_filesystem_path(path) == MP_FLIPPER_IMPORT_STAT_NO_EXIST) {
            break;
        }

        if(!storage_file_open(file, furi_string_get_cstr(path), access_mode, open_mode)) {
            break;
        }
    } while(false);

    if(!storage_file_is_open(file)) {
        storage_file_close(file);
        storage_file_free(file);

        return NULL;
    }

    return file;
}

inline bool mp_flipper_file_close(void* handle) {
    File* file = handle;

    bool success = storage_file_is_open(file) && storage_file_close(file);

    storage_file_free(file);

    return success;
}

inline size_t mp_flipper_file_seek(void* handle, uint32_t offset) {
    return storage_file_seek(handle, offset, true);
}

inline size_t mp_flipper_file_tell(void* handle) {
    return storage_file_tell(handle);
}

inline size_t mp_flipper_file_size(void* handle) {
    return storage_file_size(handle);
}

inline bool mp_flipper_file_sync(void* handle) {
    return storage_file_sync(handle);
}

inline bool mp_flipper_file_eof(void* handle) {
    return storage_file_eof(handle);
}

inline size_t mp_flipper_file_read(void* handle, void* buffer, size_t size, int* errcode) {
    File* file = handle;

    *errcode = 0; // TODO handle error

    return storage_file_read(file, buffer, size);
}

inline size_t mp_flipper_file_write(void* handle, const void* buffer, size_t size, int* errcode) {
    File* file = handle;

    *errcode = 0; // TODO handle error

    return storage_file_write(file, buffer, size);
}
