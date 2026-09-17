#include "dfc_credential_i.h"

#include <stdio.h>

#include "dfc_der.h"
#include "dfc_text.h"

#define TAG "DfcCredential"

void dfc_credential_reset_application(DfcApplication* app) {
    if(!app) return;
    memset(app, 0, sizeof(DfcApplication));
    app->key_offset = DFC_KEY_POOL_NONE;
#if DFC_ENABLE_KEY_SETS
    app->num_key_sets = 1;
    app->key_set_initialized[0] = true;
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
    // The PICC record survives a format, and its keys share the pool with the
    // applications', so the application slices are released one at a time rather
    // than the whole pool being dropped.
    for(size_t i = 0; i < credential->num_apps; i++) {
        dfc_credential_keys_release(credential, &credential->apps[i]);
    }
    for(size_t i = 0; i < DFC_MAX_APPS; i++) {
        dfc_credential_reset_application(&credential->apps[i]);
    }
    memset(credential->files, 0, sizeof(credential->files));
    credential->num_apps = 0;
    credential->num_files = 0;
    memset(credential->file_pool, 0, sizeof(credential->file_pool));
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
    file->app_index = app_index;
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
    credential->file_pool_used -= freed_len;
    for(size_t i = 0; i < credential->num_files; i++) {
        DfcFile* other = &credential->files[i];
        if(other->data_offset != DFC_FILE_POOL_NONE && other->data_offset > freed_offset) {
            other->data_offset -= freed_len;
        }
    }
}

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

    if(file->data_offset != DFC_FILE_POOL_NONE && file->data_len == data_len) {
        return true;
    }

    // Release existing slice first so the free space can be reused.
    if(file->data_offset != DFC_FILE_POOL_NONE) {
        size_t old_off = file->data_offset;
        size_t old_len = file->data_len;
        file->data_offset = DFC_FILE_POOL_NONE;
        file->data_len = 0;
        dfc_file_pool_compact_after_release(credential, old_off, old_len);
    }

    if(data_len == 0) {
        return true;
    }

    if(credential->file_pool_used + data_len > DFC_FILE_POOL_SIZE) {
        return false;
    }

    file->data_offset = credential->file_pool_used;
    file->data_len = data_len;
    memset(credential->file_pool + file->data_offset, 0, data_len);
    credential->file_pool_used += data_len;
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

// Slice bookkeeping lives on the application, or on the credential itself for
// the PICC record; these three pick whichever the caller selected.
static size_t* key_offset_of(DfcCredential* credential, DfcApplication* app) {
    return app ? &app->key_offset : &credential->picc_key_offset;
}

static size_t* num_keys_of(DfcCredential* credential, DfcApplication* app) {
    return app ? &app->num_keys : &credential->picc_num_keys;
}

static size_t* key_len_of(DfcCredential* credential, DfcApplication* app) {
    return app ? &app->key_len : &credential->picc_key_len;
}

static size_t* key_pool_len_of(DfcCredential* credential, DfcApplication* app) {
    return app ? &app->key_pool_len : &credential->picc_key_pool_len;
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
    credential->key_pool_used -= freed_len;
    // Every record that indexes past the hole, not only the created ones: an
    // application is given its slice before it is counted.
    for(size_t i = 0; i < DFC_MAX_APPS; i++) {
        size_t* off = &credential->apps[i].key_offset;
        if(*off != DFC_KEY_POOL_NONE && *off > freed_offset) *off -= freed_len;
    }

    if(credential->picc_key_offset != DFC_KEY_POOL_NONE &&
       credential->picc_key_offset > freed_offset) {
        credential->picc_key_offset -= freed_len;
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
    size_t storage_key_len = *key_len_of(credential, app);
#if DFC_ENABLE_KEY_SETS
    if(app) storage_key_len = app->key_storage_len;
#endif
    const uint8_t* p = dfc_credential_key_at(
        credential,
        *key_offset_of(credential, app),
        *key_pool_len_of(credential, app),
        *num_keys_of(credential, app),
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
    if(allocated_keys < num_keys) return false;

    size_t* offset = key_offset_of(credential, app);
    size_t* pool_len = key_pool_len_of(credential, app);
    size_t* stored_num = num_keys_of(credential, app);
    size_t* stored_len = key_len_of(credential, app);

    size_t stride = dfc_credential_stored_key_length(storage_key_len);
    if(allocated_keys > DFC_KEY_POOL_SIZE / stride) return false;
    size_t want = allocated_keys * stride;
    if(want > DFC_KEY_POOL_SIZE) return false;

    if(*offset != DFC_KEY_POOL_NONE && *pool_len == want && want > 0) {
        // Same footprint: keep the material, only the counts can change.
        *stored_num = num_keys;
        *stored_len = key_len;
#if DFC_ENABLE_KEY_SETS
        if(app) app->key_storage_len = storage_key_len;
#endif
        return true;
    }

    // Release the old slice first so the space it held can be reused.
    dfc_credential_keys_release(credential, app);

    if(want == 0) {
        *stored_num = num_keys;
        *stored_len = key_len;
        return true;
    }

    if(credential->key_pool_used + want > DFC_KEY_POOL_SIZE) return false;

    *offset = credential->key_pool_used;
    *pool_len = want;
    memset(credential->key_pool + *offset, 0, want);
    credential->key_pool_used += want;
    *stored_num = num_keys;
    *stored_len = key_len;
#if DFC_ENABLE_KEY_SETS
    if(app) app->key_storage_len = storage_key_len;
#endif
    return true;
}

bool dfc_credential_keys_resize(
    DfcCredential* credential,
    DfcApplication* app,
    size_t num_keys,
    size_t key_len) {
#if DFC_ENABLE_KEY_SETS
    if(app) {
        app->num_key_sets = 1;
        app->key_set_initialized[0] = true;
        memset(app->key_set_versions, 0, sizeof(app->key_set_versions));
        memset(app->key_set_types, 0, sizeof(app->key_set_types));
        memset(app->additional_key_versions, 0, sizeof(app->additional_key_versions));
    }
#endif
    return dfc_credential_keys_resize_total(
        credential, app, num_keys, num_keys, key_len, key_len);
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
    if(!dfc_credential_keys_resize_total(
           credential,
           app,
           num_keys,
           num_keys * num_key_sets,
           active_key_len,
           max_key_size))
        return false;
    app->num_key_sets = (uint8_t)num_key_sets;
    app->max_key_size = (uint8_t)max_key_size;
    memset(app->key_set_versions, 0, sizeof(app->key_set_versions));
    memset(app->key_set_types, 0, sizeof(app->key_set_types));
    memset(app->key_set_initialized, 0, sizeof(app->key_set_initialized));
    memset(app->additional_key_versions, 0, sizeof(app->additional_key_versions));
    for(size_t index = 0; index < num_key_sets; index++) app->key_set_initialized[index] = true;
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
    return (uint8_t*)dfc_credential_key_in_set_const(
        credential, app, key_set_number, slot);
}

uint8_t* dfc_credential_key_version_in_set(
    DfcApplication* app,
    size_t key_set_number,
    size_t slot) {
    if(!app || key_set_number >= app->num_key_sets || slot >= app->num_keys) return NULL;
    return key_set_number == 0 ? &app->key_versions[slot] :
                                 &app->additional_key_versions[key_set_number - 1][slot];
}

static void dfc_credential_rotate_key_set_metadata(DfcApplication* app) {
    uint8_t first_versions[DFC_MAX_KEYS];
    memcpy(first_versions, app->key_versions, sizeof(first_versions));
    memcpy(app->key_versions, app->additional_key_versions[0], sizeof(app->key_versions));
    if(app->num_key_sets > DFC_KEY_SET_MINIMUM_COUNT) {
        memmove(
            app->additional_key_versions[0],
            app->additional_key_versions[1],
            (app->num_key_sets - DFC_KEY_SET_MINIMUM_COUNT) * sizeof(app->additional_key_versions[0]));
    }
    memcpy(
        app->additional_key_versions[app->num_key_sets - DFC_KEY_SET_MINIMUM_COUNT],
        first_versions,
        sizeof(first_versions));

    uint8_t first_set_version = app->key_set_versions[0];
    uint8_t first_set_type = app->key_set_types[0];
    bool first_initialized = app->key_set_initialized[0];
    memmove(
        app->key_set_versions,
        app->key_set_versions + 1,
        (app->num_key_sets - 1) * sizeof(app->key_set_versions[0]));
    memmove(
        app->key_set_types,
        app->key_set_types + 1,
        (app->num_key_sets - 1) * sizeof(app->key_set_types[0]));
    memmove(
        app->key_set_initialized,
        app->key_set_initialized + 1,
        (app->num_key_sets - 1) * sizeof(app->key_set_initialized[0]));
    app->key_set_versions[app->num_key_sets - 1] = first_set_version;
    app->key_set_types[app->num_key_sets - 1] = first_set_type;
    app->key_set_initialized[app->num_key_sets - 1] = first_initialized;
}

bool dfc_credential_roll_key_set(
    DfcCredential* credential,
    DfcApplication* app,
    size_t key_set_number) {
    if(!credential || !app || key_set_number == 0 || key_set_number >= app->num_key_sets)
        return false;
    size_t set_length = app->num_keys * dfc_credential_stored_key_length(app->key_storage_len);
    uint8_t first_set[DFC_MAX_KEYS * DFC_MAX_KEY_LEN];
    for(size_t rotation = 0; rotation < key_set_number; rotation++) {
        uint8_t* keys = credential->key_pool + app->key_offset;
        memcpy(first_set, keys, set_length);
        memmove(keys, keys + set_length, (app->num_key_sets - 1) * set_length);
        memcpy(keys + (app->num_key_sets - 1) * set_length, first_set, set_length);
        dfc_credential_rotate_key_set_metadata(app);
    }
    return true;
}
#endif

void dfc_credential_keys_release(DfcCredential* credential, DfcApplication* app) {
    if(!credential) return;
    size_t* offset = key_offset_of(credential, app);
    size_t* pool_len = key_pool_len_of(credential, app);

    size_t old_offset = *offset;
    size_t old_len = *pool_len;
    *offset = DFC_KEY_POOL_NONE;
    *pool_len = 0;
    *num_keys_of(credential, app) = 0;
    *key_len_of(credential, app) = 0;
#if DFC_ENABLE_KEY_SETS
    if(app) app->key_storage_len = 0;
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
    memcpy(credential->uid, loaded->uid, sizeof(credential->uid));
    credential->uid_len = loaded->uid_len;
    credential->picc_key_settings_1 = loaded->picc_key_settings_1;
    credential->picc_key_settings_2 = loaded->picc_key_settings_2;
    credential->picc_auth_command = loaded->picc_auth_command;
    credential->picc_key_offset = loaded->picc_key_offset;
    memcpy(
        credential->picc_key_versions,
        loaded->picc_key_versions,
        sizeof(credential->picc_key_versions));
    credential->picc_num_keys = loaded->picc_num_keys;
    credential->picc_key_len = loaded->picc_key_len;
    credential->card = loaded->card;
    credential->picc_random_id = loaded->picc_random_id;
    credential->picc_format_disabled = loaded->picc_format_disabled;
    memcpy(credential->picc_ats, loaded->picc_ats, sizeof(credential->picc_ats));
    credential->picc_ats_len = loaded->picc_ats_len;
    credential->picc_has_sak = loaded->picc_has_sak;
    credential->picc_sak = loaded->picc_sak;
    credential->picc_has_atqa = loaded->picc_has_atqa;
    memcpy(credential->picc_atqa, loaded->picc_atqa, sizeof(credential->picc_atqa));
    credential->picc_has_sm_disable = loaded->picc_has_sm_disable;
    credential->picc_sm_disable = loaded->picc_sm_disable;
#if DFC_ENABLE_EV2_SECURE_MESSAGING
    credential->picc_has_ev2_capabilities = loaded->picc_has_ev2_capabilities;
    memcpy(
        credential->picc_ev2_capabilities,
        loaded->picc_ev2_capabilities,
        sizeof(credential->picc_ev2_capabilities));
#endif
#if DFC_ENABLE_PROXIMITY_CHECK
    credential->picc_has_proximity_key = loaded->picc_has_proximity_key;
    memcpy(
        credential->picc_proximity_key,
        loaded->picc_proximity_key,
        sizeof(credential->picc_proximity_key));
    credential->picc_proximity_option = loaded->picc_proximity_option;
    credential->picc_proximity_published_response_time =
        loaded->picc_proximity_published_response_time;
    credential->picc_has_proximity_bitrate = loaded->picc_has_proximity_bitrate;
    credential->picc_proximity_bitrate = loaded->picc_proximity_bitrate;
#endif
#if DFC_ENABLE_STATIC_SIGNATURE
    credential->picc_has_static_signature = loaded->picc_has_static_signature;
    memcpy(
        credential->picc_static_signature,
        loaded->picc_static_signature,
        sizeof(credential->picc_static_signature));
#endif
#if DFC_ENABLE_VIRTUAL_CARD
    credential->virtual_card_configured = loaded->virtual_card_configured;
    memcpy(
        credential->virtual_card_installation_id,
        loaded->virtual_card_installation_id,
        sizeof(credential->virtual_card_installation_id));
    credential->virtual_card_installation_id_len = loaded->virtual_card_installation_id_len;
    credential->virtual_card_information = loaded->virtual_card_information;
    memcpy(
        credential->virtual_card_capabilities,
        loaded->virtual_card_capabilities,
        sizeof(credential->virtual_card_capabilities));
    memcpy(
        credential->virtual_card_uid,
        loaded->virtual_card_uid,
        sizeof(credential->virtual_card_uid));
    credential->virtual_card_uid_len = loaded->virtual_card_uid_len;
    memcpy(
        credential->virtual_card_select_mac_key,
        loaded->virtual_card_select_mac_key,
        sizeof(credential->virtual_card_select_mac_key));
    memcpy(
        credential->virtual_card_select_encryption_key,
        loaded->virtual_card_select_encryption_key,
        sizeof(credential->virtual_card_select_encryption_key));
    credential->virtual_card_authentication_mandatory =
        loaded->virtual_card_authentication_mandatory;
    credential->virtual_card_proximity_mandatory = loaded->virtual_card_proximity_mandatory;
#endif
#if DFC_ENABLE_DELEGATED_APPLICATIONS
    credential->picc_has_dam_keys = loaded->picc_has_dam_keys;
    memcpy(
        credential->picc_dam_auth_key,
        loaded->picc_dam_auth_key,
        sizeof(credential->picc_dam_auth_key));
    memcpy(
        credential->picc_dam_mac_key,
        loaded->picc_dam_mac_key,
        sizeof(credential->picc_dam_mac_key));
    memcpy(
        credential->picc_dam_encryption_key,
        loaded->picc_dam_encryption_key,
        sizeof(credential->picc_dam_encryption_key));
#endif
    memcpy(credential->apps, loaded->apps, sizeof(credential->apps));
    credential->num_apps = loaded->num_apps;
    memcpy(credential->files, loaded->files, sizeof(credential->files));
    credential->num_files = loaded->num_files;
    // files[] holds offsets into file_pool, so the pool moves with them.
    memcpy(credential->file_pool, loaded->file_pool, sizeof(credential->file_pool));
    credential->file_pool_used = loaded->file_pool_used;
    // apps[] and the PICC record hold offsets into key_pool, so it moves too.
    memcpy(credential->key_pool, loaded->key_pool, sizeof(credential->key_pool));
    credential->key_pool_used = loaded->key_pool_used;
    credential->dirty = loaded->dirty;
    snprintf(credential->name, sizeof(credential->name), "%s", loaded->name);
}

bool dfc_file_set_data_size(DfcCredential* credential, DfcFile* file, uint32_t size) {
    if(!credential || !file) return false;
    if(!dfc_file_resize(credential, file, size)) return false;
    file->declared_size = size;
    file->contents_complete = true;
    return true;
}

bool dfc_credential_materialize_contents(DfcCredential* credential, uint8_t* scratch, size_t scratch_len) {
    if(!credential || !scratch) return false;
    bool complete = true;
    for(size_t i = 0; i < credential->num_files; i++) {
        DfcFile* file = &credential->files[i];
        size_t want;
        if(file->type == 0x00 || file->type == 0x01) {
            want = file->declared_size;
        } else if(file->type == 0x03 || file->type == 0x04) {
            want = (size_t)file->record_count * (size_t)file->record_size;
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

DfcCredential* dfc_credential_alloc() {
    DfcCredential* credential = malloc(sizeof(DfcCredential));
    memset(credential, 0, sizeof(DfcCredential));
    return credential;
}

bool dfc_credential_clear(DfcCredential* credential) {
    memset(credential, 0, sizeof(DfcCredential));
    credential->picc_key_settings_1 = 0x0F;
    credential->picc_key_settings_2 = DFC_KEY_TYPE_DES_2K3DES | 1;
    credential->picc_auth_command = DFC_CMD_AUTHENTICATE_LEGACY;
    credential->card.generation = DfcGenerationEv1;
    credential->card.storage = DFC_DEFAULT_CARD_STORAGE;
    credential->card.uid_provenance = DfcUidProvenanceReal;
    credential->file_pool_used = 0;
    for(size_t i = 0; i < DFC_MAX_APPS; i++) {
        dfc_credential_reset_application(&credential->apps[i]);
    }
    credential->picc_key_offset = DFC_KEY_POOL_NONE;
    credential->key_pool_used = 0;
    dfc_credential_keys_resize(credential, NULL, 1, 8);
    return true;
}

void dfc_credential_init_blank(DfcCredential* credential) {
    dfc_credential_clear(credential);

    credential->uid_len = DFC_DESFIRE_UID_LEN;
    dfc_random_fill(credential->uid, credential->uid_len);
    credential->uid[0] = DFC_DESFIRE_UID_FIRST_BYTE;

    const uint8_t aid_desfire_order[3] = {0x01, 0x00, 0x00};
    DfcApplication* app = dfc_credential_create_application_desfire_order(
        credential, aid_desfire_order, 0x0F, DFC_KEY_TYPE_DES_2K3DES | 1);
    if(!app) return;

    DfcFile* file = dfc_credential_create_file(credential, 0, 0x00);
    if(!file) return;
    file->number = 0x00;
    file->type = 0x00;
    file->comm_settings = DFC_COMM_PLAIN;
    file->access_rights = 0x0000;
    file->data_offset = DFC_FILE_POOL_NONE;
    file->data_len = 0;
    if(!dfc_file_set_data_size(credential, file, DFC_BLANK_FILE_SIZE)) {
        file->data_offset = DFC_FILE_POOL_NONE;
        file->data_len = 0;
        file->declared_size = 0;
    }
}

void dfc_credential_free(DfcCredential* credential) {
    DFC_ASSERT(credential);
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



