/* Credential persistence for the Flipper application.
 *
 * The engine holds no filesystem. This file owns the storage handles, the
 * load and save paths, and the dialogs a failure shows.
 */
#include "dfc_credential_storage.h"

#include <furi.h>
#include <furi/core/memmgr.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <flipper_application/flipper_application.h>
#include <flipper_application/plugins/plugin_manager.h>
#include <loader/firmware_api/firmware_api.h>
#include <lib/toolbox/path.h>

#include "dfc_credential_i.h"
#include "dfc_der.h"
#include "dfc_text_codec_plugin.h"
#include "dfc_i.h"

#define TAG "DfcCredentialStorage"

// One application, one filesystem. Keeping the handles here means the engine's
// credential carries no platform state and every call site keeps its shape.
static struct {
    FuriString* load_path;
    Storage* storage;
    DialogsApp* dialogs;
} store;

typedef struct {
    PluginManager* manager;
    const DfcTextCodecPlugin* api;
} DfcTextCodec;

static const DfcTextHostApi text_host_api = {
    .dfc_credential_app_auth_commands = dfc_credential_app_auth_commands,
    .dfc_credential_clear = dfc_credential_clear,
    .dfc_credential_key = dfc_credential_key,
    .dfc_credential_key_const = dfc_credential_key_const,
    .dfc_credential_key_in_set = dfc_credential_key_in_set,
    .dfc_credential_key_in_set_const = dfc_credential_key_in_set_const,
    .dfc_credential_key_length = dfc_credential_key_length,
    .dfc_credential_key_sets_resize = dfc_credential_key_sets_resize,
    .dfc_credential_key_version_in_set = dfc_credential_key_version_in_set,
    .dfc_credential_keys_resize = dfc_credential_keys_resize,
    .dfc_credential_picc_auth_commands = dfc_credential_picc_auth_commands,
    .dfc_credential_reset_application = dfc_credential_reset_application,
    .dfc_der_decode = dfc_der_decode,
    .dfc_der_status_name = dfc_der_status_name,
    .dfc_der_validate_model = dfc_der_validate_model,
    .dfc_file_data = dfc_file_data,
    .dfc_file_data_const = dfc_file_data_const,
    .dfc_file_resize = dfc_file_resize,
};

static bool dfc_text_codec_open(DfcTextCodec* codec, bool for_write) {
    const char* id = for_write ? DFC_TEXT_EXPORT_PLUGIN_ID : DFC_TEXT_IMPORT_PLUGIN_ID;
    const char* path = for_write ? DFC_TEXT_EXPORT_PLUGIN_PATH : DFC_TEXT_IMPORT_PLUGIN_PATH;
    codec->manager =
        plugin_manager_alloc(id, DFC_TEXT_CODEC_PLUGIN_API_VERSION, firmware_api_interface);
    if(!codec->manager) {
        FURI_LOG_E(TAG, "%s manager allocation failed", id);
        return false;
    }
    PluginManagerError plugin_error = plugin_manager_load_single(codec->manager, path);
    if(plugin_error != PluginManagerErrorNone) {
        FURI_LOG_E(
            TAG,
            "%s plugin load failed: %u free=%u largest=%u",
            id,
            (unsigned)plugin_error,
            (unsigned)memmgr_get_free_heap(),
            (unsigned)memmgr_heap_get_max_free_block());
        plugin_manager_free(codec->manager);
        codec->manager = NULL;
        return false;
    }
    if(plugin_manager_get_count(codec->manager) != 1) {
        plugin_manager_free(codec->manager);
        codec->manager = NULL;
        return false;
    }
    codec->api = plugin_manager_get_ep(codec->manager, 0);
    if(!codec->api || !codec->api->bind || !codec->api->status_name ||
       (for_write ? !codec->api->write : !codec->api->parse)) {
        plugin_manager_free(codec->manager);
        codec->manager = NULL;
        codec->api = NULL;
        return false;
    }
    codec->api->bind(&text_host_api);
    return true;
}

static void dfc_text_codec_close(DfcTextCodec* codec) {
    codec->api = NULL;
    if(codec->manager) plugin_manager_free(codec->manager);
    codec->manager = NULL;
}

void dfc_credential_storage_show_error(const char* message) {
    dialog_message_show_storage_error(store.dialogs, message);
}

FuriString* dfc_credential_storage_load_path(void) {
    return store.load_path;
}

void dfc_credential_storage_init(void) {
    store.load_path = furi_string_alloc();
    store.storage = furi_record_open(RECORD_STORAGE);
    store.dialogs = furi_record_open(RECORD_DIALOGS);
}

void dfc_credential_storage_deinit(void) {
    furi_string_free(store.load_path);
    store.load_path = NULL;
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_DIALOGS);
    store.storage = NULL;
    store.dialogs = NULL;
}

// Text is an authoring format only: every load compiles it to the binary
// encoding and reads the model back from those octets, so a credential the text
// grammar accepts but the binary codec would refuse never reaches the emulator.
// A .dfcb file is already those octets and skips the text stage.
#define DFC_BINARY_FIRST_OCTET          0x60u
#define DFC_FLIPPER_CREDENTIAL_MAX_SIZE (16u * 1024u)
#define DFC_BINARY_LOAD_HEAP_RESERVE    0u
#define DFC_TEXT_LOAD_HEAP_RESERVE      (2u * 1024u)

static bool dfc_credential_has_heap_for(size_t allocation, size_t reserve) {
    size_t free_heap = memmgr_get_free_heap();
    return allocation <= memmgr_heap_get_max_free_block() && allocation <= free_heap &&
           reserve <= free_heap - allocation;
}

static bool dfc_credential_ats_is_radio_supported(const DfcCredential* credential) {
    if(credential->picc_ats_len == 0) return true;
    const uint8_t* ats = credential->picc_ats;
    size_t length = credential->picc_ats_len;
    if(length < 2 || ats[0] != length) return false;
    uint8_t t0 = ats[1];
    if((t0 & 0x0F) != 5) return false; // 64-byte frame size used by this listener.
    size_t offset = 2;
    if(t0 & 0x10) {
        if(offset >= length || ats[offset++] != 0) return false; // 106 kbit/s only.
    }
    if(t0 & 0x20) {
        if(offset >= length || (ats[offset++] >> 4) < 8) return false;
    } else {
        return false; // The default frame wait time is too short for this app.
    }
    if(t0 & 0x40) {
        if(offset >= length || (ats[offset++] & 0x01)) return false; // No NAD handling.
    }
    return true;
}

static DfcCredentialLoadStatus dfc_credential_status_for(DfcTextStatus status) {
    switch(status) {
    case DfcTextOk:
        return DfcCredentialLoadStatusOk;
    case DfcTextUnsupported:
        return DfcCredentialLoadStatusUnsupportedFormat;
    case DfcTextCapacity:
        return DfcCredentialLoadStatusCapacity;
    case DfcTextMalformed:
    default:
        return DfcCredentialLoadStatusMalformedFile;
    }
}

static bool dfc_credential_write_file(DfcCredential* credential, const char* path) {
    bool saved = false;
    char* text = NULL;
    DfcTextCodec codec = {0};
    File* file = storage_file_alloc(store.storage);

    do {
        if(!dfc_text_codec_open(&codec, true)) break;
        size_t needed = 0;
        DfcTextStatus st = codec.api->write(credential, NULL, 0, &needed);
        if(st != DfcTextCapacity && st != DfcTextOk) {
            FURI_LOG_E(TAG, "cannot render credential: %s", codec.api->status_name(st));
            break;
        }
        if(needed > DFC_FLIPPER_CREDENTIAL_MAX_SIZE) break;
        text = malloc(needed + 1);
        if(!text) break;
        size_t len = 0;
        st = codec.api->write(credential, text, needed + 1, &len);
        if(st != DfcTextOk) {
            FURI_LOG_E(TAG, "cannot render credential: %s", codec.api->status_name(st));
            break;
        }
        dfc_text_codec_close(&codec);

        if(!storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) break;
        if(storage_file_write(file, text, len) != len) break;
        saved = true;
    } while(false);

    if(!saved) {
        dialog_message_show_storage_error(store.dialogs, "Can not save\nfile");
    }
    if(text) free(text);
    dfc_text_codec_close(&codec);
    storage_file_close(file);
    storage_file_free(file);
    return saved;
}

bool dfc_credential_save(DfcCredential* credential, const char* dev_name) {
    FuriString* path = furi_string_alloc();
    if(!furi_string_empty(store.load_path)) {
        path_extract_dirname(furi_string_get_cstr(store.load_path), path);
        furi_string_cat_printf(path, "/%s%s", dev_name, DFC_APP_EXTENSION);
    } else {
        furi_string_printf(
            path, "%s/%s%s", STORAGE_APP_DATA_PATH_PREFIX, dev_name, DFC_APP_EXTENSION);
    }

    bool saved = dfc_credential_write_file(credential, furi_string_get_cstr(path));
    if(saved) {
        furi_string_set(store.load_path, path);
        dfc_credential_clear_dirty(credential);
    }
    furi_string_free(path);
    return saved;
}

bool dfc_credential_save_loaded(DfcCredential* credential) {
    if(furi_string_empty(store.load_path)) return false;

    // Saving always writes text, so a credential loaded from compiled octets is
    // saved beside them rather than overwriting them with a different encoding.
    FuriString* path = furi_string_alloc_set(store.load_path);
    if(furi_string_end_with(path, DFC_BINARY_EXTENSION)) {
        furi_string_left(path, furi_string_size(path) - strlen(DFC_BINARY_EXTENSION));
        furi_string_cat_str(path, DFC_APP_EXTENSION);
    }

    bool saved = dfc_credential_write_file(credential, furi_string_get_cstr(path));
    if(saved) {
        furi_string_set(store.load_path, path);
        dfc_credential_clear_dirty(credential);
    }
    furi_string_free(path);
    return saved;
}

// Read the whole file into a fresh buffer. The caller frees it.
static uint8_t* dfc_credential_read_whole_file(
    const char* path,
    size_t* out_len,
    DfcCredentialLoadStatus* status) {
    uint8_t* buffer = NULL;
    File* file = storage_file_alloc(store.storage);

    do {
        if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            if(status) *status = DfcCredentialLoadStatusOpenFailed;
            break;
        }
        uint64_t size = storage_file_size(file);
        if(size == 0) {
            if(status) *status = DfcCredentialLoadStatusMalformedFile;
            break;
        }
        if(size > DFC_FLIPPER_CREDENTIAL_MAX_SIZE) {
            if(status) *status = DfcCredentialLoadStatusCapacity;
            break;
        }
        size_t path_len = strlen(path);
        bool is_text = path_len >= strlen(DFC_APP_EXTENSION) &&
                       strcmp(path + path_len - strlen(DFC_APP_EXTENSION), DFC_APP_EXTENSION) == 0;
        if(!dfc_credential_has_heap_for(
               (size_t)size,
               is_text ? DFC_TEXT_LOAD_HEAP_RESERVE : DFC_BINARY_LOAD_HEAP_RESERVE)) {
            if(status) *status = DfcCredentialLoadStatusCapacity;
            break;
        }
        buffer = malloc((size_t)size);
        if(!buffer) {
            if(status) *status = DfcCredentialLoadStatusCapacity;
            break;
        }
        if(storage_file_read(file, buffer, (size_t)size) != size) {
            free(buffer);
            buffer = NULL;
            if(status) *status = DfcCredentialLoadStatusOpenFailed;
            break;
        }
        *out_len = (size_t)size;
    } while(false);

    storage_file_close(file);
    storage_file_free(file);
    return buffer;
}

static bool dfc_credential_file_load(
    DfcCredential* credential,
    FuriString* path,
    const char* selected_name,
    DfcCredentialLoadStatus* status) {
    bool parsed = false;
    size_t raw_len = 0;
    uint8_t* octets = NULL;
    DfcTextCodec codec = {0};

    if(status) *status = DfcCredentialLoadStatusMalformedFile;

    dfc_log_memory("before credential read");
    uint8_t* raw = dfc_credential_read_whole_file(furi_string_get_cstr(path), &raw_len, status);
    if(!raw) return false;
    dfc_log_memory("after credential read");

    do {
        size_t octets_len = 0;
        if(raw[0] == DFC_BINARY_FIRST_OCTET) {
            // Already the emulator's native input.
            octets = raw;
            octets_len = raw_len;
        } else {
            if(!dfc_text_codec_open(&codec, false)) {
                if(status) *status = DfcCredentialLoadStatusCodecUnavailable;
                break;
            }
            DfcTextError detail = {0};
            DfcTextStatus text_status =
                codec.api->parse(credential, (const char*)raw, raw_len, &detail);
            if(text_status != DfcTextOk) {
                FURI_LOG_E(
                    TAG,
                    "line %u: %s (%s)",
                    (unsigned)detail.line,
                    detail.message,
                    codec.api->status_name(text_status));
                if(status) *status = dfc_credential_status_for(text_status);
                break;
            }
            dfc_text_codec_close(&codec);
            size_t needed = 0;
            DfcTextStatus size_status = dfc_der_encoded_size(credential, &needed);
            if(size_status != DfcDerOk) {
                if(status) *status = dfc_credential_status_for(size_status);
                break;
            }
            // Parsing is finished. Keep the model, then release the source
            // before allocating its binary round-trip copy.
            free(raw);
            raw = NULL;
            dfc_log_memory("before encoded copy");
            if(!dfc_credential_has_heap_for(needed, DFC_BINARY_LOAD_HEAP_RESERVE)) {
                if(status) *status = DfcCredentialLoadStatusCapacity;
                break;
            }
            octets = malloc(needed);
            if(!octets) {
                if(status) *status = DfcCredentialLoadStatusCapacity;
                break;
            }
            DfcDerStatus encoded = dfc_der_encode(credential, octets, needed, &octets_len);
            if(encoded != DfcDerOk) {
                if(status) *status = dfc_credential_status_for(encoded);
                break;
            }
        }

        DfcDerStatus decoded = dfc_der_decode(credential, octets, octets_len);
        if(decoded != DfcDerOk) {
            FURI_LOG_E(TAG, "binary rejected: %s", dfc_der_status_name(decoded));
            if(status) *status = dfc_credential_status_for(decoded);
            break;
        }
        if(!dfc_desfire_uid_is_detectable(credential->uid, credential->uid_len)) break;
        if(!dfc_credential_picc_ats_is_consistent(credential)) {
            // Well formed, but naming an answer to RATS this engine will not
            // transmit, which is the unsupported class rather than a broken file.
            FURI_LOG_E(TAG, "user ATS length octet does not describe the ATS");
            if(status) *status = DfcCredentialLoadStatusUnsupportedFormat;
            break;
        }
        if(!dfc_credential_ats_is_radio_supported(credential)) {
            if(status) *status = DfcCredentialLoadStatusUnsupportedFormat;
            break;
        }

        parsed = true;
        dfc_log_memory("credential load peak");
        if(status) *status = DfcCredentialLoadStatusOk;
    } while(false);

    if(parsed) {
        snprintf(credential->name, sizeof(credential->name), "%s", selected_name);
    }

    if(octets && octets != raw) free(octets);
    free(raw);
    dfc_text_codec_close(&codec);
    if(!parsed) dfc_credential_clear(credential);

    dfc_log_memory("after credential load");

    return parsed;
}

bool dfc_credential_load_selected_file(DfcCredential* credential, DfcCredentialLoadStatus* status) {
    furi_assert(credential);
    if(furi_string_empty(store.load_path)) {
        if(status) *status = DfcCredentialLoadStatusOpenFailed;
        return false;
    }

    char selected_name[DFC_FILE_NAME_MAX_LENGTH + 1];
    snprintf(selected_name, sizeof(selected_name), "%s", credential->name);
    if(selected_name[0] == '\0') {
        FuriString* filename = furi_string_alloc();
        path_extract_filename(store.load_path, filename, true);
        snprintf(selected_name, sizeof(selected_name), "%s", furi_string_get_cstr(filename));
        furi_string_free(filename);
    }

    bool loaded = dfc_credential_file_load(credential, store.load_path, selected_name, status);
    if(!loaded) {
        dfc_credential_clear(credential);
    }
    return loaded;
}

bool dfc_credential_delete(DfcCredential* credential, bool use_load_path) {
    furi_assert(credential);
    bool deleted = false;
    FuriString* file_path = furi_string_alloc();

    do {
        if(use_load_path && !furi_string_empty(store.load_path)) {
            furi_string_set(file_path, store.load_path);
        } else {
            furi_string_printf(
                file_path, APP_DATA_PATH("%s%s"), credential->name, DFC_APP_EXTENSION);
        }
        if(!storage_simply_remove(store.storage, furi_string_get_cstr(file_path))) break;
        deleted = true;
    } while(0);

    if(!deleted) {
        dialog_message_show_storage_error(store.dialogs, "Can not remove file");
    }

    furi_string_free(file_path);
    return deleted;
}
