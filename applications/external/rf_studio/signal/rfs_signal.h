#pragma once

#include "../rfs_types.h"
#include "../protocol/rfs_protocol.h"

/*
 * Signal Library (Specification Section 8, Addendum 001 Section 7.3).
 * Each signal is a standard .sub file stored as signals/<id>.sub. Metadata lives in a single
 * index file so screens never need to parse every signal file (Section 16.1).
 */

typedef struct {
    char id[RFS_ID_LEN];
    char name[RFS_NAME_LEN + 1];
    char protocol[RFS_PROTOCOL_LEN];
    char preset[RFS_PRESET_LEN];
    uint32_t frequency;
    uint32_t created;
    uint8_t kind; /* RfsSignalKind */
    uint8_t source; /* RfsSignalSource */
    bool verified; /* user confirmed a raw signal works */
} RfsSignalMeta;

typedef struct {
    RfsSignalMeta* items;
    size_t count;
    size_t capacity;
} RfsSignalIndex;

/** Result of validating a .sub file (SIG-012). */
typedef enum {
    RfsSubOk,
    RfsSubErrOpen,
    RfsSubErrHeader,
    RfsSubErrFrequency,
    RfsSubErrPreset,
    RfsSubErrProtocol,
    RfsSubErrData,
    RfsSubErrEmpty,
} RfsSubResult;

void rfs_signal_index_init(RfsSignalIndex* index);
void rfs_signal_index_clear(RfsSignalIndex* index);

bool rfs_signal_index_load(Storage* storage, RfsSignalIndex* index);
bool rfs_signal_index_load_path(Storage* storage, const char* path, RfsSignalIndex* index);
bool rfs_signal_index_save(Storage* storage, const RfsSignalIndex* index);
bool rfs_signal_index_save_path(Storage* storage, const char* path, const RfsSignalIndex* index);

RfsSignalMeta* rfs_signal_find(RfsSignalIndex* index, const char* id);

/** Append a blank entry. Returns NULL when memory is too low. */
RfsSignalMeta* rfs_signal_add(RfsSignalIndex* index);

void rfs_signal_remove(RfsSignalIndex* index, const char* id);

/** signals/<id>.sub */
void rfs_signal_path(const char* id, FuriString* out);

/** Replay eligibility for a stored signal. */
RfsReplay rfs_signal_replay(const RfsSignalMeta* meta);

/** Read and validate a .sub file, filling frequency, preset, protocol and kind in meta. */
RfsSubResult rfs_signal_inspect(Storage* storage, const char* path, RfsSignalMeta* meta);
const char* rfs_sub_result_text(RfsSubResult result);

/** Copy an external .sub file into the library. The original is never modified (SIG-013). */
RfsSubResult rfs_signal_import(
    Storage* storage,
    RfsSignalIndex* index,
    const char* source_path,
    RfsSignalMeta** out_meta);

/** Remove a signal file and its index entry. */
bool rfs_signal_delete(Storage* storage, RfsSignalIndex* index, const char* id);

/** Copy a signal to the firmware Sub-GHz folder (SIG-011). */
bool rfs_signal_export(Storage* storage, const RfsSignalMeta* meta, FuriString* out_path);

/** Signal details text for the details screen. */
void rfs_signal_describe(const RfsSignalMeta* meta, FuriString* out);
