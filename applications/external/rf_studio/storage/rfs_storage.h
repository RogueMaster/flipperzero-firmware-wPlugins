#pragma once

#include "../rfs_types.h"
#include <flipper_format/flipper_format.h>

typedef struct {
    RfsRadioMode radio_mode;
    RfsDiscoverScope discover_scope;
    int32_t rssi_threshold; /* dBm */
    RfsSort sort;
    char default_preset[RFS_PRESET_LEN];
    bool logging;
} RfsSettings;

#define RFS_RSSI_MIN     (-95)
#define RFS_RSSI_MAX     (-40)
#define RFS_RSSI_STEP    5
#define RFS_RSSI_DEFAULT (-70)

/** Create the library folder tree. Returns false if the SD card is unavailable. */
bool rfs_storage_init(Storage* storage);

/** True when the SD card is mounted and usable. */
bool rfs_storage_sd_ready(Storage* storage);

/** Generate a unique identifier such as "R1A2B3C4D", checked against existing files in dir. */
void rfs_storage_new_id(Storage* storage, char prefix, const char* dir, const char* ext, char* out);

/** Current RTC time as a Unix timestamp. */
uint32_t rfs_storage_now(void);

/** Format a timestamp as "YYYY-MM-DD HH:MM". */
void rfs_storage_format_time(uint32_t timestamp, FuriString* out);

/*
 * Recoverable writes (Specification Section 12.6).
 * Data is written to "<path>.tmp", closed, then swapped into place.
 */
FlipperFormat* rfs_storage_atomic_begin(Storage* storage, const char* path);
bool rfs_storage_atomic_commit(Storage* storage, FlipperFormat* ff, const char* path, bool ok);

/** Repair a file after an interrupted atomic write. Call before reading a library file. */
void rfs_storage_recover(Storage* storage, const char* path);

/** Copy every regular file in src_dir into dst_dir (not recursive). Returns files copied. */
int32_t rfs_storage_copy_dir_files(Storage* storage, const char* src_dir, const char* dst_dir);

/** Back up remotes, signals and settings into backups/<timestamp>/. */
bool rfs_storage_backup(Storage* storage, FuriString* out_path);

/** Replace characters that are unsafe in file names. */
void rfs_storage_safe_filename(const char* name, FuriString* out);

void rfs_settings_defaults(RfsSettings* settings);
bool rfs_settings_load(Storage* storage, RfsSettings* settings);
bool rfs_settings_save(Storage* storage, const RfsSettings* settings);

/** Case-insensitive comparison helpers. */
int rfs_strcasecmp(const char* a, const char* b);
bool rfs_strcasestr(const char* haystack, const char* needle);
