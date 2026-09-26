#include "include/domain/habit.h"
#include "include/persistence/habit_store.h"
#include "include/ports/hf_store_port.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define HABITFLOW_BIN      "/ext/apps_data/habitflow/habitflow.bin"
#define HABITFLOW_BIN_TMP  HABITFLOW_BIN ".tmp"
#define HABITFLOW_BIN_BAD  HABITFLOW_BIN ".bad"
#define HABITFLOW_BIN_BAD1 HABITFLOW_BIN ".bad1"

#define FAKE_MAX_FILES 16
#define FAKE_MAX_PATH  64
#define FAKE_MAX_DATA  512

typedef struct {
    char path[FAKE_MAX_PATH];
    uint8_t data[FAKE_MAX_DATA];
    size_t len;
    bool used;
} FakeFile;

typedef struct {
    FakeFile files[FAKE_MAX_FILES];
    // A rename crashes right after removing the destination, mirroring the firmware's
    // non-atomic rename (remove destination, copy, remove source) stopping before any
    // copy starts: the destination ends up missing, the source untouched.
    bool crash_rename_after_remove;
    // A rename crashes mid-copy: the destination gets only half the source's bytes, and
    // the source (only removed once the copy fully succeeds) is left untouched.
    bool crash_rename_after_partial_copy;
    // Any rename targeting this path fails outright (nothing removed or written), so a
    // specific rename call can be made to fail without affecting an earlier one in the
    // same load, e.g. a quarantine that succeeds followed by a promote that doesn't.
    const char* fail_rename_to;
    // A write still truncates its target (FSOM_CREATE_ALWAYS truncates on open, before any
    // bytes are written) but then reports failure, mirroring a disk error mid-write.
    bool fail_write;
} FakeFs;

static FakeFile* fake_find(FakeFs* fs, const char* path) {
    for(int i = 0; i < FAKE_MAX_FILES; i++) {
        if(fs->files[i].used && strcmp(fs->files[i].path, path) == 0) {
            return &fs->files[i];
        }
    }
    return NULL;
}

static FakeFile* fake_slot(FakeFs* fs, const char* path) {
    FakeFile* existing = fake_find(fs, path);
    if(existing) {
        return existing;
    }
    for(int i = 0; i < FAKE_MAX_FILES; i++) {
        if(!fs->files[i].used) {
            fs->files[i].used = true;
            snprintf(fs->files[i].path, sizeof(fs->files[i].path), "%s", path);
            fs->files[i].len = 0;
            return &fs->files[i];
        }
    }
    return NULL;
}

static HfPathStat fake_stat(void* self, const char* path) {
    return fake_find((FakeFs*)self, path) ? HfPathOk : HfPathMissing;
}

static size_t fake_read(void* self, const char* path, uint8_t* buf, size_t cap) {
    const FakeFile* f = fake_find((FakeFs*)self, path);
    if(!f || f->len > cap) {
        return 0;
    }
    memcpy(buf, f->data, f->len);
    return f->len;
}

static bool fake_write(void* self, const char* path, const uint8_t* data, size_t len) {
    FakeFs* fs = self;
    FakeFile* f = fake_slot(fs, path);
    if(!f) {
        return false;
    }
    f->len = 0; // FSOM_CREATE_ALWAYS truncates on open, before any bytes are written
    if(fs->fail_write || len > FAKE_MAX_DATA) {
        return false;
    }
    memcpy(f->data, data, len);
    f->len = len;
    return true;
}

static bool fake_remove(void* self, const char* path) {
    FakeFile* f = fake_find((FakeFs*)self, path);
    if(f) {
        f->used = false;
    }
    return true;
}

static bool fake_rename(void* self, const char* old_path, const char* new_path) {
    FakeFs* fs = self;
    if(fs->fail_rename_to && strcmp(new_path, fs->fail_rename_to) == 0) {
        return false;
    }
    const FakeFile* src = fake_find(fs, old_path);
    if(!src) {
        return false;
    }
    fake_remove(fs, new_path);
    if(fs->crash_rename_after_remove) {
        return false;
    }
    FakeFile* dst = fake_slot(fs, new_path);
    if(!dst) {
        return false;
    }
    if(fs->crash_rename_after_partial_copy) {
        size_t partial = src->len / 2;
        memcpy(dst->data, src->data, partial);
        dst->len = partial;
        return false;
    }
    memcpy(dst->data, src->data, src->len);
    dst->len = src->len;
    fake_remove(fs, old_path);
    return true;
}

static bool fake_mkdir(void* self, const char* path) {
    (void)self;
    (void)path;
    return true;
}

static HfStorePort fake_port(FakeFs* fs) {
    return (HfStorePort){
        .self = fs,
        .stat = fake_stat,
        .read = fake_read,
        .write = fake_write,
        .rename = fake_rename,
        .remove = fake_remove,
        .mkdir = fake_mkdir,
    };
}

static void fake_seed(FakeFs* fs, const char* path, const uint8_t* data, size_t len) {
    FakeFile* f = fake_slot(fs, path);
    memcpy(f->data, data, len);
    f->len = len;
}

static Habit make_habit(const char* name) {
    Habit h;
    hf_habit_set_defaults(&h, name);
    return h;
}

// (a) corrupt data survives a later user save: the original bytes end up in .bad, and the
// live file holds only the new data.
static void test_corrupt_survives_next_save(void) {
    FakeFs fs = {0};
    const uint8_t garbage[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    fake_seed(&fs, HABITFLOW_BIN, garbage, sizeof(garbage));
    HfStorePort port = fake_port(&fs);

    HabitStore store;
    char notice[256];
    assert(habit_store_load(&port, &store, notice, sizeof(notice)) == HfStoreLoadCorrupt);
    assert(store.habit_count == 0);
    assert(fake_find(&fs, HABITFLOW_BIN) == NULL);
    assert(fake_find(&fs, HABITFLOW_BIN_BAD) != NULL);

    Habit h = make_habit("Run");
    assert(habit_store_add(&port, &store, &h));

    FakeFile* bad = fake_find(&fs, HABITFLOW_BIN_BAD);
    assert(bad && bad->len == sizeof(garbage) && memcmp(bad->data, garbage, sizeof(garbage)) == 0);

    HabitStore reloaded;
    char notice2[256];
    assert(habit_store_load(&port, &reloaded, notice2, sizeof(notice2)) == HfStoreLoadOk);
    assert(reloaded.habit_count == 1);
    assert(strcmp(reloaded.habits[0].name, "Run") == 0);
}

// (b) a missing file is not an error, and loading it writes nothing at all.
static void test_missing_writes_nothing(void) {
    FakeFs fs = {0};
    HfStorePort port = fake_port(&fs);

    HabitStore store;
    char notice[256];
    assert(habit_store_load(&port, &store, notice, sizeof(notice)) == HfStoreLoadMissing);
    assert(store.habit_count == 0);
    assert(notice[0] == '\0');
    for(int i = 0; i < FAKE_MAX_FILES; i++) {
        assert(!fs.files[i].used);
    }
}

// (c) a save that crashes right after the destination is removed (rename's first step,
// per hf_store_port.h) is recovered from the still-intact temp file on the next load.
static void test_recovers_from_interrupted_save(void) {
    FakeFs fs = {0};
    HfStorePort port = fake_port(&fs);

    HabitStore store;
    habit_store_init(&store);
    store.habits[0] = make_habit("Run");
    store.habits[1] = make_habit("Read");
    store.habit_count = 2;
    assert(habit_store_save(&port, &store));

    store.habits[2] = make_habit("Gym");
    store.habit_count = 3;
    fs.crash_rename_after_remove = true;
    assert(!habit_store_save(&port, &store));
    // cppcheck-suppress redundantAssignment ; read by fake_rename through the port's
    // function pointer, which cppcheck can't trace.
    fs.crash_rename_after_remove = false;

    assert(fake_find(&fs, HABITFLOW_BIN) == NULL);
    assert(fake_find(&fs, HABITFLOW_BIN_TMP) != NULL);

    HabitStore recovered;
    char notice[256];
    assert(habit_store_load(&port, &recovered, notice, sizeof(notice)) == HfStoreLoadOk);
    assert(recovered.habit_count == 3);
    assert(strcmp(recovered.habits[2].name, "Gym") == 0);
    assert(fake_find(&fs, HABITFLOW_BIN) != NULL);
    assert(fake_find(&fs, HABITFLOW_BIN_TMP) == NULL);
}

// (d) a second corruption keeps the first .bad instead of overwriting it.
static void test_second_corruption_keeps_first_backup(void) {
    FakeFs fs = {0};
    const uint8_t first_garbage[4] = {0xAA, 0xBB, 0xCC, 0xDD};
    fake_seed(&fs, HABITFLOW_BIN, first_garbage, sizeof(first_garbage));
    HfStorePort port = fake_port(&fs);

    HabitStore store;
    char notice[256];
    assert(habit_store_load(&port, &store, notice, sizeof(notice)) == HfStoreLoadCorrupt);
    assert(fake_find(&fs, HABITFLOW_BIN_BAD) != NULL);

    const uint8_t second_garbage[4] = {0x11, 0x22, 0x33, 0x44};
    fake_seed(&fs, HABITFLOW_BIN, second_garbage, sizeof(second_garbage));

    HabitStore store2;
    char notice2[256];
    assert(habit_store_load(&port, &store2, notice2, sizeof(notice2)) == HfStoreLoadCorrupt);

    FakeFile* bad = fake_find(&fs, HABITFLOW_BIN_BAD);
    FakeFile* bad1 = fake_find(&fs, HABITFLOW_BIN_BAD1);
    assert(bad && memcmp(bad->data, first_garbage, sizeof(first_garbage)) == 0);
    assert(bad1 && memcmp(bad1->data, second_garbage, sizeof(second_garbage)) == 0);
}

// (e) a non-missing stat error blocks saves and never attempts a quarantine rename.
static HfPathStat error_stat(void* self, const char* path) {
    if(strcmp(path, HABITFLOW_BIN) == 0) {
        return HfPathError;
    }
    return fake_stat(self, path);
}

static void test_stat_error_blocks_without_quarantine(void) {
    FakeFs fs = {0};
    const uint8_t garbage[4] = {1, 2, 3, 4};
    fake_seed(&fs, HABITFLOW_BIN, garbage, sizeof(garbage));
    HfStorePort port = fake_port(&fs);
    port.stat = error_stat;

    HabitStore store;
    char notice[256];
    assert(habit_store_load(&port, &store, notice, sizeof(notice)) == HfStoreLoadStatError);
    assert(notice[0] != '\0');
    assert(fake_find(&fs, HABITFLOW_BIN_BAD) == NULL);

    Habit h = make_habit("Run");
    assert(!habit_store_add(&port, &store, &h));
}

static void backup_slot_path(int slot, char* out, size_t out_size) {
    if(slot == 0) {
        snprintf(out, out_size, "%s", HABITFLOW_BIN_BAD);
    } else {
        snprintf(out, out_size, "%s%d", HABITFLOW_BIN_BAD, slot);
    }
}

static void fill_all_backup_slots(FakeFs* fs) {
    const uint8_t filler[1] = {0xFF};
    for(int i = 0; i < HF_STORE_BACKUP_SLOTS; i++) {
        char path[FAKE_MAX_PATH];
        backup_slot_path(i, path, sizeof(path));
        fake_seed(fs, path, filler, sizeof(filler));
    }
}

// (f) the mid-copy crash window: a save's rename crashes after copying only half of
// `.tmp` into `bin`, leaving `bin` short and `.tmp` (only removed once the copy fully
// succeeds) still complete. Every habit must still load, and the partial `bin` bytes are
// quarantined rather than silently overwritten by promoting `.tmp` over them.
static void test_recovers_from_mid_copy_crash(void) {
    FakeFs fs = {0};
    HfStorePort port = fake_port(&fs);

    HabitStore store;
    habit_store_init(&store);
    store.habits[0] = make_habit("Run");
    store.habits[1] = make_habit("Read");
    store.habits[2] = make_habit("Gym");
    store.habit_count = 3;
    assert(habit_store_save(&port, &store));

    store.habits[2] = make_habit("Yoga");
    fs.crash_rename_after_partial_copy = true;
    assert(!habit_store_save(&port, &store));
    // cppcheck-suppress redundantAssignment ; read by fake_rename through the port's
    // function pointer, which cppcheck can't trace.
    fs.crash_rename_after_partial_copy = false;

    HabitStore recovered;
    char notice[256];
    assert(habit_store_load(&port, &recovered, notice, sizeof(notice)) == HfStoreLoadOk);
    assert(recovered.habit_count == 3);
    assert(strcmp(recovered.habits[0].name, "Run") == 0);
    assert(strcmp(recovered.habits[1].name, "Read") == 0);
    assert(strcmp(recovered.habits[2].name, "Yoga") == 0);

    assert(fake_find(&fs, HABITFLOW_BIN_BAD) != NULL);
    assert(fake_find(&fs, HABITFLOW_BIN) != NULL);
    assert(fake_find(&fs, HABITFLOW_BIN_TMP) == NULL);
}

// (g) `bin` is unreadable and `.tmp` would rescue it, but every backup slot is already
// taken: `bin` must be left exactly as the crash left it (never overwritten by the
// promote), the session still recovers from `.tmp`, and saves are blocked, just like the
// plain all-slots-taken case with no `.tmp` involved.
static void test_bin_unreadable_tmp_rescues_but_no_free_slot(void) {
    FakeFs fs = {0};
    HfStorePort port = fake_port(&fs);
    fill_all_backup_slots(&fs);

    HabitStore store;
    habit_store_init(&store);
    store.habits[0] = make_habit("Run");
    store.habit_count = 1;
    assert(habit_store_save(&port, &store));

    store.habits[1] = make_habit("Read");
    store.habit_count = 2;
    fs.crash_rename_after_partial_copy = true;
    assert(!habit_store_save(&port, &store));
    // cppcheck-suppress redundantAssignment ; read by fake_rename through the port's
    // function pointer, which cppcheck can't trace.
    fs.crash_rename_after_partial_copy = false;

    HabitStore recovered;
    char notice[256];
    HfStoreLoadStatus status = habit_store_load(&port, &recovered, notice, sizeof(notice));
    assert(status == HfStoreLoadCorrupt);
    assert(recovered.habit_count == 2);
    assert(strcmp(recovered.habits[1].name, "Read") == 0);
    assert(notice[0] != '\0');

    assert(fake_find(&fs, HABITFLOW_BIN) != NULL);
    assert(fake_find(&fs, HABITFLOW_BIN_TMP) != NULL);

    Habit h = make_habit("Gym");
    assert(!habit_store_add(&port, &recovered, &h));
}

// (h) bin missing, .tmp decodes, but the load's own promote rename fails: the recovered
// data is used for this session, but saving must be blocked -- otherwise the very next
// save would overwrite .tmp, the only good copy, before it's ever confirmed safe at `bin`.
static void test_failed_promote_from_missing_bin_blocks_saves(void) {
    FakeFs fs = {0};
    HfStorePort port = fake_port(&fs);

    HabitStore store;
    habit_store_init(&store);
    store.habits[0] = make_habit("Run");
    store.habit_count = 1;
    assert(habit_store_save(&port, &store));

    // A second save's rename crashes right after removing the destination, leaving `bin`
    // missing and `.tmp` (the source, untouched until a full copy) as the only good copy.
    store.habits[0] = make_habit("Read");
    fs.crash_rename_after_remove = true;
    assert(!habit_store_save(&port, &store));
    // cppcheck-suppress redundantAssignment ; read by fake_rename through the port's
    // function pointer, which cppcheck can't trace.
    fs.crash_rename_after_remove = false;

    assert(fake_find(&fs, HABITFLOW_BIN) == NULL);
    assert(fake_find(&fs, HABITFLOW_BIN_TMP) != NULL);

    // The load's own attempt to promote .tmp over the missing `bin` then fails too.
    fs.fail_rename_to = HABITFLOW_BIN;
    HabitStore recovered;
    char notice[256];
    HfStoreLoadStatus status = habit_store_load(&port, &recovered, notice, sizeof(notice));
    // cppcheck-suppress redundantAssignment ; read by fake_rename through the port's
    // function pointer, which cppcheck can't trace.
    fs.fail_rename_to = NULL;

    assert(status == HfStoreLoadCorrupt);
    assert(recovered.habit_count == 1);
    assert(strcmp(recovered.habits[0].name, "Read") == 0);
    assert(notice[0] != '\0');

    const FakeFile* tmp_before = fake_find(&fs, HABITFLOW_BIN_TMP);
    assert(tmp_before != NULL);
    const size_t tmp_len_before = tmp_before->len;

    Habit h = make_habit("Read");
    assert(!habit_store_add(&port, &recovered, &h));

    // Blocked before it ever touched disk: .tmp -- the only good copy -- is untouched.
    const FakeFile* tmp_after = fake_find(&fs, HABITFLOW_BIN_TMP);
    assert(tmp_after != NULL && tmp_after->len == tmp_len_before);
}

// (i) bin is unreadable, .tmp decodes and bin is successfully quarantined, but restoring
// .tmp over the now-vacated slot then fails too: still block saves rather than assume the
// disk ended up in a safe state.
static void test_failed_promote_after_quarantine_blocks_saves(void) {
    FakeFs fs = {0};
    HfStorePort port = fake_port(&fs);

    HabitStore store;
    habit_store_init(&store);
    store.habits[0] = make_habit("Run");
    store.habit_count = 1;
    assert(habit_store_save(&port, &store));

    // `.tmp` still holds a complete, valid copy (as if a save had written it and then
    // crashed before promoting), while `bin` is separately unreadable.
    const FakeFile* bin = fake_find(&fs, HABITFLOW_BIN);
    fake_seed(&fs, HABITFLOW_BIN_TMP, bin->data, bin->len);
    const uint8_t garbage[4] = {1, 2, 3, 4};
    fake_seed(&fs, HABITFLOW_BIN, garbage, sizeof(garbage));

    // The quarantine rename (bin -> .bad) must succeed; only the later promote rename
    // (.tmp -> bin) fails.
    fs.fail_rename_to = HABITFLOW_BIN;
    HabitStore recovered;
    char notice[256];
    HfStoreLoadStatus status = habit_store_load(&port, &recovered, notice, sizeof(notice));
    // cppcheck-suppress redundantAssignment ; read by fake_rename through the port's
    // function pointer, which cppcheck can't trace.
    fs.fail_rename_to = NULL;

    assert(status == HfStoreLoadCorrupt);
    assert(recovered.habit_count == 1);
    assert(strcmp(recovered.habits[0].name, "Run") == 0);
    assert(notice[0] != '\0');

    FakeFile* bad = fake_find(&fs, HABITFLOW_BIN_BAD);
    assert(bad && bad->len == sizeof(garbage) && memcmp(bad->data, garbage, sizeof(garbage)) == 0);
    assert(fake_find(&fs, HABITFLOW_BIN_TMP) != NULL);

    Habit h = make_habit("Read");
    assert(!habit_store_add(&port, &recovered, &h));
}

// (j) a .tmp that exists but can't be read (a read error, not just a decode failure) must
// not be treated as absent: block saves and never attempt a quarantine rename.
static size_t no_read(void* self, const char* path, uint8_t* buf, size_t cap) {
    if(strcmp(path, HABITFLOW_BIN_TMP) == 0) {
        return 0;
    }
    return fake_read(self, path, buf, cap);
}

static void test_tmp_read_error_blocks_without_quarantine(void) {
    FakeFs fs = {0};
    const uint8_t garbage[4] = {1, 2, 3, 4};
    fake_seed(&fs, HABITFLOW_BIN, garbage, sizeof(garbage));
    const uint8_t tmp_bytes[4] = {5, 6, 7, 8};
    fake_seed(&fs, HABITFLOW_BIN_TMP, tmp_bytes, sizeof(tmp_bytes));
    HfStorePort port = fake_port(&fs);
    port.read = no_read;

    HabitStore store;
    char notice[256];
    HfStoreLoadStatus status = habit_store_load(&port, &store, notice, sizeof(notice));
    assert(status == HfStoreLoadStatError);
    assert(notice[0] != '\0');
    assert(fake_find(&fs, HABITFLOW_BIN_BAD) == NULL);
    assert(fake_find(&fs, HABITFLOW_BIN) != NULL);
    assert(fake_find(&fs, HABITFLOW_BIN_TMP) != NULL);

    Habit h = make_habit("Run");
    assert(!habit_store_add(&port, &store, &h));
}

// (k) same as (j), but `bin` is missing rather than merely unreadable.
static void test_tmp_read_error_with_missing_bin_blocks(void) {
    FakeFs fs = {0};
    const uint8_t tmp_bytes[4] = {5, 6, 7, 8};
    fake_seed(&fs, HABITFLOW_BIN_TMP, tmp_bytes, sizeof(tmp_bytes));
    HfStorePort port = fake_port(&fs);
    port.read = no_read;

    HabitStore store;
    char notice[256];
    HfStoreLoadStatus status = habit_store_load(&port, &store, notice, sizeof(notice));
    assert(status == HfStoreLoadStatError);
    assert(notice[0] != '\0');
    assert(fake_find(&fs, HABITFLOW_BIN) == NULL);
    assert(fake_find(&fs, HABITFLOW_BIN_TMP) != NULL);

    Habit h = make_habit("Run");
    assert(!habit_store_add(&port, &store, &h));
}

// (l) hole 1, the exact reported sequence: a save's rename fails right after removing
// `bin`, leaving `.tmp` as the only good copy; a *later* save's own write to `.tmp` then
// fails outright (after truncating it). `bin` must already have been confirmed good --
// by promoting the still-good `.tmp` -- before that failing write ever risks it, so the
// habits survive regardless of what the failed write does to `.tmp`.
static void test_save_promotes_tmp_before_risking_it_on_a_failed_write(void) {
    FakeFs fs = {0};
    HfStorePort port = fake_port(&fs);

    HabitStore store;
    habit_store_init(&store);
    store.habits[0] = make_habit("Run");
    store.habits[1] = make_habit("Read");
    store.habits[2] = make_habit("Gym");
    store.habit_count = 3;
    assert(habit_store_save(&port, &store));

    // A second save's rename fails right after removing `bin`: `.tmp` (this same 3-habit
    // data) is now the only good copy.
    fs.crash_rename_after_remove = true;
    assert(!habit_store_save(&port, &store));
    // cppcheck-suppress redundantAssignment ; read by fake_rename through the port's
    // function pointer, which cppcheck can't trace.
    fs.crash_rename_after_remove = false;
    assert(fake_find(&fs, HABITFLOW_BIN) == NULL);
    assert(fake_find(&fs, HABITFLOW_BIN_TMP) != NULL);

    // A third save's own write to `.tmp` fails outright, after truncating it.
    fs.fail_write = true;
    assert(!habit_store_save(&port, &store));
    // cppcheck-suppress redundantAssignment ; read by fake_write through the port's
    // function pointer, which cppcheck can't trace.
    fs.fail_write = false;

    HabitStore recovered;
    char notice[256];
    assert(habit_store_load(&port, &recovered, notice, sizeof(notice)) == HfStoreLoadOk);
    assert(recovered.habit_count == 3);
    assert(strcmp(recovered.habits[0].name, "Run") == 0);
    assert(strcmp(recovered.habits[1].name, "Read") == 0);
    assert(strcmp(recovered.habits[2].name, "Gym") == 0);
}

// (m) hole 2, part 1: a read that returns fewer bytes than the file actually holds
// without itself signalling failure (as an uncorrected mid-file disk error would look) is
// still just a decode failure for `.tmp` -- `bin`, separately corrupt, is quarantined
// exactly as if `.tmp` had never been consulted at all.
static size_t short_read(void* self, const char* path, uint8_t* buf, size_t cap) {
    if(strcmp(path, HABITFLOW_BIN_TMP) != 0) {
        return fake_read(self, path, buf, cap);
    }
    size_t n = fake_read(self, path, buf, cap);
    return n > 4 ? n / 2 : n;
}

static void test_tmp_short_read_with_corrupt_bin_still_quarantines_bin(void) {
    FakeFs fs = {0};
    HfStorePort setup_port = fake_port(&fs);

    HabitStore seed;
    habit_store_init(&seed);
    seed.habits[0] = make_habit("Run");
    seed.habit_count = 1;
    assert(habit_store_save(&setup_port, &seed));

    // `.tmp` gets a valid, complete copy (as if left over from an earlier save), while
    // `bin` is separately corrupt.
    const FakeFile* bin = fake_find(&fs, HABITFLOW_BIN);
    fake_seed(&fs, HABITFLOW_BIN_TMP, bin->data, bin->len);
    const uint8_t garbage[4] = {1, 2, 3, 4};
    fake_seed(&fs, HABITFLOW_BIN, garbage, sizeof(garbage));

    HfStorePort port = fake_port(&fs);
    port.read = short_read;

    HabitStore store;
    char notice[256];
    HfStoreLoadStatus status = habit_store_load(&port, &store, notice, sizeof(notice));
    assert(status == HfStoreLoadCorrupt);
    assert(store.habit_count == 0); // `.tmp`'s short read didn't decode, so no rescue
    assert(notice[0] != '\0');

    const FakeFile* bad = fake_find(&fs, HABITFLOW_BIN_BAD);
    assert(bad && bad->len == sizeof(garbage) && memcmp(bad->data, garbage, sizeof(garbage)) == 0);
}

// (n) hole 2, part 2: `bin` is missing and `.tmp` exists but only short-reads (so it
// doesn't decode) -- it must be quarantined, not silently discarded as if nothing had
// ever been saved.
static void test_tmp_short_read_with_missing_bin_quarantines_tmp(void) {
    FakeFs fs = {0};
    HfStorePort setup_port = fake_port(&fs);

    HabitStore seed;
    habit_store_init(&seed);
    seed.habits[0] = make_habit("Run");
    seed.habit_count = 1;
    assert(habit_store_save(&setup_port, &seed));

    const FakeFile* bin = fake_find(&fs, HABITFLOW_BIN);
    const size_t bin_len = bin->len;
    uint8_t bin_bytes[FAKE_MAX_DATA];
    memcpy(bin_bytes, bin->data, bin_len);
    fake_seed(&fs, HABITFLOW_BIN_TMP, bin_bytes, bin_len);
    fake_remove(&fs, HABITFLOW_BIN);

    HfStorePort port = fake_port(&fs);
    port.read = short_read;

    HabitStore store;
    char notice[256];
    HfStoreLoadStatus status = habit_store_load(&port, &store, notice, sizeof(notice));
    assert(status == HfStoreLoadCorrupt);
    assert(store.habit_count == 0);
    assert(notice[0] != '\0');

    const FakeFile* bad = fake_find(&fs, HABITFLOW_BIN_BAD);
    assert(bad != NULL);
    assert(bad->len == bin_len && memcmp(bad->data, bin_bytes, bin_len) == 0);
    assert(fake_find(&fs, HABITFLOW_BIN_TMP) == NULL);

    // The quarantine freed `.tmp`'s path (there was a slot for it), so a fresh save is
    // allowed, same as after a corrupt `bin` is successfully quarantined.
    Habit h = make_habit("Read");
    assert(habit_store_add(&port, &store, &h));

    HabitStore reloaded;
    char notice2[256];
    assert(habit_store_load(&port, &reloaded, notice2, sizeof(notice2)) == HfStoreLoadOk);
    assert(reloaded.habit_count == 1);
    assert(strcmp(reloaded.habits[0].name, "Read") == 0);
}

int main(void) {
    test_corrupt_survives_next_save();
    test_missing_writes_nothing();
    test_recovers_from_interrupted_save();
    test_second_corruption_keeps_first_backup();
    test_stat_error_blocks_without_quarantine();
    test_recovers_from_mid_copy_crash();
    test_bin_unreadable_tmp_rescues_but_no_free_slot();
    test_failed_promote_from_missing_bin_blocks_saves();
    test_failed_promote_after_quarantine_blocks_saves();
    test_tmp_read_error_blocks_without_quarantine();
    test_tmp_read_error_with_missing_bin_blocks();
    test_save_promotes_tmp_before_risking_it_on_a_failed_write();
    test_tmp_short_read_with_corrupt_bin_still_quarantines_bin();
    test_tmp_short_read_with_missing_bin_quarantines_tmp();
    puts("test_habit_store: ok");
    return 0;
}
