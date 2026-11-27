#include "include/db.h"
#include "include/fs_compat.h"
#include "include/stock_recovery.h"

static StoreStat port_stat(void* ctx, const char* path) {
    (void)ctx;
    return fs_stat(path);
}

static bool port_exists(void* ctx, const char* path) {
    (void)ctx;
    return fs_exists(path);
}

static StockReadOutcome
    port_read_all(void* ctx, const char* path, StockItem** out_items, size_t* out_count) {
    (void)ctx;
    return fs_read_all_stock_items_ex(path, out_items, out_count);
}

static bool port_write_all(void* ctx, const char* path, const StockItem* items, size_t count) {
    (void)ctx;
    return fs_write_replace(path, items, count * sizeof(StockItem));
}

static bool port_rename(void* ctx, const char* src, const char* dst) {
    (void)ctx;
    return fs_rename(src, dst);
}

static bool port_remove(void* ctx, const char* path) {
    (void)ctx;
    return fs_remove(path);
}

static bool port_read_raw(void* ctx, const char* path, uint8_t** out_bytes, size_t* out_len) {
    (void)ctx;
    return fs_read_raw_bytes(path, out_bytes, out_len);
}

static const StockStoragePort real_port = {
    .ctx = NULL,
    .max_bytes = STOCK_DB_MAX_BYTES,
    .stat = port_stat,
    .exists = port_exists,
    .read_all = port_read_all,
    .read_raw = port_read_raw,
    .write_all = port_write_all,
    .rename = port_rename,
    .remove = port_remove,
};

bool stock_db_find_by_uid(
    const char* filepath,
    const uint8_t* uid,
    uint8_t uid_len,
    StockItem* found_item) {
    return fs_find_stock_by_uid(filepath, uid, uid_len, found_item);
}

StockWriteResult stock_db_upsert(const char* filepath, const StockItem* item) {
    if(!filepath || !item) {
        StockWriteResult result = {StockWriteFailed, {0}};
        return result;
    }
    return stock_recovery_upsert(&real_port, filepath, item);
}

StockWriteResult stock_db_delete_at(const char* filepath, size_t index) {
    if(!filepath) {
        StockWriteResult result = {StockWriteFailed, {0}};
        return result;
    }
    return stock_recovery_delete_at(&real_port, filepath, index);
}
