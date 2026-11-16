#include "dfc_credential_i.h"

#include <stdio.h>

DfcFile* dfc_credential_find_transaction_mac_file(DfcCredential* credential, size_t app_index) {
#if DFC_ENABLE_TRANSACTION_MAC
    for(size_t i = 0; i < credential->num_files; i++) {
        DfcFile* file = &credential->files[i];
        if(file->app_index == app_index && file->type == DFC_FILE_TYPE_TRANSACTION_MAC)
            return file;
    }
#else
    DFC_UNUSED(credential);
    DFC_UNUSED(app_index);
#endif
    return NULL;
}

#define TAG "DfcCredential"

uint8_t dfc_credential_compiled_auth_commands(void) {
    uint8_t commands = 0;
#if DFC_ENABLE_AUTH_D40
    commands |= DFC_AUTH_COMMAND_D40;
#endif
#if DFC_ENABLE_AUTH_ISO
    commands |= DFC_AUTH_COMMAND_ISO_NATIVE;
#endif
#if DFC_ENABLE_AUTH_AES
    commands |= DFC_AUTH_COMMAND_AES;
#endif
#if DFC_ENABLE_EV2_SECURE_MESSAGING
    commands |= DFC_AUTH_COMMAND_EV2_FIRST | DFC_AUTH_COMMAND_EV2_NON_FIRST;
#endif
#if DFC_ENABLE_ISO7816_AUTH
    commands |= DFC_AUTH_COMMAND_ISO7816;
#endif
    return commands;
}

uint8_t dfc_credential_possible_auth_commands(uint8_t key_settings_2, DfcGeneration generation) {
    uint8_t commands;
    switch(key_settings_2 & DFC_KEY_TYPE_MASK) {
    case DFC_KEY_TYPE_AES:
        commands = DFC_AUTH_COMMAND_AES |
                   (generation >= DfcGenerationEv2 ?
                        DFC_AUTH_COMMAND_EV2_FIRST | DFC_AUTH_COMMAND_EV2_NON_FIRST :
                        0);
        break;
    case DFC_KEY_TYPE_3K3DES:
        commands = DFC_AUTH_COMMAND_ISO_NATIVE;
        break;
    default:
        commands = DFC_AUTH_COMMAND_D40 | DFC_AUTH_COMMAND_ISO_NATIVE;
        break;
    }
    return commands;
}

uint8_t dfc_credential_default_auth_commands(uint8_t key_settings_2, DfcGeneration generation) {
    return dfc_credential_possible_auth_commands(key_settings_2, generation) &
           dfc_credential_compiled_auth_commands();
}

uint8_t dfc_credential_picc_auth_commands(const DfcCredential* credential) {
    if(credential->picc_has_auth_commands) return credential->picc_auth_commands;
    return dfc_credential_default_auth_commands(
        credential->picc_key_settings_2, credential->card.generation);
}

uint8_t
    dfc_credential_app_auth_commands(const DfcCredential* credential, const DfcApplication* app) {
    if(app->has_auth_commands) return app->auth_commands;
    return dfc_credential_default_auth_commands(app->key_settings_2, credential->card.generation);
}

void dfc_credential_reset_application(DfcApplication* app) {
    if(!app) return;
    memset(app, 0, sizeof(DfcApplication));
    app->key_offset = DFC_KEY_POOL_NONE;
#if DFC_ENABLE_KEY_SETS
    app->num_key_sets = 1;
    app->key_set_metadata_offset = DFC_KEY_SET_METADATA_POOL_NONE;
#endif
}

static void dfc_application_clear(DfcApplication* app) {
    dfc_credential_reset_application(app);
}

DfcApplication* dfc_credential_get_application(DfcCredential* credential, size_t app_index) {
    if(app_index >= credential->num_apps) return NULL;
    return &credential->apps[app_index];
}

const DfcApplication*
    dfc_credential_get_application_const(const DfcCredential* credential, size_t app_index) {
    if(app_index >= credential->num_apps) return NULL;
    return &credential->apps[app_index];
}

DfcApplication* dfc_credential_get_primary_application(DfcCredential* credential) {
    return dfc_credential_get_application(credential, 0);
}

const DfcApplication*
    dfc_credential_get_primary_application_const(const DfcCredential* credential) {
    return dfc_credential_get_application_const(credential, 0);
}

DfcApplication* dfc_credential_find_application(DfcCredential* credential, const uint8_t aid[3]) {
    for(size_t i = 0; i < credential->num_apps; i++) {
        if(memcmp(credential->apps[i].aid, aid, 3) == 0) return &credential->apps[i];
    }
    return NULL;
}

DfcApplication*
    dfc_credential_find_application_desfire_order(DfcCredential* credential, const uint8_t aid[3]) {
    for(size_t i = 0; i < credential->num_apps; i++) {
        DfcApplication* app = &credential->apps[i];
        if(aid[0] == app->aid[2] && aid[1] == app->aid[1] && aid[2] == app->aid[0]) {
            return app;
        }
    }
    return NULL;
}

size_t dfc_credential_application_index(DfcCredential* credential, DfcApplication* application) {
    if(!application) return SIZE_MAX;
    for(size_t i = 0; i < credential->num_apps; i++) {
        if(&credential->apps[i] == application) return i;
    }
    return SIZE_MAX;
}

DfcApplication* dfc_credential_create_application_desfire_order(
    DfcCredential* credential,
    const uint8_t aid[3],
    uint8_t key_settings_1,
    uint8_t key_settings_2) {
    if(credential->num_apps >= DFC_MAX_APPS) return NULL;
    if(dfc_credential_find_application_desfire_order(credential, aid)) return NULL;

    size_t num_keys = key_settings_2 & DFC_NUM_KEYS_MASK;
    if(num_keys == 0 || num_keys > DFC_EV1_MAX_KEYS_PER_APP || num_keys > DFC_MAX_KEYS) {
        return NULL;
    }

    DfcApplication* app = &credential->apps[credential->num_apps];
    dfc_application_clear(app);
    // Key material has to be reserved before the application counts as created,
    // so a pool that cannot hold it leaves the credential untouched.
    if(!dfc_credential_keys_resize(
           credential, app, num_keys, dfc_credential_key_length(key_settings_2))) {
        return NULL;
    }
    credential->num_apps++;
    app->aid[0] = aid[2];
    app->aid[1] = aid[1];
    app->aid[2] = aid[0];
    app->key_settings_1 = key_settings_1;
    app->key_settings_2 = key_settings_2;
    uint8_t key_type = key_settings_2 & DFC_KEY_TYPE_MASK;
    if(key_type == DFC_KEY_TYPE_AES) {
        app->auth_command = DFC_CMD_AUTHENTICATE_AES;
    } else if(key_type == DFC_KEY_TYPE_3K3DES) {
        app->auth_command = DFC_CMD_AUTHENTICATE_ISO;
    } else {
        app->auth_command = DFC_CMD_AUTHENTICATE_LEGACY;
    }
    return app;
}

bool dfc_credential_delete_application_at(DfcCredential* credential, size_t app_index) {
    if(app_index >= credential->num_apps) return false;

    // Release before the array shifts: the release fixes up the offsets held by
    // the other records, which is only correct while they are still in place.
    dfc_credential_keys_release(credential, &credential->apps[app_index]);

    for(size_t i = 0; i < credential->num_files;) {
        if(credential->files[i].app_index == app_index) {
            dfc_file_release(credential, &credential->files[i]);
            if(i + 1 < credential->num_files) {
                memmove(
                    &credential->files[i],
                    &credential->files[i + 1],
                    (credential->num_files - i - 1) * sizeof(DfcFile));
            }
            credential->num_files--;
            memset(&credential->files[credential->num_files], 0, sizeof(DfcFile));
            continue;
        }
        // Only real application indices shift. DFC_FILE_OWNER_PICC is larger
        // than any of them, so renumbering it would quietly turn a PICC-level
        // file into a file of an application that does not exist.
        if(credential->files[i].app_index != DFC_FILE_OWNER_PICC &&
           credential->files[i].app_index > app_index) {
            credential->files[i].app_index--;
        }
        i++;
    }

    if(app_index + 1 < credential->num_apps) {
        memmove(
            &credential->apps[app_index],
            &credential->apps[app_index + 1],
            (credential->num_apps - app_index - 1) * sizeof(DfcApplication));
    }
    credential->num_apps--;
    dfc_credential_reset_application(&credential->apps[credential->num_apps]);
    return true;
}

void dfc_credential_format_picc(DfcCredential* credential) {
    // The PICC record survives a format; move its key slice to the head of the
    // pool in one pass and wipe whatever application key material trailed it.
    size_t old_key_used = credential->key_pool_used;
    if(credential->picc_key_offset != DFC_KEY_POOL_NONE && credential->picc_key_pool_len > 0 &&
       credential->picc_key_offset <= old_key_used &&
       credential->picc_key_pool_len <= old_key_used - credential->picc_key_offset) {
        if(credential->picc_key_offset > 0) {
            memmove(
                credential->key_pool,
                credential->key_pool + credential->picc_key_offset,
                credential->picc_key_pool_len);
        }
        credential->picc_key_offset = 0;
        credential->key_pool_used = credential->picc_key_pool_len;
    } else {
        credential->picc_key_offset = DFC_KEY_POOL_NONE;
        credential->picc_key_pool_len = 0;
        credential->key_pool_used = 0;
    }
    if(old_key_used > credential->key_pool_used) {
        dfc_secure_wipe(
            credential->key_pool + credential->key_pool_used,
            old_key_used - credential->key_pool_used);
    }
#if DFC_ENABLE_KEY_SETS
    if(credential->key_set_metadata_pool_used > 0) {
        dfc_secure_wipe(credential->key_set_metadata_pool, credential->key_set_metadata_pool_used);
        credential->key_set_metadata_pool_used = 0;
    }
#endif
    for(size_t i = 0; i < DFC_MAX_APPS; i++) {
        dfc_credential_reset_application(&credential->apps[i]);
    }
    if(credential->num_files > 0) {
        dfc_secure_wipe(credential->files, credential->num_files * sizeof(DfcFile));
    }
    if(credential->file_pool_used > 0) {
        dfc_secure_wipe(credential->file_pool, credential->file_pool_used);
    }
    credential->num_apps = 0;
    credential->num_files = 0;
    credential->file_pool_used = 0;
}

DfcFile* dfc_credential_find_file(DfcCredential* credential, uint8_t number) {
    return dfc_credential_find_file_in_app(credential, 0, number);
}

DfcFile*
    dfc_credential_find_file_in_app(DfcCredential* credential, size_t app_index, uint8_t number) {
    for(size_t i = 0; i < credential->num_files; i++) {
        if(credential->files[i].app_index == app_index && credential->files[i].number == number) {
            return &credential->files[i];
        }
    }
    return NULL;
}

DfcFile* dfc_credential_create_file(DfcCredential* credential, size_t app_index, uint8_t number) {
    if(app_index >= credential->num_apps) return NULL;
    if(credential->num_files >= DFC_MAX_FILES) return NULL;
    if(dfc_credential_find_file_in_app(credential, app_index, number)) return NULL;

    DfcFile* file = &credential->files[credential->num_files++];
    memset(file, 0, sizeof(DfcFile));
    file->app_index = (uint8_t)app_index;
    file->number = number;
    file->data_offset = DFC_FILE_POOL_NONE;
    file->data_len = 0;
    return file;
}

static void dfc_file_pool_compact_after_release(
    DfcCredential* credential,
    size_t freed_offset,
    size_t freed_len) {
    if(freed_len == 0 || freed_offset == DFC_FILE_POOL_NONE) return;
    size_t tail = credential->file_pool_used - (freed_offset + freed_len);
    if(tail > 0) {
        memmove(
            credential->file_pool + freed_offset,
            credential->file_pool + freed_offset + freed_len,
            tail);
    }
    credential->file_pool_used = (DfcFilePoolSize)(credential->file_pool_used - freed_len);
    dfc_secure_wipe(credential->file_pool + credential->file_pool_used, freed_len);
    for(size_t i = 0; i < credential->num_files; i++) {
        DfcFile* other = &credential->files[i];
        if(other->data_offset != DFC_FILE_POOL_NONE && other->data_offset > freed_offset) {
            other->data_offset = (DfcFilePoolSize)(other->data_offset - freed_len);
        }
    }
}

static bool dfc_pool_slice_resolve(
    size_t offset,
    size_t length,
    size_t used,
    size_t none,
    size_t* resolved_offset) {
    if(length == 0) {
        if(offset != 0 && offset != none) return false;
        *resolved_offset = none;
        return true;
    }
    if(offset == none || offset > used || length > used - offset) return false;
    *resolved_offset = offset;
    return true;
}

#if DFC_ENABLE_KEY_SETS
static size_t dfc_key_set_metadata_length(size_t num_key_sets, size_t num_keys) {
    if(num_key_sets < DFC_KEY_SET_MINIMUM_COUNT || num_key_sets > DFC_MAX_KEY_SETS ||
       num_keys == 0 || num_keys > DFC_MAX_KEYS) {
        return 0;
    }
    return 2u * num_key_sets + sizeof(uint16_t) + (num_key_sets - 1u) * num_keys;
}

static bool dfc_key_set_metadata_slice(
    const DfcCredential* credential,
    const DfcApplication* app,
    const uint8_t** data) {
    if(!credential || !app || app->num_key_sets < DFC_KEY_SET_MINIMUM_COUNT ||
       credential->key_set_metadata_pool_used > DFC_KEY_SET_METADATA_POOL_SIZE) {
        return false;
    }
    size_t expected = dfc_key_set_metadata_length(app->num_key_sets, app->num_keys);
    if(expected == 0 || app->key_set_metadata_len != expected ||
       app->key_set_metadata_offset == DFC_KEY_SET_METADATA_POOL_NONE ||
       app->key_set_metadata_offset > DFC_KEY_SET_METADATA_POOL_SIZE ||
       expected > DFC_KEY_SET_METADATA_POOL_SIZE - app->key_set_metadata_offset ||
       app->key_set_metadata_offset > credential->key_set_metadata_pool_used ||
       expected > credential->key_set_metadata_pool_used - app->key_set_metadata_offset) {
        return false;
    }
    *data = credential->key_set_metadata_pool + app->key_set_metadata_offset;
    return true;
}

static void dfc_key_set_metadata_compact_after_release(
    DfcCredential* credential,
    size_t freed_offset,
    size_t freed_len) {
    if(freed_len == 0 || freed_offset == DFC_KEY_SET_METADATA_POOL_NONE) return;
    size_t tail = credential->key_set_metadata_pool_used - (freed_offset + freed_len);
    if(tail > 0) {
        memmove(
            credential->key_set_metadata_pool + freed_offset,
            credential->key_set_metadata_pool + freed_offset + freed_len,
            tail);
    }
    credential->key_set_metadata_pool_used =
        (uint16_t)(credential->key_set_metadata_pool_used - freed_len);
    dfc_secure_wipe(
        credential->key_set_metadata_pool + credential->key_set_metadata_pool_used, freed_len);
    for(size_t i = 0; i < DFC_MAX_APPS; i++) {
        DfcApplication* other = &credential->apps[i];
        if(other->key_set_metadata_offset != DFC_KEY_SET_METADATA_POOL_NONE &&
           other->key_set_metadata_offset > freed_offset) {
            other->key_set_metadata_offset =
                (uint16_t)(other->key_set_metadata_offset - freed_len);
        }
    }
}

static void dfc_key_set_metadata_release(DfcCredential* credential, DfcApplication* app) {
    if(!credential || !app) return;
    size_t old_offset = app->key_set_metadata_offset;
    size_t old_len = app->key_set_metadata_len;
    app->key_set_metadata_offset = DFC_KEY_SET_METADATA_POOL_NONE;
    app->key_set_metadata_len = 0;
    if(old_offset == DFC_KEY_SET_METADATA_POOL_NONE || old_len == 0) return;
    if(old_offset > credential->key_set_metadata_pool_used ||
       old_len > credential->key_set_metadata_pool_used - old_offset) {
        return;
    }
    dfc_key_set_metadata_compact_after_release(credential, old_offset, old_len);
}

static bool dfc_key_set_metadata_can_resize(
    const DfcCredential* credential,
    const DfcApplication* app,
    size_t want) {
    if(!credential || !app || want > DFC_KEY_SET_METADATA_POOL_SIZE ||
       credential->key_set_metadata_pool_used > DFC_KEY_SET_METADATA_POOL_SIZE) {
        return false;
    }
    size_t old_offset;
    if(!dfc_pool_slice_resolve(
           app->key_set_metadata_offset,
           app->key_set_metadata_len,
           credential->key_set_metadata_pool_used,
           DFC_KEY_SET_METADATA_POOL_NONE,
           &old_offset)) {
        return false;
    }
    size_t used_without_old = credential->key_set_metadata_pool_used;
    if(old_offset != DFC_KEY_SET_METADATA_POOL_NONE) used_without_old -= app->key_set_metadata_len;
    return want <= DFC_KEY_SET_METADATA_POOL_SIZE - used_without_old;
}

static bool
    dfc_key_set_metadata_resize(DfcCredential* credential, DfcApplication* app, size_t want) {
    if(!dfc_key_set_metadata_can_resize(credential, app, want)) return false;
    dfc_key_set_metadata_release(credential, app);
    if(want == 0) return true;
    app->key_set_metadata_offset = (uint16_t)credential->key_set_metadata_pool_used;
    app->key_set_metadata_len = (uint16_t)want;
    memset(credential->key_set_metadata_pool + credential->key_set_metadata_pool_used, 0, want);
    credential->key_set_metadata_pool_used =
        (uint16_t)(credential->key_set_metadata_pool_used + want);
    return true;
}

static uint8_t* dfc_key_set_metadata_mutable(DfcCredential* credential, DfcApplication* app) {
    const uint8_t* data;
    return dfc_key_set_metadata_slice(credential, app, &data) ? (uint8_t*)data : NULL;
}

uint8_t* dfc_credential_key_set_version(
    DfcCredential* credential,
    DfcApplication* app,
    size_t key_set_number) {
    uint8_t* data = dfc_key_set_metadata_mutable(credential, app);
    return data && key_set_number < app->num_key_sets ? &data[key_set_number] : NULL;
}

const uint8_t* dfc_credential_key_set_version_const(
    const DfcCredential* credential,
    const DfcApplication* app,
    size_t key_set_number) {
    const uint8_t* data;
    return dfc_key_set_metadata_slice(credential, app, &data) &&
                   key_set_number < app->num_key_sets ?
               &data[key_set_number] :
               NULL;
}

uint8_t* dfc_credential_key_set_type(
    DfcCredential* credential,
    DfcApplication* app,
    size_t key_set_number) {
    uint8_t* data = dfc_key_set_metadata_mutable(credential, app);
    return data && key_set_number < app->num_key_sets ? &data[app->num_key_sets + key_set_number] :
                                                        NULL;
}

const uint8_t* dfc_credential_key_set_type_const(
    const DfcCredential* credential,
    const DfcApplication* app,
    size_t key_set_number) {
    const uint8_t* data;
    return dfc_key_set_metadata_slice(credential, app, &data) &&
                   key_set_number < app->num_key_sets ?
               &data[app->num_key_sets + key_set_number] :
               NULL;
}

bool dfc_credential_key_set_is_initialized(
    const DfcCredential* credential,
    const DfcApplication* app,
    size_t key_set_number) {
    if(!app || key_set_number >= app->num_key_sets) return false;
    if(app->num_key_sets < DFC_KEY_SET_MINIMUM_COUNT) return key_set_number == 0;
    const uint8_t* data;
    if(!dfc_key_set_metadata_slice(credential, app, &data)) return false;
    size_t mask_offset = 2u * app->num_key_sets;
    uint16_t mask = (uint16_t)(data[mask_offset] | ((uint16_t)data[mask_offset + 1] << 8));
    return (mask & ((uint16_t)1u << key_set_number)) != 0;
}

bool dfc_credential_key_set_set_initialized(
    DfcCredential* credential,
    DfcApplication* app,
    size_t key_set_number,
    bool initialized) {
    uint8_t* data = dfc_key_set_metadata_mutable(credential, app);
    if(!data || key_set_number >= app->num_key_sets) return false;
    size_t mask_offset = 2u * app->num_key_sets;
    uint16_t mask = (uint16_t)(data[mask_offset] | ((uint16_t)data[mask_offset + 1] << 8));
    uint16_t bit = (uint16_t)1u << key_set_number;
    mask = initialized ? (uint16_t)(mask | bit) : (uint16_t)(mask & (uint16_t)~bit);
    data[mask_offset] = (uint8_t)mask;
    data[mask_offset + 1] = (uint8_t)(mask >> 8);
    return true;
}
#endif

uint8_t* dfc_file_data(DfcCredential* credential, DfcFile* file) {
    if(!credential || !file || file->data_offset == DFC_FILE_POOL_NONE || file->data_len == 0) {
        return NULL;
    }
    if(file->data_offset + file->data_len > credential->file_pool_used) return NULL;
    return credential->file_pool + file->data_offset;
}

const uint8_t* dfc_file_data_const(const DfcCredential* credential, const DfcFile* file) {
    if(!credential || !file || file->data_offset == DFC_FILE_POOL_NONE || file->data_len == 0) {
        return NULL;
    }
    if(file->data_offset + file->data_len > credential->file_pool_used) return NULL;
    return credential->file_pool + file->data_offset;
}

bool dfc_file_resize(DfcCredential* credential, DfcFile* file, size_t data_len) {
    if(!credential || !file) return false;
    if(data_len > DFC_MAX_FILE_DATA) return false;
    if(data_len > DFC_FILE_POOL_SIZE) return false;
    if(credential->file_pool_used > DFC_FILE_POOL_SIZE) return false;
    size_t old_offset;
    if(!dfc_pool_slice_resolve(
           file->data_offset,
           file->data_len,
           credential->file_pool_used,
           DFC_FILE_POOL_NONE,
           &old_offset)) {
        return false;
    }

    if(old_offset != DFC_FILE_POOL_NONE && file->data_len == data_len) {
        return true;
    }

    size_t used_without_old = credential->file_pool_used;
    if(old_offset != DFC_FILE_POOL_NONE) {
        if(file->data_len > used_without_old) return false;
        used_without_old -= file->data_len;
    }
    if(data_len > DFC_FILE_POOL_SIZE - used_without_old) return false;

    // Capacity is known before releasing the old slice, so a failed resize
    // never destroys file data that still backs the live model.
    if(old_offset != DFC_FILE_POOL_NONE) {
        size_t old_len = file->data_len;
        file->data_offset = DFC_FILE_POOL_NONE;
        file->data_len = 0;
        dfc_file_pool_compact_after_release(credential, old_offset, old_len);
    }

    if(data_len == 0) {
        file->data_offset = DFC_FILE_POOL_NONE;
        file->data_len = 0;
        return true;
    }

    file->data_offset = (DfcFilePoolSize)credential->file_pool_used;
    file->data_len = (DfcFilePoolSize)data_len;
    memset(credential->file_pool + file->data_offset, 0, data_len);
    credential->file_pool_used = (DfcFilePoolSize)(credential->file_pool_used + data_len);
    return true;
}

void dfc_file_release(DfcCredential* credential, DfcFile* file) {
    if(!credential || !file) return;
    if(file->data_offset == DFC_FILE_POOL_NONE) {
        file->data_len = 0;
        return;
    }
    size_t off = file->data_offset;
    size_t len = file->data_len;
    file->data_offset = DFC_FILE_POOL_NONE;
    file->data_len = 0;
    dfc_file_pool_compact_after_release(credential, off, len);
}

size_t dfc_credential_file_pool_free(const DfcCredential* credential) {
    if(!credential) return 0;
    if(credential->file_pool_used >= DFC_FILE_POOL_SIZE) return 0;
    return DFC_FILE_POOL_SIZE - credential->file_pool_used;
}

size_t dfc_credential_stored_key_length(size_t key_len) {
    return key_len == 24 ? 24 : 16;
}

// Application and PICC records use compact fields whose concrete widths can
// differ. Access them through values rather than type-punning pointers, then
// narrow only after the common capacity checks have succeeded.
static size_t key_offset_of(const DfcCredential* credential, const DfcApplication* app) {
    return app ? app->key_offset : credential->picc_key_offset;
}

static void set_key_offset(DfcCredential* credential, DfcApplication* app, DfcKeyPoolSize offset) {
    if(app)
        app->key_offset = offset;
    else
        credential->picc_key_offset = offset;
}

static size_t num_keys_of(const DfcCredential* credential, const DfcApplication* app) {
    return app ? app->num_keys : credential->picc_num_keys;
}

static void set_num_keys(DfcCredential* credential, DfcApplication* app, uint8_t count) {
    if(app)
        app->num_keys = count;
    else
        credential->picc_num_keys = count;
}

static size_t key_len_of(const DfcCredential* credential, const DfcApplication* app) {
    return app ? app->key_len : credential->picc_key_len;
}

static void set_key_len(DfcCredential* credential, DfcApplication* app, uint8_t length) {
    if(app)
        app->key_len = length;
    else
        credential->picc_key_len = length;
}

static size_t key_pool_len_of(const DfcCredential* credential, const DfcApplication* app) {
    return app ? app->key_pool_len : credential->picc_key_pool_len;
}

static void
    set_key_pool_len(DfcCredential* credential, DfcApplication* app, DfcKeyPoolSize length) {
    if(app)
        app->key_pool_len = length;
    else
        credential->picc_key_pool_len = length;
}

static void dfc_key_pool_compact_after_release(
    DfcCredential* credential,
    size_t freed_offset,
    size_t freed_len) {
    if(freed_len == 0 || freed_offset == DFC_KEY_POOL_NONE) return;
    size_t tail = credential->key_pool_used - (freed_offset + freed_len);
    if(tail > 0) {
        memmove(
            credential->key_pool + freed_offset,
            credential->key_pool + freed_offset + freed_len,
            tail);
    }
    credential->key_pool_used = (DfcKeyPoolSize)(credential->key_pool_used - freed_len);
    dfc_secure_wipe(credential->key_pool + credential->key_pool_used, freed_len);
    // Every record that indexes past the hole, not only the created ones: an
    // application is given its slice before it is counted.
    for(size_t i = 0; i < DFC_MAX_APPS; i++) {
        DfcApplication* app = &credential->apps[i];
        if(app->key_offset != DFC_KEY_POOL_NONE && app->key_offset > freed_offset)
            app->key_offset = (DfcKeyPoolSize)(app->key_offset - freed_len);
    }

    if(credential->picc_key_offset != DFC_KEY_POOL_NONE &&
       credential->picc_key_offset > freed_offset) {
        credential->picc_key_offset = (DfcKeyPoolSize)(credential->picc_key_offset - freed_len);
    }
}

static const uint8_t* dfc_credential_key_at(
    const DfcCredential* credential,
    size_t offset,
    size_t pool_len,
    size_t num_keys,
    size_t key_len,
    size_t slot) {
    if(!credential || offset == DFC_KEY_POOL_NONE || pool_len == 0) return NULL;
    if(slot >= num_keys) return NULL;
    size_t stride = dfc_credential_stored_key_length(key_len);
    if((slot + 1) * stride > pool_len) return NULL;
    if(offset + pool_len > credential->key_pool_used) return NULL;
    return credential->key_pool + offset + slot * stride;
}

uint8_t* dfc_credential_key(DfcCredential* credential, DfcApplication* app, size_t slot) {
    if(!credential) return NULL;
    size_t storage_key_len = key_len_of(credential, app);
#if DFC_ENABLE_KEY_SETS
    if(app) storage_key_len = app->key_storage_len;
#endif
    const uint8_t* p = dfc_credential_key_at(
        credential,
        key_offset_of(credential, app),
        key_pool_len_of(credential, app),
        num_keys_of(credential, app),
        storage_key_len,
        slot);
    // Cast back: the const form exists for const credentials, not to make the
    // pool read-only. ChangeKey writes through this pointer.
    return (uint8_t*)p;
}

const uint8_t* dfc_credential_key_const(
    const DfcCredential* credential,
    const DfcApplication* app,
    size_t slot) {
    if(!credential) return NULL;
    size_t storage_key_len = app ? app->key_len : credential->picc_key_len;
#if DFC_ENABLE_KEY_SETS
    if(app) storage_key_len = app->key_storage_len;
#endif
    return dfc_credential_key_at(
        credential,
        app ? app->key_offset : credential->picc_key_offset,
        app ? app->key_pool_len : credential->picc_key_pool_len,
        app ? app->num_keys : credential->picc_num_keys,
        storage_key_len,
        slot);
}

static bool dfc_credential_keys_resize_total(
    DfcCredential* credential,
    DfcApplication* app,
    size_t num_keys,
    size_t allocated_keys,
    size_t key_len,
    size_t storage_key_len) {
    if(!credential) return false;
    if(num_keys > DFC_MAX_KEYS) return false;
    if(key_len > DFC_MAX_KEY_LEN || storage_key_len > DFC_MAX_KEY_LEN) return false;
    if(allocated_keys < num_keys) return false;

    size_t offset = key_offset_of(credential, app);
    size_t pool_len = key_pool_len_of(credential, app);

    size_t stride = dfc_credential_stored_key_length(storage_key_len);
    if(allocated_keys > DFC_KEY_POOL_SIZE / stride) return false;
    size_t want = allocated_keys * stride;
    if(want > DFC_KEY_POOL_SIZE) return false;
    if(credential->key_pool_used > DFC_KEY_POOL_SIZE) return false;
    size_t old_offset;
    if(!dfc_pool_slice_resolve(
           offset, pool_len, credential->key_pool_used, DFC_KEY_POOL_NONE, &old_offset)) {
        return false;
    }

    if(old_offset != DFC_KEY_POOL_NONE && pool_len == want && want > 0) {
        // Same footprint: keep the material, only the counts can change.
        set_num_keys(credential, app, (uint8_t)num_keys);
        set_key_len(credential, app, (uint8_t)key_len);
#if DFC_ENABLE_KEY_SETS
        if(app) app->key_storage_len = (uint8_t)storage_key_len;
#endif
        return true;
    }

    size_t used_without_old = credential->key_pool_used;
    if(old_offset != DFC_KEY_POOL_NONE) {
        if(pool_len > used_without_old) return false;
        used_without_old -= pool_len;
    }
    if(want > DFC_KEY_POOL_SIZE - used_without_old) return false;

    // Capacity is known before releasing the old slice, so a failed resize
    // never destroys the existing key material.
    dfc_credential_keys_release(credential, app);

    if(want == 0) {
        set_num_keys(credential, app, (uint8_t)num_keys);
        set_key_len(credential, app, (uint8_t)key_len);
        return true;
    }

    set_key_offset(credential, app, (DfcKeyPoolSize)credential->key_pool_used);
    set_key_pool_len(credential, app, (DfcKeyPoolSize)want);
    memset(credential->key_pool + credential->key_pool_used, 0, want);
    credential->key_pool_used = (DfcKeyPoolSize)(credential->key_pool_used + want);
    set_num_keys(credential, app, (uint8_t)num_keys);
    set_key_len(credential, app, (uint8_t)key_len);
#if DFC_ENABLE_KEY_SETS
    if(app) app->key_storage_len = (uint8_t)storage_key_len;
#endif
    return true;
}

bool dfc_credential_keys_resize(
    DfcCredential* credential,
    DfcApplication* app,
    size_t num_keys,
    size_t key_len) {
    if(!dfc_credential_keys_resize_total(credential, app, num_keys, num_keys, key_len, key_len))
        return false;
#if DFC_ENABLE_KEY_SETS
    if(app) {
        app->num_key_sets = 1;
        app->max_key_size = 0;
        app->key_set_settings = 0;
        if(!dfc_key_set_metadata_resize(credential, app, 0)) return false;
    }
#endif
    return true;
}

#if DFC_ENABLE_KEY_SETS
bool dfc_credential_key_sets_resize(
    DfcCredential* credential,
    DfcApplication* app,
    size_t num_key_sets,
    size_t num_keys,
    size_t active_key_len,
    size_t max_key_size) {
    if(!app || num_key_sets < DFC_KEY_SET_MINIMUM_COUNT || num_key_sets > DFC_MAX_KEY_SETS)
        return false;
    if(num_keys > SIZE_MAX / num_key_sets) return false;
    size_t metadata_len = dfc_key_set_metadata_length(num_key_sets, num_keys);
    if(metadata_len == 0 || !dfc_key_set_metadata_can_resize(credential, app, metadata_len))
        return false;
    if(!dfc_credential_keys_resize_total(
           credential, app, num_keys, num_keys * num_key_sets, active_key_len, max_key_size))
        return false;
    if(!dfc_key_set_metadata_resize(credential, app, metadata_len)) return false;
    app->num_key_sets = (uint8_t)num_key_sets;
    app->max_key_size = (uint8_t)max_key_size;
    uint8_t* metadata = dfc_key_set_metadata_mutable(credential, app);
    if(!metadata) return false;
    size_t mask_offset = 2u * num_key_sets;
    uint16_t initialized_mask =
        num_key_sets == 16 ? UINT16_MAX : (uint16_t)(((uint16_t)1u << num_key_sets) - 1u);
    metadata[mask_offset] = (uint8_t)initialized_mask;
    metadata[mask_offset + 1] = (uint8_t)(initialized_mask >> 8);
    return true;
}

const uint8_t* dfc_credential_key_in_set_const(
    const DfcCredential* credential,
    const DfcApplication* app,
    size_t key_set_number,
    size_t slot) {
    if(!credential || !app || key_set_number >= app->num_key_sets || slot >= app->num_keys)
        return NULL;
    size_t allocated_keys = app->num_keys * app->num_key_sets;
    return dfc_credential_key_at(
        credential,
        app->key_offset,
        app->key_pool_len,
        allocated_keys,
        app->key_storage_len,
        key_set_number * app->num_keys + slot);
}

uint8_t* dfc_credential_key_in_set(
    DfcCredential* credential,
    DfcApplication* app,
    size_t key_set_number,
    size_t slot) {
    return (uint8_t*)dfc_credential_key_in_set_const(credential, app, key_set_number, slot);
}

const uint8_t* dfc_credential_key_version_in_set_const(
    const DfcCredential* credential,
    const DfcApplication* app,
    size_t key_set_number,
    size_t slot) {
    if(!credential || !app || key_set_number >= app->num_key_sets || slot >= app->num_keys)
        return NULL;
    if(key_set_number == 0) return &app->key_versions[slot];
    const uint8_t* metadata;
    if(!dfc_key_set_metadata_slice(credential, app, &metadata)) return NULL;
    return metadata ? &metadata
                          [2u * app->num_key_sets + sizeof(uint16_t) +
                           (key_set_number - 1u) * app->num_keys + slot] :
                      NULL;
}

uint8_t* dfc_credential_key_version_in_set(
    DfcCredential* credential,
    DfcApplication* app,
    size_t key_set_number,
    size_t slot) {
    return (
        uint8_t*)dfc_credential_key_version_in_set_const(credential, app, key_set_number, slot);
}

static bool
    dfc_credential_rotate_key_set_metadata(DfcCredential* credential, DfcApplication* app) {
    uint8_t* metadata = dfc_key_set_metadata_mutable(credential, app);
    if(!metadata) return false;
    uint8_t* set_versions = metadata;
    uint8_t* set_types = metadata + app->num_key_sets;
    size_t mask_offset = 2u * app->num_key_sets;
    uint16_t initialized_mask =
        (uint16_t)(metadata[mask_offset] | ((uint16_t)metadata[mask_offset + 1] << 8));
    uint8_t* additional_versions = metadata + mask_offset + sizeof(uint16_t);
    uint8_t first_versions[DFC_MAX_KEYS];
    memcpy(first_versions, app->key_versions, sizeof(first_versions));
    memcpy(app->key_versions, additional_versions, app->num_keys);
    if(app->num_key_sets > DFC_KEY_SET_MINIMUM_COUNT) {
        memmove(
            additional_versions,
            additional_versions + app->num_keys,
            (app->num_key_sets - DFC_KEY_SET_MINIMUM_COUNT) * app->num_keys);
    }
    memcpy(
        additional_versions + (app->num_key_sets - DFC_KEY_SET_MINIMUM_COUNT) * app->num_keys,
        first_versions,
        app->num_keys);
    dfc_secure_wipe(first_versions, sizeof(first_versions));

    uint8_t first_set_version = set_versions[0];
    uint8_t first_set_type = set_types[0];
    bool first_initialized = (initialized_mask & 1u) != 0;
    memmove(set_versions, set_versions + 1, (app->num_key_sets - 1) * sizeof(set_versions[0]));
    memmove(set_types, set_types + 1, (app->num_key_sets - 1) * sizeof(set_types[0]));
    set_versions[app->num_key_sets - 1] = first_set_version;
    set_types[app->num_key_sets - 1] = first_set_type;
    initialized_mask = (uint16_t)(initialized_mask >> 1);
    if(first_initialized) initialized_mask |= (uint16_t)1u << (app->num_key_sets - 1u);
    metadata[mask_offset] = (uint8_t)initialized_mask;
    metadata[mask_offset + 1] = (uint8_t)(initialized_mask >> 8);
    return true;
}

static size_t dfc_greatest_common_divisor(size_t a, size_t b) {
    while(b != 0) {
        size_t remainder = a % b;
        a = b;
        b = remainder;
    }
    return a;
}

// Rotate fixed-width key blocks without copying a complete key set onto the
// embedded stack. Each cycle wipes the one-key scratch after restoring it.
static void dfc_rotate_key_blocks_left(
    uint8_t* blocks,
    size_t block_count,
    size_t block_width,
    size_t shift,
    uint8_t scratch[DFC_MAX_KEY_LEN]) {
    size_t cycles = dfc_greatest_common_divisor(block_count, shift);
    for(size_t cycle = 0; cycle < cycles; cycle++) {
        size_t current = cycle;
        memcpy(scratch, blocks + current * block_width, block_width);
        while(true) {
            size_t next = (current + shift) % block_count;
            if(next == cycle) break;
            memcpy(blocks + current * block_width, blocks + next * block_width, block_width);
            current = next;
        }
        memcpy(blocks + current * block_width, scratch, block_width);
        dfc_secure_wipe(scratch, DFC_MAX_KEY_LEN);
    }
}

bool dfc_credential_roll_key_set(
    DfcCredential* credential,
    DfcApplication* app,
    size_t key_set_number) {
    if(!credential || !app || key_set_number == 0 || key_set_number >= app->num_key_sets)
        return false;
    if(!dfc_key_set_metadata_mutable(credential, app)) return false;
    size_t key_width = dfc_credential_stored_key_length(app->key_storage_len);
    size_t block_count = app->num_keys * app->num_key_sets;
    size_t shift = key_set_number * app->num_keys;
    uint8_t scratch[DFC_MAX_KEY_LEN];
    dfc_rotate_key_blocks_left(
        credential->key_pool + app->key_offset, block_count, key_width, shift, scratch);
    for(size_t rotation = 0; rotation < key_set_number; rotation++) {
        if(!dfc_credential_rotate_key_set_metadata(credential, app)) return false;
    }
    const uint8_t* active_type = dfc_credential_key_set_type_const(credential, app, 0);
    if(!active_type) return false;
    app->key_len = dfc_key_set_type_length(*active_type);
    app->key_settings_2 = (uint8_t)((app->key_settings_2 & ~DFC_KEY_TYPE_MASK) |
                                    dfc_key_type_for_set_type(*active_type));
    return true;
}
#endif

void dfc_credential_keys_release(DfcCredential* credential, DfcApplication* app) {
    if(!credential) return;
    size_t old_offset = key_offset_of(credential, app);
    size_t old_len = key_pool_len_of(credential, app);
    set_key_offset(credential, app, DFC_KEY_POOL_NONE);
    set_key_pool_len(credential, app, 0);
    set_num_keys(credential, app, 0);
    set_key_len(credential, app, 0);
#if DFC_ENABLE_KEY_SETS
    if(app) {
        app->key_storage_len = 0;
        dfc_key_set_metadata_release(credential, app);
    }
#endif
    if(old_offset != DFC_KEY_POOL_NONE && old_len > 0) {
        dfc_key_pool_compact_after_release(credential, old_offset, old_len);
    }
}

size_t dfc_credential_key_pool_free(const DfcCredential* credential) {
    if(!credential) return 0;
    if(credential->key_pool_used >= DFC_KEY_POOL_SIZE) return 0;
    return DFC_KEY_POOL_SIZE - credential->key_pool_used;
}

bool dfc_credential_delete_file(DfcCredential* credential, size_t app_index, uint8_t number) {
    for(size_t i = 0; i < credential->num_files; i++) {
        if(credential->files[i].app_index == app_index && credential->files[i].number == number) {
            dfc_file_release(credential, &credential->files[i]);
            if(i + 1 < credential->num_files) {
                memmove(
                    &credential->files[i],
                    &credential->files[i + 1],
                    (credential->num_files - i - 1) * sizeof(DfcFile));
            }
            credential->num_files--;
            memset(&credential->files[credential->num_files], 0, sizeof(DfcFile));
            return true;
        }
    }
    return false;
}

size_t dfc_credential_count_files_in_app(const DfcCredential* credential, size_t app_index) {
    size_t count = 0;
    for(size_t i = 0; i < credential->num_files; i++) {
        if(credential->files[i].app_index == app_index) count++;
    }
    return count;
}

void dfc_credential_copy_model(DfcCredential* credential, const DfcCredential* loaded) {
    if(!credential || !loaded) return;
    // DfcCredential is deliberately a pointer-free value type. Assignment
    // includes every current and future inline field, including pool metadata.
    *credential = *loaded;
}

bool dfc_file_set_data_size(DfcCredential* credential, DfcFile* file, uint32_t size) {
    if(!credential || !file) return false;
    if(!dfc_file_type_is_data(file->type)) return false;
    if(!dfc_file_resize(credential, file, size)) return false;
    if(!dfc_file_set_declared_size(file, size)) return false;
    file->contents_complete = true;
    return true;
}

bool dfc_credential_materialize_contents(
    DfcCredential* credential,
    uint8_t* scratch,
    size_t scratch_len) {
    if(!credential || !scratch) return false;
    bool complete = true;
    for(size_t i = 0; i < credential->num_files; i++) {
        DfcFile* file = &credential->files[i];
        size_t want;
        if(file->type == 0x00 || file->type == 0x01) {
            uint32_t declared_size;
            if(!dfc_file_get_declared_size(file, &declared_size)) return false;
            want = declared_size;
        } else if(file->type == 0x03 || file->type == 0x04) {
            want = (size_t)file->state.record.count * (size_t)file->state.record.size;
        } else {
            continue; // value files hold no pool slice
        }
        if(file->data_len >= want) {
            file->contents_complete = true;
            continue;
        }
        // The known prefix has to survive the grow: dfc_file_resize releases the
        // old slice first, and the compaction that follows moves the pool tail
        // down over it. Park the bytes in the caller's scratch, which is the
        // transfer buffer the encoding arrived in and is free by now.
        size_t known_len = file->data_len;
        if(known_len > scratch_len) {
            complete = false;
            continue;
        }
        if(known_len > 0 && file->data_offset != DFC_FILE_POOL_NONE &&
           file->data_offset + known_len == credential->file_pool_used &&
           want <= DFC_MAX_FILE_DATA &&
           want - known_len <= dfc_credential_file_pool_free(credential)) {
            size_t grow = want - known_len;
            memset(credential->file_pool + credential->file_pool_used, 0, grow);
            credential->file_pool_used = (DfcFilePoolSize)(credential->file_pool_used + grow);
            file->data_len = (DfcFilePoolSize)want;
            file->contents_complete = true;
            continue;
        }
        const uint8_t* src = dfc_file_data_const(credential, file);
        if(src && known_len) memcpy(scratch, src, known_len);

        if(!dfc_file_resize(credential, file, want)) {
            complete = false;
            continue;
        }
        uint8_t* dst = dfc_file_data(credential, file);
        if(dst && known_len) memcpy(dst, scratch, known_len);
        file->contents_complete = true;
    }
    return complete;
}

void dfc_credential_mark_dirty(DfcCredential* credential) {
    credential->dirty = true;
}

void dfc_credential_clear_dirty(DfcCredential* credential) {
    credential->dirty = false;
}

bool dfc_credential_picc_ats_is_consistent(const DfcCredential* credential) {
    if(!credential) return false;
    if(credential->picc_ats_len == 0) return true;
    if(credential->picc_ats_len > DFC_PICC_ATS_MAX) return false;
    // An ATS is self-describing: its first octet is the length of the whole
    // answer. One that contradicts itself cannot be transmitted, only clamped or
    // padded, and either would put a frame on the wire that no card would send.
    return credential->picc_ats[0] == (uint8_t)credential->picc_ats_len;
}

DfcCredential* dfc_credential_alloc(void) {
    DfcCredential* credential = malloc(sizeof(DfcCredential));
    if(credential) memset(credential, 0, sizeof(DfcCredential));
    return credential;
}

bool dfc_credential_clear(DfcCredential* credential) {
    // Whatever the credential held, key material included, is wiped first: this
    // is also how a decoder discards a credential it only partly filled.
    dfc_secure_wipe(credential, sizeof(DfcCredential));
    credential->picc_key_settings_1 = 0x0F;
    credential->picc_key_settings_2 = DFC_KEY_TYPE_DES_2K3DES | 1;
    credential->picc_auth_command = DFC_CMD_AUTHENTICATE_LEGACY;
    credential->card.generation = DfcGenerationEv1;
    credential->card.storage = DFC_DEFAULT_CARD_STORAGE;
    credential->card.uid_provenance = DfcUidProvenanceReal;
    for(size_t i = 0; i < DFC_MAX_APPS; i++) {
        credential->apps[i].key_offset = DFC_KEY_POOL_NONE;
#if DFC_ENABLE_KEY_SETS
        credential->apps[i].num_key_sets = 1;
        credential->apps[i].key_set_metadata_offset = DFC_KEY_SET_METADATA_POOL_NONE;
#endif
    }
    credential->picc_key_offset = DFC_KEY_POOL_NONE;
    dfc_credential_keys_resize(credential, NULL, 1, 8);
    return true;
}

bool dfc_credential_init_factory(DfcCredential* credential) {
    uint8_t uid[DFC_DESFIRE_UID_LEN];
    if(!dfc_random_fill(uid, sizeof(uid))) return false;
    dfc_credential_clear(credential);

    credential->uid_len = DFC_DESFIRE_UID_LEN;
    memcpy(credential->uid, uid, sizeof(uid));
    credential->uid[0] = DFC_DESFIRE_UID_FIRST_BYTE;
    return true;
}

bool dfc_credential_init_blank(DfcCredential* credential) {
    if(!dfc_credential_init_factory(credential)) return false;
    const uint8_t aid_desfire_order[3] = {0x01, 0x00, 0x00};
    DfcApplication* app = dfc_credential_create_application_desfire_order(
        credential, aid_desfire_order, 0x0F, DFC_KEY_TYPE_DES_2K3DES | 1);
    if(!app) return false;

    DfcFile* file = dfc_credential_create_file(credential, 0, 0x00);
    if(!file) return false;
    file->number = 0x00;
    file->type = 0x00;
    file->comm_settings = DFC_COMM_PLAIN;
    file->access_rights = 0x0000;
    file->data_offset = DFC_FILE_POOL_NONE;
    file->data_len = 0;
    if(!dfc_file_set_data_size(credential, file, DFC_BLANK_FILE_SIZE)) {
        file->data_offset = DFC_FILE_POOL_NONE;
        file->data_len = 0;
        (void)dfc_file_set_declared_size(file, 0);
        return false;
    }
    return true;
}

void dfc_credential_free(DfcCredential* credential) {
    DFC_ASSERT(credential);
    dfc_secure_wipe(credential, sizeof(*credential));
    free(credential);
}

size_t dfc_credential_key_length(uint8_t key_settings_2) {
    switch(key_settings_2 & DFC_KEY_TYPE_MASK) {
    case DFC_KEY_TYPE_3K3DES:
        return 24;
    case DFC_KEY_TYPE_AES:
        return 16;
    case DFC_KEY_TYPE_DES_2K3DES:
    default:
        return 16;
    }
}

bool dfc_credential_uid_is_detectable(DfcCredential* credential) {
    return dfc_desfire_uid_is_detectable(credential->uid, credential->uid_len);
}
