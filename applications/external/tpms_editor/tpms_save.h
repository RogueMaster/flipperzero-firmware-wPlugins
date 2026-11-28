#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <furi.h>

#include "tpms_encoder.h"

typedef struct TPMSApp TPMSApp;

#define TPMS_PROFILE_FOLDER EXT_PATH("apps_data/tpms_editor/profiles")
#define TPMS_PROFILE_RX_FOLDER EXT_PATH("apps_data/tpms_editor/profiles/rx")
#define TPMS_PROFILE_EDITED_FOLDER EXT_PATH("apps_data/tpms_editor/profiles/edited")
#define TPMS_AUTOSAVE_FOLDER EXT_PATH("apps_data/tpms_editor/autosave")
#define TPMS_AUTOSAVE_CONFIG EXT_PATH("apps_data/tpms_editor/autosave.cfg")
#define TPMS_AUTOSAVE_DEFAULT_ENABLED true

/** Save the exact signed RAW timings retained for a received history item. */
bool tpms_save_original_raw(
    TPMSApp* app,
    uint16_t history_index,
    const char* protocol,
    uint32_t id,
    FuriString* saved_path,
    bool* truncated);

/** Same as tpms_save_original_raw(), but prefixes the file with a user label. */
bool tpms_save_original_raw_named(
    TPMSApp* app,
    uint16_t history_index,
    const char* protocol,
    uint32_t id,
    const char* label,
    FuriString* saved_path,
    bool* truncated);

/** Build the edited frame and save it as a replayable Flipper RAW .sub file. */
bool tpms_save_edited_raw(
    TPMSApp* app,
    const TpmsEditValues* values,
    const char* protocol,
    FuriString* saved_path);

/** Save/load a compact editable TPMS profile used by the in-app Saved list. */
bool tpms_profile_save(
    const TpmsEditValues* values,
    const char* protocol,
    bool edited,
    FuriString* saved_path);

/** Save a profile with an optional user-facing label (e.g. OPEL PP). */
bool tpms_profile_save_named(
    const TpmsEditValues* values,
    const char* protocol,
    bool edited,
    const char* label,
    FuriString* saved_path);

/** Read the optional user-facing label from a saved profile. */
bool tpms_profile_read_label(
    const char* path,
    char* label,
    size_t label_size,
    uint32_t* id);

bool tpms_profile_load(
    const char* path,
    TpmsEditValues* values,
    char* protocol,
    size_t protocol_size);

/** Save compact editable profile to an exact path (used by volatile LAB slots). */
bool tpms_profile_save_to_path(
    const char* path, const TpmsEditValues* values, const char* protocol);

/** Same exact-path save, with an optional persistent user-facing label. */
bool tpms_profile_save_to_path_named(
    const char* path,
    const TpmsEditValues* values,
    const char* protocol,
    const char* label);

/* TPMS 4.5 FIX2 RX autosave. Without a saved config it defaults to ON.
   Duplicate suppression is session-only and keyed by protocol+ID+payload. */
bool tpms_autosave_load_enabled(void);
bool tpms_autosave_set_enabled(TPMSApp* app, bool enabled);
void tpms_autosave_reset_session(TPMSApp* app);
bool tpms_autosave_process_decoder(
    TPMSApp* app,
    void* decoder_base,
    float frame_rssi,
    uint8_t wake_profile,
    bool wake_profile_valid);
