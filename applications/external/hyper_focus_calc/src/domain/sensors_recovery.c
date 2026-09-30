#include "include/domain/sensors_recovery.h"
#include <stdio.h>
#include <string.h>

// Read failures and parse failures are kept apart on purpose: a file we can't even read is
// a storage fault (refuse to touch it further), while one we can read but that doesn't
// decode — including one too large to be a valid record at all — is ordinary corruption
// (safe to move aside).
typedef enum {
    PathOk = 0,
    PathMissing,
    PathGarbage,
    PathIoError,
} PathState;

static PathState read_and_decode(const StoragePort* port, const char* path, SensorsRecord* out) {
    const StoreStat stat = port->stat(port->ctx, path);
    if(stat == StoreStatMissing) {
        return PathMissing;
    }
    if(stat == StoreStatError) {
        return PathIoError;
    }
    uint8_t buf[SENSORS_FILE_MAX_BYTES];
    size_t len = 0;
    const ReadResult r = port->read_all(port->ctx, path, buf, sizeof(buf), &len);
    if(r == ReadError) {
        return PathIoError; // stat says it's there, but reading it failed outright
    }
    if(r == ReadTooLarge) {
        return PathGarbage; // bigger than any valid record: corrupt, not a storage fault
    }
    return sensors_codec_decode(buf, len, out) ? PathOk : PathGarbage;
}

static void seed_default(SensorsRecord* out) {
    memset(out, 0, sizeof(*out));
    sensor_data_set_defaults(&out->sensors[0]);
    out->count = 1;
    out->active_index = 0;
}

// SENSORS_BACKUP_SLOTS is 10, so slot is always a single digit; a fixed-width suffix keeps
// the destination size provably safe instead of relying on %d's worst case.
static void backup_path_for_slot(const char* main_path, int slot, char* out, size_t out_size) {
    const char digit[2] = {slot > 0 ? (char)('0' + slot) : '\0', '\0'};
    snprintf(out, out_size, "%s.bad%s", main_path, digit);
}

typedef enum {
    QuarantineOk = 0,
    QuarantineNoFreeSlot,
    QuarantineRenameFailed,
} QuarantineOutcome;

// Quarantines `source_path` (which may be `main_path` itself, or its `.tmp` when main is
// missing and .tmp is the only evidence left) to the first free "<name_key>.bad" slot.
static QuarantineOutcome quarantine_file(
    const StoragePort* port,
    const char* source_path,
    const char* name_key,
    char* kept_path,
    size_t kept_path_size) {
    bool taken[SENSORS_BACKUP_SLOTS];
    char candidate[SENSORS_PATH_MAX];
    for(int i = 0; i < SENSORS_BACKUP_SLOTS; i++) {
        backup_path_for_slot(name_key, i, candidate, sizeof(candidate));
        taken[i] = port->exists(port->ctx, candidate);
    }

    const int slot = sensors_recovery_pick_slot(taken);
    if(slot < 0) {
        return QuarantineNoFreeSlot;
    }
    backup_path_for_slot(name_key, slot, kept_path, kept_path_size);
    if(!port->rename(port->ctx, source_path, kept_path)) {
        kept_path[0] = '\0';
        return QuarantineRenameFailed;
    }
    return QuarantineOk;
}

int sensors_recovery_pick_slot(const bool taken[SENSORS_BACKUP_SLOTS]) {
    for(int i = 0; i < SENSORS_BACKUP_SLOTS; i++) {
        if(!taken[i]) {
            return i;
        }
    }
    return -1;
}

SensorsRecoveryResult
    sensors_recovery_load(const StoragePort* port, const char* main_path, SensorsRecord* out) {
    SensorsRecoveryResult result = {0};

    const PathState main_state = read_and_decode(port, main_path, out);
    if(main_state == PathOk) {
        result.status = StoreLoadOk;
        return result;
    }

    char tmp_path[SENSORS_PATH_MAX];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", main_path);
    const PathState tmp_state = read_and_decode(port, tmp_path, out);

    if(tmp_state == PathOk) {
        if(port->rename(port->ctx, tmp_path, main_path)) {
            result.status = StoreLoadOk;
            return result;
        }
        // The only good copy is stuck in .tmp and couldn't be moved into place: refuse
        // every future save rather than risk it being overwritten.
        result.status = StoreLoadIoError;
        result.save_blocked = true;
        seed_default(out);
        return result;
    }

    if(tmp_state == PathIoError) {
        // .tmp is there but a read on it failed: don't touch main, just refuse saves.
        result.status = StoreLoadIoError;
        result.save_blocked = true;
        seed_default(out);
        return result;
    }

    if(tmp_state == PathGarbage && main_state == PathMissing) {
        // Main is genuinely absent, but .tmp holds bytes that read fine and still don't
        // parse: that's the only evidence there is, so it's quarantined like any other
        // unreadable file instead of silently being treated as a plain first run.
        result.status = StoreLoadCorrupt;
        const QuarantineOutcome q =
            quarantine_file(port, tmp_path, main_path, result.kept_path, sizeof(result.kept_path));
        if(q != QuarantineOk) {
            result.save_blocked = true;
            result.no_free_backup_slot = (q == QuarantineNoFreeSlot);
        }
        seed_default(out);
        return result;
    }

    // tmp_state is Missing, or Garbage alongside a main problem of its own (that problem
    // already dominates; the stale .tmp is left alone and overwritten by the next save).
    if(main_state == PathMissing) {
        result.status = StoreLoadMissing;
    } else if(main_state == PathIoError) {
        result.status = StoreLoadIoError;
        result.save_blocked = true;
    } else { // PathGarbage: main exists, reads fine, just doesn't decode
        result.status = StoreLoadCorrupt;
        const QuarantineOutcome q = quarantine_file(
            port, main_path, main_path, result.kept_path, sizeof(result.kept_path));
        if(q != QuarantineOk) {
            result.save_blocked = true;
            result.no_free_backup_slot = (q == QuarantineNoFreeSlot);
        }
    }

    seed_default(out);
    return result;
}

// Confirms it's safe to let a save overwrite .tmp. If main already holds valid data,
// anything in .tmp is disposable. Otherwise, .tmp might hold the only surviving copy of an
// earlier save whose rename never completed — promote it into main first so a fresh write
// can never destroy it. A stat/read fault on either path refuses rather than guesses.
static bool ensure_main_ready_for_save(const StoragePort* port, const char* main_path) {
    SensorsRecord scratch;
    const PathState main_state = read_and_decode(port, main_path, &scratch);
    if(main_state == PathOk) {
        return true;
    }
    if(main_state == PathIoError) {
        return false;
    }

    char tmp_path[SENSORS_PATH_MAX];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", main_path);
    const PathState tmp_state = read_and_decode(port, tmp_path, &scratch);
    if(tmp_state == PathIoError) {
        return false;
    }
    if(tmp_state != PathOk) {
        return true; // missing or garbage: nothing recoverable, safe to overwrite
    }
    return port->rename(port->ctx, tmp_path, main_path);
}

SensorsSaveOutcome sensors_recovery_save(
    const StoragePort* port,
    const char* main_path,
    const SensorsRecord* rec,
    bool already_blocked) {
    if(already_blocked) {
        return SensorsSaveBlocked;
    }
    if(rec->count == 0 || rec->count > SENSOR_MAX_COUNT || rec->active_index >= rec->count) {
        return SensorsSaveFailed;
    }
    if(!ensure_main_ready_for_save(port, main_path)) {
        return SensorsSaveBlocked;
    }

    char tmp_path[SENSORS_PATH_MAX];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", main_path);

    uint8_t buf[SENSORS_FILE_MAX_BYTES];
    const size_t len = sensors_codec_encode(rec, buf, sizeof(buf));
    if(len == 0) {
        return SensorsSaveFailed;
    }
    if(!port->write_all(port->ctx, tmp_path, buf, len)) {
        return SensorsSaveFailed;
    }
    if(!port->rename(port->ctx, tmp_path, main_path)) {
        return SensorsSaveBlocked;
    }
    return SensorsSaveOk;
}
