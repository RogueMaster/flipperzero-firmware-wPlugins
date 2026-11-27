#ifdef HOST_BUILD
/* Expose access(2)/stat(2) in unistd.h/sys/stat.h with glibc when using
 * -std=c11. */
#define _DEFAULT_SOURCE 1
#endif

#include "include/fs_compat.h"
#include "include/clock.h"
#include <stdlib.h>
#include <string.h>

#ifdef HOST_BUILD
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

uint32_t app_now_timestamp(void) {
    return (uint32_t)time(NULL);
}

bool fs_write_replace(const char* path, const void* data, size_t len) {
    FILE* f = fopen(path, "wb");
    if(!f) return false;
    size_t w = fwrite(data, 1, len, f);
    fclose(f);
    return w == len;
}

bool fs_read_exact(const char* path, void* buf, size_t len) {
    FILE* f = fopen(path, "rb");
    if(!f) return false;
    size_t r = fread(buf, 1, len, f);
    fclose(f);
    return r == len;
}

bool fs_append_bytes(const char* path, const void* data, size_t len) {
    FILE* f = fopen(path, "ab");
    if(!f) return false;
    size_t w = fwrite(data, 1, len, f);
    fclose(f);
    return w == len;
}

bool fs_read_stock_at(const char* path, uint32_t index, StockItem* out) {
    FILE* f = fopen(path, "rb");
    if(!f || !out) return false;
    if(fseek(f, (long)(index * sizeof(StockItem)), SEEK_SET) != 0) {
        fclose(f);
        return false;
    }
    size_t r = fread(out, sizeof(StockItem), 1, f);
    fclose(f);
    return r == 1;
}

bool fs_find_stock_by_uid(
    const char* filepath,
    const uint8_t* uid,
    uint8_t uid_len,
    StockItem* found_item) {
    if(!filepath || !uid || !found_item) return false;
    FILE* f = fopen(filepath, "rb");
    if(!f) return false;
    StockItem temp;
    bool found = false;
    while(fread(&temp, sizeof(StockItem), 1, f) == 1) {
        if(temp.uid_len == uid_len && memcmp(temp.uid, uid, uid_len) == 0) {
            *found_item = temp;
            found = true;
            break;
        }
    }
    fclose(f);
    return found;
}

StockReadOutcome
    fs_read_all_stock_items_ex(const char* path, StockItem** out_items, size_t* out_count) {
    *out_items = NULL;
    *out_count = 0;
    FILE* f = fopen(path, "rb");
    if(!f) return StockReadBlocked;
    if(fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return StockReadBlocked;
    }
    long sz = ftell(f);
    if(sz < 0) {
        fclose(f);
        return StockReadBlocked;
    }
    rewind(f);
    if((size_t)sz % sizeof(StockItem) != 0) {
        fclose(f);
        return StockReadFraming;
    }
    if((size_t)sz > STOCK_DB_MAX_BYTES) {
        fclose(f);
        return StockReadTooLarge;
    }
    size_t n = (size_t)sz / sizeof(StockItem);
    if(n == 0) {
        fclose(f);
        return StockReadOk;
    }
    StockItem* buf = malloc((size_t)sz);
    if(!buf) {
        fclose(f);
        return StockReadBlocked;
    }
    size_t r = fread(buf, 1, (size_t)sz, f);
    const bool read_error = ferror(f) != 0;
    fclose(f);
    if(read_error || r != (size_t)sz) {
        /* The declared size was well-formed; a short/failed read here is an I/O
     * error, not proof the file's content is corrupt -- never quarantine on a
     * guess. */
        free(buf);
        return StockReadBlocked;
    }
    *out_items = buf;
    *out_count = n;
    return StockReadOk;
}

bool fs_read_all_stock_items(const char* path, StockItem** out_items, size_t* out_count) {
    if(!out_items || !out_count) return false;
    return fs_read_all_stock_items_ex(path, out_items, out_count) == StockReadOk;
}

StoreStat fs_stat(const char* path) {
    struct stat st;
    if(stat(path, &st) == 0) {
        return StoreStatOk;
    }
    return (errno == ENOENT) ? StoreStatMissing : StoreStatError;
}

bool fs_exists(const char* path) {
    return access(path, F_OK) == 0;
}

bool fs_rename(const char* src, const char* dst) {
    return rename(src, dst) == 0;
}

bool fs_remove(const char* path) {
    return remove(path) == 0;
}

bool fs_read_raw_bytes(const char* path, uint8_t** out_bytes, size_t* out_len) {
    *out_bytes = NULL;
    *out_len = 0;
    FILE* f = fopen(path, "rb");
    if(!f) return false;
    if(fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }
    long sz = ftell(f);
    if(sz < 0) {
        fclose(f);
        return false;
    }
    rewind(f);
    if((size_t)sz > STOCK_DB_MAX_BYTES) {
        fclose(f);
        return false;
    }
    if(sz == 0) {
        fclose(f);
        return true;
    }
    uint8_t* buf = malloc((size_t)sz);
    if(!buf) {
        fclose(f);
        return false;
    }
    size_t r = fread(buf, 1, (size_t)sz, f);
    const bool read_error = ferror(f) != 0;
    fclose(f);
    if(read_error || r != (size_t)sz) {
        free(buf);
        return false;
    }
    *out_bytes = buf;
    *out_len = (size_t)sz;
    return true;
}

#else

#include <furi.h>
#include <storage/storage.h>

uint32_t app_now_timestamp(void) {
    return furi_get_tick();
}

static bool
    open_file(File** file, Storage** storage, const char* path, FS_AccessMode am, FS_OpenMode om) {
    *storage = furi_record_open(RECORD_STORAGE);
    *file = storage_file_alloc(*storage);
    if(!storage_file_open(*file, path, am, om)) {
        storage_file_free(*file);
        furi_record_close(RECORD_STORAGE);
        *file = NULL;
        *storage = NULL;
        return false;
    }
    return true;
}

static void close_file(File* file, Storage* storage) {
    (void)storage;
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

bool fs_write_replace(const char* path, const void* data, size_t len) {
    Storage* storage = NULL;
    File* file = NULL;
    if(!open_file(&file, &storage, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) return false;
    size_t w = storage_file_write(file, data, len);
    close_file(file, storage);
    return w == len;
}

bool fs_read_exact(const char* path, void* buf, size_t len) {
    Storage* storage = NULL;
    File* file = NULL;
    if(!open_file(&file, &storage, path, FSAM_READ, FSOM_OPEN_EXISTING)) return false;
    size_t r = storage_file_read(file, buf, len);
    close_file(file, storage);
    return r == len;
}

bool fs_append_bytes(const char* path, const void* data, size_t len) {
    Storage* storage = NULL;
    File* file = NULL;
    if(!open_file(&file, &storage, path, FSAM_WRITE, FSOM_OPEN_APPEND)) return false;
    size_t w = storage_file_write(file, data, len);
    close_file(file, storage);
    return w == len;
}

bool fs_read_stock_at(const char* path, uint32_t index, StockItem* out) {
    if(!out) return false;
    Storage* storage = NULL;
    File* file = NULL;
    if(!open_file(&file, &storage, path, FSAM_READ, FSOM_OPEN_EXISTING)) return false;
    if(!storage_file_seek(file, (uint32_t)(index * sizeof(StockItem)), true)) {
        close_file(file, storage);
        return false;
    }
    size_t r = storage_file_read(file, out, sizeof(StockItem));
    close_file(file, storage);
    return r == sizeof(StockItem);
}

bool fs_find_stock_by_uid(
    const char* filepath,
    const uint8_t* uid,
    uint8_t uid_len,
    StockItem* found_item) {
    if(!filepath || !uid || !found_item) return false;
    Storage* storage = NULL;
    File* file = NULL;
    if(!open_file(&file, &storage, filepath, FSAM_READ, FSOM_OPEN_EXISTING)) return false;
    StockItem temp;
    bool found = false;
    while(storage_file_read(file, &temp, sizeof(StockItem)) == sizeof(StockItem)) {
        if(temp.uid_len == uid_len && memcmp(temp.uid, uid, uid_len) == 0) {
            *found_item = temp;
            found = true;
            break;
        }
    }
    close_file(file, storage);
    return found;
}

StockReadOutcome
    fs_read_all_stock_items_ex(const char* path, StockItem** out_items, size_t* out_count) {
    *out_items = NULL;
    *out_count = 0;
    Storage* storage = NULL;
    File* file = NULL;
    if(!open_file(&file, &storage, path, FSAM_READ, FSOM_OPEN_EXISTING)) return StockReadBlocked;
    uint64_t sz = storage_file_size(file);
    if(sz % sizeof(StockItem) != 0) {
        close_file(file, storage);
        return StockReadFraming;
    }
    if(sz > STOCK_DB_MAX_BYTES) {
        close_file(file, storage);
        return StockReadTooLarge;
    }
    size_t n = (size_t)(sz / sizeof(StockItem));
    if(n == 0) {
        close_file(file, storage);
        return StockReadOk;
    }
    StockItem* buf = malloc((size_t)sz);
    if(!buf) {
        close_file(file, storage);
        return StockReadBlocked;
    }
    if(!storage_file_seek(file, 0, true)) {
        close_file(file, storage);
        free(buf);
        return StockReadBlocked;
    }
    size_t r = storage_file_read(file, buf, (size_t)sz);
    const bool read_error = storage_file_get_error(file) != FSE_OK;
    close_file(file, storage);
    if(read_error || r != (size_t)sz) {
        /* The declared size was well-formed; a short/failed read here is an I/O
     * error, not proof the file's content is corrupt -- never quarantine on a
     * guess. */
        free(buf);
        return StockReadBlocked;
    }
    *out_items = buf;
    *out_count = n;
    return StockReadOk;
}

bool fs_read_all_stock_items(const char* path, StockItem** out_items, size_t* out_count) {
    if(!out_items || !out_count) return false;
    return fs_read_all_stock_items_ex(path, out_items, out_count) == StockReadOk;
}

StoreStat fs_stat(const char* path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    const FS_Error error = storage_common_stat(storage, path, NULL);
    furi_record_close(RECORD_STORAGE);
    if(error == FSE_OK) {
        return StoreStatOk;
    }
    return (error == FSE_NOT_EXIST) ? StoreStatMissing : StoreStatError;
}

bool fs_exists(const char* path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    const bool exists = storage_common_exists(storage, path);
    furi_record_close(RECORD_STORAGE);
    return exists;
}

bool fs_rename(const char* src, const char* dst) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    const bool ok = storage_common_rename(storage, src, dst) == FSE_OK;
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool fs_remove(const char* path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    const bool ok = storage_common_remove(storage, path) == FSE_OK;
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool fs_read_raw_bytes(const char* path, uint8_t** out_bytes, size_t* out_len) {
    *out_bytes = NULL;
    *out_len = 0;
    Storage* storage = NULL;
    File* file = NULL;
    if(!open_file(&file, &storage, path, FSAM_READ, FSOM_OPEN_EXISTING)) return false;
    uint64_t sz = storage_file_size(file);
    if(sz > STOCK_DB_MAX_BYTES) {
        close_file(file, storage);
        return false;
    }
    if(sz == 0) {
        close_file(file, storage);
        return true;
    }
    uint8_t* buf = malloc((size_t)sz);
    if(!buf) {
        close_file(file, storage);
        return false;
    }
    if(!storage_file_seek(file, 0, true)) {
        close_file(file, storage);
        free(buf);
        return false;
    }
    size_t r = storage_file_read(file, buf, (size_t)sz);
    const bool read_error = storage_file_get_error(file) != FSE_OK;
    close_file(file, storage);
    if(read_error || r != (size_t)sz) {
        free(buf);
        return false;
    }
    *out_bytes = buf;
    *out_len = (size_t)sz;
    return true;
}

#endif
