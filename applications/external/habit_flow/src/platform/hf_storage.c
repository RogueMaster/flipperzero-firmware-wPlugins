#include "include/platform/hf_storage.h"

static HfPathStat storage_stat(void* self, const char* path) {
    FileInfo info;
    FS_Error err = storage_common_stat((Storage*)self, path, &info);
    if(err == FSE_OK) {
        return HfPathOk;
    }
    if(err == FSE_NOT_EXIST) {
        return HfPathMissing;
    }
    return HfPathError;
}

static size_t storage_read(void* self, const char* path, uint8_t* buf, size_t cap) {
    Storage* storage = self;
    File* file = storage_file_alloc(storage);
    size_t n = 0;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t size = storage_file_size(file);
        n = storage_file_read(file, buf, cap);
        // A mid-file disk error can return a short count with the error id set instead of
        // failing outright (storage_ext_file_read), which would otherwise look just like
        // a legitimately short file. Treat both as no read at all: get_error must be read
        // before the file closes below.
        bool short_of_both = n < cap && (uint64_t)n < size;
        if(storage_file_get_error(file) != FSE_OK || short_of_both) {
            n = 0;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return n;
}

static bool storage_write(void* self, const char* path, const uint8_t* data, size_t len) {
    Storage* storage = self;
    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        ok = storage_file_write(file, data, len) == len;
    }
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

static bool storage_rename(void* self, const char* old_path, const char* new_path) {
    return storage_common_rename((Storage*)self, old_path, new_path) == FSE_OK;
}

static bool storage_remove(void* self, const char* path) {
    return storage_common_remove((Storage*)self, path) == FSE_OK;
}

static bool storage_mkdir(void* self, const char* path) {
    FS_Error err = storage_common_mkdir((Storage*)self, path);
    return err == FSE_OK || err == FSE_EXIST;
}

HfStorePort hf_storage_port(Storage* storage) {
    return (HfStorePort){
        .self = storage,
        .stat = storage_stat,
        .read = storage_read,
        .write = storage_write,
        .rename = storage_rename,
        .remove = storage_remove,
        .mkdir = storage_mkdir,
    };
}
