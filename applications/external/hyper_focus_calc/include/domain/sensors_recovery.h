#pragma once

#include "include/domain/sensors_codec.h"
#include "include/domain/storage_port.h"
#include <stdbool.h>

#define SENSORS_BACKUP_SLOTS 10
#define SENSORS_PATH_MAX     64

typedef enum {
    StoreLoadOk = 0,
    StoreLoadMissing,
    StoreLoadCorrupt,
    StoreLoadIoError,
} StoreLoadStatus;

typedef struct {
    StoreLoadStatus status;
    bool save_blocked; // true once saves must be refused until storage is fixed
    bool no_free_backup_slot; // true only when Corrupt and every ".bad" slot was taken
    char kept_path[SENSORS_PATH_MAX]; // '\0' unless a file was just quarantined
} SensorsRecoveryResult;

typedef enum {
    SensorsSaveOk = 0,
    SensorsSaveFailed, // a transient failure (an invalid record, a write error); retryable
    SensorsSaveBlocked, // a rename into place failed, here or during a prior load: the
        // caller must stop saving until storage is fixed
} SensorsSaveOutcome;

// Picks the first free backup slot ("<file>.bad", then ".bad1".."bad9") so a second
// corruption never overwrites an earlier kept copy. taken[i] must be true if slot i is
// already occupied on disk. Returns the slot index, or -1 if every slot is taken.
int sensors_recovery_pick_slot(const bool taken[SENSORS_BACKUP_SLOTS]);

// Loads `main_path` through `port`. Precedence: `main_path` decodes -> use it. Else
// `<main_path>.tmp` decodes -> promote it into `main_path` and use it (a crashed save can
// leave the rename half-done); if that promotion itself fails, the only good copy is stuck
// in `.tmp`, so every future save is refused rather than risk it being overwritten. Else
// `.tmp` exists but a read on it failed outright -> refuse saves without touching
// `main_path` at all. Else if `main_path` is genuinely missing (stat FSE_NOT_EXIST) but
// `.tmp` exists and reads fine yet still doesn't parse, `.tmp` itself is quarantined (it's
// the only evidence there is) instead of being treated as a plain first run. Otherwise
// classify `main_path` alone: truly missing seeds one default sensor and nothing is
// written; present but not parsing is quarantined to the first free ".bad" slot; any other
// stat or read failure on `main_path` refuses saves without quarantining anything (we don't
// rename something we couldn't reliably inspect). `out` is filled with the recovered data,
// or one seeded default sensor when nothing could be recovered.
SensorsRecoveryResult
    sensors_recovery_load(const StoragePort* port, const char* main_path, SensorsRecord* out);

// Encodes `rec` to `<main_path>.tmp` and renames it into `main_path`. Before writing,
// confirms `main_path` already holds valid data, or that `.tmp` has nothing worth
// protecting — promoting an orphaned `.tmp` into `main_path` first if it does — so a save
// can never clobber a copy that was never safely moved into place. Refused outright when
// `already_blocked` or `rec` fails its own invariants; blocked when that promotion or the
// final rename into `main_path` fails.
SensorsSaveOutcome sensors_recovery_save(
    const StoragePort* port,
    const char* main_path,
    const SensorsRecord* rec,
    bool already_blocked);
