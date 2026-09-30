#include "include/domain/sensors_recovery.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define FAKE_MAX_FILES 16
#define FAKE_MAX_BYTES 2048
#define MAIN_PATH      "/fake/sensors.bin"
#define TMP_PATH       MAIN_PATH ".tmp"

typedef struct {
    char path[64];
    uint8_t data[FAKE_MAX_BYTES];
    size_t len;
    bool present;
} FakeFile;

typedef enum {
    FakeRenameNormal = 0,
    FakeRenameAlwaysFail, // fails before touching anything
    FakeRenameCrashAfterRemoveDst, // dst removed, nothing written; src intact
    FakeRenameCrashPartialCopy, // dst removed, partially written; src intact
    FakeRenameCrashBeforeRemoveSrc, // dst fully written; src NOT removed (both present)
} FakeRenameMode;

typedef struct {
    FakeFile files[FAKE_MAX_FILES];
    FakeRenameMode rename_mode;
    bool force_stat_error; // simulates a device I/O fault stat'ing ANY path
    // Models the firmware's partial-read-with-error-set quirk (storage_ext.c): our port's
    // read_all contract collapses that to a plain false for this exact path when set.
    char short_read_path[64];
    int write_calls;
    int rename_calls;
} FakeFs;

static FakeFile* fake_find(FakeFs* fs, const char* path, bool create) {
    for(int i = 0; i < FAKE_MAX_FILES; i++) {
        if(fs->files[i].present && strcmp(fs->files[i].path, path) == 0) {
            return &fs->files[i];
        }
    }
    if(!create) {
        return NULL;
    }
    for(int i = 0; i < FAKE_MAX_FILES; i++) {
        if(!fs->files[i].present) {
            fs->files[i].present = true;
            snprintf(fs->files[i].path, sizeof(fs->files[i].path), "%s", path);
            fs->files[i].len = 0;
            return &fs->files[i];
        }
    }
    return NULL;
}

static void fake_remove(FakeFs* fs, const char* path) {
    FakeFile* f = fake_find(fs, path, false);
    if(f) {
        f->present = false;
    }
}

static StoreStat fake_stat(void* ctx, const char* path) {
    FakeFs* fs = ctx;
    if(fs->force_stat_error) {
        return StoreStatError;
    }
    return fake_find(fs, path, false) ? StoreStatOk : StoreStatMissing;
}

static bool fake_exists(void* ctx, const char* path) {
    return fake_stat(ctx, path) == StoreStatOk;
}

static ReadResult
    fake_read_all(void* ctx, const char* path, uint8_t* buf, size_t buf_size, size_t* out_len) {
    FakeFs* fs = ctx;
    if(fs->short_read_path[0] != '\0' && strcmp(path, fs->short_read_path) == 0) {
        return ReadError;
    }
    const FakeFile* f = fake_find(fs, path, false);
    if(!f) {
        return ReadError;
    }
    if(f->len > buf_size) {
        return ReadTooLarge;
    }
    memcpy(buf, f->data, f->len);
    if(out_len) {
        *out_len = f->len;
    }
    return ReadOk;
}

static bool fake_write_all(void* ctx, const char* path, const uint8_t* buf, size_t len) {
    FakeFs* fs = ctx;
    fs->write_calls++;
    if(len > FAKE_MAX_BYTES) {
        return false;
    }
    FakeFile* f = fake_find(fs, path, true);
    if(!f) {
        return false;
    }
    memcpy(f->data, buf, len);
    f->len = len;
    return true;
}

static bool fake_rename(void* ctx, const char* src, const char* dst) {
    FakeFs* fs = ctx;
    fs->rename_calls++;
    if(fs->rename_mode == FakeRenameAlwaysFail) {
        return false;
    }
    const FakeFile* s = fake_find(fs, src, false);
    if(!s) {
        return false;
    }
    fake_remove(fs, dst);
    if(fs->rename_mode == FakeRenameCrashAfterRemoveDst) {
        return false;
    }
    if(fs->rename_mode == FakeRenameCrashPartialCopy) {
        FakeFile* d = fake_find(fs, dst, true);
        if(d) {
            const size_t partial = s->len / 2;
            memcpy(d->data, s->data, partial);
            d->len = partial;
        }
        return false;
    }
    FakeFile* d = fake_find(fs, dst, true);
    if(!d) {
        return false;
    }
    memcpy(d->data, s->data, s->len);
    d->len = s->len;
    if(fs->rename_mode == FakeRenameCrashBeforeRemoveSrc) {
        return false;
    }
    fake_remove(fs, src);
    return true;
}

static StoragePort fake_port(FakeFs* fs) {
    return (StoragePort){
        .ctx = fs,
        .stat = fake_stat,
        .exists = fake_exists,
        .read_all = fake_read_all,
        .write_all = fake_write_all,
        .rename = fake_rename,
    };
}

static void set_corrupt(FakeFs* fs, const char* path, uint8_t filler) {
    uint8_t garbage[32];
    memset(garbage, filler, sizeof(garbage));
    fake_write_all(fs, path, garbage, sizeof(garbage));
}

// 1600 bytes: bigger than SENSORS_FILE_MAX_BYTES can ever be for a valid record.
static void set_oversized(FakeFs* fs, const char* path) {
    uint8_t garbage[1600];
    memset(garbage, 0xEE, sizeof(garbage));
    fake_write_all(fs, path, garbage, sizeof(garbage));
}

static void make_record(SensorsRecord* rec, uint32_t seed) {
    memset(rec, 0, sizeof(*rec));
    rec->count = 1;
    rec->active_index = 0;
    snprintf(rec->sensors[0].name, SENSOR_NAME_MAX, "S%u", seed);
    rec->sensors[0].w = 10.0f + (float)seed;
    rec->sensors[0].h = 20.0f + (float)seed;
    rec->sensors[0].coc = 0.02f;
}

static void write_record(FakeFs* fs, const char* path, const SensorsRecord* rec) {
    uint8_t buf[SENSORS_FILE_MAX_BYTES];
    const size_t len = sensors_codec_encode(rec, buf, sizeof(buf));
    fake_write_all(fs, path, buf, len);
}

static void test_codec_round_trip(void) {
    SensorsRecord rec;
    make_record(&rec, 7);
    uint8_t buf[SENSORS_FILE_MAX_BYTES];
    const size_t len = sensors_codec_encode(&rec, buf, sizeof(buf));
    assert(len > 0);

    SensorsRecord decoded;
    assert(sensors_codec_decode(buf, len, &decoded));
    assert(decoded.count == rec.count);
    assert(decoded.active_index == rec.active_index);
    assert(strcmp(decoded.sensors[0].name, rec.sensors[0].name) == 0);
}

static void test_pick_slot_first_free_and_exhausted(void) {
    bool none_taken[SENSORS_BACKUP_SLOTS] = {0};
    assert(sensors_recovery_pick_slot(none_taken) == 0);

    bool all_taken[SENSORS_BACKUP_SLOTS];
    for(int i = 0; i < SENSORS_BACKUP_SLOTS; i++) {
        all_taken[i] = true;
    }
    assert(sensors_recovery_pick_slot(all_taken) == -1);
}

// A missing file seeds one default sensor in memory only — the adapter's launch policy is
// to never write on a plain first run; the user's first save is what creates the file.
static void test_missing_creates_no_write(void) {
    FakeFs fs = {0};
    StoragePort port = fake_port(&fs);
    SensorsRecord out;
    const SensorsRecoveryResult result = sensors_recovery_load(&port, MAIN_PATH, &out);

    assert(result.status == StoreLoadMissing);
    assert(!result.save_blocked);
    assert(result.kept_path[0] == '\0');
    assert(!fake_exists(&fs, MAIN_PATH ".bad"));
    assert(fs.write_calls == 0);
    assert(fs.rename_calls == 0);
    assert(out.count == 1);
}

// corrupt -> save -> .bad holds the original.
static void test_corrupt_then_save_keeps_original_in_bad(void) {
    FakeFs fs = {0};
    set_corrupt(&fs, MAIN_PATH, 0xAA);
    const FakeFile original = *fake_find(&fs, MAIN_PATH, false);

    StoragePort port = fake_port(&fs);
    SensorsRecord out;
    const SensorsRecoveryResult result = sensors_recovery_load(&port, MAIN_PATH, &out);
    assert(result.status == StoreLoadCorrupt);
    assert(!result.save_blocked);
    assert(strcmp(result.kept_path, MAIN_PATH ".bad") == 0);
    assert(!fake_exists(&fs, MAIN_PATH));

    SensorsRecord new_data;
    make_record(&new_data, 3);
    assert(
        sensors_recovery_save(&port, MAIN_PATH, &new_data, result.save_blocked) == SensorsSaveOk);

    const FakeFile* bad = fake_find(&fs, MAIN_PATH ".bad", false);
    assert(bad && bad->len == original.len && memcmp(bad->data, original.data, bad->len) == 0);

    SensorsRecord after;
    const SensorsRecoveryResult reload = sensors_recovery_load(&port, MAIN_PATH, &after);
    assert(reload.status == StoreLoadOk);
    assert(strcmp(after.sensors[0].name, new_data.sensors[0].name) == 0);
}

// A crash mid-save (rename dies right after removing the main file) must leave the .tmp
// recoverable, and the next load must restore it and re-promote it into place.
static void test_crash_mid_save_recovers_from_tmp(void) {
    FakeFs fs = {0};
    StoragePort port = fake_port(&fs);

    SensorsRecord original;
    make_record(&original, 1);
    assert(sensors_recovery_save(&port, MAIN_PATH, &original, false) == SensorsSaveOk);

    SensorsRecord updated;
    make_record(&updated, 2);
    fs.rename_mode = FakeRenameCrashAfterRemoveDst;
    assert(sensors_recovery_save(&port, MAIN_PATH, &updated, false) == SensorsSaveBlocked);
    fs.rename_mode = FakeRenameNormal;

    assert(!fake_exists(&fs, MAIN_PATH));
    assert(fake_exists(&fs, TMP_PATH));

    SensorsRecord recovered;
    const SensorsRecoveryResult result = sensors_recovery_load(&port, MAIN_PATH, &recovered);
    assert(result.status == StoreLoadOk);
    assert(!result.save_blocked);
    assert(strcmp(recovered.sensors[0].name, updated.sensors[0].name) == 0);
    assert(fake_exists(&fs, MAIN_PATH));
    assert(!fake_exists(&fs, TMP_PATH));
}

// A crash mid-copy leaves main partially written instead of merely absent; recovery from
// .tmp must work exactly the same way regardless of what garbage the crash left behind.
static void test_partial_copy_still_recovers_from_tmp(void) {
    FakeFs fs = {0};
    StoragePort port = fake_port(&fs);

    SensorsRecord original;
    make_record(&original, 1);
    assert(sensors_recovery_save(&port, MAIN_PATH, &original, false) == SensorsSaveOk);

    SensorsRecord updated;
    make_record(&updated, 2);
    fs.rename_mode = FakeRenameCrashPartialCopy;
    assert(sensors_recovery_save(&port, MAIN_PATH, &updated, false) == SensorsSaveBlocked);
    fs.rename_mode = FakeRenameNormal;

    assert(fake_exists(&fs, MAIN_PATH)); // present, but only half-written
    assert(fake_exists(&fs, TMP_PATH));

    SensorsRecord recovered;
    const SensorsRecoveryResult result = sensors_recovery_load(&port, MAIN_PATH, &recovered);
    assert(result.status == StoreLoadOk);
    assert(strcmp(recovered.sensors[0].name, updated.sensors[0].name) == 0);
    assert(!fake_exists(&fs, TMP_PATH));
}

// A crash between the copy and the final "remove src" leaves both files present with
// identical, valid content. Main already decodes, so load must use it directly and never
// even look at the leftover .tmp — it's harmless clutter, cleared by the next save.
static void test_crash_before_remove_src_is_harmless(void) {
    FakeFs fs = {0};
    StoragePort port = fake_port(&fs);

    SensorsRecord original;
    make_record(&original, 1);
    assert(sensors_recovery_save(&port, MAIN_PATH, &original, false) == SensorsSaveOk);

    SensorsRecord updated;
    make_record(&updated, 2);
    fs.rename_mode = FakeRenameCrashBeforeRemoveSrc;
    assert(sensors_recovery_save(&port, MAIN_PATH, &updated, false) == SensorsSaveBlocked);
    fs.rename_mode = FakeRenameNormal;

    assert(fake_exists(&fs, MAIN_PATH));
    assert(fake_exists(&fs, TMP_PATH));

    SensorsRecord loaded;
    const SensorsRecoveryResult result = sensors_recovery_load(&port, MAIN_PATH, &loaded);
    assert(result.status == StoreLoadOk);
    assert(!result.save_blocked);
    assert(strcmp(loaded.sensors[0].name, updated.sensors[0].name) == 0);
}

// A second corruption must not overwrite the first kept copy.
static void test_second_corruption_keeps_first_copy(void) {
    FakeFs fs = {0};
    StoragePort port = fake_port(&fs);

    set_corrupt(&fs, MAIN_PATH, 0x11);
    const FakeFile first_bad_expected = *fake_find(&fs, MAIN_PATH, false);
    SensorsRecord out;
    const SensorsRecoveryResult first = sensors_recovery_load(&port, MAIN_PATH, &out);
    assert(first.status == StoreLoadCorrupt);
    assert(strcmp(first.kept_path, MAIN_PATH ".bad") == 0);

    set_corrupt(&fs, MAIN_PATH, 0x22);
    const SensorsRecoveryResult second = sensors_recovery_load(&port, MAIN_PATH, &out);
    assert(second.status == StoreLoadCorrupt);
    assert(strcmp(second.kept_path, MAIN_PATH ".bad1") == 0);

    const FakeFile* bad0 = fake_find(&fs, MAIN_PATH ".bad", false);
    assert(
        bad0 && bad0->len == first_bad_expected.len &&
        memcmp(bad0->data, first_bad_expected.data, bad0->len) == 0);
}

// Every backup slot already taken: the unreadable file is left exactly where it is, saves
// are blocked, and the reason is distinguishable from any other quarantine failure.
static void test_all_backup_slots_taken_blocks_and_leaves_file(void) {
    FakeFs fs = {0};
    set_corrupt(&fs, MAIN_PATH, 0x99);
    char slot_path[64];
    for(int i = 0; i < SENSORS_BACKUP_SLOTS; i++) {
        if(i == 0) {
            snprintf(slot_path, sizeof(slot_path), "%s.bad", MAIN_PATH);
        } else {
            snprintf(slot_path, sizeof(slot_path), "%s.bad%d", MAIN_PATH, i);
        }
        fake_write_all(&fs, slot_path, (const uint8_t*)"x", 1);
    }
    const FakeFile bad0_before = *fake_find(&fs, MAIN_PATH ".bad", false);

    StoragePort port = fake_port(&fs);
    SensorsRecord out;
    const SensorsRecoveryResult result = sensors_recovery_load(&port, MAIN_PATH, &out);

    assert(result.status == StoreLoadCorrupt);
    assert(result.save_blocked);
    assert(result.no_free_backup_slot);
    assert(result.kept_path[0] == '\0');
    assert(fake_exists(&fs, MAIN_PATH)); // left in place, not destroyed

    const FakeFile* bad0_after = fake_find(&fs, MAIN_PATH ".bad", false);
    assert(
        bad0_after && bad0_after->len == bad0_before.len &&
        memcmp(bad0_after->data, bad0_before.data, bad0_before.len) == 0);
}

// A quarantine rename that fails for a reason OTHER than "no free slot" must be
// distinguishable from that case (so the notice never blames a full backup set that isn't
// actually full).
static void test_quarantine_rename_failure_is_neutral(void) {
    FakeFs fs = {0};
    set_corrupt(&fs, MAIN_PATH, 0x77);
    fs.rename_mode = FakeRenameAlwaysFail;

    StoragePort port = fake_port(&fs);
    SensorsRecord out;
    const SensorsRecoveryResult result = sensors_recovery_load(&port, MAIN_PATH, &out);

    assert(result.status == StoreLoadCorrupt);
    assert(result.save_blocked);
    assert(!result.no_free_backup_slot);
    assert(result.kept_path[0] == '\0');
    assert(fake_exists(&fs, MAIN_PATH));
}

// Only FSE_NOT_EXIST means missing; any other stat error must block saves without ever
// attempting a quarantine rename.
static void test_stat_error_blocks_without_quarantine(void) {
    FakeFs fs = {0};
    fs.force_stat_error = true;
    StoragePort port = fake_port(&fs);

    SensorsRecord out;
    const SensorsRecoveryResult result = sensors_recovery_load(&port, MAIN_PATH, &out);

    assert(result.status == StoreLoadIoError);
    assert(result.save_blocked);
    assert(!result.no_free_backup_slot);
    assert(result.kept_path[0] == '\0');
    assert(out.count == 1);
    assert(fs.rename_calls == 0);
}

// A .tmp that exists but can't be read (a storage fault, not corruption) must block saves
// without touching main at all — main is left exactly as found, never quarantined.
static void test_tmp_read_failure_blocks_without_quarantine(void) {
    FakeFs fs = {0};
    set_corrupt(&fs, MAIN_PATH, 0x33);
    fake_write_all(&fs, TMP_PATH, (const uint8_t*)"x", 1);
    snprintf(fs.short_read_path, sizeof(fs.short_read_path), "%s", TMP_PATH);

    StoragePort port = fake_port(&fs);
    SensorsRecord out;
    const SensorsRecoveryResult result = sensors_recovery_load(&port, MAIN_PATH, &out);

    assert(result.status == StoreLoadIoError);
    assert(result.save_blocked);
    assert(result.kept_path[0] == '\0');
    assert(fake_exists(&fs, MAIN_PATH)); // untouched
    assert(!fake_exists(&fs, MAIN_PATH ".bad"));
    assert(fs.rename_calls == 0);
}

// Main genuinely missing, .tmp exists but a read on it fails: an IO fault, not corruption
// and not a plain first run. Block saves; quarantine nothing (we can't trust what's there).
static void test_tmp_short_read_with_missing_main_blocks(void) {
    FakeFs fs = {0};
    fake_write_all(&fs, TMP_PATH, (const uint8_t*)"x", 1);
    snprintf(fs.short_read_path, sizeof(fs.short_read_path), "%s", TMP_PATH);

    StoragePort port = fake_port(&fs);
    SensorsRecord out;
    const SensorsRecoveryResult result = sensors_recovery_load(&port, MAIN_PATH, &out);

    assert(result.status == StoreLoadIoError);
    assert(result.save_blocked);
    assert(result.kept_path[0] == '\0');
    assert(!fake_exists(&fs, MAIN_PATH ".bad"));
    assert(fs.rename_calls == 0);
}

// Main genuinely missing, .tmp exists, reads fine, but doesn't decode: that garbage is the
// only evidence there is, so it must be quarantined — never silently read as "first run".
static void test_tmp_garbage_with_missing_main_quarantines_tmp(void) {
    FakeFs fs = {0};
    set_corrupt(&fs, TMP_PATH, 0x66);
    const FakeFile tmp_before = *fake_find(&fs, TMP_PATH, false);

    StoragePort port = fake_port(&fs);
    SensorsRecord out;
    const SensorsRecoveryResult result = sensors_recovery_load(&port, MAIN_PATH, &out);

    assert(result.status == StoreLoadCorrupt);
    assert(!result.save_blocked);
    assert(strcmp(result.kept_path, MAIN_PATH ".bad") == 0);
    assert(!fake_exists(&fs, TMP_PATH));
    assert(!fake_exists(&fs, MAIN_PATH));

    const FakeFile* bad = fake_find(&fs, MAIN_PATH ".bad", false);
    assert(bad && bad->len == tmp_before.len && memcmp(bad->data, tmp_before.data, bad->len) == 0);
    assert(out.count == 1);
}

// A main file bigger than any valid record could be is corruption, not a storage fault: it
// must be quarantined like any other unreadable file, never permanently block saves.
static void test_oversized_main_is_quarantined_not_blocked(void) {
    FakeFs fs = {0};
    set_oversized(&fs, MAIN_PATH);
    const FakeFile main_before = *fake_find(&fs, MAIN_PATH, false);

    StoragePort port = fake_port(&fs);
    SensorsRecord out;
    const SensorsRecoveryResult result = sensors_recovery_load(&port, MAIN_PATH, &out);

    assert(result.status == StoreLoadCorrupt);
    assert(!result.save_blocked);
    assert(!result.no_free_backup_slot);
    assert(strcmp(result.kept_path, MAIN_PATH ".bad") == 0);
    assert(!fake_exists(&fs, MAIN_PATH));

    const FakeFile* bad = fake_find(&fs, MAIN_PATH ".bad", false);
    assert(
        bad && bad->len == main_before.len && memcmp(bad->data, main_before.data, bad->len) == 0);
    assert(out.count == 1);

    // Saves must work normally afterward — this must not have blocked anything.
    SensorsRecord new_data;
    make_record(&new_data, 5);
    assert(
        sensors_recovery_save(&port, MAIN_PATH, &new_data, result.save_blocked) == SensorsSaveOk);
}

// Same, but for an oversized .tmp sitting next to a genuinely missing main: quarantined,
// not silently treated as a first run, and not blocked as an IO fault either.
static void test_oversized_tmp_with_missing_main_is_quarantined(void) {
    FakeFs fs = {0};
    set_oversized(&fs, TMP_PATH);
    const FakeFile tmp_before = *fake_find(&fs, TMP_PATH, false);

    StoragePort port = fake_port(&fs);
    SensorsRecord out;
    const SensorsRecoveryResult result = sensors_recovery_load(&port, MAIN_PATH, &out);

    assert(result.status == StoreLoadCorrupt);
    assert(!result.save_blocked);
    assert(strcmp(result.kept_path, MAIN_PATH ".bad") == 0);
    assert(!fake_exists(&fs, TMP_PATH));

    const FakeFile* bad = fake_find(&fs, MAIN_PATH ".bad", false);
    assert(bad && bad->len == tmp_before.len && memcmp(bad->data, tmp_before.data, bad->len) == 0);
}

// No crash needed at test time: a
// rename fails right after removing main (leaving main gone, .tmp holding the real data),
// then a later save's .tmp read is hit by the firmware's short-read quirk. That must block
// the save before it ever writes — never truncate/replace .tmp — or the real data is gone
// for good with main already missing.
static void test_recovered_tmp_survives_next_short_read(void) {
    FakeFs fs = {0};
    StoragePort port = fake_port(&fs);

    SensorsRecord original;
    make_record(&original, 1);
    assert(sensors_recovery_save(&port, MAIN_PATH, &original, false) == SensorsSaveOk);

    SensorsRecord real_data;
    make_record(&real_data, 2);
    fs.rename_mode = FakeRenameCrashAfterRemoveDst;
    assert(sensors_recovery_save(&port, MAIN_PATH, &real_data, false) == SensorsSaveBlocked);
    fs.rename_mode = FakeRenameNormal;

    assert(!fake_exists(&fs, MAIN_PATH));
    const FakeFile real_data_before = *fake_find(&fs, TMP_PATH, false);

    snprintf(fs.short_read_path, sizeof(fs.short_read_path), "%s", TMP_PATH);
    const int writes_before = fs.write_calls;
    SensorsRecord throwaway;
    make_record(&throwaway, 3);
    const SensorsSaveOutcome outcome = sensors_recovery_save(&port, MAIN_PATH, &throwaway, false);
    fs.short_read_path[0] = '\0';

    assert(outcome == SensorsSaveBlocked);
    assert(
        fs.write_calls == writes_before); // .tmp was never opened for writing, let alone truncated
    const FakeFile* real_data_after = fake_find(&fs, TMP_PATH, false);
    assert(
        real_data_after && real_data_after->len == real_data_before.len &&
        memcmp(real_data_after->data, real_data_before.data, real_data_before.len) == 0);

    SensorsRecord recovered;
    fs.rename_mode = FakeRenameNormal;
    const SensorsRecoveryResult result = sensors_recovery_load(&port, MAIN_PATH, &recovered);
    assert(result.status == StoreLoadOk);
    assert(strcmp(recovered.sensors[0].name, real_data.sensors[0].name) == 0);
}

// A save must promote an orphaned, unpromoted .tmp into main before writing anything new,
// so the new save can never destroy that copy. The end result must still be the caller's
// new data, not the orphaned one.
static void test_save_guard_promotes_orphaned_tmp_before_overwriting(void) {
    FakeFs fs = {0};
    StoragePort port = fake_port(&fs);

    SensorsRecord first;
    make_record(&first, 1);
    assert(sensors_recovery_save(&port, MAIN_PATH, &first, false) == SensorsSaveOk);

    set_corrupt(&fs, MAIN_PATH, 0x44);
    SensorsRecord orphan;
    make_record(&orphan, 9);
    write_record(&fs, TMP_PATH, &orphan);

    SensorsRecord next;
    make_record(&next, 3);
    assert(sensors_recovery_save(&port, MAIN_PATH, &next, false) == SensorsSaveOk);

    SensorsRecord loaded;
    const SensorsRecoveryResult result = sensors_recovery_load(&port, MAIN_PATH, &loaded);
    assert(result.status == StoreLoadOk);
    assert(strcmp(loaded.sensors[0].name, next.sensors[0].name) == 0);
}

// When that promotion itself can't complete, the save must refuse outright rather than let
// the caller's new write destroy the only good (orphaned) copy sitting in .tmp.
static void test_save_blocked_when_guard_cannot_promote(void) {
    FakeFs fs = {0};
    StoragePort port = fake_port(&fs);

    SensorsRecord first;
    make_record(&first, 1);
    assert(sensors_recovery_save(&port, MAIN_PATH, &first, false) == SensorsSaveOk);

    set_corrupt(&fs, MAIN_PATH, 0x44);
    SensorsRecord orphan;
    make_record(&orphan, 9);
    write_record(&fs, TMP_PATH, &orphan);
    const FakeFile orphan_before = *fake_find(&fs, TMP_PATH, false);

    fs.rename_mode = FakeRenameCrashAfterRemoveDst;
    const int writes_before = fs.write_calls;
    SensorsRecord next;
    make_record(&next, 3);
    const SensorsSaveOutcome outcome = sensors_recovery_save(&port, MAIN_PATH, &next, false);
    fs.rename_mode = FakeRenameNormal;

    assert(outcome == SensorsSaveBlocked);
    assert(fs.write_calls == writes_before); // never even tried to write the new data
    assert(!fake_exists(&fs, MAIN_PATH)); // removed by the failed promote attempt
    const FakeFile* orphan_after = fake_find(&fs, TMP_PATH, false);
    assert(
        orphan_after && orphan_after->len == orphan_before.len &&
        memcmp(orphan_after->data, orphan_before.data, orphan_before.len) == 0);
}

int main(void) {
    test_codec_round_trip();
    test_pick_slot_first_free_and_exhausted();
    test_missing_creates_no_write();
    test_corrupt_then_save_keeps_original_in_bad();
    test_crash_mid_save_recovers_from_tmp();
    test_partial_copy_still_recovers_from_tmp();
    test_crash_before_remove_src_is_harmless();
    test_second_corruption_keeps_first_copy();
    test_all_backup_slots_taken_blocks_and_leaves_file();
    test_quarantine_rename_failure_is_neutral();
    test_stat_error_blocks_without_quarantine();
    test_tmp_read_failure_blocks_without_quarantine();
    test_tmp_short_read_with_missing_main_blocks();
    test_tmp_garbage_with_missing_main_quarantines_tmp();
    test_oversized_main_is_quarantined_not_blocked();
    test_oversized_tmp_with_missing_main_is_quarantined();
    test_recovered_tmp_survives_next_short_read();
    test_save_guard_promotes_orphaned_tmp_before_overwriting();
    test_save_blocked_when_guard_cannot_promote();
    puts("test_sensors_recovery: ok");
    return 0;
}
