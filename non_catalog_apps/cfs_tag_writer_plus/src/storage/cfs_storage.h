#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <storage/storage.h>

#include "src/data/cfs_data.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CFS_STORAGE_DIR       APP_DATA_PATH("tags")
// Templates are the same .nfc records, kept in their own folder.
#define CFS_TEMPLATE_DIR      APP_DATA_PATH("templates")
#define CFS_TEMPLATES_MAX     (32)
#define CFS_STORAGE_EXTENSION ".nfc"
#define CFS_STORAGE_NAME_MAX  (64)

/** Ensure the app's tag storage directory exists. */
void cfs_storage_ensure_dir(Storage* storage);

// First run only: copy saved tags and settings from the original CFS Tag
// Writer (apps_data/cfs_tag_writer), so they show up here too. Copies, never
// moves: the original app keeps working alongside this one.
void cfs_storage_import_original(Storage* storage);

/**
 * Save a tag as a Flipper MIFARE Classic 1K .nfc dump under CFS_STORAGE_DIR
 * using the given base name (without extension).
 *
 * The 48-byte plaintext is encrypted (fixed-key) into blocks 4/5/6; block 0 is
 * built from the UID (or a zero placeholder when uid_len == 0); block 7 carries
 * the UID-derived sector keys + access bits FF 07 80 69; all other sectors get
 * default keys. Returns true on success.
 */
bool cfs_storage_save(
    Storage* storage,
    const uint8_t plaintext[CFS_PLAINTEXT_SIZE],
    const uint8_t* uid,
    size_t uid_len,
    const char* name);

/** Same as cfs_storage_save, into the given folder (created if missing). */
bool cfs_storage_save_in(
    Storage* storage,
    const char* dir,
    const uint8_t plaintext[CFS_PLAINTEXT_SIZE],
    const uint8_t* uid,
    size_t uid_len,
    const char* name);

/** Same as cfs_storage_exists, in the given folder. */
bool cfs_storage_exists_in(Storage* storage, const char* dir, const char* name);

/**
 * Base names (no folder, no extension) of the .nfc records in dir, sorted
 * A-Z, at most max of them. Returns how many were found.
 */
size_t cfs_storage_list(
    Storage* storage,
    const char* dir,
    char (*names)[CFS_STORAGE_NAME_MAX],
    size_t max);

/** Full path of a record from its folder and base name. */
void cfs_storage_path_in(const char* dir, const char* name, char* out, size_t out_size);

/**
 * True if a saved record already exists for the given base name (no directory,
 * no extension) — i.e. saving it would overwrite an existing file.
 */
bool cfs_storage_exists(Storage* storage, const char* name);

/** Result of a successful cfs_storage_load: decoded fields plus the tag's
 * identity (UID) and exact captured payload, needed by both the read-result
 * scene and a later save/clone. */
typedef struct {
    CfsTagData data;
    uint8_t uid[10];
    size_t uid_len;
    uint8_t plain[CFS_PLAINTEXT_SIZE];
} CfsLoadedTag;

/**
 * Load a saved .nfc dump by full path (as returned by the file browser).
 * Extracts blocks 4/5/6 (fails if any is unknown "??"), decrypts them, and
 * decodes the fields, UID, and raw plaintext into *out. Block 7 is ignored.
 */
bool cfs_storage_load(Storage* storage, const char* path, CfsLoadedTag* out);

/** Delete a saved record by full path. */
bool cfs_storage_delete(Storage* storage, const char* path);

#ifdef __cplusplus
}
#endif
