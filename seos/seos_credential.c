#include "seos_credential_i.h"

#define SEADER_PATH          "/ext/apps_data/seader"
#define SEADER_APP_EXTENSION ".credential"

#define TAG "SeosCredential"

static uint8_t empty[16] =
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

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

bool seos_credential_save(SeosCredential* seos_credential, const char* dev_name) {
    bool saved = false;
    FlipperFormat* file = flipper_format_file_alloc(seos_credential->storage);
    FuriString* temp_str = furi_string_alloc();
    bool use_load_path = true;

    do {
        if(use_load_path && !furi_string_empty(seos_credential->load_path)) {
            // Get directory name
            path_extract_dirname(furi_string_get_cstr(seos_credential->load_path), temp_str);
            // Make path to file to save
            furi_string_cat_printf(temp_str, "/%s%s", dev_name, SEOS_APP_EXTENSION);
        } else {
            // First remove file if it was saved
            furi_string_printf(
                temp_str, "%s/%s%s", STORAGE_APP_DATA_PATH_PREFIX, dev_name, SEOS_APP_EXTENSION);
        }

        // Open file
        if(!flipper_format_file_open_always(file, furi_string_get_cstr(temp_str))) break;

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
    furi_string_free(temp_str);
    flipper_format_free(file);
    return saved;
}

static bool
    seos_credential_file_load(SeosCredential* seos_credential, FuriString* path, bool show_dialog) {
    bool parsed = false;
    FlipperFormat* file = flipper_format_file_alloc(seos_credential->storage);
    FuriString* temp_str;
    temp_str = furi_string_alloc();
    bool deprecated_version = false;

    if(seos_credential->loading_cb) {
        seos_credential->loading_cb(seos_credential->loading_cb_ctx, true);
    }

    memset(seos_credential->diversifier, 0, sizeof(seos_credential->diversifier));
    memset(seos_credential->sio, 0, sizeof(seos_credential->sio));
    do {
        if(!flipper_format_file_open_existing(file, furi_string_get_cstr(path))) break;

        // Read and verify file header
        uint32_t version = 0;
        if(!flipper_format_read_header(file, temp_str, &version)) break;
        if(furi_string_cmp_str(temp_str, seos_file_header) || (version != seos_file_version)) {
            deprecated_version = true;
            break;
        }

        if(!flipper_format_read_uint32(
               file, "Diversifier Length", (uint32_t*)&(seos_credential->diversifier_len), 1))
            break;
        if(!flipper_format_read_hex(
               file, "Diversifier", seos_credential->diversifier, seos_credential->diversifier_len))
            break;

        if(!flipper_format_read_uint32(
               file, "SIO Length", (uint32_t*)&(seos_credential->sio_len), 1))
            break;
        if(!flipper_format_read_hex(file, "SIO", seos_credential->sio, seos_credential->sio_len))
            break;

        // optional
        memset(seos_credential->priv_key, 0, sizeof(seos_credential->priv_key));
        memset(seos_credential->auth_key, 0, sizeof(seos_credential->auth_key));
        memset(seos_credential->adf_response, 0, sizeof(seos_credential->adf_response));
        flipper_format_read_hex(
            file, "Priv Key", seos_credential->priv_key, sizeof(seos_credential->priv_key));
        flipper_format_read_hex(
            file, "Auth Key", seos_credential->auth_key, sizeof(seos_credential->auth_key));
        if(memcmp(seos_credential->priv_key, empty, sizeof(empty)) != 0) {
            FURI_LOG_I(TAG, "+ Priv Key");
        }
        if(memcmp(seos_credential->priv_key, empty, sizeof(empty)) != 0) {
            FURI_LOG_I(TAG, "+ Auth Key");
        }
        flipper_format_read_hex(
            file,
            "ADF Response",
            seos_credential->adf_response,
            sizeof(seos_credential->adf_response));

        flipper_format_read_uint32(
            file, "ADF OID Length", (uint32_t*)&(seos_credential->adf_oid_len), 1);
        flipper_format_read_hex(
            file, "ADF OID", seos_credential->adf_oid, seos_credential->adf_oid_len);

        parsed = true;
    } while(false);

    if(seos_credential->loading_cb) {
        seos_credential->loading_cb(seos_credential->loading_cb_ctx, false);
    }

    if((!parsed) && (show_dialog)) {
        if(deprecated_version) {
            dialog_message_show_storage_error(seos_credential->dialogs, "File format deprecated");
        } else {
            dialog_message_show_storage_error(seos_credential->dialogs, "Can not parse\nfile");
        }
    }

    furi_string_free(temp_str);
    flipper_format_free(file);

    return parsed;
}

bool seos_credential_file_select_seos(SeosCredential* seos_credential) {
    furi_assert(seos_credential);
    bool res = false;

    FuriString* seos_app_folder = furi_string_alloc_set(STORAGE_APP_DATA_PATH_PREFIX);

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, SEOS_APP_EXTENSION, &I_Nfc_10px);
    browser_options.base_path = STORAGE_APP_DATA_PATH_PREFIX;

    res = dialog_file_browser_show(
        seos_credential->dialogs, seos_credential->load_path, seos_app_folder, &browser_options);

    furi_string_free(seos_app_folder);
    if(res) {
        FuriString* filename;
        filename = furi_string_alloc();
        path_extract_filename(seos_credential->load_path, filename, true);
        strncpy(seos_credential->name, furi_string_get_cstr(filename), SEOS_FILE_NAME_MAX_LENGTH);
        res = seos_credential_file_load(seos_credential, seos_credential->load_path, true);
        furi_string_free(filename);
    }

    return res;
}

static bool seos_credential_file_load_seader(
    SeosCredential* seos_credential,
    FuriString* path,
    bool show_dialog) {
    bool parsed = false;
    FlipperFormat* file = flipper_format_file_alloc(seos_credential->storage);
    FuriString* reason = furi_string_alloc_set("Couldn't load file");
    FuriString* temp_str;
    temp_str = furi_string_alloc();
    const char* seader_file_header = "Flipper Seader Credential";
    const uint32_t seader_file_version = 1;

    if(seos_credential->loading_cb) {
        seos_credential->loading_cb(seos_credential->loading_cb_ctx, true);
    }

    memset(seos_credential->diversifier, 0, sizeof(seos_credential->diversifier));
    memset(seos_credential->sio, 0, sizeof(seos_credential->sio));
    do {
        if(!flipper_format_file_open_existing(file, furi_string_get_cstr(path))) break;

        // Read and verify file header
        uint32_t version = 0;
        if(!flipper_format_read_header(file, temp_str, &version)) break;
        if(furi_string_cmp_str(temp_str, seader_file_header) || (version != seader_file_version)) {
            furi_string_printf(reason, "Deprecated file format");
            break;
        }
        // Don't forget, order of keys is important

        if(!flipper_format_key_exist(file, "SIO")) {
            furi_string_printf(reason, "Missing SIO");
            break;
        }
        seos_credential->sio_len = 64; // Seader SIO size
        // We can't check the return status because it will be false if less than the requested length of bytes was read.
        flipper_format_read_hex(file, "SIO", seos_credential->sio, seos_credential->sio_len);
        seos_credential->sio_len =
            seos_credential->sio[1] + 4; // 2 for type and length, 2 for null after SIO data

        // -------------
        if(!flipper_format_key_exist(file, "Diversifier")) {
            furi_string_printf(reason, "Missing Diversifier");
            break;
        }
        seos_credential->diversifier_len = 8; //Seader diversifier size
        flipper_format_read_hex(
            file, "Diversifier", seos_credential->diversifier, seos_credential->diversifier_len);
        uint8_t* end = memchr(seos_credential->diversifier, 0, 8);
        if(end) {
            seos_credential->diversifier_len = end - seos_credential->diversifier;
        } // Returns NULL if char cannot be found

        SeosCredential* cred = seos_credential;
        char display[128 * 2 + 1];

        memset(display, 0, sizeof(display));
        for(uint8_t i = 0; i < cred->sio_len; i++) {
            snprintf(display + (i * 2), sizeof(display), "%02x", cred->sio[i]);
        }
        FURI_LOG_D(TAG, "SIO: %s", display);

        memset(display, 0, sizeof(display));
        for(uint8_t i = 0; i < cred->diversifier_len; i++) {
            snprintf(display + (i * 2), sizeof(display), "%02x", cred->diversifier[i]);
        }
        FURI_LOG_D(TAG, "Diversifier: %s", display);

        parsed = true;
    } while(false);

    if(seos_credential->loading_cb) {
        seos_credential->loading_cb(seos_credential->loading_cb_ctx, false);
    }

    if((!parsed) && (show_dialog)) {
        dialog_message_show_storage_error(seos_credential->dialogs, furi_string_get_cstr(reason));
    }

    furi_string_free(reason);
    furi_string_free(temp_str);
    flipper_format_free(file);

    return parsed;
}

bool seos_credential_file_select_seader(SeosCredential* seos_credential) {
    furi_assert(seos_credential);
    bool res = false;

    FuriString* app_folder = furi_string_alloc_set(SEADER_PATH);

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, SEADER_APP_EXTENSION, &I_Nfc_10px);
    browser_options.base_path = SEADER_PATH;

    res = dialog_file_browser_show(
        seos_credential->dialogs, seos_credential->load_path, app_folder, &browser_options);

    furi_string_free(app_folder);
    if(res) {
        FuriString* filename;
        filename = furi_string_alloc();
        path_extract_filename(seos_credential->load_path, filename, true);
        strncpy(seos_credential->name, furi_string_get_cstr(filename), SEOS_FILE_NAME_MAX_LENGTH);
        res = seos_credential_file_load_seader(seos_credential, seos_credential->load_path, true);
        furi_string_free(filename);
    }

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
