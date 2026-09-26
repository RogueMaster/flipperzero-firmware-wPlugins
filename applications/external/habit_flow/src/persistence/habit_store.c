#include "include/persistence/habit_store.h"
#include <stdio.h>
#include <string.h>

#define HF_STORE_MAGIC   0x48464231u
#define HF_STORE_VERSION 1u

#define APPS_DATA_DIR      "/ext/apps_data/habitflow"
#define HABITFLOW_BIN      APPS_DATA_DIR "/habitflow.bin"
#define HABITFLOW_BIN_TMP  HABITFLOW_BIN ".tmp"
#define HF_BACKUP_PATH_MAX 48

typedef struct __attribute__((packed)) {
    char name[HF_HABIT_NAME_MAX];
    uint16_t streak;
    uint16_t max_streak;
    uint8_t completed_today;
    uint8_t history[HF_HISTORY_DAYS];
    uint16_t goal_days;
    uint8_t mastered;
} HabitSerialized;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t version;
    uint32_t session_date_packed;
    uint32_t habit_count;
} HabitStoreHeader;

#define HF_STORE_BLOB_MAX \
    (sizeof(HabitStoreHeader) + (size_t)HF_HABITS_MAX * sizeof(HabitSerialized))

static void ser_to_habit(const HabitSerialized* s, Habit* h) {
    memset(h, 0, sizeof(*h));
    memcpy(h->name, s->name, sizeof(h->name));
    h->name[HF_HABIT_NAME_MAX - 1] = '\0';
    h->streak = s->streak;
    h->max_streak = s->max_streak;
    h->completed_today = s->completed_today != 0;
    memcpy(h->history, s->history, sizeof(h->history));
    h->goal_days = s->goal_days;
    if(h->goal_days < HF_GOAL_MIN) {
        h->goal_days = HF_DEFAULT_GOAL;
    }
    if(h->goal_days > HF_GOAL_MAX) {
        h->goal_days = HF_GOAL_MAX;
    }
    h->mastered = s->mastered != 0;
}

static void habit_to_ser(const Habit* h, HabitSerialized* s) {
    memset(s, 0, sizeof(*s));
    memcpy(s->name, h->name, sizeof(s->name));
    s->streak = h->streak;
    s->max_streak = h->max_streak;
    s->completed_today = h->completed_today ? 1u : 0u;
    memcpy(s->history, h->history, sizeof(s->history));
    s->goal_days = h->goal_days;
    s->mastered = h->mastered ? 1u : 0u;
}

void habit_store_init(HabitStore* store) {
    memset(store, 0, sizeof(*store));
}

static bool decode_store(HabitStore* store, const uint8_t* buf, size_t n) {
    HabitStoreHeader head;
    if(n < sizeof(head)) {
        return false;
    }
    memcpy(&head, buf, sizeof(head));
    if(head.magic != HF_STORE_MAGIC || head.version != HF_STORE_VERSION ||
       head.habit_count > HF_HABITS_MAX) {
        return false;
    }
    size_t need = sizeof(head) + (size_t)head.habit_count * sizeof(HabitSerialized);
    if(n < need) {
        return false;
    }

    habit_store_init(store);
    store->session_date_packed = head.session_date_packed;
    store->habit_count = head.habit_count;
    const uint8_t* p = buf + sizeof(head);
    for(uint32_t i = 0; i < store->habit_count; i++) {
        HabitSerialized ser;
        memcpy(&ser, p, sizeof(ser));
        ser_to_habit(&ser, &store->habits[i]);
        p += sizeof(ser);
    }
    return true;
}

static size_t encode_store(const HabitStore* store, uint8_t* buf, size_t cap) {
    const HabitStoreHeader head = {
        .magic = HF_STORE_MAGIC,
        .version = HF_STORE_VERSION,
        .session_date_packed = store->session_date_packed,
        .habit_count = store->habit_count,
    };
    size_t need = sizeof(head) + (size_t)store->habit_count * sizeof(HabitSerialized);
    if(need > cap) {
        return 0;
    }
    memcpy(buf, &head, sizeof(head));
    uint8_t* p = buf + sizeof(head);
    for(uint32_t i = 0; i < store->habit_count; i++) {
        HabitSerialized ser;
        habit_to_ser(&store->habits[i], &ser);
        memcpy(p, &ser, sizeof(ser));
        p += sizeof(ser);
    }
    return need;
}

static bool decode_path(const HfStorePort* port, const char* path, HabitStore* out) {
    uint8_t buf[HF_STORE_BLOB_MAX];
    size_t n = port->read(port->self, path, buf, sizeof(buf));
    return n > 0 && decode_store(out, buf, n);
}

// HF_STORE_BACKUP_SLOTS is 10, so slot is always a single digit; a fixed-width suffix
// keeps the destination size provably safe instead of relying on %d's worst case.
static void backup_path_for_slot(int slot, char* out, size_t out_size) {
    const char digit[2] = {slot > 0 ? (char)('0' + slot) : '\0', '\0'};
    snprintf(out, out_size, "%s.bad%s", HABITFLOW_BIN, digit);
}

typedef enum {
    HfQuarantineKept,
    HfQuarantineAllSlotsTaken,
    HfQuarantineRenameFailed,
} HfQuarantineResult;

// Renames the unreadable `source` file to the first free backup slot so a later
// corruption never erases an earlier one.
static HfQuarantineResult quarantine_unreadable(
    const HfStorePort* port,
    const char* source,
    char* path_buf,
    size_t path_buf_size) {
    bool taken[HF_STORE_BACKUP_SLOTS];
    char candidate[HF_BACKUP_PATH_MAX];
    for(int i = 0; i < HF_STORE_BACKUP_SLOTS; i++) {
        backup_path_for_slot(i, candidate, sizeof(candidate));
        taken[i] = port->stat(port->self, candidate) != HfPathMissing;
    }

    int slot = hf_store_backup_slot(taken);
    if(slot < 0) {
        return HfQuarantineAllSlotsTaken;
    }
    backup_path_for_slot(slot, path_buf, path_buf_size);
    return port->rename(port->self, source, path_buf) ? HfQuarantineKept :
                                                        HfQuarantineRenameFailed;
}

static void fill_quarantine_notice(
    HfQuarantineResult result,
    const char* source,
    const char* backup_path,
    char* notice,
    size_t notice_size) {
    if(!notice || notice_size == 0) {
        return;
    }
    if(result == HfQuarantineKept) {
        snprintf(
            notice,
            notice_size,
            "Couldn't read your saved data.\nA copy was kept as:\n%s",
            backup_path);
    } else if(result == HfQuarantineAllSlotsTaken) {
        snprintf(
            notice,
            notice_size,
            "Couldn't read your saved data, and every backup slot is full.\n"
            "It's still at:\n%s\nClear old .bad copies via qFlipper, then reopen "
            "the app.\nChanges won't be saved until then.",
            source);
    } else {
        snprintf(
            notice,
            notice_size,
            "Couldn't read your saved data, and it couldn't be backed up.\nIt's "
            "still at:\n%s\nCheck the SD card, then reopen the app.\nChanges "
            "won't be saved until then.",
            source);
    }
}

// A stat/IO failure on the storage itself: we cannot tell what's actually on disk, so
// nothing may be written until the user relaunches and it's checked again.
static void fill_check_failed_notice(char* notice, size_t notice_size) {
    if(!notice || notice_size == 0) {
        return;
    }
    snprintf(
        notice,
        notice_size,
        "Couldn't check your saved data.\nReopen the app to try "
        "again.\nChanges won't be saved until then.");
}

// .tmp decoded, but promoting it over `bin` (or the destination bin's slot after a
// quarantine) failed: the recovered data is used for this session, but nothing may be
// saved until a relaunch confirms it's actually safe on disk.
static void fill_restore_failed_notice(char* notice, size_t notice_size) {
    if(!notice || notice_size == 0) {
        return;
    }
    snprintf(
        notice,
        notice_size,
        "Recovered your saved data, but couldn't finish saving "
        "it.\nReopen the app to try again.\nChanges won't be "
        "saved until then.");
}

typedef enum {
    HfTmpMissing, // genuinely not there
    HfTmpDecoded, // read fine and parsed into *recovered
    HfTmpUnreadable, // exists (or its own stat failed) but couldn't be safely read
    HfTmpGarbage, // read fine, but the bytes don't decode
} HfTmpStatus;

// A stat/IO error on .tmp, or a read that comes back empty despite .tmp existing (a real
// save never writes an empty .tmp), must not be treated as "absent": that could hide the
// only good copy of the data behind what looks like a routine missing file.
static HfTmpStatus probe_tmp(const HfStorePort* port, HabitStore* recovered) {
    HfPathStat stat = port->stat(port->self, HABITFLOW_BIN_TMP);
    if(stat == HfPathMissing) {
        return HfTmpMissing;
    }
    if(stat == HfPathError) {
        return HfTmpUnreadable;
    }
    uint8_t buf[HF_STORE_BLOB_MAX];
    size_t n = port->read(port->self, HABITFLOW_BIN_TMP, buf, sizeof(buf));
    if(n == 0) {
        return HfTmpUnreadable;
    }
    return decode_store(recovered, buf, n) ? HfTmpDecoded : HfTmpGarbage;
}

HfStoreLoadStatus
    habit_store_load(const HfStorePort* port, HabitStore* store, char* notice, size_t notice_size) {
    habit_store_init(store);
    if(notice && notice_size > 0) {
        notice[0] = '\0';
    }

    HfPathStat bin_stat = port->stat(port->self, HABITFLOW_BIN);
    if(bin_stat == HfPathError) {
        store->save_blocked = true;
        fill_check_failed_notice(notice, notice_size);
        return HfStoreLoadStatError;
    }

    if(bin_stat == HfPathOk) {
        if(decode_path(port, HABITFLOW_BIN, store)) {
            return HfStoreLoadOk;
        }
        // habit_store_save's rename can leave habitflow.bin missing or short partway
        // through (see hf_store_port.h): the temp file it wrote first is only removed
        // after the copy fully succeeds, so it may still hold the complete data.
        HabitStore recovered;
        HfTmpStatus tmp_status = probe_tmp(port, &recovered);
        if(tmp_status == HfTmpUnreadable) {
            store->save_blocked = true;
            fill_check_failed_notice(notice, notice_size);
            return HfStoreLoadStatError;
        }
        bool tmp_ok = tmp_status == HfTmpDecoded;

        // Whatever is unreadable at `bin` must never be silently destroyed by promoting a
        // rescued .tmp over it: quarantine it first, exactly as when nothing rescues it.
        char backup_path[HF_BACKUP_PATH_MAX];
        HfQuarantineResult result =
            quarantine_unreadable(port, HABITFLOW_BIN, backup_path, sizeof(backup_path));

        bool promoted = result == HfQuarantineKept && tmp_ok &&
                        port->rename(port->self, HABITFLOW_BIN_TMP, HABITFLOW_BIN);
        if(tmp_ok) {
            *store = recovered;
        }
        if(promoted) {
            return HfStoreLoadOk;
        }
        if(result == HfQuarantineKept && tmp_ok) {
            // Backing up `bin` worked, but restoring the rescued .tmp over its
            // now-vacated slot didn't: don't guess at the disk's state afterward.
            store->save_blocked = true;
            fill_restore_failed_notice(notice, notice_size);
        } else {
            store->save_blocked = (result != HfQuarantineKept);
            fill_quarantine_notice(result, HABITFLOW_BIN, backup_path, notice, notice_size);
        }
        return HfStoreLoadCorrupt;
    }

    // bin_stat == HfPathMissing
    HabitStore recovered;
    HfTmpStatus tmp_status = probe_tmp(port, &recovered);
    if(tmp_status == HfTmpUnreadable) {
        store->save_blocked = true;
        fill_check_failed_notice(notice, notice_size);
        return HfStoreLoadStatError;
    }
    if(tmp_status == HfTmpGarbage) {
        // `bin` was never written, but `.tmp` holds something that doesn't decode --
        // maybe the only copy of an interrupted save. Preserve it exactly like an
        // unreadable `bin`, rather than discarding it as if nothing had ever been saved.
        char backup_path[HF_BACKUP_PATH_MAX];
        HfQuarantineResult result =
            quarantine_unreadable(port, HABITFLOW_BIN_TMP, backup_path, sizeof(backup_path));
        store->save_blocked = (result != HfQuarantineKept);
        fill_quarantine_notice(result, HABITFLOW_BIN_TMP, backup_path, notice, notice_size);
        return HfStoreLoadCorrupt;
    }
    if(tmp_status != HfTmpDecoded) {
        return HfStoreLoadMissing;
    }
    *store = recovered;
    if(port->rename(port->self, HABITFLOW_BIN_TMP, HABITFLOW_BIN)) {
        return HfStoreLoadOk;
    }
    // The only good copy is still sitting in .tmp and wasn't confirmed safe at `bin`:
    // a save must not be allowed to overwrite it before that's sorted out.
    store->save_blocked = true;
    fill_restore_failed_notice(notice, notice_size);
    return HfStoreLoadCorrupt;
}

bool habit_store_save(const HfStorePort* port, HabitStore* store) {
    if(store->save_blocked || store->habit_count > HF_HABITS_MAX) {
        return false;
    }

    // `bin` must be known-good before this save is allowed to touch `.tmp`: a previous
    // save's rename can fail partway through (see hf_store_port.h) and leave `.tmp` as the
    // only surviving copy of earlier data. Writing `.tmp` create-always truncates it
    // immediately, even if the write itself then fails, so that copy must be promoted to
    // `bin` first -- never risked on a write before it's safe to lose.
    HabitStore existing;
    if(!decode_path(port, HABITFLOW_BIN, &existing)) {
        HabitStore recovered;
        HfTmpStatus tmp_status = probe_tmp(port, &recovered);
        if(tmp_status == HfTmpDecoded) {
            if(!port->rename(port->self, HABITFLOW_BIN_TMP, HABITFLOW_BIN)) {
                store->save_blocked = true;
                return false;
            }
        } else if(tmp_status != HfTmpMissing) {
            // `.tmp` exists but can't be trusted (unreadable, or doesn't decode) while
            // `bin` is also bad: don't risk destroying it with a blind overwrite.
            store->save_blocked = true;
            return false;
        }
        // HfTmpMissing: nothing to protect -- this is an ordinary fresh save (e.g. right
        // after a load quarantined an unreadable `bin` and vacated its path).
    }

    port->mkdir(port->self, APPS_DATA_DIR);

    uint8_t buf[HF_STORE_BLOB_MAX];
    size_t n = encode_store(store, buf, sizeof(buf));
    if(n == 0 || !port->write(port->self, HABITFLOW_BIN_TMP, buf, n)) {
        // Safe to discard: `bin` is already known-good, either checked above or just
        // promoted from `.tmp`, so nothing but this attempt's own half-write is lost.
        port->remove(port->self, HABITFLOW_BIN_TMP);
        return false;
    }

    // Not atomic: the firmware's rename removes the destination, copies, then removes the
    // source (see hf_store_port.h). habitflow.bin.tmp is only removed once the copy has
    // fully succeeded, so a crash here leaves either the old habitflow.bin or the fresh
    // .tmp intact for habit_store_load to recover from, never a half-written habitflow.bin.
    return port->rename(port->self, HABITFLOW_BIN_TMP, HABITFLOW_BIN);
}

bool habit_store_add(const HfStorePort* port, HabitStore* store, const Habit* habit) {
    if(store->habit_count >= HF_HABITS_MAX) {
        return false;
    }
    store->habits[store->habit_count] = *habit;
    store->habit_count++;
    return habit_store_save(port, store);
}

bool habit_store_replace_at(
    const HfStorePort* port,
    HabitStore* store,
    size_t index,
    const Habit* habit) {
    if(index >= store->habit_count) {
        return false;
    }
    store->habits[index] = *habit;
    return habit_store_save(port, store);
}

bool habit_store_delete_at(const HfStorePort* port, HabitStore* store, size_t index) {
    if(index >= store->habit_count || store->habit_count == 0) {
        return false;
    }
    for(size_t i = index + 1; i < store->habit_count; i++) {
        store->habits[i - 1] = store->habits[i];
    }
    store->habit_count--;
    memset(&store->habits[store->habit_count], 0, sizeof(Habit));
    return habit_store_save(port, store);
}
