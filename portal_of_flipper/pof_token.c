#include <toolbox/path.h>
#include <flipper_format/flipper_format.h>

#include <portal_of_flipper_icons.h>
#include "pof_token.h"

#define TAG "PoFToken"

static uint8_t pof_token_sector_0_key[] = {0x4b, 0x0b, 0x20, 0x10, 0x7c, 0xcb};

PoFToken* pof_token_alloc() {
    PoFToken* pof_token = malloc(sizeof(PoFToken));
    memset(pof_token, 0, sizeof(PoFToken));
    pof_token->storage = furi_record_open(RECORD_STORAGE);
    pof_token->dialogs = furi_record_open(RECORD_DIALOGS);
    pof_token->load_path = furi_string_alloc();
    pof_token->loaded = false;
    pof_token->change = false;
    pof_token->nfc_device = nfc_device_alloc();
    memset(pof_token->UID, 0, sizeof(pof_token->UID));
    return pof_token;
}

void pof_token_set_name(PoFToken* pof_token, const char* name) {
    furi_assert(pof_token);

    strlcpy(pof_token->dev_name, name, sizeof(pof_token->dev_name));
}

static bool pof_token_load_data(PoFToken* pof_token, FuriString* path, bool show_dialog) {
    FuriString* reason = furi_string_alloc_set("Couldn't load file");

    if(pof_token->loading_cb) {
        pof_token->loading_cb(pof_token->loading_cb_ctx, true);
    }

    do {
        NfcDevice* nfc_device = pof_token->nfc_device;
        if(!nfc_device_load(nfc_device, furi_string_get_cstr(path))) break;

        NfcProtocol protocol = nfc_device_get_protocol(nfc_device);
        if(protocol != NfcProtocolMfClassic) {
            furi_string_printf(reason, "Not Mifare Classic");
            break;
        }

        const MfClassicData* data = nfc_device_get_data(nfc_device, NfcProtocolMfClassic);
        if(!mf_classic_is_card_read(data)) {
            furi_string_printf(reason, "Incomplete data");
            break;
        }

        MfClassicKey key = mf_classic_get_key(data, 0, MfClassicKeyTypeA);
        if(memcmp(key.data, pof_token_sector_0_key, MF_CLASSIC_KEY_SIZE) != 0) {
            furi_string_printf(reason, "Wrong key");
            break;
        }

        size_t uid_len = 0;
        const uint8_t* uid = nfc_device_get_uid(nfc_device, &uid_len);
        memcpy(pof_token->UID, uid, sizeof(pof_token->UID));
        pof_token->loaded = true;
        pof_token->change = true;
    } while(false);

    if(pof_token->loading_cb) {
        pof_token->loading_cb(pof_token->loading_cb_ctx, false);
    }

    if((!pof_token->loaded) && (show_dialog)) {
        dialog_message_show_storage_error(pof_token->dialogs, furi_string_get_cstr(reason));
    }

    furi_string_free(reason);

    return pof_token->loaded;
}

void pof_token_clear(PoFToken* pof_token, bool save) {
    furi_assert(pof_token);
    if(save) {
        // Saving during app clean up causes a crash
        nfc_device_save(pof_token->nfc_device, furi_string_get_cstr(pof_token->load_path));
    }
    nfc_device_clear(pof_token->nfc_device);
    furi_string_reset(pof_token->load_path);
    memset(pof_token->dev_name, 0, sizeof(pof_token->dev_name));
    pof_token->loaded = false;
    pof_token->change = true;
}

void pof_token_free(PoFToken* pof_token) {
    furi_assert(pof_token);
    pof_token_clear(pof_token, false);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_DIALOGS);
    furi_string_free(pof_token->load_path);
    nfc_device_free(pof_token->nfc_device);
    free(pof_token);
}

bool pof_file_select(PoFToken* pof_token) {
    furi_assert(pof_token);

    FuriString* pof_app_folder;

    // If "Skylanders" folder exists
    if(storage_dir_exists(pof_token->storage, "/ext/nfc/Skylanders")) {
        pof_app_folder = furi_string_alloc_set("/ext/nfc/Skylanders");
    } else {
        pof_app_folder = furi_string_alloc_set("/ext/nfc");
    }

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, ".nfc", &I_Nfc_10px);
    browser_options.base_path = furi_string_get_cstr(pof_app_folder);

    bool res = dialog_file_browser_show(
        pof_token->dialogs, pof_token->load_path, pof_app_folder, &browser_options);

    furi_string_free(pof_app_folder);
    if(res) {
        FuriString* filename;
        filename = furi_string_alloc();
        path_extract_filename(pof_token->load_path, filename, true);
        strlcpy(pof_token->dev_name, furi_string_get_cstr(filename), sizeof(pof_token->dev_name));
        res = pof_token_load_data(pof_token, pof_token->load_path, true);
        if(res) {
            pof_token_set_name(pof_token, pof_token->dev_name);
        }
        furi_string_free(filename);
    }

    return res;
}

void pof_token_set_loading_callback(
    PoFToken* pof_token,
    PoFLoadingCallback callback,
    void* context) {
    furi_assert(pof_token);

    pof_token->loading_cb = callback;
    pof_token->loading_cb_ctx = context;
}
