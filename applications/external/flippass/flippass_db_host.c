#include "flippass_db.h"

#include "flippass.h"
#include "kdbx/memzero.h"

#include <string.h>
#include <stdlib.h>

/* Larger than the open-builder default (256 B) to halve the number of heap
 * calls when building a new-database arena from scratch. Each malloc is a
 * potential fragmentation interleave point; fewer calls means less scattered layout. */
#define FLIPPASS_DB_MUTABLE_ARENA_CHUNK_SIZE 512U

/* Total free-heap headroom kept for ephemeral allocations during mutation
 * (FuriStrings, format buffers, CRUD plugin workspace, etc.). */
#define FLIPPASS_DB_MUTATION_EPHEMERAL_RESERVE_BYTES (4U * 1024U)

/* When max_free_block drops below this threshold at the start of a mutation,
 * proactively promote the RAM vault to the /ext session file.  Promotion frees
 * all vault RAM pages (typically 8-15 KiB) and creates a contiguous free region
 * that the mutation can then use for arena chunks without fragmenting the save
 * block.  Must be > FLIPPASS_DB_SAVE_MIN_MAX_FREE_BYTES + expected per-mutation
 * arena growth so the promotion fires before the budget guard blocks writes. */
#define FLIPPASS_DB_PROACTIVE_PROMOTE_THRESHOLD_BYTES (20U * 1024U)

static bool flippass_db_load_entry_title_into_cache(App* app, KDBXEntry* entry, FuriString* error);

static size_t flippass_db_model_committed_bytes(const App* app) {
    size_t committed = 0U;

    furi_assert(app);

    if(app->db_arena != NULL) {
        committed += kdbx_arena_bytes(app->db_arena);
    }
    if(app->vault != NULL) {
        committed += kdbx_vault_index_bytes(app->vault);
        committed += kdbx_vault_page_bytes(app->vault);
    }

    return committed;
}

void flippass_db_refresh_model_budget(App* app) {
    furi_assert(app);

    app->model_budget_committed = flippass_db_model_committed_bytes(app);
    const size_t free_heap = memmgr_get_free_heap();
    const size_t max_free = memmgr_heap_get_max_free_block();

    /* Guard 1: keep total free headroom for ephemeral allocations. */
    const size_t free_room = (free_heap > FLIPPASS_DB_MUTATION_EPHEMERAL_RESERVE_BYTES) ?
                                 (free_heap - FLIPPASS_DB_MUTATION_EPHEMERAL_RESERVE_BYTES) :
                                 0U;

    /* Guard 2: keep the largest contiguous block above the save minimum so
     * that loading the save stages or the editor (each section is one large
     * contiguous malloc) can succeed regardless of heap fragmentation state. */
    const size_t block_room = (max_free > FLIPPASS_DB_SAVE_MIN_MAX_FREE_BYTES) ?
                                  (max_free - FLIPPASS_DB_SAVE_MIN_MAX_FREE_BYTES) :
                                  0U;

    const size_t growth = (free_room < block_room) ? free_room : block_room;
    app->model_budget_limit = app->model_budget_committed + growth;

    if(app->db_arena != NULL) {
        kdbx_arena_set_budget(
            app->db_arena, &app->model_budget_committed, app->model_budget_limit);
    }
    if(app->vault != NULL) {
        kdbx_vault_set_budget(app->vault, &app->model_budget_committed, app->model_budget_limit);
    }
}

static void flippass_db_trim_for_model_mutation(App* app) {
    furi_assert(app);

    flippass_db_deactivate_entry(app);
    flippass_db_browser_clear_title_cache(app);
}

static bool flippass_db_can_promote_vault_for_mutation(const App* app) {
    if(app == NULL || app->vault == NULL ||
       kdbx_vault_get_backend(app->vault) != KDBXVaultBackendRam ||
       !kdbx_vault_backend_supported(KDBXVaultBackendFileExt)) {
        return false;
    }

    return app->allow_ext_vault_promotion || app->always_allow_ext;
}

static bool flippass_db_promote_vault_for_mutation(App* app, FuriString* error) {
    KDBXVault* target = NULL;

    furi_assert(app);

    if(!flippass_db_can_promote_vault_for_mutation(app)) {
        return false;
    }

    FLIPPASS_PROBE_LOG_EVENT(
        app,
        "MUTATION_VAULT_PROMOTE_START committed=%lu limit=%lu free=%lu max=%lu",
        (unsigned long)app->model_budget_committed,
        (unsigned long)app->model_budget_limit,
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());

    target = kdbx_vault_alloc(KDBXVaultBackendFileExt, NULL, 0U);
    if(target == NULL || kdbx_vault_storage_failed(target)) {
        if(target != NULL) {
            kdbx_vault_free(target);
        }
        if(error != NULL) {
            furi_string_set_str(
                error, "The encrypted RAM vault could not continue on the SD-card session file.");
        }
        return false;
    }

    if(!kdbx_vault_promote_ram_to_file(app->vault, target)) {
        const char* reason = kdbx_vault_failure_reason(target);
        kdbx_vault_free(target);
        if(error != NULL) {
            furi_string_set_str(
                error,
                (reason != NULL && strcmp(reason, "none") != 0) ?
                    reason :
                    "The encrypted RAM vault could not continue on the SD-card session file.");
        }
        return false;
    }

    kdbx_vault_free(app->vault);
    app->vault = target;
    app->active_vault_backend = KDBXVaultBackendFileExt;
    app->allow_ext_vault_promotion = true;
    flippass_db_refresh_model_budget(app);

    FLIPPASS_PROBE_LOG_EVENT(
        app,
        "MUTATION_VAULT_PROMOTE_OK committed=%lu limit=%lu free=%lu max=%lu",
        (unsigned long)app->model_budget_committed,
        (unsigned long)app->model_budget_limit,
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());
    return true;
}

/* Proactively promote the session vault from RAM to the /ext file when heap
 * pressure is detected.  Moving vault pages off-heap frees a contiguous region
 * that future arena chunks can occupy without fragmenting the save block.
 * Best-effort: silently skips if the vault is already file-backed, if /ext is
 * unavailable, or if promotion is not approved for this session. */
void flippass_db_proactive_promote_if_needed(App* app) {
    furi_assert(app);

    if(memmgr_heap_get_max_free_block() < FLIPPASS_DB_PROACTIVE_PROMOTE_THRESHOLD_BYTES) {
        flippass_db_promote_vault_for_mutation(app, NULL);
    }
}

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

static bool
    flippass_db_base64_encode(const uint8_t* data, size_t data_size, char* out, size_t out_size) {
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t out_index = 0U;

    furi_assert(data);
    furi_assert(out);

    for(size_t index = 0U; index < data_size; index += 3U) {
        const size_t remaining = data_size - index;
        const uint32_t triple = ((uint32_t)data[index] << 16) |
                                ((remaining > 1U ? data[index + 1U] : 0U) << 8) |
                                (remaining > 2U ? data[index + 2U] : 0U);

        if((out_index + 4U) >= out_size) {
            return false;
        }

        out[out_index++] = alphabet[(triple >> 18) & 0x3FU];
        out[out_index++] = alphabet[(triple >> 12) & 0x3FU];
        out[out_index++] = (remaining > 1U) ? alphabet[(triple >> 6) & 0x3FU] : '=';
        out[out_index++] = (remaining > 2U) ? alphabet[triple & 0x3FU] : '=';
    }

    if(out_index >= out_size) {
        return false;
    }

    out[out_index] = '\0';
    return true;
}

static bool flippass_db_generate_uuid_base64(char out[25]) {
    uint8_t uuid[16];

    furi_hal_random_fill_buf(uuid, sizeof(uuid));
    const bool ok = flippass_db_base64_encode(uuid, sizeof(uuid), out, 25U);
    memzero(uuid, sizeof(uuid));
    return ok;
}

static bool flippass_db_write_secret_field(
    App* app,
    const char* value,
    KDBXFieldRef* out_ref,
    FuriString* error) {
    KDBXFieldRef ref = {0};
    uint8_t pending[KDBX_VAULT_RECORD_PLAIN_MAX];
    KDBXVaultWriter writer;
    bool retried_after_promotion = false;

    furi_assert(app);
    furi_assert(out_ref);

    if(value == NULL || value[0] == '\0') {
        *out_ref = ref;
        return true;
    }

    if(app->vault == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "The editable session vault is not available.");
        }
        return false;
    }

retry:
    kdbx_vault_writer_reset_with_pending(&writer, app->vault, pending, sizeof(pending));
    if(!kdbx_vault_writer_write(&writer, (const uint8_t*)value, strlen(value)) ||
       !kdbx_vault_writer_finish(&writer, &ref)) {
        kdbx_vault_writer_abort(&writer);
        if(!retried_after_promotion && flippass_db_promote_vault_for_mutation(app, NULL)) {
            retried_after_promotion = true;
            memset(&ref, 0, sizeof(ref));
            goto retry;
        }
        if(error != NULL) {
            furi_string_set_str(
                error,
                kdbx_vault_failure_reason(app->vault) != NULL ?
                    kdbx_vault_failure_reason(app->vault) :
                    "The editable session vault could not store the updated field.");
        }
        return false;
    }

    *out_ref = ref;
    return true;
}

static bool flippass_db_uses_deferred_uuid_refs(const App* app) {
    return app != NULL && app->vault != NULL &&
           kdbx_vault_get_backend(app->vault) != KDBXVaultBackendRam;
}

static bool flippass_db_set_group_uuid_model(
    App* app,
    KDBXGroup* group,
    const char* uuid,
    FuriString* error) {
    KDBXFieldRef ref = {0};

    furi_assert(app);
    furi_assert(group);

    if(flippass_db_uses_deferred_uuid_refs(app)) {
        if(!flippass_db_write_secret_field(app, uuid, &ref, error) ||
           !kdbx_group_set_uuid_ref(group, &ref)) {
            if(error != NULL && furi_string_empty(error)) {
                furi_string_set_str(error, "FlipPass could not initialize the group UUID.");
            }
            return false;
        }
        group->uuid = NULL;
        return true;
    }

    if(kdbx_group_set_uuid(group, app->db_arena, uuid)) {
        return true;
    }

    if(flippass_db_promote_vault_for_mutation(app, NULL) &&
       flippass_db_uses_deferred_uuid_refs(app)) {
        if(flippass_db_write_secret_field(app, uuid, &ref, error) &&
           kdbx_group_set_uuid_ref(group, &ref)) {
            group->uuid = NULL;
            return true;
        }
    }

    if(error != NULL && furi_string_empty(error)) {
        furi_string_set_str(error, "FlipPass could not initialize the group UUID.");
    }
    return false;
}

static bool flippass_db_set_entry_uuid_model(
    App* app,
    KDBXEntry* entry,
    const char* uuid,
    FuriString* error) {
    KDBXFieldRef ref = {0};

    furi_assert(app);
    furi_assert(entry);

    if(flippass_db_uses_deferred_uuid_refs(app)) {
        if(!flippass_db_write_secret_field(app, uuid, &ref, error) ||
           !kdbx_entry_set_uuid_ref(entry, &ref)) {
            if(error != NULL && furi_string_empty(error)) {
                furi_string_set_str(error, "FlipPass could not initialize the entry UUID.");
            }
            return false;
        }
        entry->uuid = NULL;
        return true;
    }

    if(kdbx_entry_set_uuid(entry, app->db_arena, uuid)) {
        return true;
    }

    if(flippass_db_promote_vault_for_mutation(app, NULL) &&
       flippass_db_uses_deferred_uuid_refs(app)) {
        if(flippass_db_write_secret_field(app, uuid, &ref, error) &&
           kdbx_entry_set_uuid_ref(entry, &ref)) {
            entry->uuid = NULL;
            return true;
        }
    }

    if(error != NULL && furi_string_empty(error)) {
        furi_string_set_str(error, "FlipPass could not initialize the entry UUID.");
    }
    return false;
}

static bool flippass_db_set_group_name_model(
    App* app,
    KDBXGroup* group,
    const char* name,
    FuriString* error) {
    furi_assert(app);
    furi_assert(group);

    if(kdbx_group_set_name(group, app->db_arena, name != NULL ? name : "")) {
        return true;
    }

    if(flippass_db_promote_vault_for_mutation(app, NULL) &&
       kdbx_group_set_name(group, app->db_arena, name != NULL ? name : "")) {
        return true;
    }

    if(error != NULL && furi_string_empty(error)) {
        furi_string_set_str(error, "FlipPass could not allocate the group name.");
    }
    return false;
}

static void flippass_db_append_group(KDBXGroup* parent, KDBXGroup* group) {
    furi_assert(parent);
    furi_assert(group);

    group->parent = parent;
    group->next = NULL;
    if(parent->children == NULL) {
        parent->children = group;
        return;
    }

    KDBXGroup* tail = parent->children;
    while(tail->next != NULL) {
        tail = tail->next;
    }
    tail->next = group;
}

static void flippass_db_append_entry(KDBXGroup* parent, KDBXEntry* entry) {
    furi_assert(parent);
    furi_assert(entry);

    entry->next = NULL;
    if(parent->entries == NULL) {
        parent->entries = entry;
        return;
    }

    KDBXEntry* tail = parent->entries;
    while(tail->next != NULL) {
        tail = tail->next;
    }
    tail->next = entry;
}

static bool flippass_db_is_standard_entry_key(const char* key) {
    return key != NULL &&
           (strcmp(key, "Title") == 0 || strcmp(key, "UserName") == 0 ||
            strcmp(key, "Password") == 0 || strcmp(key, "URL") == 0 || strcmp(key, "Notes") == 0 ||
            strcmp(key, "UUID") == 0 || strcmp(key, "AutoType") == 0);
}

static KDBXCustomField* flippass_db_find_custom_field_by_key(
    KDBXEntry* entry,
    const char* key,
    const KDBXCustomField* exclude) {
    if(entry == NULL || key == NULL || key[0] == '\0') {
        return NULL;
    }

    for(KDBXCustomField* field = entry->custom_fields; field != NULL; field = field->next) {
        if(field != exclude && field->key != NULL && strcmp(field->key, key) == 0) {
            return field;
        }
    }

    return NULL;
}

static bool flippass_db_set_entry_field(
    App* app,
    KDBXEntry* entry,
    uint32_t field_mask,
    const char* value,
    FuriString* error) {
    KDBXFieldRef ref = {0};

    furi_assert(app);
    furi_assert(entry);

    if(!flippass_db_write_secret_field(app, value, &ref, error)) {
        return false;
    }

    if(!kdbx_entry_set_field_ref(entry, field_mask, &ref)) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass could not update the edited field reference.");
        }
        return false;
    }

    if(!kdbx_entry_set_loaded_text(entry, field_mask, NULL, 0U)) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass could not clear the replaced plaintext field.");
        }
        return false;
    }

    return true;
}

static bool flippass_db_validate_custom_field_name(
    KDBXEntry* entry,
    const char* name,
    const KDBXCustomField* exclude,
    FuriString* error) {
    if(name == NULL || name[0] == '\0') {
        if(error != NULL) {
            furi_string_set_str(error, "Field name is required.");
        }
        return false;
    }

    if(flippass_db_is_standard_entry_key(name)) {
        if(error != NULL) {
            furi_string_set_str(error, "Use a custom field name.");
        }
        return false;
    }

    if(flippass_db_find_custom_field_by_key(entry, name, exclude) != NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "A field with that name already exists.");
        }
        return false;
    }

    return true;
}

static bool flippass_db_validate_custom_field_value(const char* value, FuriString* error) {
    if(value == NULL || value[0] == '\0') {
        if(error != NULL) {
            furi_string_set_str(error, "Field value is required.");
        }
        return false;
    }

    return true;
}

static bool flippass_db_prepare_editable_session(App* app, FuriString* error) {
    furi_assert(app);

    flippass_db_trim_for_model_mutation(app);

    /* Safety-net promotion: if the call site did not already promote (e.g. the
     * mutation was triggered outside the normal editor commit path), try here.
     * With the CRUD plugin already in memory the window is smaller, but vault
     * page freeing still provides meaningful headroom for this mutation. */
    flippass_db_proactive_promote_if_needed(app);

    if(app->db_arena == NULL) {
        app->db_arena = kdbx_arena_alloc(FLIPPASS_DB_MUTABLE_ARENA_CHUNK_SIZE, NULL, 0U);
        if(app->db_arena == NULL) {
            if(error != NULL) {
                furi_string_set_str(error, "Not enough RAM is available for the editable model.");
            }
            return false;
        }
    }

    if(app->vault == NULL) {
        app->vault = kdbx_vault_alloc(KDBXVaultBackendRam, NULL, 0U);
        if(app->vault == NULL) {
            if(error != NULL) {
                furi_string_set_str(error, "Not enough RAM is available for the editable vault.");
            }
            return false;
        }
        app->active_vault_backend = KDBXVaultBackendRam;
    }

    flippass_db_refresh_model_budget(app);
    return true;
}

bool flippass_db_load_with_backend(App* app, KDBXVaultBackend backend, FuriString* error) {
    furi_assert(app);
    furi_assert(error);

    app->requested_vault_backend = backend;
    app->allow_ext_vault_promotion = app->always_allow_ext || backend != KDBXVaultBackendRam;
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

    if(entry->title == NULL && kdbx_entry_has_field(entry, KDBXEntryFieldTitle) &&
       !flippass_db_load_entry_title_into_cache(app, entry, error)) {
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

bool flippass_db_copy_group_uuid(
    App* app,
    const KDBXGroup* group,
    FuriString* out,
    FuriString* error) {
    furi_assert(app);
    furi_assert(group);
    furi_assert(out);

    if(group->uuid != NULL) {
        furi_string_set_str(out, group->uuid);
        return true;
    }

    return flippass_db_copy_ref_text(app, kdbx_group_get_uuid_ref(group), out, error);
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

/* Heap headroom kept free before admitting an extra group's titles into the
 * cache window. Tuned to the 8 KiB stack reserve baseline; falls in line with
 * the open-path field allocator that already keeps 8 KiB headroom. */
#define FLIPPASS_TITLE_CACHE_HEADROOM_BYTES (8U * 1024U)

static bool
    flippass_db_load_entry_title_into_cache(App* app, KDBXEntry* entry, FuriString* error) {
    const KDBXFieldRef* ref = kdbx_entry_get_field_ref(entry, KDBXEntryFieldTitle);
    char* plain = NULL;
    size_t plain_size = 0U;

    if(ref == NULL || kdbx_vault_ref_is_empty(ref)) {
        return true;
    }

    if(app->vault == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "The encrypted session vault is not available.");
        }
        return false;
    }

    if(!kdbx_vault_load_text(app->vault, ref, &plain, &plain_size)) {
        if(error != NULL) {
            furi_string_set_str(error, "The encrypted session vault could not be read.");
        }
        return false;
    }

    if(!kdbx_entry_take_loaded_text(entry, KDBXEntryFieldTitle, plain, plain_size)) {
        if(error != NULL) {
            furi_string_set_str(error, "Not enough RAM is available to materialize this title.");
        }
        memzero(plain, plain_size + 1U);
        free(plain);
        return false;
    }

    return true;
}

bool flippass_db_ensure_entry_title(App* app, KDBXEntry* entry, FuriString* error) {
    furi_assert(app);
    furi_assert(entry);

    if(entry->title != NULL) {
        return true;
    }

    return flippass_db_load_entry_title_into_cache(app, entry, error);
}

void flippass_db_clear_entry_title_cached(KDBXEntry* entry) {
    if(entry == NULL) {
        return;
    }
    kdbx_entry_clear_loaded_field_mask(entry, KDBXEntryFieldTitle);
}

bool flippass_db_copy_entry_title(
    App* app,
    const KDBXEntry* entry,
    FuriString* out,
    FuriString* error) {
    furi_assert(entry);
    furi_assert(out);

    if(entry->title != NULL) {
        furi_string_set_str(out, entry->title);
        return true;
    }

    /* Lazy materialize: the entry may be in a group outside the title cache
     * window. Borrow the entry's title cache for this one read; the caller
     * keeps the FuriString copy after we return. */
    if(!flippass_db_ensure_entry_title(app, (KDBXEntry*)entry, error)) {
        furi_string_reset(out);
        return false;
    }

    furi_string_set_str(out, entry->title != NULL ? entry->title : "");
    return true;
}

static bool flippass_db_title_cache_keep_group(
    KDBXGroup* group,
    KDBXGroup* keep_current,
    KDBXGroup* keep_previous,
    KDBXGroup* keep_children_of) {
    if(group == NULL) {
        return false;
    }
    if(group == keep_current || group == keep_previous) {
        return true;
    }
    if(keep_children_of != NULL) {
        for(KDBXGroup* child = keep_children_of->children; child != NULL; child = child->next) {
            if(child == group) {
                return true;
            }
        }
    }
    return false;
}

static bool flippass_db_title_cache_walk_evict(
    KDBXGroup* group,
    KDBXGroup* keep_current,
    KDBXGroup* keep_previous,
    KDBXGroup* keep_children_of) {
    if(group == NULL) {
        return false;
    }

    bool released = false;
    for(KDBXGroup* node = group; node != NULL; node = kdbx_group_next_in_subtree(node, group)) {
        const bool keep_titles = flippass_db_title_cache_keep_group(
            node, keep_current, keep_previous, keep_children_of);
        if(!keep_titles) {
            for(KDBXEntry* entry = node->entries; entry != NULL; entry = entry->next) {
                const bool was_loaded = entry->title != NULL;
                kdbx_entry_clear_loaded_field_mask(entry, KDBXEntryFieldTitle);
                released |= was_loaded && entry->title == NULL;
            }
        }
    }
    return released;
}

static void flippass_db_title_cache_load_group(App* app, KDBXGroup* group, bool gate_on_heap) {
    if(group == NULL) {
        return;
    }

    for(KDBXEntry* entry = group->entries; entry != NULL; entry = entry->next) {
        if(gate_on_heap &&
           memmgr_heap_get_max_free_block() < FLIPPASS_TITLE_CACHE_HEADROOM_BYTES) {
            return;
        }
        if(entry->title != NULL) {
            continue;
        }
        if(!flippass_db_load_entry_title_into_cache(app, entry, NULL)) {
            /* Best-effort prefetch; if a single title can't fit, stop trying
             * to admit more from this group instead of thrashing the vault. */
            return;
        }
    }
}

void flippass_db_browser_enter_group(App* app, KDBXGroup* new_current, KDBXGroup* old_current) {
    furi_assert(app);

    /* The new "previous" is whatever group we just left, unless the navigation
     * was a no-op or a tree reset. */
    KDBXGroup* new_previous = (old_current != new_current) ? old_current : NULL;

    if(app->root_group != NULL) {
        flippass_db_title_cache_walk_evict(
            app->root_group, new_current, new_previous, new_current);
    }

    /* Mandatory: the current group must be browsable. If even this fails,
     * fall through silently; the browser falls back to "Title unavailable"
     * for the offending entry on its own. */
    flippass_db_title_cache_load_group(app, new_current, false);

    /* Best-effort window: previous group, then children of the new current. */
    if(new_previous != NULL && new_previous != new_current) {
        flippass_db_title_cache_load_group(app, new_previous, true);
    }
    if(new_current != NULL) {
        for(KDBXGroup* child = new_current->children; child != NULL; child = child->next) {
            if(memmgr_heap_get_max_free_block() < FLIPPASS_TITLE_CACHE_HEADROOM_BYTES) {
                break;
            }
            flippass_db_title_cache_load_group(app, child, true);
        }
    }

    app->title_cache_previous = new_previous;
}

bool flippass_db_browser_clear_title_cache(App* app) {
    furi_assert(app);
    const bool released = flippass_db_title_cache_walk_evict(app->root_group, NULL, NULL, NULL);
    app->title_cache_previous = NULL;
    return released;
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

void flippass_db_mark_clean(App* app) {
    furi_assert(app);

    app->database_dirty = false;
    app->database_new = false;
}

void flippass_db_mark_dirty(App* app) {
    furi_assert(app);
    app->database_dirty = true;
}

bool flippass_db_create_new_database(
    App* app,
    const char* root_name,
    FlipPassKdbxCipher cipher,
    uint32_t compression,
    FuriString* error) {
    char uuid_base64[25];

    furi_assert(app);

    flippass_reset_database(app);
    if(!flippass_db_prepare_editable_session(app, error)) {
        return false;
    }

    app->root_group = kdbx_group_alloc(app->db_arena);
    if(app->root_group == NULL && flippass_db_promote_vault_for_mutation(app, NULL)) {
        app->root_group = kdbx_group_alloc(app->db_arena);
    }
    if(app->root_group == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "Not enough RAM is available for the root group.");
        }
        return false;
    }

    kdbx_group_reset(app->root_group);
    if(!flippass_db_generate_uuid_base64(uuid_base64) ||
       !flippass_db_set_group_uuid_model(app, app->root_group, uuid_base64, error) ||
       !flippass_db_set_group_name_model(
           app,
           app->root_group,
           (root_name != NULL && root_name[0] != '\0') ? root_name : "Root",
           error)) {
        if(error != NULL) {
            if(furi_string_empty(error)) {
                furi_string_set_str(error, "FlipPass could not initialize the new database root.");
            }
        }
        return false;
    }

    app->current_group = app->root_group;
    app->active_group = app->root_group;
    app->database_loaded = true;
    app->parse_failed = false;
    app->database_cipher = cipher;
    app->database_compression = compression;
    app->database_new = true;
    app->database_dirty = true;
    return true;
}

bool flippass_db_create_group(
    App* app,
    KDBXGroup* parent,
    const char* name,
    KDBXGroup** out_group,
    FuriString* error) {
    char uuid_base64[25];
    KDBXGroup* group = NULL;

    furi_assert(app);
    furi_assert(parent);

    if(out_group != NULL) {
        *out_group = NULL;
    }

    if(!flippass_db_prepare_editable_session(app, error)) {
        return false;
    }

    group = kdbx_group_alloc(app->db_arena);
    if(group == NULL && flippass_db_promote_vault_for_mutation(app, NULL)) {
        group = kdbx_group_alloc(app->db_arena);
    }
    if(group == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "Not enough RAM is available for the new group.");
        }
        return false;
    }

    kdbx_group_reset(group);
    if(!flippass_db_generate_uuid_base64(uuid_base64) ||
       !flippass_db_set_group_uuid_model(app, group, uuid_base64, error) ||
       !flippass_db_set_group_name_model(app, group, name != NULL ? name : "", error)) {
        if(error != NULL) {
            if(furi_string_empty(error)) {
                furi_string_set_str(error, "FlipPass could not allocate the new group.");
            }
        }
        return false;
    }

    flippass_db_append_group(parent, group);
    flippass_db_mark_dirty(app);
    if(out_group != NULL) {
        *out_group = group;
    }
    return true;
}

bool flippass_db_update_group(App* app, KDBXGroup* group, const char* name, FuriString* error) {
    furi_assert(app);
    furi_assert(group);

    if(!flippass_db_prepare_editable_session(app, error)) {
        return false;
    }

    if(!flippass_db_set_group_name_model(app, group, name != NULL ? name : "", error)) {
        if(error != NULL) {
            if(furi_string_empty(error)) {
                furi_string_set_str(error, "FlipPass could not update the selected group.");
            }
        }
        return false;
    }

    flippass_db_mark_dirty(app);
    return true;
}

bool flippass_db_create_entry(
    App* app,
    KDBXGroup* parent,
    const char* title,
    const char* username,
    const char* password,
    const char* url,
    const char* notes,
    const char* autotype,
    KDBXEntry** out_entry,
    FuriString* error) {
    char uuid_base64[25];
    KDBXEntry* entry = NULL;

    furi_assert(app);
    furi_assert(parent);

    if(out_entry != NULL) {
        *out_entry = NULL;
    }

    if(!flippass_db_prepare_editable_session(app, error)) {
        return false;
    }

    entry = kdbx_entry_alloc(app->db_arena);
    if(entry == NULL && flippass_db_promote_vault_for_mutation(app, NULL)) {
        entry = kdbx_entry_alloc(app->db_arena);
    }
    if(entry == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "Not enough RAM is available for the new entry.");
        }
        return false;
    }

    kdbx_entry_reset(entry);
    if(!flippass_db_generate_uuid_base64(uuid_base64) ||
       !flippass_db_set_entry_uuid_model(app, entry, uuid_base64, error)) {
        if(error != NULL) {
            if(furi_string_empty(error)) {
                furi_string_set_str(error, "FlipPass could not initialize the new entry.");
            }
        }
        return false;
    }

    if(!flippass_db_set_entry_field(app, entry, KDBXEntryFieldTitle, title, error) ||
       !flippass_db_set_entry_field(app, entry, KDBXEntryFieldUsername, username, error) ||
       !flippass_db_set_entry_field(app, entry, KDBXEntryFieldPassword, password, error) ||
       !flippass_db_set_entry_field(app, entry, KDBXEntryFieldUrl, url, error) ||
       !flippass_db_set_entry_field(app, entry, KDBXEntryFieldNotes, notes, error) ||
       !flippass_db_set_entry_field(app, entry, KDBXEntryFieldAutotype, autotype, error)) {
        return false;
    }

    flippass_db_append_entry(parent, entry);
    flippass_db_mark_dirty(app);
    if(out_entry != NULL) {
        *out_entry = entry;
    }
    return true;
}

bool flippass_db_update_entry(
    App* app,
    KDBXEntry* entry,
    const char* title,
    const char* username,
    const char* password,
    const char* url,
    const char* notes,
    const char* autotype,
    FuriString* error) {
    furi_assert(app);
    furi_assert(entry);

    if(!flippass_db_prepare_editable_session(app, error)) {
        return false;
    }

    if(!flippass_db_set_entry_field(app, entry, KDBXEntryFieldTitle, title, error) ||
       !flippass_db_set_entry_field(app, entry, KDBXEntryFieldUsername, username, error) ||
       !flippass_db_set_entry_field(app, entry, KDBXEntryFieldPassword, password, error) ||
       !flippass_db_set_entry_field(app, entry, KDBXEntryFieldUrl, url, error) ||
       !flippass_db_set_entry_field(app, entry, KDBXEntryFieldNotes, notes, error) ||
       !flippass_db_set_entry_field(app, entry, KDBXEntryFieldAutotype, autotype, error)) {
        if(error != NULL && furi_string_empty(error)) {
            furi_string_set_str(error, "FlipPass could not update the selected entry.");
        }
        return false;
    }

    flippass_db_mark_dirty(app);
    return true;
}

bool flippass_db_delete_group(App* app, KDBXGroup* group, FuriString* error) {
    furi_assert(app);

    if(group == NULL || group == app->root_group || group->parent == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "The root group cannot be deleted.");
        }
        return false;
    }

    KDBXGroup* parent = group->parent;
    KDBXGroup** link = &parent->children;
    while(*link != NULL && *link != group) {
        link = &(*link)->next;
    }

    if(*link != group) {
        if(error != NULL) {
            furi_string_set_str(error, "The selected group is no longer in this database.");
        }
        return false;
    }

    if(app->active_entry != NULL) {
        flippass_db_deactivate_entry(app);
    }
    if(app->current_group == group || app->active_group == group) {
        app->current_group = parent;
        app->active_group = parent;
    }

    *link = group->next;
    group->parent = NULL;
    group->next = NULL;
    flippass_db_mark_dirty(app);
    return true;
}

bool flippass_db_delete_entry(App* app, KDBXEntry* entry, FuriString* error) {
    furi_assert(app);

    if(entry == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "The selected entry is no longer in this database.");
        }
        return false;
    }

    KDBXGroup* group = app->editor_group != NULL ? app->editor_group : app->current_group;
    KDBXEntry** link = group != NULL ? &group->entries : NULL;
    while(link != NULL && *link != NULL && *link != entry) {
        link = &(*link)->next;
    }

    if(link == NULL || *link != entry) {
        if(error != NULL) {
            furi_string_set_str(error, "The selected entry is no longer in this group.");
        }
        return false;
    }

    if(app->active_entry == entry) {
        flippass_db_deactivate_entry(app);
    }
    if(app->current_entry == entry) {
        app->current_entry = NULL;
    }

    *link = entry->next;
    entry->next = NULL;
    flippass_db_mark_dirty(app);
    return true;
}

bool flippass_db_create_custom_field(
    App* app,
    KDBXEntry* entry,
    const char* name,
    const char* value,
    bool protected_value,
    KDBXCustomField** out_field,
    FuriString* error) {
    KDBXFieldRef ref = {0};
    KDBXCustomField* field = NULL;

    furi_assert(app);
    furi_assert(entry);

    if(out_field != NULL) {
        *out_field = NULL;
    }

    if(!flippass_db_validate_custom_field_name(entry, name, NULL, error) ||
       !flippass_db_validate_custom_field_value(value, error) ||
       !flippass_db_prepare_editable_session(app, error) ||
       !flippass_db_write_secret_field(app, value, &ref, error)) {
        return false;
    }

    field = kdbx_entry_add_custom_field_ex(entry, app->db_arena, name, &ref, protected_value);
    if(field == NULL && flippass_db_promote_vault_for_mutation(app, NULL)) {
        field = kdbx_entry_add_custom_field_ex(entry, app->db_arena, name, &ref, protected_value);
    }
    if(field == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass could not allocate the custom field.");
        }
        return false;
    }

    flippass_db_mark_dirty(app);
    if(out_field != NULL) {
        *out_field = field;
    }
    return true;
}

bool flippass_db_update_custom_field(
    App* app,
    KDBXEntry* entry,
    KDBXCustomField* field,
    const char* name,
    const char* value,
    bool protected_value,
    FuriString* error) {
    KDBXFieldRef ref = {0};

    furi_assert(app);
    furi_assert(entry);
    furi_assert(field);

    if(!flippass_db_validate_custom_field_name(entry, name, field, error) ||
       !flippass_db_validate_custom_field_value(value, error) ||
       !flippass_db_prepare_editable_session(app, error) ||
       !flippass_db_write_secret_field(app, value, &ref, error)) {
        return false;
    }

    bool updated = kdbx_custom_field_set_key(field, app->db_arena, name) &&
                   kdbx_custom_field_set_ref(field, &ref);
    if(!updated && flippass_db_promote_vault_for_mutation(app, NULL)) {
        updated = kdbx_custom_field_set_key(field, app->db_arena, name) &&
                  kdbx_custom_field_set_ref(field, &ref);
    }
    if(!updated) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass could not update the custom field.");
        }
        return false;
    }

    kdbx_custom_field_set_protected(field, protected_value);
    kdbx_custom_field_set_loaded_text(field, NULL, 0U);
    flippass_db_mark_dirty(app);
    return true;
}

bool flippass_db_delete_custom_field(
    App* app,
    KDBXEntry* entry,
    KDBXCustomField* field,
    FuriString* error) {
    furi_assert(app);
    furi_assert(entry);
    furi_assert(field);

    KDBXCustomField** link = &entry->custom_fields;
    while(*link != NULL && *link != field) {
        link = &(*link)->next;
    }

    if(*link != field) {
        if(error != NULL) {
            furi_string_set_str(error, "The selected field is no longer in this entry.");
        }
        return false;
    }

    *link = field->next;
    field->next = NULL;
    kdbx_custom_field_set_loaded_text(field, NULL, 0U);
    flippass_db_mark_dirty(app);
    return true;
}
