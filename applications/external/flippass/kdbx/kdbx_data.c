#include "kdbx_data.h"

#include "kdbx_includes.h"

#include <stdlib.h>
#include <string.h>

static void kdbx_secure_free_string(char* value) {
    if(value == NULL) {
        return;
    }

    memzero(value, strlen(value));
    free(value);
}

static bool kdbx_group_set_arena_string(
    char** target,
    KDBXArena* arena,
    const char* value,
    bool reuse_arena_slot) {
    if(target == NULL || arena == NULL) {
        return false;
    }

    const char* text = value != NULL ? value : "";
    const size_t len = strlen(text);
    if(reuse_arena_slot && *target != NULL) {
        const size_t old_len = strlen(*target);
        if(len <= old_len) {
            /* Arena strings are individually owned. Equal/shorter edits can
             * reuse their slot instead of retaining an unreachable old copy. */
            memmove(*target, text, len + 1U);
            memzero(*target + len + 1U, old_len - len);
            return true;
        }
    }
    char* copy = kdbx_arena_strdup(arena, text);
    if(copy == NULL) {
        return false;
    }

    *target = copy;
    return true;
}

static void kdbx_custom_field_clear_loaded_value(KDBXCustomField* field) {
    if(field == NULL) {
        return;
    }

    kdbx_secure_free_string(field->value);
    field->value = NULL;
}

static char** kdbx_entry_field_ptr(KDBXEntry* entry, uint32_t field_mask) {
    if(entry == NULL) {
        return NULL;
    }

    switch(field_mask) {
    case KDBXEntryFieldTitle:
        return &entry->title;
    case KDBXEntryFieldUuid:
        return &entry->uuid;
    case KDBXEntryFieldUsername:
        return &entry->username;
    case KDBXEntryFieldPassword:
        return &entry->password;
    case KDBXEntryFieldUrl:
        return &entry->url;
    case KDBXEntryFieldNotes:
        return &entry->notes;
    case KDBXEntryFieldAutotype:
        return &entry->autotype_sequence;
    default:
        return NULL;
    }
}

static KDBXFieldRef* kdbx_entry_field_ref_ptr(KDBXEntry* entry, uint32_t field_mask) {
    if(entry == NULL) {
        return NULL;
    }

    switch(field_mask) {
    case KDBXEntryFieldUuid:
        return &entry->uuid_ref;
    case KDBXEntryFieldTitle:
        return &entry->title_ref;
    case KDBXEntryFieldUsername:
        return &entry->username_ref;
    case KDBXEntryFieldPassword:
        return &entry->password_ref;
    case KDBXEntryFieldUrl:
        return &entry->url_ref;
    case KDBXEntryFieldNotes:
        return &entry->notes_ref;
    case KDBXEntryFieldAutotype:
        return &entry->autotype_ref;
    default:
        return NULL;
    }
}

KDBXGroup* kdbx_group_alloc(KDBXArena* arena) {
    if(arena == NULL) {
        return NULL;
    }

    /* Arena chunks are zeroed; only the group's non-zero default needs a store. */
    KDBXGroup* group = kdbx_arena_alloc_block(arena, sizeof(KDBXGroup), sizeof(void*));
    if(group != NULL) {
        group->icon_id = KDBX_ICON_ID_GROUP_DEFAULT;
    }
    return group;
}

KDBXEntry* kdbx_entry_alloc(KDBXArena* arena) {
    if(arena == NULL) {
        return NULL;
    }

    return kdbx_arena_alloc_block(arena, sizeof(KDBXEntry), sizeof(void*));
}

void kdbx_group_reset(KDBXGroup* group) {
    if(group == NULL) {
        return;
    }

    group->name = NULL;
    group->uuid = NULL;
    memset(&group->uuid_ref, 0, sizeof(group->uuid_ref));
    group->parent = NULL;
    group->children = NULL;
    group->next = NULL;
    group->entries = NULL;
    group->icon_id = KDBX_ICON_ID_GROUP_DEFAULT;
}

void kdbx_entry_clear_loaded_field_mask(KDBXEntry* entry, uint32_t field_mask) {
    if(entry == NULL || field_mask == 0U) {
        return;
    }

    const uint32_t to_clear = entry->loaded_mask & field_mask;

    if((to_clear & KDBXEntryFieldUuid) != 0U) {
        kdbx_secure_free_string(entry->uuid);
        entry->uuid = NULL;
    }
    if((to_clear & KDBXEntryFieldTitle) != 0U) {
        kdbx_secure_free_string(entry->title);
        entry->title = NULL;
    }
    if((to_clear & KDBXEntryFieldUsername) != 0U) {
        kdbx_secure_free_string(entry->username);
        entry->username = NULL;
    }
    if((to_clear & KDBXEntryFieldPassword) != 0U) {
        kdbx_secure_free_string(entry->password);
        entry->password = NULL;
    }
    if((to_clear & KDBXEntryFieldUrl) != 0U) {
        kdbx_secure_free_string(entry->url);
        entry->url = NULL;
    }
    if((to_clear & KDBXEntryFieldNotes) != 0U) {
        kdbx_secure_free_string(entry->notes);
        entry->notes = NULL;
    }
    if((to_clear & KDBXEntryFieldAutotype) != 0U) {
        kdbx_secure_free_string(entry->autotype_sequence);
        entry->autotype_sequence = NULL;
    }

    entry->loaded_mask &= ~to_clear;
}

void kdbx_entry_clear_loaded_fields(KDBXEntry* entry) {
    if(entry == NULL) {
        return;
    }

    kdbx_entry_clear_loaded_field_mask(
        entry,
        KDBXEntryFieldUuid | KDBXEntryFieldTitle | KDBXEntryFieldUsername |
            KDBXEntryFieldPassword | KDBXEntryFieldUrl | KDBXEntryFieldNotes |
            KDBXEntryFieldAutotype);

    for(KDBXCustomField* field = entry->custom_fields; field != NULL; field = field->next) {
        kdbx_custom_field_clear_loaded_value(field);
    }
    entry->loaded_mask = 0U;
}

void kdbx_entry_reset(KDBXEntry* entry) {
    if(entry == NULL) {
        return;
    }

    kdbx_entry_clear_loaded_fields(entry);
    entry->uuid = NULL;
    entry->title = NULL;
    entry->field_mask = 0U;
    entry->icon_id = KDBX_ICON_ID_ENTRY_DEFAULT;
    memset(&entry->uuid_ref, 0, sizeof(entry->uuid_ref));
    memset(&entry->title_ref, 0, sizeof(entry->title_ref));
    memset(&entry->username_ref, 0, sizeof(entry->username_ref));
    memset(&entry->password_ref, 0, sizeof(entry->password_ref));
    memset(&entry->url_ref, 0, sizeof(entry->url_ref));
    memset(&entry->notes_ref, 0, sizeof(entry->notes_ref));
    memset(&entry->autotype_ref, 0, sizeof(entry->autotype_ref));
    entry->custom_fields = NULL;
    entry->next = NULL;
}

void kdbx_group_free(KDBXGroup* group) {
    /* Nodes belong to the arena, so clearing fields leaves traversal links live.
     * Preserve the historical forest (root plus its siblings) cleanup contract. */
    for(KDBXGroup* root = group; root != NULL; root = root->next) {
        for(KDBXGroup* node = root; node != NULL; node = kdbx_group_next_in_subtree(node, root)) {
            kdbx_entry_free(node->entries);
        }
    }
}

void kdbx_group_restore_source_entry_order(KDBXGroup* root) {
    if(root == NULL) return;

    for(KDBXGroup* group = root; group != NULL; group = kdbx_group_next_in_subtree(group, root)) {
        KDBXEntry* reversed = NULL;
        KDBXEntry* entry = group->entries;
        while(entry != NULL) {
            KDBXEntry* next = entry->next;
            entry->next = reversed;
            reversed = entry;
            entry = next;
        }
        group->entries = reversed;
    }
}

void kdbx_entry_free(KDBXEntry* entry) {
    for(; entry != NULL; entry = entry->next) {
        kdbx_entry_clear_loaded_fields(entry);
    }
}

bool kdbx_group_set_name(KDBXGroup* group, KDBXArena* arena, const char* name) {
    return group != NULL && kdbx_group_set_arena_string(&group->name, arena, name, true);
}

bool kdbx_group_set_uuid(KDBXGroup* group, KDBXArena* arena, const char* uuid) {
    return group != NULL && kdbx_group_set_arena_string(&group->uuid, arena, uuid, true);
}

bool kdbx_group_set_uuid_ref(KDBXGroup* group, const KDBXFieldRef* ref) {
    if(group == NULL) {
        return false;
    }

    if(ref != NULL) {
        group->uuid_ref = *ref;
    } else {
        memset(&group->uuid_ref, 0, sizeof(group->uuid_ref));
    }

    return true;
}

const KDBXFieldRef* kdbx_group_get_uuid_ref(const KDBXGroup* group) {
    return group != NULL ? &group->uuid_ref : NULL;
}

bool kdbx_entry_set_uuid(KDBXEntry* entry, KDBXArena* arena, const char* uuid) {
    return entry != NULL && kdbx_group_set_arena_string(&entry->uuid, arena, uuid, false);
}

bool kdbx_entry_set_title(KDBXEntry* entry, KDBXArena* arena, const char* title) {
    return entry != NULL && kdbx_group_set_arena_string(&entry->title, arena, title, false);
}

bool kdbx_entry_set_uuid_ref(KDBXEntry* entry, const KDBXFieldRef* ref) {
    if(entry == NULL) {
        return false;
    }

    if(ref != NULL) {
        entry->uuid_ref = *ref;
    } else {
        memset(&entry->uuid_ref, 0, sizeof(entry->uuid_ref));
    }

    return true;
}

const KDBXFieldRef* kdbx_entry_get_uuid_ref(const KDBXEntry* entry) {
    return entry != NULL ? &entry->uuid_ref : NULL;
}

bool kdbx_entry_set_loaded_text(
    KDBXEntry* entry,
    uint32_t field_mask,
    const char* value,
    size_t value_len) {
    char** target = kdbx_entry_field_ptr(entry, field_mask);
    if(entry == NULL || target == NULL) {
        return false;
    }

    char* copy = NULL;
    if(value != NULL && value_len > 0U) {
        copy = malloc(value_len + 1U);
        if(copy == NULL) {
            return false;
        }

        memcpy(copy, value, value_len);
        copy[value_len] = '\0';
    }

    return kdbx_entry_take_loaded_text(entry, field_mask, copy, value_len);
}

bool kdbx_entry_take_loaded_text(
    KDBXEntry* entry,
    uint32_t field_mask,
    char* value,
    size_t value_len) {
    char** target = kdbx_entry_field_ptr(entry, field_mask);
    if(entry == NULL || target == NULL) {
        return false;
    }

    if(value != NULL && value_len == 0U) {
        kdbx_secure_free_string(value);
        value = NULL;
    }

    /* Only secure-free the previous slot when it was a loaded (malloc'd)
     * value. Arena-backed strings (e.g. legacy `kdbx_entry_set_title`) must
     * not be passed to free(); just overwrite the slot. */
    if((entry->loaded_mask & field_mask) != 0U) {
        kdbx_secure_free_string(*target);
    }
    *target = value;
    if(value != NULL) {
        entry->loaded_mask |= field_mask;
    } else {
        entry->loaded_mask &= ~field_mask;
    }
    return true;
}

bool kdbx_entry_set_username(KDBXEntry* entry, const char* username) {
    return kdbx_entry_set_loaded_text(
        entry, KDBXEntryFieldUsername, username, username != NULL ? strlen(username) : 0U);
}

bool kdbx_entry_set_password(KDBXEntry* entry, const char* password) {
    return kdbx_entry_set_loaded_text(
        entry, KDBXEntryFieldPassword, password, password != NULL ? strlen(password) : 0U);
}

bool kdbx_entry_set_url(KDBXEntry* entry, const char* url) {
    return kdbx_entry_set_loaded_text(
        entry, KDBXEntryFieldUrl, url, url != NULL ? strlen(url) : 0U);
}

bool kdbx_entry_set_notes(KDBXEntry* entry, const char* notes) {
    return kdbx_entry_set_loaded_text(
        entry, KDBXEntryFieldNotes, notes, notes != NULL ? strlen(notes) : 0U);
}

bool kdbx_entry_set_autotype_sequence(KDBXEntry* entry, const char* autotype_sequence) {
    return kdbx_entry_set_loaded_text(
        entry,
        KDBXEntryFieldAutotype,
        autotype_sequence,
        autotype_sequence != NULL ? strlen(autotype_sequence) : 0U);
}

bool kdbx_entry_set_field_ref(KDBXEntry* entry, uint32_t field_mask, const KDBXFieldRef* ref) {
    KDBXFieldRef* target = kdbx_entry_field_ref_ptr(entry, field_mask);
    if(entry == NULL || target == NULL || ref == NULL) {
        return false;
    }

    *target = *ref;
    if(ref->plain_len > 0U) {
        entry->field_mask |= field_mask;
    } else {
        entry->field_mask &= ~field_mask;
    }
    return true;
}

const KDBXFieldRef* kdbx_entry_get_field_ref(const KDBXEntry* entry, uint32_t field_mask) {
    return kdbx_entry_field_ref_ptr((KDBXEntry*)entry, field_mask);
}

KDBXCustomField* kdbx_entry_add_custom_field(
    KDBXEntry* entry,
    KDBXArena* arena,
    const char* key,
    const KDBXFieldRef* ref) {
    return kdbx_entry_add_custom_field_ex(entry, arena, key, ref, false);
}

KDBXCustomField* kdbx_entry_add_custom_field_ex(
    KDBXEntry* entry,
    KDBXArena* arena,
    const char* key,
    const KDBXFieldRef* ref,
    bool protected_value) {
    if(entry == NULL || arena == NULL || key == NULL || key[0] == '\0' || ref == NULL ||
       ref->plain_len == 0U) {
        return NULL;
    }

    KDBXCustomField* field = kdbx_arena_alloc_block(arena, sizeof(KDBXCustomField), sizeof(void*));
    if(field == NULL) {
        return NULL;
    }

    memset(field, 0, sizeof(*field));
    if(!kdbx_group_set_arena_string(&field->key, arena, key, true)) {
        return NULL;
    }

    field->value_ref = *ref;
    field->protected_value = protected_value;

    if(entry->custom_fields == NULL) {
        entry->custom_fields = field;
    } else {
        KDBXCustomField* tail = entry->custom_fields;
        while(tail->next != NULL) {
            tail = tail->next;
        }
        tail->next = field;
    }

    return field;
}

const KDBXCustomField* kdbx_entry_get_custom_fields(const KDBXEntry* entry) {
    return entry != NULL ? entry->custom_fields : NULL;
}

const KDBXFieldRef* kdbx_custom_field_get_ref(const KDBXCustomField* field) {
    return field != NULL ? &field->value_ref : NULL;
}

bool kdbx_custom_field_set_key(KDBXCustomField* field, KDBXArena* arena, const char* key) {
    if(field == NULL || key == NULL || key[0] == '\0') {
        return false;
    }

    return kdbx_group_set_arena_string(&field->key, arena, key, true);
}

bool kdbx_custom_field_set_ref(KDBXCustomField* field, const KDBXFieldRef* ref) {
    if(field == NULL || ref == NULL || ref->plain_len == 0U) {
        return false;
    }

    field->value_ref = *ref;
    return true;
}

void kdbx_custom_field_set_protected(KDBXCustomField* field, bool protected_value) {
    if(field != NULL) {
        field->protected_value = protected_value;
    }
}

bool kdbx_custom_field_is_loaded(const KDBXCustomField* field) {
    return field != NULL && field->value != NULL;
}

bool kdbx_custom_field_set_loaded_text(KDBXCustomField* field, const char* value, size_t value_len) {
    if(field == NULL) {
        return false;
    }

    char* copy = NULL;
    if(value != NULL && value_len > 0U) {
        copy = malloc(value_len + 1U);
        if(copy == NULL) {
            return false;
        }

        memcpy(copy, value, value_len);
        copy[value_len] = '\0';
    }

    return kdbx_custom_field_take_loaded_text(field, copy, value_len);
}

bool kdbx_custom_field_take_loaded_text(KDBXCustomField* field, char* value, size_t value_len) {
    if(field == NULL) {
        return false;
    }

    if(value != NULL && value_len == 0U) {
        kdbx_secure_free_string(value);
        value = NULL;
    }

    kdbx_custom_field_clear_loaded_value(field);
    field->value = value;
    return true;
}

bool kdbx_entry_has_field(const KDBXEntry* entry, uint32_t field_mask) {
    return entry != NULL && (entry->field_mask & field_mask) != 0U;
}

bool kdbx_entry_is_loaded(const KDBXEntry* entry, uint32_t field_mask) {
    return entry != NULL && (entry->loaded_mask & field_mask) != 0U;
}
