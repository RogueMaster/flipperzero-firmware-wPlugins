#include "include/stock_recovery.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void tmp_path_for(const char* main_path, char* out, size_t out_size) {
    snprintf(out, out_size, "%s.tmp", main_path);
}

/* STORE_BACKUP_SLOTS is 10, so slot is always a single digit; a fixed-width
 * suffix keeps the destination size provably safe instead of relying on %d's
 * worst case. */
static void backup_path_for_slot(const char* main_path, int slot, char* out, size_t out_size) {
    const char digit[2] = {slot > 0 ? (char)('0' + slot) : '\0', '\0'};
    snprintf(out, out_size, "%s.bad%s", main_path, digit);
}

typedef struct {
    bool ok;
    bool slots_full;
    char kept_path[STOCK_PATH_MAX];
} QuarantineOutcome;

/* `source_path` is what actually gets renamed away; `main_path` only names the
 * slots
 * (".bad", ".bad1".."bad9"), since both the main file and an orphaned `.tmp`
 * are quarantined into the same slot sequence rooted at the main path. */
static QuarantineOutcome
    quarantine_file(const StockStoragePort* port, const char* main_path, const char* source_path) {
    QuarantineOutcome out = {0};
    bool taken[STORE_BACKUP_SLOTS];
    char candidate[STOCK_PATH_MAX];
    for(int i = 0; i < STORE_BACKUP_SLOTS; i++) {
        backup_path_for_slot(main_path, i, candidate, sizeof(candidate));
        taken[i] = port->exists(port->ctx, candidate);
    }

    const int slot = store_status_backup_slot(taken);
    if(slot < 0) {
        out.slots_full = true;
        return out;
    }

    backup_path_for_slot(main_path, slot, out.kept_path, sizeof(out.kept_path));
    out.ok = port->rename(port->ctx, source_path, out.kept_path);
    if(!out.ok) {
        out.kept_path[0] = '\0';
    }
    return out;
}

/* The DB has no header, so a main file that decodes as a valid (possibly empty)
 * set of records is not automatically the last complete save: a rename's copy
 * step can crash partway and still leave a whole number of records behind, or
 * nothing at all. Compare main's bytes against `.tmp`'s raw bytes before
 * trusting main outright:
 *   - no `.tmp` (confirmed missing) -- main already decoded fine, done.
 *   - `.tmp`'s stat failed, or it exists but can't be read -- we can't tell
 *     whether it still holds more than main; block rather than guess.
 *   - main is a strict byte-prefix of tmp (tmp is longer, its first bytes match
 *     main exactly, and its length is itself a whole number of records) --
 *     main is that partial copy; promote tmp. If the promote rename itself
 *     fails, the copy step may already have completed on disk before whatever
 *     crashed it, making the `main_items` already in hand stale -- block
 *     instead of risking a save built on it.
 *   - main and tmp are byte-identical -- the rename finished except removing
 *     its source; just remove tmp.
 *   - anything else -- tmp is unrelated or itself a torn write; main is
 *     already known good, so clear tmp out of the way rather than reconcile
 *     against it again later.
 * Ownership: on every non-blocked return this frees exactly one of
 * `main_items` or the raw tmp buffer it read, and returns the other as
 * `*out_items`; on a blocked return both are freed and `*out_items` is left
 * untouched by the caller. */
static StockLoadResult reconcile_main_with_tmp(
    const StockStoragePort* port,
    const char* main_path,
    const char* tmp_path,
    StockItem* main_items,
    size_t main_count,
    StockItem** out_items,
    size_t* out_count) {
    StockLoadResult result = {0};
    result.status = StoreLoadOk;

    const StoreStat tmp_stat = port->stat(port->ctx, tmp_path);

    if(tmp_stat == StoreStatMissing) {
        /* No tmp at all: main already decoded fine, nothing to reconcile against.
     */
        *out_items = main_items;
        *out_count = main_count;
        return result;
    }

    if(tmp_stat == StoreStatError) {
        /* We can't tell what's in `.tmp` -- it might still hold more than main (a
     * partial copy in progress), so main can't be trusted outright either. */
        free(main_items);
        result.status = StoreLoadIoError;
        result.save_blocked = true;
        return result;
    }

    uint8_t* tmp_raw = NULL;
    size_t tmp_raw_len = 0;
    if(!port->read_raw(port->ctx, tmp_path, &tmp_raw, &tmp_raw_len)) {
        /* `.tmp` exists but we can't read it (I/O error or allocation failure):
     * same reasoning as a stat error -- block rather than guess. */
        free(main_items);
        result.status = StoreLoadIoError;
        result.save_blocked = true;
        return result;
    }

    const uint8_t* main_bytes = (const uint8_t*)main_items;
    const size_t main_len = main_count * sizeof(StockItem);
    const bool tmp_aligned = (tmp_raw_len % sizeof(StockItem)) == 0;
    const bool main_is_prefix = tmp_raw_len >= main_len &&
                                (main_len == 0 || memcmp(main_bytes, tmp_raw, main_len) == 0);

    if(tmp_aligned && main_is_prefix && tmp_raw_len > main_len) {
        if(port->rename(port->ctx, tmp_path, main_path)) {
            free(main_items);
            *out_items = (StockItem*)tmp_raw;
            *out_count = tmp_raw_len / sizeof(StockItem);
            return result;
        }
        /* Promotion failed: the copy step may have already completed on disk before
     * the crash that made `rename` return false (e.g. one right before removing
     * the source), so the `main_items` read moments ago can already be stale --
     * trusting it here could save a merge that silently drops whatever `.tmp`
     * (or main's own new content) actually holds. Block instead; a fresh load
     * will see the real post-crash state. */
        free(main_items);
        free(tmp_raw);
        result.status = StoreLoadIoError;
        result.save_blocked = true;
        return result;
    }

    /* Byte-identical (the rename completed except removing its source), or an
   * unrelated/torn `.tmp`: neither needs to be kept once main is already
   * trusted. */
    port->remove(port->ctx, tmp_path);
    free(tmp_raw);
    *out_items = main_items;
    *out_count = main_count;
    return result;
}

StockLoadResult stock_recovery_load(
    const StockStoragePort* port,
    const char* main_path,
    StockItem** out_items,
    size_t* out_count) {
    StockLoadResult result = {0};
    *out_items = NULL;
    *out_count = 0;

    char tmp_path[STOCK_PATH_MAX];
    tmp_path_for(main_path, tmp_path, sizeof(tmp_path));

    const StoreStat main_stat = port->stat(port->ctx, main_path);

    if(main_stat == StoreStatError) {
        /* A genuine stat failure: completely unknown state. Never promote or
     * quarantine anything over it on a guess. */
        result.status = StoreLoadIoError;
        result.save_blocked = true;
        return result;
    }

    bool main_confirmed_bad = false; /* Framing or TooLarge: known corrupt, never "unknown" */
    if(main_stat == StoreStatOk) {
        StockItem* main_items = NULL;
        size_t main_count = 0;
        const StockReadOutcome ro = port->read_all(port->ctx, main_path, &main_items, &main_count);
        if(ro == StockReadOk) {
            return reconcile_main_with_tmp(
                port, main_path, tmp_path, main_items, main_count, out_items, out_count);
        }
        if(ro == StockReadBlocked) {
            /* Unknown state (allocation or a read error despite a well-formed size):
       * block without ever looking at `.tmp` -- promoting over an
       * unread-but-possibly-good main would destroy it on a guess. */
            result.status = StoreLoadIoError;
            result.save_blocked = true;
            return result;
        }
        main_confirmed_bad = true; /* Framing or TooLarge */
    }

    /* From here main is either Missing or confirmed bad -- never merely
   * "unknown". Only FSE_NOT_EXIST means "no tmp"; any other stat outcome means
   * we can't tell what's in it. A `.tmp` that exists but won't parse is
   * normally treated the same way -- it's an internal recovery file, not the
   * user's data -- except when the main file is confirmed missing: then `.tmp`
   * is the only trace of whatever the user last tried to save, and it gets
   * quarantined like any other unreadable data. */
    const StoreStat tmp_stat = port->stat(port->ctx, tmp_path);
    bool tmp_unknown = (tmp_stat == StoreStatError);
    bool tmp_exists_but_unreadable = false;
    StockItem* tmp_items = NULL;
    size_t tmp_count = 0;
    bool tmp_parsed_ok = false;
    if(tmp_stat == StoreStatOk) {
        const StockReadOutcome ro = port->read_all(port->ctx, tmp_path, &tmp_items, &tmp_count);
        if(ro == StockReadOk) {
            tmp_parsed_ok = true;
        } else if(ro == StockReadBlocked) {
            tmp_unknown = true;
        } else {
            tmp_exists_but_unreadable = true;
        }
    }

    if(tmp_parsed_ok) {
        if(main_confirmed_bad) {
            /* Quarantine the confirmed-bad main before promoting tmp over it -- its
       * bytes must not simply vanish under the rename. */
            const QuarantineOutcome q = quarantine_file(port, main_path, main_path);
            if(!q.ok) {
                free(tmp_items);
                result.status = StoreLoadCorrupt;
                result.save_blocked = true;
                result.quarantine_slots_full = q.slots_full;
                return result;
            }
            if(port->rename(port->ctx, tmp_path, main_path)) {
                *out_items = tmp_items;
                *out_count = tmp_count;
                result.status = StoreLoadCorrupt; /* main was bad; still worth telling the user */
                strncpy(result.kept_path, q.kept_path, sizeof(result.kept_path) - 1);
                return result;
            }
            free(tmp_items);
            result.status = StoreLoadIoError;
            result.save_blocked = true;
            return result;
        }

        if(port->rename(port->ctx, tmp_path, main_path)) {
            *out_items = tmp_items;
            *out_count = tmp_count;
            result.status = StoreLoadOk;
            return result;
        }
        /* Promotion failed: the firmware's rename never removes its source on a
     * failed copy, so `.tmp` is still the only good copy. Leave it alone and
     * refuse saves rather than risk clobbering it with a new one. */
        free(tmp_items);
        result.status = StoreLoadIoError;
        result.save_blocked = true;
        return result;
    }

    if(main_stat == StoreStatMissing && tmp_exists_but_unreadable) {
        result.status = StoreLoadCorrupt;
        const QuarantineOutcome q = quarantine_file(port, main_path, tmp_path);
        if(q.ok) {
            strncpy(result.kept_path, q.kept_path, sizeof(result.kept_path) - 1);
        } else {
            result.save_blocked = true;
            result.quarantine_slots_full = q.slots_full;
        }
        return result;
    }

    if(tmp_unknown) {
        result.status = StoreLoadIoError;
        result.save_blocked = true;
        return result;
    }

    if(main_stat == StoreStatMissing) {
        result.status = StoreLoadMissing;
        return result;
    }

    /* main_confirmed_bad, with no usable tmp to fall back on: quarantine main
   * alone. */
    result.status = StoreLoadCorrupt;
    const QuarantineOutcome q = quarantine_file(port, main_path, main_path);
    if(q.ok) {
        strncpy(result.kept_path, q.kept_path, sizeof(result.kept_path) - 1);
    } else {
        result.save_blocked = true;
        result.quarantine_slots_full = q.slots_full;
    }
    return result;
}

/* Writes `items`/`count` to `<main_path>.tmp`, then renames it into
 * `main_path`. Only used internally by upsert/delete_at, which always resolve
 * `.tmp` via stock_recovery_load first, so main is already known good by the
 * time this runs -- a failed write's leftover partial `.tmp` is safe to discard
 * immediately. */
static bool stock_recovery_save(
    const StockStoragePort* port,
    const char* main_path,
    const StockItem* items,
    size_t count) {
    char tmp_path[STOCK_PATH_MAX];
    tmp_path_for(main_path, tmp_path, sizeof(tmp_path));
    if(!port->write_all(port->ctx, tmp_path, items, count)) {
        port->remove(port->ctx, tmp_path);
        return false;
    }
    return port->rename(port->ctx, tmp_path, main_path);
}

static StockWriteOutcome blocked_outcome(const StockLoadResult* load) {
    if(load->status == StoreLoadCorrupt) {
        return load->quarantine_slots_full ? StockWriteBlockedSlotsFull :
                                             StockWriteBlockedQuarantineFailed;
    }
    return StockWriteBlockedIoError;
}

static bool merge_by_uid(
    const StockItem* items,
    size_t count,
    const StockItem* item,
    StockItem** out_items,
    size_t* out_count) {
    size_t idx = (size_t)-1;
    for(size_t i = 0; i < count; i++) {
        if(items[i].uid_len == item->uid_len &&
           memcmp(items[i].uid, item->uid, item->uid_len) == 0) {
            idx = i;
            break;
        }
    }

    if(idx != (size_t)-1) {
        StockItem* copy = malloc(count * sizeof(StockItem));
        if(!copy) {
            return false;
        }
        memcpy(copy, items, count * sizeof(StockItem));
        copy[idx] = *item;
        *out_items = copy;
        *out_count = count;
        return true;
    }

    StockItem* grown = malloc((count + 1) * sizeof(StockItem));
    if(!grown) {
        return false;
    }
    if(count > 0) {
        memcpy(grown, items, count * sizeof(StockItem));
    }
    grown[count] = *item;
    *out_items = grown;
    *out_count = count + 1;
    return true;
}

StockWriteResult stock_recovery_upsert(
    const StockStoragePort* port,
    const char* main_path,
    const StockItem* item) {
    StockWriteResult result = {StockWriteFailed, {0}};

    StockItem* items = NULL;
    size_t count = 0;
    const StockLoadResult load = stock_recovery_load(port, main_path, &items, &count);
    if(load.save_blocked) {
        free(items);
        result.outcome = blocked_outcome(&load);
        return result;
    }
    /* A quarantine already happened on disk even if everything below now fails;
   * the notice about it must not be dropped along with a later failure. */
    if(load.status == StoreLoadCorrupt) {
        strncpy(result.kept_path, load.kept_path, sizeof(result.kept_path) - 1);
    }

    StockItem* merged = NULL;
    size_t merged_count = 0;
    const bool merged_ok = merge_by_uid(items, count, item, &merged, &merged_count);
    free(items);
    if(!merged_ok) {
        return result; /* StockWriteFailed; nothing new was written */
    }

    if(merged_count * sizeof(StockItem) > port->max_bytes) {
        free(merged);
        result.outcome = StockWriteRefusedOverLimit;
        return result;
    }

    const bool saved = stock_recovery_save(port, main_path, merged, merged_count);
    free(merged);
    if(!saved) {
        return result; /* StockWriteFailed */
    }

    result.outcome = load.status == StoreLoadCorrupt ? StockWriteOkQuarantined : StockWriteOk;
    return result;
}

StockWriteResult
    stock_recovery_delete_at(const StockStoragePort* port, const char* main_path, size_t index) {
    StockWriteResult result = {StockWriteFailed, {0}};

    StockItem* items = NULL;
    size_t count = 0;
    const StockLoadResult load = stock_recovery_load(port, main_path, &items, &count);
    if(load.save_blocked) {
        free(items);
        result.outcome = blocked_outcome(&load);
        return result;
    }
    if(load.status == StoreLoadCorrupt) {
        strncpy(result.kept_path, load.kept_path, sizeof(result.kept_path) - 1);
    }

    if(index >= count) {
        free(items);
        return result; /* StockWriteFailed: nothing at that index */
    }

    memmove(&items[index], &items[index + 1], (count - index - 1) * sizeof(StockItem));
    const bool saved = stock_recovery_save(port, main_path, items, count - 1);
    free(items);
    if(!saved) {
        return result; /* StockWriteFailed */
    }

    result.outcome = load.status == StoreLoadCorrupt ? StockWriteOkQuarantined : StockWriteOk;
    return result;
}
