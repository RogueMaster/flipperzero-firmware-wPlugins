/**
 * Exercises stock_recovery_load/save/upsert/delete_at against an in-memory fake
 * whose rename() reproduces the firmware's own (non-atomic)
 * storage_common_rename: remove dst, copy src to dst, then remove src -- with
 * an injectable crash after each step, including a partial copy.
 */
#include "include/fs_compat.h" /* STOCK_DB_MAX_BYTES */
#include "include/stock_recovery.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FAKE_FS_CAPACITY 16 /* main + tmp + 10 .bad slots, plus headroom */

typedef enum {
    CrashNone = 0,
    CrashAfterRemoveDst, /* dst gone, src (copy source) still fully intact */
    CrashDuringCopy, /* dst ends up with a truncated/garbled copy; src untouched
                    */
    CrashAfterCopyBeforeRemoveSrc, /* both src and dst end up holding the same
                                    content */
} RenameCrashPoint;

typedef struct {
    bool used;
    char path[STOCK_PATH_MAX];
    uint8_t* data;
    size_t len;
} FakeFile;

typedef struct {
    FakeFile files[FAKE_FS_CAPACITY];
    RenameCrashPoint crash_point; /* consumed (reset to CrashNone) by the next rename() call */
    char force_stat_error_path[STOCK_PATH_MAX];
    char force_blocked_read_path[STOCK_PATH_MAX];
    char force_write_fail_path[STOCK_PATH_MAX];
} FakePort;

static void fake_port_init(FakePort* fp) {
    memset(fp, 0, sizeof(*fp));
}

static FakeFile* fake_find(FakePort* fp, const char* path) {
    for(int i = 0; i < FAKE_FS_CAPACITY; i++) {
        if(fp->files[i].used && strcmp(fp->files[i].path, path) == 0) {
            return &fp->files[i];
        }
    }
    return NULL;
}

static FakeFile* fake_find_or_alloc(FakePort* fp, const char* path) {
    FakeFile* existing = fake_find(fp, path);
    if(existing) {
        return existing;
    }
    for(int i = 0; i < FAKE_FS_CAPACITY; i++) {
        if(!fp->files[i].used) {
            fp->files[i].used = true;
            strncpy(fp->files[i].path, path, sizeof(fp->files[i].path) - 1);
            fp->files[i].data = NULL;
            fp->files[i].len = 0;
            return &fp->files[i];
        }
    }
    assert(0 && "fake filesystem out of slots");
    return NULL;
}

static void fake_remove(FakePort* fp, const char* path) {
    FakeFile* f = fake_find(fp, path);
    if(f) {
        free(f->data);
        memset(f, 0, sizeof(*f));
    }
}

static void fake_set_bytes(FakePort* fp, const char* path, const uint8_t* data, size_t len) {
    FakeFile* f = fake_find_or_alloc(fp, path);
    free(f->data);
    f->data = NULL;
    if(len > 0) {
        f->data = malloc(len);
        assert(f->data);
        memcpy(f->data, data, len);
    }
    f->len = len;
}

static void fake_set_items(FakePort* fp, const char* path, const StockItem* items, size_t count) {
    fake_set_bytes(fp, path, (const uint8_t*)items, count * sizeof(StockItem));
}

static StoreStat fake_stat(void* ctx, const char* path) {
    FakePort* fp = ctx;
    if(fp->force_stat_error_path[0] && strcmp(fp->force_stat_error_path, path) == 0) {
        return StoreStatError;
    }
    return fake_find(fp, path) ? StoreStatOk : StoreStatMissing;
}

static bool fake_exists(void* ctx, const char* path) {
    FakePort* fp = ctx;
    return fake_find(fp, path) != NULL;
}

static StockReadOutcome
    fake_read_all(void* ctx, const char* path, StockItem** out_items, size_t* out_count) {
    FakePort* fp = ctx;
    if(fp->force_blocked_read_path[0] && strcmp(fp->force_blocked_read_path, path) == 0) {
        return StockReadBlocked;
    }
    const FakeFile* f = fake_find(fp, path);
    if(!f) {
        return StockReadBlocked;
    }
    if(f->len % sizeof(StockItem) != 0) {
        return StockReadFraming;
    }
    if(f->len > STOCK_DB_MAX_BYTES) {
        return StockReadTooLarge;
    }
    const size_t n = f->len / sizeof(StockItem);
    if(n == 0) {
        *out_items = NULL;
        *out_count = 0;
        return StockReadOk;
    }
    StockItem* buf = malloc(f->len);
    assert(buf);
    memcpy(buf, f->data, f->len);
    *out_items = buf;
    *out_count = n;
    return StockReadOk;
}

static bool fake_write_all(void* ctx, const char* path, const StockItem* items, size_t count) {
    FakePort* fp = ctx;
    if(fp->force_write_fail_path[0] && strcmp(fp->force_write_fail_path, path) == 0) {
        /* Mirrors real fs_write_replace: fopen(path, "wb") truncates to empty
     * immediately, then the write itself fails, leaving a 0-byte file rather
     * than none at all. */
        fake_set_bytes(fp, path, NULL, 0);
        return false;
    }
    fake_set_items(fp, path, items, count);
    return true;
}

static bool fake_read_raw(void* ctx, const char* path, uint8_t** out_bytes, size_t* out_len) {
    FakePort* fp = ctx;
    if(fp->force_blocked_read_path[0] && strcmp(fp->force_blocked_read_path, path) == 0) {
        return false;
    }
    const FakeFile* f = fake_find(fp, path);
    if(!f || f->len > STOCK_DB_MAX_BYTES) {
        return false;
    }
    if(f->len == 0) {
        *out_bytes = NULL;
        *out_len = 0;
        return true;
    }
    uint8_t* buf = malloc(f->len);
    assert(buf);
    memcpy(buf, f->data, f->len);
    *out_bytes = buf;
    *out_len = f->len;
    return true;
}

static bool fake_remove_port(void* ctx, const char* path) {
    FakePort* fp = ctx;
    if(!fake_find(fp, path)) {
        return false;
    }
    fake_remove(fp, path);
    return true;
}

static bool fake_rename(void* ctx, const char* src, const char* dst) {
    FakePort* fp = ctx;
    const FakeFile* s = fake_find(fp, src);
    if(!s) {
        return false;
    }

    const RenameCrashPoint crash = fp->crash_point;
    fp->crash_point = CrashNone;

    fake_remove(fp, dst);
    if(crash == CrashAfterRemoveDst) {
        return false;
    }

    if(crash == CrashDuringCopy) {
        fake_set_bytes(fp, dst, s->data, s->len / 2);
        return false;
    }

    fake_set_bytes(fp, dst, s->data, s->len);
    if(crash == CrashAfterCopyBeforeRemoveSrc) {
        return false;
    }

    fake_remove(fp, src);
    return true;
}

static void fake_port_bind(FakePort* fp, StockStoragePort* port) {
    port->ctx = fp;
    port->max_bytes = STOCK_DB_MAX_BYTES;
    port->stat = fake_stat;
    port->exists = fake_exists;
    port->read_all = fake_read_all;
    port->read_raw = fake_read_raw;
    port->write_all = fake_write_all;
    port->rename = fake_rename;
    port->remove = fake_remove_port;
}

static StockItem make_item(uint8_t uid_byte, const char* name) {
    StockItem it;
    memset(&it, 0, sizeof(it));
    it.uid[0] = uid_byte;
    it.uid_len = 1;
    strncpy(it.name, name, MAX_ITEM_NAME - 1);
    it.quantity = 1;
    strncpy(it.location, "L", MAX_LOCATION - 1);
    return it;
}

#define MAIN_PATH "warehouse.bin"
#define TMP_PATH  MAIN_PATH ".tmp"
#define BAD_PATH  MAIN_PATH ".bad"
#define BAD1_PATH MAIN_PATH ".bad1"

static size_t file_record_count(FakePort* fp, const char* path) {
    const FakeFile* f = fake_find(fp, path);
    return f ? f->len / sizeof(StockItem) : 0;
}

static bool file_has_uid(FakePort* fp, const char* path, uint8_t uid) {
    const FakeFile* f = fake_find(fp, path);
    if(!f) {
        return false;
    }
    const StockItem* it = (const StockItem*)f->data;
    for(size_t i = 0; i < f->len / sizeof(StockItem); i++) {
        if(it[i].uid[0] == uid) {
            return true;
        }
    }
    return false;
}

static bool any_bad_has_uid(FakePort* fp, uint8_t uid) {
    char p[STOCK_PATH_MAX];
    for(int i = 0; i < STORE_BACKUP_SLOTS; i++) {
        if(i == 0) {
            snprintf(p, sizeof(p), "%s.bad", MAIN_PATH);
        } else {
            snprintf(p, sizeof(p), "%s.bad%d", MAIN_PATH, i);
        }
        if(file_has_uid(fp, p, uid)) {
            return true;
        }
    }
    return false;
}

/* True if `uid` is findable anywhere at all -- main, tmp, or any backup slot.
 */
static bool survives_anywhere(FakePort* fp, uint8_t uid) {
    return file_has_uid(fp, MAIN_PATH, uid) || file_has_uid(fp, TMP_PATH, uid) ||
           any_bad_has_uid(fp, uid);
}

static void test_missing_creates_no_backup(void) {
    FakePort fp;
    fake_port_init(&fp);
    StockStoragePort port;
    fake_port_bind(&fp, &port);

    StockItem* items = NULL;
    size_t count = 0;
    StockLoadResult r = stock_recovery_load(&port, MAIN_PATH, &items, &count);

    assert(r.status == StoreLoadMissing);
    assert(!r.save_blocked);
    assert(count == 0);
    assert(!fake_exists(&fp, BAD_PATH));
    free(items);

    printf("test_missing_creates_no_backup: PASSED\n");
}

static void test_corrupt_then_upsert_keeps_original_in_bad(void) {
    FakePort fp;
    fake_port_init(&fp);
    StockStoragePort port;
    fake_port_bind(&fp, &port);

    const uint8_t garbage[3] = {0xDE, 0xAD, 0xBE};
    fake_set_bytes(&fp, MAIN_PATH, garbage, sizeof(garbage));

    StockItem item = make_item(0xAA, "New");
    StockWriteResult result = stock_recovery_upsert(&port, MAIN_PATH, &item);

    assert(result.outcome == StockWriteOkQuarantined);
    assert(strcmp(result.kept_path, BAD_PATH) == 0);

    FakeFile* bad = fake_find(&fp, BAD_PATH);
    assert(bad && bad->len == sizeof(garbage) && memcmp(bad->data, garbage, sizeof(garbage)) == 0);

    StockItem* items = NULL;
    size_t count = 0;
    const StockReadOutcome final_ro = fake_read_all(&fp, MAIN_PATH, &items, &count);
    assert(final_ro == StockReadOk);
    assert(count == 1);
    assert(items[0].uid[0] == 0xAA);
    free(items);

    printf("test_corrupt_then_upsert_keeps_original_in_bad: PASSED\n");
}

static void test_second_corruption_lands_in_bad1_keeps_first(void) {
    FakePort fp;
    fake_port_init(&fp);
    StockStoragePort port;
    fake_port_bind(&fp, &port);

    const uint8_t first[3] = {0x01, 0x02, 0x03};
    fake_set_bytes(&fp, MAIN_PATH, first, sizeof(first));
    StockItem item_a = make_item(0xAA, "A");
    StockWriteResult r1 = stock_recovery_upsert(&port, MAIN_PATH, &item_a);
    assert(r1.outcome == StockWriteOkQuarantined);

    const uint8_t second[5] = {0x04, 0x05, 0x06, 0x07, 0x08};
    fake_set_bytes(&fp, MAIN_PATH, second, sizeof(second));
    StockItem item_b = make_item(0xBB, "B");
    StockWriteResult r2 = stock_recovery_upsert(&port, MAIN_PATH, &item_b);
    assert(r2.outcome == StockWriteOkQuarantined);
    assert(strcmp(r2.kept_path, BAD1_PATH) == 0);

    FakeFile* bad = fake_find(&fp, BAD_PATH);
    assert(bad && bad->len == sizeof(first) && memcmp(bad->data, first, sizeof(first)) == 0);
    FakeFile* bad1 = fake_find(&fp, BAD1_PATH);
    assert(bad1 && bad1->len == sizeof(second) && memcmp(bad1->data, second, sizeof(second)) == 0);

    printf("test_second_corruption_lands_in_bad1_keeps_first: PASSED\n");
}

static void test_all_backup_slots_full_blocks_without_overwriting(void) {
    FakePort fp;
    fake_port_init(&fp);
    StockStoragePort port;
    fake_port_bind(&fp, &port);

    char slot_path[STOCK_PATH_MAX];
    for(int i = 0; i < STORE_BACKUP_SLOTS; i++) {
        const uint8_t marker[1] = {(uint8_t)(0xF0 + i)};
        if(i == 0) {
            snprintf(slot_path, sizeof(slot_path), "%s.bad", MAIN_PATH);
        } else {
            snprintf(slot_path, sizeof(slot_path), "%s.bad%d", MAIN_PATH, i);
        }
        fake_set_bytes(&fp, slot_path, marker, sizeof(marker));
    }

    const uint8_t garbage[3] = {0xAA, 0xBB, 0xCC};
    fake_set_bytes(&fp, MAIN_PATH, garbage, sizeof(garbage));

    StockItem item = make_item(0x01, "X");
    StockWriteResult result = stock_recovery_upsert(&port, MAIN_PATH, &item);

    assert(result.outcome == StockWriteBlockedSlotsFull);
    FakeFile* main_file = fake_find(&fp, MAIN_PATH);
    assert(
        main_file && main_file->len == sizeof(garbage) &&
        memcmp(main_file->data, garbage, sizeof(garbage)) == 0);

    for(int i = 0; i < STORE_BACKUP_SLOTS; i++) {
        if(i == 0) {
            snprintf(slot_path, sizeof(slot_path), "%s.bad", MAIN_PATH);
        } else {
            snprintf(slot_path, sizeof(slot_path), "%s.bad%d", MAIN_PATH, i);
        }
        FakeFile* slot = fake_find(&fp, slot_path);
        const uint8_t expected = (uint8_t)(0xF0 + i);
        assert(slot && slot->len == 1 && slot->data[0] == expected);
    }

    printf("test_all_backup_slots_full_blocks_without_overwriting: PASSED\n");
}

static void test_quarantine_rename_failure_is_distinct_from_slots_full(void) {
    FakePort fp;
    fake_port_init(&fp);
    StockStoragePort port;
    fake_port_bind(&fp, &port);

    const uint8_t garbage[3] = {0x01, 0x02, 0x03};
    fake_set_bytes(&fp, MAIN_PATH, garbage, sizeof(garbage));

    /* A free slot exists, but the rename into it fails: crash the quarantine's
   * own rename call right after it removes its (absent) destination, which
   * still returns false without ever touching main. */
    fp.crash_point = CrashAfterRemoveDst;

    StockItem item = make_item(0x01, "X");
    StockWriteResult result = stock_recovery_upsert(&port, MAIN_PATH, &item);

    assert(result.outcome == StockWriteBlockedQuarantineFailed);
    FakeFile* main_file = fake_find(&fp, MAIN_PATH);
    assert(
        main_file && main_file->len == sizeof(garbage) &&
        memcmp(main_file->data, garbage, sizeof(garbage)) == 0);
    assert(!fake_exists(&fp, BAD_PATH));

    printf("test_quarantine_rename_failure_is_distinct_from_slots_full: PASSED\n");
}

static void test_over_limit_quarantines_as_garbage(void) {
    FakePort fp;
    fake_port_init(&fp);
    StockStoragePort port;
    fake_port_bind(&fp, &port);

    const size_t huge_count = STOCK_DB_MAX_BYTES / sizeof(StockItem) + 1;
    const size_t huge_len = huge_count * sizeof(StockItem);
    uint8_t* huge = calloc(1, huge_len);
    assert(huge);
    fake_set_bytes(&fp, MAIN_PATH, huge, huge_len);
    free(huge);

    StockItem item = make_item(0x01, "X");
    StockWriteResult result = stock_recovery_upsert(&port, MAIN_PATH, &item);

    /* Oversized-but-well-formed is confirmed bad, not merely "unknown": it gets
   * quarantined (like framing corruption), never blocked forever. */
    assert(result.outcome == StockWriteOkQuarantined);
    assert(strcmp(result.kept_path, BAD_PATH) == 0);
    FakeFile* bad = fake_find(&fp, BAD_PATH);
    assert(bad && bad->len == huge_len);

    StockItem* items = NULL;
    size_t count = 0;
    const StockReadOutcome final_ro = fake_read_all(&fp, MAIN_PATH, &items, &count);
    assert(final_ro == StockReadOk);
    assert(count == 1);
    free(items);

    printf("test_over_limit_quarantines_as_garbage: PASSED\n");
}

static void test_read_error_blocks_without_quarantine(void) {
    FakePort fp;
    fake_port_init(&fp);
    StockStoragePort port;
    fake_port_bind(&fp, &port);

    const uint8_t bytes[sizeof(StockItem)] = {0};
    fake_set_bytes(&fp, MAIN_PATH, bytes, sizeof(bytes));
    strncpy(fp.force_blocked_read_path, MAIN_PATH, sizeof(fp.force_blocked_read_path) - 1);

    StockItem item = make_item(0x01, "X");
    StockWriteResult result = stock_recovery_upsert(&port, MAIN_PATH, &item);

    assert(result.outcome == StockWriteBlockedIoError);
    assert(!fake_exists(&fp, BAD_PATH));
    FakeFile* main_file = fake_find(&fp, MAIN_PATH);
    assert(main_file && main_file->len == sizeof(bytes));

    printf("test_read_error_blocks_without_quarantine: PASSED\n");
}

static void test_crash_mid_save_recovers_from_tmp(void) {
    FakePort fp;
    fake_port_init(&fp);
    StockStoragePort port;
    fake_port_bind(&fp, &port);

    StockItem item_a = make_item(0xAA, "A");
    assert(stock_recovery_upsert(&port, MAIN_PATH, &item_a).outcome == StockWriteOk);

    fp.crash_point = CrashAfterRemoveDst;
    StockItem item_b = make_item(0xBB, "B");
    StockWriteResult crashed = stock_recovery_upsert(&port, MAIN_PATH, &item_b);
    assert(crashed.outcome == StockWriteFailed);
    assert(!fake_exists(&fp, MAIN_PATH));
    assert(fake_exists(&fp, TMP_PATH));

    StockItem* items = NULL;
    size_t count = 0;
    StockLoadResult recovered = stock_recovery_load(&port, MAIN_PATH, &items, &count);
    assert(recovered.status == StoreLoadOk);
    assert(count == 2);
    assert(fake_exists(&fp, MAIN_PATH));
    assert(!fake_exists(&fp, TMP_PATH));
    free(items);

    printf("test_crash_mid_save_recovers_from_tmp: PASSED\n");
}

static void test_missing_main_with_undecodable_tmp_quarantines_tmp(void) {
    FakePort fp;
    fake_port_init(&fp);
    StockStoragePort port;
    fake_port_bind(&fp, &port);

    const uint8_t garbage[3] = {0x9, 0x9, 0x9};
    fake_set_bytes(&fp, TMP_PATH, garbage, sizeof(garbage));

    StockItem* items = NULL;
    size_t count = 0;
    StockLoadResult r = stock_recovery_load(&port, MAIN_PATH, &items, &count);

    assert(r.status == StoreLoadCorrupt);
    assert(!r.save_blocked);
    assert(strcmp(r.kept_path, BAD_PATH) == 0);
    assert(count == 0);
    assert(!fake_exists(&fp, TMP_PATH));
    FakeFile* bad = fake_find(&fp, BAD_PATH);
    assert(bad && bad->len == sizeof(garbage) && memcmp(bad->data, garbage, sizeof(garbage)) == 0);
    free(items);

    printf("test_missing_main_with_undecodable_tmp_quarantines_tmp: PASSED\n");
}

static void test_tmp_read_error_blocks_without_quarantine_when_main_missing(void) {
    FakePort fp;
    fake_port_init(&fp);
    StockStoragePort port;
    fake_port_bind(&fp, &port);

    const uint8_t bytes[sizeof(StockItem)] = {0};
    fake_set_bytes(&fp, TMP_PATH, bytes, sizeof(bytes));
    strncpy(fp.force_blocked_read_path, TMP_PATH, sizeof(fp.force_blocked_read_path) - 1);

    StockItem* items = NULL;
    size_t count = 0;
    StockLoadResult r = stock_recovery_load(&port, MAIN_PATH, &items, &count);

    assert(r.status == StoreLoadIoError);
    assert(r.save_blocked);
    assert(!fake_exists(&fp, BAD_PATH));
    assert(fake_exists(&fp, TMP_PATH));
    free(items);

    printf("test_tmp_read_error_blocks_without_quarantine_when_main_missing: "
           "PASSED\n");
}

static void test_promote_failure_blocks_without_deleting_tmp(void) {
    FakePort fp;
    fake_port_init(&fp);
    StockStoragePort port;
    fake_port_bind(&fp, &port);

    StockItem item_a = make_item(0xAA, "A");
    const StockItem items_before[] = {item_a};
    fake_set_items(&fp, TMP_PATH, items_before, 1);

    /* main is missing; .tmp parses fine but the promote (rename tmp->main) itself
   * crashes right after removing the (absent) destination. */
    fp.crash_point = CrashAfterRemoveDst;

    StockItem* items = NULL;
    size_t count = 0;
    StockLoadResult r = stock_recovery_load(&port, MAIN_PATH, &items, &count);

    assert(r.status == StoreLoadIoError);
    assert(r.save_blocked);
    assert(!fake_exists(&fp, BAD_PATH));
    FakeFile* tmp = fake_find(&fp, TMP_PATH);
    assert(tmp && tmp->len == sizeof(StockItem));
    free(items);

    printf("test_promote_failure_blocks_without_deleting_tmp: PASSED\n");
}

/* An earlier save's rename fails right
 * after removing the destination (main goes missing, .tmp keeps the full merged
 * content); the *next* save recovers/promotes that .tmp normally, then its own
 * new write fails. The already-promoted data must survive that second failure
 * untouched. */
static void test_promote_then_next_write_failure_preserves_promoted_data(void) {
    FakePort fp;
    fake_port_init(&fp);
    StockStoragePort port;
    fake_port_bind(&fp, &port);

    StockItem item_a = make_item(0xAA, "A");
    assert(stock_recovery_upsert(&port, MAIN_PATH, &item_a).outcome == StockWriteOk);

    fp.crash_point = CrashAfterRemoveDst;
    StockItem item_b = make_item(0xBB, "B");
    assert(stock_recovery_upsert(&port, MAIN_PATH, &item_b).outcome == StockWriteFailed);
    assert(!fake_exists(&fp, MAIN_PATH));
    assert(fake_exists(&fp, TMP_PATH));

    strncpy(fp.force_write_fail_path, TMP_PATH, sizeof(fp.force_write_fail_path) - 1);
    StockItem item_c = make_item(0xCC, "C");
    StockWriteResult second = stock_recovery_upsert(&port, MAIN_PATH, &item_c);
    assert(second.outcome == StockWriteFailed);

    /* The promote (main = [A, B]) must have completed before the new write was
   * attempted, so main survives even though the new save failed. */
    StockItem* items = NULL;
    size_t count = 0;
    const StockReadOutcome final_ro = fake_read_all(&fp, MAIN_PATH, &items, &count);
    assert(final_ro == StockReadOk);
    assert(count == 2);
    free(items);

    printf("test_promote_then_next_write_failure_preserves_promoted_data: PASSED\n");
}

static void test_delete_blocked_on_read_error_never_touches_file(void) {
    FakePort fp;
    fake_port_init(&fp);
    StockStoragePort port;
    fake_port_bind(&fp, &port);

    StockItem item_a = make_item(0xAA, "A");
    const StockItem items_before[] = {item_a};
    fake_set_items(&fp, MAIN_PATH, items_before, 1);
    strncpy(fp.force_blocked_read_path, MAIN_PATH, sizeof(fp.force_blocked_read_path) - 1);

    StockWriteResult result = stock_recovery_delete_at(&port, MAIN_PATH, 0);

    assert(result.outcome == StockWriteBlockedIoError);
    FakeFile* main_file = fake_find(&fp, MAIN_PATH);
    assert(main_file && main_file->len == sizeof(StockItem));
    assert(!fake_exists(&fp, TMP_PATH));

    printf("test_delete_blocked_on_read_error_never_touches_file: PASSED\n");
}

static void test_delete_removes_the_right_record(void) {
    FakePort fp;
    fake_port_init(&fp);
    StockStoragePort port;
    fake_port_bind(&fp, &port);

    StockItem item_a = make_item(0xAA, "A");
    StockItem item_b = make_item(0xBB, "B");
    const StockItem items_before[] = {item_a, item_b};
    fake_set_items(&fp, MAIN_PATH, items_before, 2);

    StockWriteResult result = stock_recovery_delete_at(&port, MAIN_PATH, 0);
    assert(result.outcome == StockWriteOk);

    StockItem* items = NULL;
    size_t count = 0;
    const StockReadOutcome final_ro = fake_read_all(&fp, MAIN_PATH, &items, &count);
    assert(final_ro == StockReadOk);
    assert(count == 1);
    assert(items[0].uid[0] == 0xBB);
    free(items);

    printf("test_delete_removes_the_right_record: PASSED\n");
}

/* --- Fix round 1: main decoding fine is not proof it's the last complete save
 * --- */

static void test_partial_copy_promotes_full_tmp(void) {
    FakePort fp;
    fake_port_init(&fp);
    StockStoragePort port;
    fake_port_bind(&fp, &port);

    StockItem a = make_item(0xA1, "A");
    StockItem b = make_item(0xB2, "B");
    StockItem c = make_item(0xC3, "C");

    assert(stock_recovery_upsert(&port, MAIN_PATH, &a).outcome == StockWriteOk);

    fp.crash_point = CrashDuringCopy;
    StockWriteResult r1 = stock_recovery_upsert(&port, MAIN_PATH, &b);
    /* An even record count truncates to exactly one whole record: main decodes
   * fine as [A] even though the intended save ([A, B]) is still fully sitting
   * in .tmp. */
    assert(file_record_count(&fp, MAIN_PATH) == 1);
    assert(file_has_uid(&fp, TMP_PATH, 0xB2));

    StockWriteResult r2 = stock_recovery_upsert(&port, MAIN_PATH, &c);
    assert(r2.outcome == StockWriteOk);
    assert(file_record_count(&fp, MAIN_PATH) == 3);
    assert(survives_anywhere(&fp, 0xA1));
    assert(survives_anywhere(&fp, 0xB2));
    assert(survives_anywhere(&fp, 0xC3));
    assert(!fake_exists(&fp, TMP_PATH));

    (void)r1;
    printf("test_partial_copy_promotes_full_tmp: PASSED\n");
}

static void test_zero_byte_main_after_crash_promotes_tmp(void) {
    FakePort fp;
    fake_port_init(&fp);
    StockStoragePort port;
    fake_port_bind(&fp, &port);

    StockItem a = make_item(0xA1, "A");
    StockItem b = make_item(0xB2, "B");
    StockItem c = make_item(0xC3, "C");
    const StockItem abc[] = {a, b, c};
    fake_set_items(&fp, TMP_PATH, abc, 3);
    fake_set_bytes(&fp, MAIN_PATH, NULL, 0); /* an empty file is a trivial byte-prefix too */

    StockItem d = make_item(0xD4, "D");
    StockWriteResult r = stock_recovery_upsert(&port, MAIN_PATH, &d);

    assert(r.outcome == StockWriteOk);
    assert(file_record_count(&fp, MAIN_PATH) == 4);
    assert(survives_anywhere(&fp, 0xA1));
    assert(survives_anywhere(&fp, 0xB2));
    assert(survives_anywhere(&fp, 0xC3));
    assert(survives_anywhere(&fp, 0xD4));

    printf("test_zero_byte_main_after_crash_promotes_tmp: PASSED\n");
}

static void test_identical_main_and_tmp_removes_tmp(void) {
    FakePort fp;
    fake_port_init(&fp);
    StockStoragePort port;
    fake_port_bind(&fp, &port);

    StockItem a = make_item(0xA1, "A");
    StockItem b = make_item(0xB2, "B");
    const StockItem ab[] = {a, b};
    fake_set_items(&fp, MAIN_PATH, ab, 2);
    fake_set_items(&fp, TMP_PATH, ab, 2);

    StockItem* items = NULL;
    size_t count = 0;
    StockLoadResult r = stock_recovery_load(&port, MAIN_PATH, &items, &count);

    assert(r.status == StoreLoadOk);
    assert(count == 2);
    assert(!fake_exists(&fp, TMP_PATH));
    free(items);

    printf("test_identical_main_and_tmp_removes_tmp: PASSED\n");
}

static void test_unrelated_tmp_is_cleared_main_kept(void) {
    FakePort fp;
    fake_port_init(&fp);
    StockStoragePort port;
    fake_port_bind(&fp, &port);

    StockItem a = make_item(0xA1, "A");
    const StockItem main_content[] = {a};
    fake_set_items(&fp, MAIN_PATH, main_content, 1);

    /* Not a prefix, not equal, and not even record-aligned: a torn write of
   * something else entirely. */
    const uint8_t junk[5] = {9, 9, 9, 9, 9};
    fake_set_bytes(&fp, TMP_PATH, junk, sizeof(junk));

    StockItem* items = NULL;
    size_t count = 0;
    StockLoadResult r = stock_recovery_load(&port, MAIN_PATH, &items, &count);

    assert(r.status == StoreLoadOk);
    assert(count == 1);
    assert(items[0].uid[0] == 0xA1);
    assert(!fake_exists(&fp, TMP_PATH));
    free(items);

    printf("test_unrelated_tmp_is_cleared_main_kept: PASSED\n");
}

static void test_main_blocked_never_promotes_tmp_over_it(void) {
    FakePort fp;
    fake_port_init(&fp);
    StockStoragePort port;
    fake_port_bind(&fp, &port);

    StockItem a = make_item(0xA1, "A");
    StockItem b = make_item(0xB2, "B");
    const StockItem ab[] = {a, b};
    fake_set_items(&fp, MAIN_PATH, ab, 2);
    fake_set_bytes(&fp, TMP_PATH, NULL, 0); /* leftover empty tmp: would parse "fine" */
    strncpy(fp.force_blocked_read_path, MAIN_PATH, sizeof(fp.force_blocked_read_path) - 1);

    StockItem n = make_item(0xEE, "N");
    StockWriteResult r = stock_recovery_upsert(&port, MAIN_PATH, &n);

    assert(r.outcome == StockWriteBlockedIoError);
    assert(survives_anywhere(&fp, 0xA1));
    assert(survives_anywhere(&fp, 0xB2));

    printf("test_main_blocked_never_promotes_tmp_over_it: PASSED\n");
}

static void test_main_stat_error_never_promotes_tmp_over_it(void) {
    FakePort fp;
    fake_port_init(&fp);
    StockStoragePort port;
    fake_port_bind(&fp, &port);

    StockItem a = make_item(0xA1, "A");
    StockItem b = make_item(0xB2, "B");
    const StockItem ab[] = {a, b};
    const StockItem only_a[] = {a};
    fake_set_items(&fp, MAIN_PATH, ab, 2);
    fake_set_items(&fp, TMP_PATH, only_a, 1); /* a good but stale tmp */
    strncpy(fp.force_stat_error_path, MAIN_PATH, sizeof(fp.force_stat_error_path) - 1);

    StockItem* items = NULL;
    size_t count = 0;
    StockLoadResult r = stock_recovery_load(&port, MAIN_PATH, &items, &count);
    free(items);
    fp.force_stat_error_path[0] = '\0';

    assert(r.status == StoreLoadIoError);
    assert(r.save_blocked);
    assert(survives_anywhere(&fp, 0xA1));
    assert(survives_anywhere(&fp, 0xB2));

    printf("test_main_stat_error_never_promotes_tmp_over_it: PASSED\n");
}

static void test_oversized_main_quarantined_before_tmp_promotes(void) {
    FakePort fp;
    fake_port_init(&fp);
    StockStoragePort port;
    fake_port_bind(&fp, &port);

    const size_t n = STOCK_DB_MAX_BYTES / sizeof(StockItem) + 1;
    StockItem* big = calloc(n, sizeof(StockItem));
    assert(big);
    for(size_t i = 0; i < n; i++) {
        big[i] = make_item(0x10, "x");
    }
    big[5] = make_item(0xB2, "B");
    fake_set_items(&fp, MAIN_PATH, big, n);
    free(big);

    StockItem a = make_item(0xA1, "A");
    const StockItem only_a[] = {a};
    fake_set_items(&fp, TMP_PATH, only_a, 1);

    StockItem* items = NULL;
    size_t count = 0;
    StockLoadResult r = stock_recovery_load(&port, MAIN_PATH, &items, &count);

    assert(r.status == StoreLoadCorrupt);
    assert(!r.save_blocked);
    assert(strcmp(r.kept_path, BAD_PATH) == 0);
    assert(count == 1);
    assert(items[0].uid[0] == 0xA1);
    assert(survives_anywhere(&fp, 0xB2)); /* preserved inside the quarantined .bad */
    free(items);

    printf("test_oversized_main_quarantined_before_tmp_promotes: PASSED\n");
}

static void test_failed_write_removes_torn_tmp(void) {
    FakePort fp;
    fake_port_init(&fp);
    StockStoragePort port;
    fake_port_bind(&fp, &port);

    StockItem a = make_item(0xA1, "A");
    assert(stock_recovery_upsert(&port, MAIN_PATH, &a).outcome == StockWriteOk);

    strncpy(fp.force_write_fail_path, TMP_PATH, sizeof(fp.force_write_fail_path) - 1);
    StockItem b = make_item(0xB2, "B");
    StockWriteResult r = stock_recovery_upsert(&port, MAIN_PATH, &b);
    fp.force_write_fail_path[0] = '\0';

    assert(r.outcome == StockWriteFailed);
    /* The fake leaves a 0-byte file where a real fopen("wb") would too; the save
   * path must clean that up immediately, since main is already known good at
   * that point. */
    assert(!fake_exists(&fp, TMP_PATH));
    assert(file_record_count(&fp, MAIN_PATH) == 1);
    assert(survives_anywhere(&fp, 0xA1));

    printf("test_failed_write_removes_torn_tmp: PASSED\n");
}

static void test_recover_after_remove_then_partial_promote(void) {
    FakePort fp;
    fake_port_init(&fp);
    StockStoragePort port;
    fake_port_bind(&fp, &port);

    StockItem a = make_item(0xA1, "A");
    StockItem b = make_item(0xB2, "B");
    StockItem c = make_item(0xC3, "C");

    assert(stock_recovery_upsert(&port, MAIN_PATH, &a).outcome == StockWriteOk);
    fp.crash_point = CrashAfterRemoveDst;
    assert(stock_recovery_upsert(&port, MAIN_PATH, &b).outcome == StockWriteFailed);
    /* main is gone, .tmp = [A, B] */

    /* The previous crash_point was consumed by fake_rename via the upsert() call
   * above; cppcheck can't see through the port's function pointer to that read.
   */
    // cppcheck-suppress redundantAssignment
    fp.crash_point = CrashDuringCopy;
    StockWriteResult r1 = stock_recovery_upsert(&port, MAIN_PATH, &c);
    /* The promotion attempt itself now gets a partial (but record-aligned) copy.
   */
    StockWriteResult r2 = stock_recovery_upsert(&port, MAIN_PATH, &c);

    assert(r2.outcome == StockWriteOk);
    assert(survives_anywhere(&fp, 0xA1));
    assert(survives_anywhere(&fp, 0xB2));
    assert(survives_anywhere(&fp, 0xC3));

    (void)r1;
    printf("test_recover_after_remove_then_partial_promote: PASSED\n");
}

static void test_over_limit_write_is_refused(void) {
    FakePort fp;
    fake_port_init(&fp);
    StockStoragePort port;
    fake_port_bind(&fp, &port);
    port.max_bytes = sizeof(StockItem); /* room for exactly one record */

    StockItem a = make_item(0xA1, "A");
    assert(stock_recovery_upsert(&port, MAIN_PATH, &a).outcome == StockWriteOk);

    StockItem b = make_item(0xB2, "B");
    StockWriteResult r = stock_recovery_upsert(&port, MAIN_PATH, &b);

    assert(r.outcome == StockWriteRefusedOverLimit);
    assert(file_record_count(&fp, MAIN_PATH) == 1);
    assert(survives_anywhere(&fp, 0xA1));
    assert(!survives_anywhere(&fp, 0xB2));
    assert(!fake_exists(&fp, TMP_PATH));

    printf("test_over_limit_write_is_refused: PASSED\n");
}

static void test_upsert_failure_after_quarantine_keeps_kept_path(void) {
    FakePort fp;
    fake_port_init(&fp);
    StockStoragePort port;
    fake_port_bind(&fp, &port);

    const uint8_t garbage[3] = {1, 2, 3};
    fake_set_bytes(&fp, MAIN_PATH, garbage, sizeof(garbage));
    strncpy(fp.force_write_fail_path, TMP_PATH, sizeof(fp.force_write_fail_path) - 1);

    StockItem item = make_item(0xA1, "A");
    StockWriteResult r = stock_recovery_upsert(&port, MAIN_PATH, &item);

    /* The quarantine of the corrupt main already happened on disk; the caller
   * must still learn where it went even though the subsequent save itself then
   * failed. */
    assert(r.outcome == StockWriteFailed);
    assert(strcmp(r.kept_path, BAD_PATH) == 0);
    const FakeFile* bad = fake_find(&fp, BAD_PATH);
    assert(bad && bad->len == sizeof(garbage));

    printf("test_upsert_failure_after_quarantine_keeps_kept_path: PASSED\n");
}

/* --- Round 2: reconcile_main_with_tmp must never hand back a stale main --- */

static void run_promote_crash_blocks_and_preserves_b(RenameCrashPoint crash) {
    FakePort fp;
    fake_port_init(&fp);
    StockStoragePort port;
    fake_port_bind(&fp, &port);

    StockItem a = make_item(0xA1, "A");
    StockItem b = make_item(0xB2, "B");
    const StockItem only_a[] = {a};
    const StockItem ab[] = {a, b};
    fake_set_items(&fp, MAIN_PATH, only_a, 1);
    fake_set_items(&fp, TMP_PATH, ab, 2);

    fp.crash_point = crash;
    StockItem c = make_item(0xC3, "C");
    StockWriteResult r = stock_recovery_upsert(&port, MAIN_PATH, &c);

    assert(r.outcome == StockWriteBlockedIoError);
    assert(survives_anywhere(&fp, 0xB2));
}

/* T10a: the promote's rename crashes right after removing its (absent)
 * destination -- main ends up missing, tmp untouched. */
static void test_promote_crash_after_remove_dst_blocks_and_preserves_b(void) {
    run_promote_crash_blocks_and_preserves_b(CrashAfterRemoveDst);
    printf("test_promote_crash_after_remove_dst_blocks_and_preserves_b: PASSED\n");
}

/* T10b: the promote's copy itself is interrupted -- main ends up with a
 * (record-aligned) partial copy. */
static void test_promote_crash_during_copy_blocks_and_preserves_b(void) {
    run_promote_crash_blocks_and_preserves_b(CrashDuringCopy);
    printf("test_promote_crash_during_copy_blocks_and_preserves_b: PASSED\n");
}

/* T10c: the copy step completes in full and only removing the source (tmp)
 * crashes -- main is *already* the full, correct content on disk by the time
 * `rename` returns false. The old code trusted the stale in-memory `main`
 * snapshot taken before this rename and would have merged on top of it,
 * silently dropping B on the next save; the fix blocks instead. A retry then
 * finds main and tmp byte-identical, clears tmp, and unblocks. */
static void test_promote_crash_after_copy_blocks_and_preserves_b(void) {
    FakePort fp;
    fake_port_init(&fp);
    StockStoragePort port;
    fake_port_bind(&fp, &port);

    StockItem a = make_item(0xA1, "A");
    StockItem b = make_item(0xB2, "B");
    const StockItem only_a[] = {a};
    const StockItem ab[] = {a, b};
    fake_set_items(&fp, MAIN_PATH, only_a, 1);
    fake_set_items(&fp, TMP_PATH, ab, 2);

    fp.crash_point = CrashAfterCopyBeforeRemoveSrc;
    StockItem c = make_item(0xC3, "C");
    StockWriteResult r = stock_recovery_upsert(&port, MAIN_PATH, &c);

    assert(r.outcome == StockWriteBlockedIoError);
    assert(file_record_count(&fp, MAIN_PATH) == 2); /* the copy already completed */
    assert(survives_anywhere(&fp, 0xB2));

    StockWriteResult retry = stock_recovery_upsert(&port, MAIN_PATH, &c);
    assert(retry.outcome == StockWriteOk);
    assert(!fake_exists(&fp, TMP_PATH));
    assert(survives_anywhere(&fp, 0xA1));
    assert(survives_anywhere(&fp, 0xB2));
    assert(survives_anywhere(&fp, 0xC3));

    printf("test_promote_crash_after_copy_blocks_and_preserves_b: PASSED\n");
}

/* The .tmp stat itself fails -- block without touching either file. */
static void test_tmp_stat_error_blocks_and_preserves_b(void) {
    FakePort fp;
    fake_port_init(&fp);
    StockStoragePort port;
    fake_port_bind(&fp, &port);

    StockItem a = make_item(0xA1, "A");
    StockItem b = make_item(0xB2, "B");
    const StockItem only_a[] = {a};
    const StockItem ab[] = {a, b};
    fake_set_items(&fp, MAIN_PATH, only_a, 1);
    fake_set_items(&fp, TMP_PATH, ab, 2);
    strncpy(fp.force_stat_error_path, TMP_PATH, sizeof(fp.force_stat_error_path) - 1);

    StockItem c = make_item(0xC3, "C");
    StockWriteResult r = stock_recovery_upsert(&port, MAIN_PATH, &c);

    assert(r.outcome == StockWriteBlockedIoError);
    assert(survives_anywhere(&fp, 0xB2));

    printf("test_tmp_stat_error_blocks_and_preserves_b: PASSED\n");
}

/* The .tmp stat is fine but reading its raw bytes fails (I/O error or
 * malloc) -- block without touching either file. */
static void test_tmp_read_raw_failure_blocks_and_preserves_b(void) {
    FakePort fp;
    fake_port_init(&fp);
    StockStoragePort port;
    fake_port_bind(&fp, &port);

    StockItem a = make_item(0xA1, "A");
    StockItem b = make_item(0xB2, "B");
    const StockItem only_a[] = {a};
    const StockItem ab[] = {a, b};
    fake_set_items(&fp, MAIN_PATH, only_a, 1);
    fake_set_items(&fp, TMP_PATH, ab, 2);
    strncpy(fp.force_blocked_read_path, TMP_PATH, sizeof(fp.force_blocked_read_path) - 1);

    StockItem c = make_item(0xC3, "C");
    StockWriteResult r = stock_recovery_upsert(&port, MAIN_PATH, &c);

    assert(r.outcome == StockWriteBlockedIoError);
    assert(survives_anywhere(&fp, 0xB2));

    printf("test_tmp_read_raw_failure_blocks_and_preserves_b: PASSED\n");
}

int main(void) {
    printf("Running stock_recovery tests...\n");
    test_missing_creates_no_backup();
    test_corrupt_then_upsert_keeps_original_in_bad();
    test_second_corruption_lands_in_bad1_keeps_first();
    test_all_backup_slots_full_blocks_without_overwriting();
    test_quarantine_rename_failure_is_distinct_from_slots_full();
    test_over_limit_quarantines_as_garbage();
    test_read_error_blocks_without_quarantine();
    test_crash_mid_save_recovers_from_tmp();
    test_missing_main_with_undecodable_tmp_quarantines_tmp();
    test_tmp_read_error_blocks_without_quarantine_when_main_missing();
    test_promote_failure_blocks_without_deleting_tmp();
    test_promote_then_next_write_failure_preserves_promoted_data();
    test_delete_blocked_on_read_error_never_touches_file();
    test_delete_removes_the_right_record();
    test_partial_copy_promotes_full_tmp();
    test_zero_byte_main_after_crash_promotes_tmp();
    test_identical_main_and_tmp_removes_tmp();
    test_unrelated_tmp_is_cleared_main_kept();
    test_main_blocked_never_promotes_tmp_over_it();
    test_main_stat_error_never_promotes_tmp_over_it();
    test_oversized_main_quarantined_before_tmp_promotes();
    test_failed_write_removes_torn_tmp();
    test_recover_after_remove_then_partial_promote();
    test_over_limit_write_is_refused();
    test_upsert_failure_after_quarantine_keeps_kept_path();
    test_promote_crash_after_remove_dst_blocks_and_preserves_b();
    test_promote_crash_during_copy_blocks_and_preserves_b();
    test_promote_crash_after_copy_blocks_and_preserves_b();
    test_tmp_stat_error_blocks_and_preserves_b();
    test_tmp_read_raw_failure_blocks_and_preserves_b();
    printf("All tests PASSED!\n");
    return 0;
}
