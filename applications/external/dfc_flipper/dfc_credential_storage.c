/* Credential persistence for the Flipper application.
 *
 * The engine holds no filesystem. This file owns the storage handles, the
 * load and save paths, and the dialogs a failure shows.
 */
#include "dfc_credential_storage.h"

#include <furi.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <lib/toolbox/path.h>

#include "dfc_credential_i.h"
#include "dfc_der.h"
#include "dfc_text.h"

#define TAG "DfcCredentialStorage"

// One application, one filesystem. Keeping the handles here means the engine's
// credential carries no platform state and every call site keeps its shape.
static struct {
    FuriString* load_path;
    Storage* storage;
    DialogsApp* dialogs;
} store;

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
#define DFC_BINARY_FIRST_OCTET 0x60u

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
    File* file = storage_file_alloc(store.storage);

    do {
        size_t needed = 0;
        DfcTextStatus st = dfc_text_write(credential, NULL, 0, &needed);
        if(st != DfcTextCapacity && st != DfcTextOk) {
            FURI_LOG_E(TAG, "cannot render credential: %s", dfc_text_status_name(st));
            break;
        }
        text = malloc(needed + 1);
        size_t len = 0;
        st = dfc_text_write(credential, text, needed + 1, &len);
        if(st != DfcTextOk) {
            FURI_LOG_E(TAG, "cannot render credential: %s", dfc_text_status_name(st));
            break;
        }

        if(!storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) break;
        if(storage_file_write(file, text, len) != len) break;
        saved = true;
    } while(false);

    if(!saved) {
        dialog_message_show_storage_error(store.dialogs, "Can not save\nfile");
    }
    if(text) free(text);
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
        if(size > DFC_TEXT_MAX_SIZE) {
            if(status) *status = DfcCredentialLoadStatusCapacity;
            break;
        }
        buffer = malloc((size_t)size);
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
    DfcCredential* staged = NULL;

    if(status) *status = DfcCredentialLoadStatusMalformedFile;

    uint8_t* raw = dfc_credential_read_whole_file(furi_string_get_cstr(path), &raw_len, status);
    if(!raw) return false;

    do {
        staged = malloc(sizeof(DfcCredential));
        memset(staged, 0, sizeof(DfcCredential));

        size_t octets_len = 0;
        if(raw[0] == DFC_BINARY_FIRST_OCTET) {
            // Already the emulator's native input.
            octets = raw;
            octets_len = raw_len;
        } else {
            DfcTextError detail = {0};
            DfcTextStatus text_status = dfc_text_parse(staged, (const char*)raw, raw_len, &detail);
            if(text_status != DfcTextOk) {
                FURI_LOG_E(
                    TAG,
                    "line %u: %s (%s)",
                    (unsigned)detail.line,
                    detail.message,
                    dfc_text_status_name(text_status));
                if(status) *status = dfc_credential_status_for(text_status);
                break;
            }
            size_t needed = 0;
            DfcTextStatus size_status = dfc_der_encoded_size(staged, &needed);
            if(size_status != DfcDerOk) {
                if(status) *status = dfc_credential_status_for(size_status);
                break;
            }
            octets = malloc(needed);
            DfcDerStatus encoded = dfc_der_encode(staged, octets, needed, &octets_len);
            if(encoded != DfcDerOk) {
                if(status) *status = dfc_credential_status_for(encoded);
                break;
            }
        }

        DfcDerStatus decoded = dfc_der_decode(staged, octets, octets_len);
        if(decoded != DfcDerOk) {
            FURI_LOG_E(TAG, "binary rejected: %s", dfc_der_status_name(decoded));
            if(status) *status = dfc_credential_status_for(decoded);
            break;
        }
        if(!dfc_desfire_uid_is_detectable(staged->uid, staged->uid_len)) break;
        if(!dfc_credential_picc_ats_is_consistent(staged)) {
            // Well formed, but naming an answer to RATS this engine will not
            // transmit, which is the unsupported class rather than a broken file.
            FURI_LOG_E(TAG, "user ATS length octet does not describe the ATS");
            if(status) *status = DfcCredentialLoadStatusUnsupportedFormat;
            break;
        }

        parsed = true;
        if(status) *status = DfcCredentialLoadStatusOk;
    } while(false);

    if(parsed) {
        dfc_credential_clear(credential);
        dfc_credential_copy_model(credential, staged);
        snprintf(credential->name, sizeof(credential->name), "%s", selected_name);
    }

    if(octets && octets != raw) free(octets);
    free(raw);
    if(staged) free(staged);

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
