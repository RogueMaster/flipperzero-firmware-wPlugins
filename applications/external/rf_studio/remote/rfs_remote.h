#pragma once

#include "../rfs_types.h"
#include "../signal/rfs_signal.h"

/*
 * Remote Manager (Specification Section 6, Addendum 001 Section 7).
 * A remote is a named, ordered, variable-length list of buttons. Buttons reference signals in
 * the Signal Library by ID and never embed signal data. Button order in the file is the display
 * order.
 */

#define RFS_REMOTE_SCHEMA_VERSION 2

typedef enum {
    RfsCategoryGeneral,
    RfsCategoryLighting,
    RfsCategoryFans,
    RfsCategoryRelays,
    RfsCategorySmartPlugs,
    RfsCategoryHomeAutomation,
    RfsCategoryAccess,
    RfsCategoryOther,
    RfsCategoryCount,
} RfsCategory;

typedef struct {
    char id[RFS_ID_LEN];
    char name[RFS_NAME_LEN + 1];
    char signal_id[RFS_ID_LEN]; /* empty when no signal is assigned */
    uint32_t created;
    uint32_t updated;
} RfsButton;

typedef struct {
    char id[RFS_ID_LEN];
    char name[RFS_NAME_LEN + 1];
    char description[64];
    uint8_t category; /* RfsCategory */
    bool favorite;
    uint32_t created;
    uint32_t updated;
    uint32_t last_used;
    uint8_t preferred_radio; /* RfsRadioMode */
    uint32_t preferred_frequency; /* 0 = none yet */
    char preferred_preset[RFS_PRESET_LEN];
    RfsButton* buttons;
    size_t count;
    size_t capacity;
} RfsRemote;

/** Lightweight entry for library screens: read from the file header only. */
typedef struct {
    char id[RFS_ID_LEN];
    char name[RFS_NAME_LEN + 1];
    uint8_t category;
    bool favorite;
    uint32_t last_used;
    uint32_t updated;
    uint32_t button_count;
} RfsRemoteSummary;

typedef struct {
    RfsRemoteSummary* items;
    size_t count;
    size_t capacity;
} RfsRemoteList;

const char* rfs_category_name(RfsCategory category);
const char* rfs_category_short(RfsCategory category);

void rfs_remote_init(RfsRemote* remote);
void rfs_remote_clear(RfsRemote* remote);

/** Start a new empty remote with a fresh identifier. */
void rfs_remote_new(Storage* storage, RfsRemote* remote, const char* name);

void rfs_remote_path(const char* id, FuriString* out);

bool rfs_remote_load(Storage* storage, const char* id, RfsRemote* remote, FuriString* error);
bool rfs_remote_load_path(Storage* storage, const char* path, RfsRemote* remote, FuriString* error);
bool rfs_remote_save(Storage* storage, RfsRemote* remote);
bool rfs_remote_save_path(Storage* storage, const char* path, const RfsRemote* remote);
bool rfs_remote_delete(Storage* storage, const char* id);

/** Append a button. Returns NULL when memory is too low. */
RfsButton* rfs_remote_add_button(
    Storage* storage,
    RfsRemote* remote,
    const char* name,
    const char* signal_id);
void rfs_remote_remove_button(RfsRemote* remote, size_t index);
bool rfs_remote_move_button(RfsRemote* remote, size_t index, bool up);
RfsButton* rfs_remote_duplicate_button(Storage* storage, RfsRemote* remote, size_t index);

/** Save a copy under a new identifier and name. */
bool rfs_remote_duplicate(Storage* storage, const RfsRemote* source, FuriString* out_name);

/** Library listing (Section 6.2). */
void rfs_remote_list_init(RfsRemoteList* list);
void rfs_remote_list_clear(RfsRemoteList* list);
bool rfs_remote_list_load(Storage* storage, RfsRemoteList* list);
void rfs_remote_list_sort(RfsRemoteList* list, RfsSort sort);

/** Count buttons in all remotes that reference a signal. Fills a readable list when out is set. */
size_t rfs_remote_signal_usage(Storage* storage, const char* signal_id, FuriString* out);

/** Export a remote and its signals to exports/<id>/ (REM-010). */
bool rfs_remote_export(
    Storage* storage,
    const RfsRemote* remote,
    RfsSignalIndex* index,
    FuriString* out_dir);

/** Import an exported remote folder. Copies missing signals into the library (REM-009). */
bool rfs_remote_import(
    Storage* storage,
    const char* remote_path,
    RfsSignalIndex* index,
    FuriString* out_name,
    FuriString* error);
