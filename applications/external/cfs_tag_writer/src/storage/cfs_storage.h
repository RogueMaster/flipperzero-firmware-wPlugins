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
#define CFS_STORAGE_EXTENSION ".nfc"
#define CFS_STORAGE_NAME_MAX  (64)

/** Ensure the app's tag storage directory exists. */
void cfs_storage_ensure_dir(Storage* storage);

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
