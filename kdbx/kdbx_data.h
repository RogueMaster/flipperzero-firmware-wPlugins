#pragma once

#include <furi.h>

#include "kdbx_arena.h"
#include "kdbx_vault.h"

typedef enum {
    KDBXEntryFieldUsername = (1U << 0),
    KDBXEntryFieldPassword = (1U << 1),
    KDBXEntryFieldUrl = (1U << 2),
    KDBXEntryFieldNotes = (1U << 3),
    KDBXEntryFieldAutotype = (1U << 4),
    KDBXEntryFieldUuid = (1U << 5),
    KDBXEntryFieldTitle = (1U << 6),
} KDBXEntryFieldMask;

/* KeePass standard icons (PwIcon): 0..KDBX_ICON_ID_COUNT-1. Absent <IconID>
 * means the KeePass default: Key for entries, Folder for groups. */
#define KDBX_ICON_ID_COUNT         69U
#define KDBX_ICON_ID_ENTRY_DEFAULT 0U
#define KDBX_ICON_ID_GROUP_DEFAULT 48U

typedef struct KDBXCustomField {
    char* key;
    char* value;
    KDBXFieldRef value_ref;
    bool protected_value;
    struct KDBXCustomField* next;
} KDBXCustomField;

typedef struct KDBXEntry {
    char* uuid;
    char* title;
    char* username;
    char* password;
    char* url;
    char* notes;
    char* autotype_sequence;
    uint8_t field_mask;
    uint8_t loaded_mask;
    uint8_t icon_id; /* KeePass IconID; fills existing padding, sizeof stays 68 on ARM */
    KDBXFieldRef uuid_ref;
    KDBXFieldRef title_ref;
    KDBXFieldRef username_ref;
    KDBXFieldRef password_ref;
    KDBXFieldRef url_ref;
    KDBXFieldRef notes_ref;
    KDBXFieldRef autotype_ref;
    KDBXCustomField* custom_fields;
    struct KDBXEntry* next;
} KDBXEntry;

typedef struct KDBXGroup {
    char* name;
    char* uuid;
    KDBXFieldRef uuid_ref;
    struct KDBXGroup* parent;
    struct KDBXGroup* children;
    struct KDBXGroup* next;
    KDBXEntry* entries;
    uint8_t icon_id; /* KeePass IconID, KDBX_ICON_ID_GROUP_DEFAULT when absent */
} KDBXGroup;

/* Preorder traversal of one subtree, using existing parent links. The root's
 * siblings are outside the boundary. Nodes and links must remain live. */
static inline KDBXGroup* kdbx_group_next_in_subtree(KDBXGroup* group, const KDBXGroup* root) {
    if(group->children != NULL) return group->children;
    while(group != root) {
        if(group->next != NULL) return group->next;
        group = group->parent;
        if(group == NULL) return NULL;
    }
    return NULL;
}

/* Reverse the open builder's prepended entry lists to recover XML source order. */
void kdbx_group_restore_source_entry_order(KDBXGroup* root);

KDBXGroup* kdbx_group_alloc(KDBXArena* arena);
void kdbx_group_free(KDBXGroup* group);
KDBXEntry* kdbx_entry_alloc(KDBXArena* arena);
void kdbx_entry_free(KDBXEntry* entry);
void kdbx_group_reset(KDBXGroup* group);
void kdbx_entry_reset(KDBXEntry* entry);
bool kdbx_group_set_name(KDBXGroup* group, KDBXArena* arena, const char* name);
bool kdbx_group_set_uuid(KDBXGroup* group, KDBXArena* arena, const char* uuid);
bool kdbx_group_set_uuid_ref(KDBXGroup* group, const KDBXFieldRef* ref);
const KDBXFieldRef* kdbx_group_get_uuid_ref(const KDBXGroup* group);
bool kdbx_entry_set_uuid(KDBXEntry* entry, KDBXArena* arena, const char* uuid);
bool kdbx_entry_set_title(KDBXEntry* entry, KDBXArena* arena, const char* title);
bool kdbx_entry_set_uuid_ref(KDBXEntry* entry, const KDBXFieldRef* ref);
const KDBXFieldRef* kdbx_entry_get_uuid_ref(const KDBXEntry* entry);
bool kdbx_entry_set_username(KDBXEntry* entry, const char* username);
bool kdbx_entry_set_password(KDBXEntry* entry, const char* password);
bool kdbx_entry_set_url(KDBXEntry* entry, const char* url);
bool kdbx_entry_set_notes(KDBXEntry* entry, const char* notes);
bool kdbx_entry_set_autotype_sequence(KDBXEntry* entry, const char* autotype_sequence);
bool kdbx_entry_set_field_ref(KDBXEntry* entry, uint32_t field_mask, const KDBXFieldRef* ref);
const KDBXFieldRef* kdbx_entry_get_field_ref(const KDBXEntry* entry, uint32_t field_mask);
KDBXCustomField* kdbx_entry_add_custom_field(
    KDBXEntry* entry,
    KDBXArena* arena,
    const char* key,
    const KDBXFieldRef* ref);
KDBXCustomField* kdbx_entry_add_custom_field_ex(
    KDBXEntry* entry,
    KDBXArena* arena,
    const char* key,
    const KDBXFieldRef* ref,
    bool protected_value);
const KDBXCustomField* kdbx_entry_get_custom_fields(const KDBXEntry* entry);
const KDBXFieldRef* kdbx_custom_field_get_ref(const KDBXCustomField* field);
bool kdbx_custom_field_set_key(KDBXCustomField* field, KDBXArena* arena, const char* key);
bool kdbx_custom_field_set_ref(KDBXCustomField* field, const KDBXFieldRef* ref);
void kdbx_custom_field_set_protected(KDBXCustomField* field, bool protected_value);
bool kdbx_custom_field_is_loaded(const KDBXCustomField* field);
bool kdbx_custom_field_set_loaded_text(KDBXCustomField* field, const char* value, size_t value_len);
bool kdbx_custom_field_take_loaded_text(KDBXCustomField* field, char* value, size_t value_len);
bool kdbx_entry_has_field(const KDBXEntry* entry, uint32_t field_mask);
bool kdbx_entry_is_loaded(const KDBXEntry* entry, uint32_t field_mask);
bool kdbx_entry_set_loaded_text(
    KDBXEntry* entry,
    uint32_t field_mask,
    const char* value,
    size_t value_len);
bool kdbx_entry_take_loaded_text(
    KDBXEntry* entry,
    uint32_t field_mask,
    char* value,
    size_t value_len);
void kdbx_entry_clear_loaded_fields(KDBXEntry* entry);
/** Clear only the loaded fields whose mask bits are set in `field_mask`. */
void kdbx_entry_clear_loaded_field_mask(KDBXEntry* entry, uint32_t field_mask);
