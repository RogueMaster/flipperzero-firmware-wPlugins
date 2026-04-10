#pragma once

#include <stdbool.h>
#include <stddef.h>

#define MFP_APP_FOLDER  "/ext/apps_data/mfp_reader"
#define MFP_FILE_EXT    ".mfp"
#define MFP_MAX_SAVED   32
#define MFP_NAME_LEN    64

/* Forward declaration — full definition is in mfp_app.h */
typedef struct MfpApp MfpApp;

/**
 * Save current card data (app->version, app->blocks[]) to
 * MFP_APP_FOLDER/<uid>.mfp.  Stores the resulting path in
 * app->save_path on success.
 */
bool mfp_storage_save(MfpApp* app);

/**
 * Save multi-sector scan results (Version 2 format) to the standard
 * UID-based path (MFP_APP_FOLDER/MFP<UID>.mfp).
 */
bool mfp_storage_save_all(MfpApp* app);

/**
 * Save multi-sector scan results (Version 2) to an arbitrary path.
 * Creates parent directories if needed and updates app->save_path.
 */
bool mfp_storage_save_all_to_path(MfpApp* app, const char* path);

/**
 * Save emulation-modified data back to the current dump.
 * If app was loaded from a file, writes to that exact file (app->save_path).
 * If app is from a fresh scan, writes to the standard UID-based path.
 * On success, out_path (if non-NULL) is filled with the resulting path.
 * out_path must be at least 128 bytes if provided.
 */
bool mfp_storage_save_modifications(MfpApp* app, char* out_path, size_t out_path_size);

/**
 * Load card data from path into app->version / app->blocks[].
 * Sets app->loaded_from_file = true on success.
 * Handles both Version 1 (single sector) and Version 2 (multi-sector).
 */
bool mfp_storage_load(MfpApp* app, const char* path);
