#pragma once

#include <furi.h>

#include "kdbx/kdbx_data.h"
#include "kdbx/kdbx_vault.h"

typedef struct App App;

bool flippass_db_load(App* app, FuriString* error);
bool flippass_db_load_with_backend(App* app, KDBXVaultBackend backend, FuriString* error);
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
bool flippass_db_copy_entry_title(App* app, const KDBXEntry* entry, FuriString* out, FuriString* error);
bool flippass_db_copy_entry_uuid(App* app, const KDBXEntry* entry, FuriString* out, FuriString* error);
KDBXVaultBackend flippass_db_parse_backend_hint(const char* text);
