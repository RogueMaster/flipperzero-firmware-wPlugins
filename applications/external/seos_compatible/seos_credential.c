#include "seos_credential_i.h"
#include "seos_credential_parse.h"
#include <seos_icons.h>

#define SEADER_PATH          "/ext/apps_data/seader"
#define SEADER_APP_EXTENSION ".credential"

#define TAG "SeosCredential"

SeosCredential* seos_credential_alloc() {
    SeosCredential* seos_credential = malloc(sizeof(SeosCredential));
    memset(seos_credential, 0, sizeof(SeosCredential));

    seos_credential->load_path = furi_string_alloc();
    seos_credential->storage = furi_record_open(RECORD_STORAGE);
    seos_credential->dialogs = furi_record_open(RECORD_DIALOGS);
    seos_credential->use_hardcoded = false;
    seos_credential->write = false;

    return seos_credential;
}

bool seos_credential_clear(SeosCredential* seos_credential) {
    memset(seos_credential->diversifier, 0, sizeof(seos_credential->diversifier));
    seos_credential->diversifier_len = 0;
    memset(seos_credential->sio, 0, sizeof(seos_credential->sio));
    seos_credential->sio_len = 0;
    memset(seos_credential->priv_key, 0, sizeof(seos_credential->priv_key));
    memset(seos_credential->auth_key, 0, sizeof(seos_credential->auth_key));
    seos_credential->adf_oid_len = 0;
    memset(seos_credential->adf_oid, 0, sizeof(seos_credential->adf_oid));
    memset(seos_credential->adf_response, 0, sizeof(seos_credential->adf_response));
    memset(seos_credential->name, 0, sizeof(seos_credential->name));
    return true;
}

void seos_credential_free(SeosCredential* seos_credential) {
    furi_assert(seos_credential);

    furi_string_free(seos_credential->load_path);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_DIALOGS);

    free(seos_credential);
}

static bool credential_write_file(SeosCredential* seos_credential, const char* path) {
    bool saved = false;
    FlipperFormat* file = flipper_format_file_alloc(seos_credential->storage);

    do {
        if(!flipper_format_file_open_always(file, path)) break;

        // Write header
        if(!flipper_format_write_header_cstr(file, seos_file_header, seos_file_version)) break;

        if(!flipper_format_write_uint32(
               file, "Diversifier Length", (uint32_t*)&(seos_credential->diversifier_len), 1))
            break;
        if(!flipper_format_write_hex(
               file, "Diversifier", seos_credential->diversifier, seos_credential->diversifier_len))
            break;
        if(!flipper_format_write_uint32(
               file, "SIO Length", (uint32_t*)&(seos_credential->sio_len), 1))
            break;
        if(!flipper_format_write_hex(file, "SIO", seos_credential->sio, seos_credential->sio_len))
            break;
        if(!flipper_format_write_hex(
               file, "Priv Key", seos_credential->priv_key, sizeof(seos_credential->priv_key)))
            break;
        if(!flipper_format_write_hex(
               file, "Auth Key", seos_credential->auth_key, sizeof(seos_credential->auth_key)))
            break;
        if(seos_credential->adf_response[0] != 0) {
            flipper_format_write_hex(
                file,
                "ADF Response",
                seos_credential->adf_response,
                sizeof(seos_credential->adf_response));
        }
        if(seos_credential->adf_oid_len > 0) {
            flipper_format_write_uint32(
                file, "ADF OID Length", (uint32_t*)&(seos_credential->adf_oid_len), 1);
            flipper_format_write_hex(
                file, "ADF OID", seos_credential->adf_oid, seos_credential->adf_oid_len);
        }

        saved = true;
    } while(false);

    if(!saved) {
        dialog_message_show_storage_error(seos_credential->dialogs, "Can not save\nfile");
    }
    flipper_format_free(file);
    return saved;
}

bool seos_credential_save(SeosCredential* seos_credential, const char* dev_name) {
    FuriString* path = furi_string_alloc();

    if(!furi_string_empty(seos_credential->load_path)) {
        path_extract_dirname(furi_string_get_cstr(seos_credential->load_path), path);
        furi_string_cat_printf(path, "/%s%s", dev_name, SEOS_APP_EXTENSION);
    } else {
        furi_string_printf(
            path, "%s/%s%s", STORAGE_APP_DATA_PATH_PREFIX, dev_name, SEOS_APP_EXTENSION);
    }

    bool saved = credential_write_file(seos_credential, furi_string_get_cstr(path));
    furi_string_free(path);
    return saved;
}

bool seos_credential_save_to_load_path(SeosCredential* seos_credential) {
    /* A credential that was never loaded from a file has no path of its own.
     * Leave it alone rather than inventing a name for it. */
    if(furi_string_empty(seos_credential->load_path)) {
        FURI_LOG_I(TAG, "Credential has no file to update");
        return false;
    }

    return credential_write_file(
        seos_credential, furi_string_get_cstr(seos_credential->load_path));
}

static bool
    seos_credential_file_load(SeosCredential* seos_credential, FuriString* path, bool show_dialog) {
    bool parsed = false;
    FlipperFormat* file = flipper_format_file_alloc(seos_credential->storage);

    if(seos_credential->loading_cb) {
        seos_credential->loading_cb(seos_credential->loading_cb_ctx, true);
    }

    if(flipper_format_file_open_existing(file, furi_string_get_cstr(path))) {
        parsed = seos_credential_parse_seos(file, seos_credential);
    }

    if(seos_credential->loading_cb) {
        seos_credential->loading_cb(seos_credential->loading_cb_ctx, false);
    }

    if((!parsed) && (show_dialog)) {
        dialog_message_show_storage_error(seos_credential->dialogs, "Can not parse file");
    }

    flipper_format_free(file);
    return parsed;
}

bool seos_credential_file_select_seos(SeosCredential* seos_credential) {
    furi_assert(seos_credential);
    bool res = false;

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(
        &browser_options, SEOS_APP_BROWSER_EXTENSIONS, &I_Nfc_10px);
    browser_options.base_path = STORAGE_APP_DATA_PATH_PREFIX;

    /* The browser starts where this path points and puts the choice back in it,
     * so it is the same string both ways. Starting from the folder it will
     * browse means it agrees with its own base path, which is what tells it
     * that back should close it rather than move up a level.
     *
     * Kept separate from load_path, which records the file this credential came
     * from: a cancelled browse must not leave a folder there for a later save
     * or delete to act on. */
    FuriString* browse_path = furi_string_alloc();
    if(furi_string_empty(seos_credential->load_path)) {
        furi_string_set_str(browse_path, STORAGE_APP_DATA_PATH_PREFIX);
    } else {
        furi_string_set(browse_path, seos_credential->load_path);
    }

    res = dialog_file_browser_show(
        seos_credential->dialogs, browse_path, browse_path, &browser_options);

    if(res) {
        /* Only a real choice updates where this credential came from. */
        furi_string_set(seos_credential->load_path, browse_path);

        FuriString* filename = furi_string_alloc();
        path_extract_filename(seos_credential->load_path, filename, true);
        strncpy(seos_credential->name, furi_string_get_cstr(filename), SEOS_FILE_NAME_MAX_LENGTH);
        res = seos_credential_file_load(seos_credential, seos_credential->load_path, true);
        furi_string_free(filename);
    }
    furi_string_free(browse_path);

    return res;
}

static bool seos_credential_file_load_seader(
    SeosCredential* seos_credential,
    FuriString* path,
    bool show_dialog) {
    bool parsed = false;
    FlipperFormat* file = flipper_format_file_alloc(seos_credential->storage);

    if(seos_credential->loading_cb) {
        seos_credential->loading_cb(seos_credential->loading_cb_ctx, true);
    }

    if(flipper_format_file_open_existing(file, furi_string_get_cstr(path))) {
        parsed = seos_credential_parse_seader(file, seos_credential);
    }

    if(seos_credential->loading_cb) {
        seos_credential->loading_cb(seos_credential->loading_cb_ctx, false);
    }

    if((!parsed) && (show_dialog)) {
        dialog_message_show_storage_error(seos_credential->dialogs, "Can not parse file");
    }

    flipper_format_free(file);
    return parsed;
}

bool seos_credential_file_select_seader(SeosCredential* seos_credential) {
    furi_assert(seos_credential);
    bool res = false;

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, SEADER_APP_EXTENSION, &I_Nfc_10px);
    browser_options.base_path = SEADER_PATH;

    /* The other tool's folder, for the same reasons as above. */
    FuriString* browse_path = furi_string_alloc();
    if(furi_string_empty(seos_credential->load_path) ||
       !furi_string_start_with_str(seos_credential->load_path, SEADER_PATH)) {
        furi_string_set_str(browse_path, SEADER_PATH);
    } else {
        furi_string_set(browse_path, seos_credential->load_path);
    }

    res = dialog_file_browser_show(
        seos_credential->dialogs, browse_path, browse_path, &browser_options);

    if(res) {
        /* Only a real choice updates where this credential came from. */
        furi_string_set(seos_credential->load_path, browse_path);

        FuriString* filename = furi_string_alloc();
        path_extract_filename(seos_credential->load_path, filename, true);
        strncpy(seos_credential->name, furi_string_get_cstr(filename), SEOS_FILE_NAME_MAX_LENGTH);
        res = seos_credential_file_load_seader(seos_credential, seos_credential->load_path, true);
        furi_string_free(filename);
    }
    furi_string_free(browse_path);

    return res;
}

bool seos_credential_file_select(SeosCredential* seos_credential) {
    if(seos_credential->load_type == SeosLoadSeos) {
        return seos_credential_file_select_seos(seos_credential);
    } else if(seos_credential->load_type == SeosLoadSeader) {
        return seos_credential_file_select_seader(seos_credential);
    }
    return false;
}

bool seos_credential_delete(SeosCredential* seos_credential, bool use_load_path) {
    furi_assert(seos_credential);
    bool deleted = false;
    FuriString* file_path;
    file_path = furi_string_alloc();

    do {
        // Delete original file
        if(use_load_path && !furi_string_empty(seos_credential->load_path)) {
            furi_string_set(file_path, seos_credential->load_path);
        } else {
            furi_string_printf(
                file_path, APP_DATA_PATH("%s%s"), seos_credential->name, SEOS_APP_EXTENSION);
        }
        if(!storage_simply_remove(seos_credential->storage, furi_string_get_cstr(file_path)))
            break;
        deleted = true;
    } while(0);

    if(!deleted) {
        dialog_message_show_storage_error(seos_credential->dialogs, "Can not remove file");
    }

    furi_string_free(file_path);
    return deleted;
}

void seos_credential_set_loading_callback(
    SeosCredential* seos_credential,
    SeosLoadingCallback callback,
    void* context) {
    furi_assert(seos_credential);

    seos_credential->loading_cb = callback;
    seos_credential->loading_cb_ctx = context;
}
