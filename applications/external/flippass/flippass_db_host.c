#include "flippass_db.h"

#include "flippass.h"
#include "kdbx/memzero.h"

#include <string.h>
#include <stdlib.h>

static bool flippass_db_load_entry_field(
    App* app,
    KDBXEntry* entry,
    uint32_t field_mask,
    FuriString* error) {
    const KDBXFieldRef* ref = kdbx_entry_get_field_ref(entry, field_mask);
    char* plain = NULL;
    size_t plain_size = 0U;

    if(ref == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "The selected field is not available.");
        }
        return false;
    }

    if(!kdbx_vault_load_text(app->vault, ref, &plain, &plain_size)) {
        if(error != NULL) {
            furi_string_set_str(error, "The encrypted session vault could not be read.");
        }
        return false;
    }

    const bool ok = kdbx_entry_take_loaded_text(entry, field_mask, plain, plain_size);
    if(ok) {
        return true;
    }

    if(error != NULL) {
        furi_string_set_str(error, "Not enough RAM is available to materialize this entry.");
    }

    memzero(plain, plain_size + 1U);
    free(plain);
    return false;
}

static bool flippass_db_load_custom_field(App* app, KDBXCustomField* field, FuriString* error) {
    const KDBXFieldRef* ref = kdbx_custom_field_get_ref(field);
    char* plain = NULL;
    size_t plain_size = 0U;

    if(ref == NULL || kdbx_vault_ref_is_empty(ref)) {
        if(error != NULL) {
            furi_string_set_str(error, "The selected field is not available.");
        }
        return false;
    }

    if(!kdbx_vault_load_text(app->vault, ref, &plain, &plain_size)) {
        if(error != NULL) {
            furi_string_set_str(error, "The encrypted session vault could not be read.");
        }
        return false;
    }

    const bool ok = kdbx_custom_field_take_loaded_text(field, plain, plain_size);
    if(ok) {
        return true;
    }

    if(error != NULL) {
        furi_string_set_str(error, "Not enough RAM is available to materialize this entry.");
    }

    memzero(plain, plain_size + 1U);
    free(plain);
    return false;
}

static bool flippass_db_load_entry_uuid(App* app, KDBXEntry* entry, FuriString* error) {
    const KDBXFieldRef* ref = kdbx_entry_get_uuid_ref(entry);
    char* plain = NULL;
    size_t plain_size = 0U;

    if(ref == NULL || kdbx_vault_ref_is_empty(ref)) {
        return true;
    }

    if(!kdbx_vault_load_text(app->vault, ref, &plain, &plain_size)) {
        if(error != NULL) {
            furi_string_set_str(error, "The encrypted session vault could not be read.");
        }
        return false;
    }

    const bool ok = kdbx_entry_take_loaded_text(entry, KDBXEntryFieldUuid, plain, plain_size);
    if(ok) {
        return true;
    }

    if(error != NULL) {
        furi_string_set_str(error, "Not enough RAM is available to materialize this entry.");
    }

    memzero(plain, plain_size + 1U);
    free(plain);
    return false;
}

static bool flippass_db_copy_ref_text(
    App* app,
    const KDBXFieldRef* ref,
    FuriString* out,
    FuriString* error) {
    char* plain = NULL;
    size_t plain_size = 0U;

    furi_assert(app);
    furi_assert(out);

    if(ref == NULL || kdbx_vault_ref_is_empty(ref)) {
        furi_string_reset(out);
        return true;
    }

    if(app->vault == NULL || !kdbx_vault_load_text(app->vault, ref, &plain, &plain_size)) {
        if(error != NULL) {
            furi_string_set_str(error, "The encrypted session vault could not be read.");
        }
        return false;
    }

    furi_string_set(out, plain);
    memzero(plain, plain_size + 1U);
    free(plain);
    return true;
}

bool flippass_db_load_with_backend(App* app, KDBXVaultBackend backend, FuriString* error) {
    furi_assert(app);
    furi_assert(error);

    app->requested_vault_backend = backend;
    app->allow_ext_vault_promotion = backend != KDBXVaultBackendRam;
    return flippass_open_execute(app, error);
}

bool flippass_db_load(App* app, FuriString* error) {
    furi_assert(app);
    furi_assert(error);
    return flippass_open_execute(app, error);
}

void flippass_db_deactivate_entry(App* app) {
    furi_assert(app);

    if(app->active_entry != NULL) {
        FLIPPASS_DIAGNOSTIC_LOG(app, "ENTRY_DEMATERIALIZE");
        kdbx_entry_clear_loaded_fields(app->active_entry);
    }

    app->active_entry = NULL;
}

bool flippass_db_activate_entry(App* app, KDBXEntry* entry, bool load_notes, FuriString* error) {
    furi_assert(app);
    furi_assert(entry);

    if(app->vault == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "No database is currently unlocked.");
        }
        return false;
    }

    if(app->active_entry != entry) {
        flippass_db_deactivate_entry(app);
        app->active_entry = entry;
        FLIPPASS_DIAGNOSTIC_LOG(app, "ENTRY_MATERIALIZE");
    }

    app->current_entry = entry;

    if(entry->uuid == NULL && !kdbx_vault_ref_is_empty(kdbx_entry_get_uuid_ref(entry)) &&
       !kdbx_entry_is_loaded(entry, KDBXEntryFieldUuid) &&
       !flippass_db_load_entry_uuid(app, entry, error)) {
        return false;
    }

    if(kdbx_entry_has_field(entry, KDBXEntryFieldUsername) &&
       !kdbx_entry_is_loaded(entry, KDBXEntryFieldUsername) &&
       !flippass_db_load_entry_field(app, entry, KDBXEntryFieldUsername, error)) {
        return false;
    }

    if(kdbx_entry_has_field(entry, KDBXEntryFieldPassword) &&
       !kdbx_entry_is_loaded(entry, KDBXEntryFieldPassword) &&
       !flippass_db_load_entry_field(app, entry, KDBXEntryFieldPassword, error)) {
        return false;
    }

    if(kdbx_entry_has_field(entry, KDBXEntryFieldUrl) &&
       !kdbx_entry_is_loaded(entry, KDBXEntryFieldUrl) &&
       !flippass_db_load_entry_field(app, entry, KDBXEntryFieldUrl, error)) {
        return false;
    }

    if(kdbx_entry_has_field(entry, KDBXEntryFieldAutotype) &&
       !kdbx_entry_is_loaded(entry, KDBXEntryFieldAutotype) &&
       !flippass_db_load_entry_field(app, entry, KDBXEntryFieldAutotype, error)) {
        return false;
    }

    if(load_notes && kdbx_entry_has_field(entry, KDBXEntryFieldNotes) &&
       !kdbx_entry_is_loaded(entry, KDBXEntryFieldNotes) &&
       !flippass_db_load_entry_field(app, entry, KDBXEntryFieldNotes, error)) {
        return false;
    }

    return true;
}

bool flippass_db_ensure_entry_field(
    App* app,
    KDBXEntry* entry,
    uint32_t field_mask,
    FuriString* error) {
    furi_assert(app);
    furi_assert(entry);

    if(!kdbx_entry_has_field(entry, field_mask)) {
        if(error != NULL) {
            furi_string_set_str(error, "The selected entry does not contain that field.");
        }
        return false;
    }

    if(entry != app->active_entry) {
        if(!flippass_db_activate_entry(app, entry, field_mask == KDBXEntryFieldNotes, error)) {
            return false;
        }
    }

    if(kdbx_entry_is_loaded(entry, field_mask)) {
        return true;
    }

    return flippass_db_load_entry_field(app, entry, field_mask, error);
}

bool flippass_db_ensure_custom_field(
    App* app,
    KDBXEntry* entry,
    KDBXCustomField* field,
    FuriString* error) {
    furi_assert(app);
    furi_assert(entry);
    furi_assert(field);

    if(entry != app->active_entry) {
        if(!flippass_db_activate_entry(app, entry, false, error)) {
            return false;
        }
    }

    if(kdbx_custom_field_is_loaded(field)) {
        return true;
    }

    return flippass_db_load_custom_field(app, field, error);
}

bool flippass_db_get_other_field_value(
    App* app,
    KDBXEntry* entry,
    uint32_t field_mask,
    KDBXCustomField* field,
    const char** out_value,
    FuriString* error) {
    furi_assert(app);
    furi_assert(entry);
    furi_assert(out_value);

    *out_value = NULL;

    if(field != NULL) {
        if(!flippass_db_ensure_custom_field(app, entry, field, error)) {
            return false;
        }

        *out_value = field->value;
        return true;
    }

    if(field_mask == 0U) {
        if(error != NULL) {
            furi_string_set_str(error, "The selected field is not available.");
        }
        return false;
    }

    if(!flippass_db_ensure_entry_field(app, entry, field_mask, error)) {
        return false;
    }

    switch(field_mask) {
    case KDBXEntryFieldUrl:
        *out_value = entry->url;
        return true;
    case KDBXEntryFieldNotes:
        *out_value = entry->notes;
        return true;
    case KDBXEntryFieldUsername:
        *out_value = entry->username;
        return true;
    case KDBXEntryFieldPassword:
        *out_value = entry->password;
        return true;
    case KDBXEntryFieldAutotype:
        *out_value = entry->autotype_sequence;
        return true;
    default:
        if(error != NULL) {
            furi_string_set_str(error, "The selected field is not available.");
        }
        return false;
    }
}

bool flippass_db_entry_has_field(const KDBXEntry* entry, uint32_t field_mask) {
    return kdbx_entry_has_field(entry, field_mask);
}

const KDBXCustomField* flippass_db_entry_get_custom_fields(const KDBXEntry* entry) {
    return kdbx_entry_get_custom_fields(entry);
}

bool flippass_db_copy_entry_uuid(
    App* app,
    const KDBXEntry* entry,
    FuriString* out,
    FuriString* error) {
    furi_assert(app);
    furi_assert(entry);
    furi_assert(out);

    if(entry->uuid != NULL) {
        furi_string_set(out, entry->uuid);
        return true;
    }

    return flippass_db_copy_ref_text(app, kdbx_entry_get_uuid_ref(entry), out, error);
}

bool flippass_db_copy_entry_title(
    App* app,
    const KDBXEntry* entry,
    FuriString* out,
    FuriString* error) {
    UNUSED(app);
    UNUSED(error);
    furi_assert(entry);
    furi_assert(out);

    furi_string_set_str(out, entry->title != NULL ? entry->title : "");
    return true;
}

KDBXVaultBackend flippass_db_parse_backend_hint(const char* text) {
    if(text == NULL || text[0] == '\0' || strcmp(text, "ram") == 0) {
        return KDBXVaultBackendRam;
    }

    if(strcmp(text, "int") == 0 || strcmp(text, "internal") == 0) {
        return KDBXVaultBackendFileInt;
    }

    if(strcmp(text, "ext") == 0 || strcmp(text, "external") == 0 || strcmp(text, "sd") == 0) {
        return KDBXVaultBackendFileExt;
    }

    return KDBXVaultBackendNone;
}
