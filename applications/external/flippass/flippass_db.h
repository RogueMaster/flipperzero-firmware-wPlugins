#pragma once

#include <furi.h>

#include "kdbx/kdbx_data.h"
#include "kdbx/kdbx_vault.h"

typedef struct App App;

/* Contiguous block the resident model must leave free for plugins that map after
 * an open: the largest such section is the editor's .text (11.5 KB), and the
 * loader wants each section's size plus 1 KB. */
#define FLIPPASS_DB_SAVE_MIN_MAX_FREE_BYTES (13U * 1024U)

bool flippass_db_load(App* app, FuriString* error);
bool flippass_db_load_with_backend(App* app, KDBXVaultBackend backend, FuriString* error);
void flippass_db_refresh_model_budget(App* app);
void flippass_db_proactive_promote_if_needed(App* app);
void flippass_db_deactivate_entry(App* app);
bool flippass_db_activate_entry(App* app, KDBXEntry* entry, bool load_notes, FuriString* error);
bool flippass_db_ensure_entry_field(
    App* app,
    KDBXEntry* entry,
    uint32_t field_mask,
    FuriString* error);
bool flippass_db_ensure_custom_field(
    App* app,
    KDBXEntry* entry,
    KDBXCustomField* field,
    FuriString* error);
bool flippass_db_get_other_field_value(
    App* app,
    KDBXEntry* entry,
    uint32_t field_mask,
    KDBXCustomField* field,
    const char** out_value,
    FuriString* error);
bool flippass_db_entry_has_field(const KDBXEntry* entry, uint32_t field_mask);
const KDBXCustomField* flippass_db_entry_get_custom_fields(const KDBXEntry* entry);
bool flippass_db_create_custom_field(
    App* app,
    KDBXEntry* entry,
    const char* name,
    const char* value,
    bool protected_value,
    KDBXCustomField** out_field,
    FuriString* error);
bool flippass_db_update_custom_field(
    App* app,
    KDBXEntry* entry,
    KDBXCustomField* field,
    const char* name,
    const char* value,
    bool protected_value,
    FuriString* error);
bool flippass_db_delete_custom_field(
    App* app,
    KDBXEntry* entry,
    KDBXCustomField* field,
    FuriString* error);
bool flippass_db_copy_group_uuid(
    App* app,
    const KDBXGroup* group,
    FuriString* out,
    FuriString* error);
bool flippass_db_copy_entry_title(
    App* app,
    const KDBXEntry* entry,
    FuriString* out,
    FuriString* error);
bool flippass_db_copy_entry_uuid(
    App* app,
    const KDBXEntry* entry,
    FuriString* out,
    FuriString* error);

/**
 * @brief Ensure the entry's title is materialized into `entry->title`, loading
 *        from the encrypted vault when necessary.
 *
 * Use this from any path that needs to read `entry->title` directly. The
 * loaded title is tracked by the title cache window and may be evicted on
 * later group navigation; do not retain the pointer past the next call to
 * `flippass_db_browser_enter_group` or `flippass_db_clear_entry_title_cached`.
 */
bool flippass_db_ensure_entry_title(App* app, KDBXEntry* entry, FuriString* error);

/**
 * @brief Drop the cached `entry->title` text without disturbing other loaded
 *        fields (username/password/notes/etc).
 */
void flippass_db_clear_entry_title_cached(KDBXEntry* entry);

/**
 * @brief Notify the title cache that group browsing moved to `new_current`,
 *        coming from `old_current`. The cache keeps titles loaded for
 *        `new_current`, then for `old_current` and the children of
 *        `new_current` while heap headroom allows; everything else is evicted.
 */
void flippass_db_browser_enter_group(App* app, KDBXGroup* new_current, KDBXGroup* old_current);

/**
 * @brief Drop every cached title across the whole tree. Used when closing the
 *        database or before save trim.
 */
bool flippass_db_browser_clear_title_cache(App* app);

KDBXVaultBackend flippass_db_parse_backend_hint(const char* text);
