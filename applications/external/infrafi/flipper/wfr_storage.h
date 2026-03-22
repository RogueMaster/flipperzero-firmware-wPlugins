#pragma once

#include "protocol/wfr_protocol.h"
#include <storage/storage.h>

#define WFR_SAVE_DIR       "/ext/apps_data/infrafi"
#define WFR_SETTINGS_FILE  "/ext/apps_data/infrafi/settings.cfg"
#define WFR_SAVE_EXT       ".wfir"
#define WFR_SAVED_MAX      20
#define WFR_FILENAME_MAX   64

/* Save WiFi credentials to SD card. Returns true on success. */
bool wfr_storage_save(Storage* storage, const WfrWifiCreds* creds);

/* Load WiFi credentials from a .wfir file. Returns true on success. */
bool wfr_storage_load(Storage* storage, const char* path, WfrWifiCreds* creds);

/* List saved credential files.
 * Populates ssids[] with display names and filenames[] with just the filename.
 * Returns the number of entries found (up to max_count). */
uint8_t wfr_storage_list(
    Storage* storage,
    char ssids[][WFR_SSID_MAX_LEN + 1],
    char filenames[][WFR_FILENAME_MAX],
    uint8_t max_count);

/* Delete a saved credential file by filename. */
bool wfr_storage_delete(Storage* storage, const char* filename);

/* Load app settings from SD card. Returns false if file doesn't exist. */
bool wfr_storage_load_settings(Storage* storage, bool* ack_enabled);

/* Save app settings to SD card. */
bool wfr_storage_save_settings(Storage* storage, bool ack_enabled);
