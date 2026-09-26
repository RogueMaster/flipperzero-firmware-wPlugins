#pragma once

#include "lib/core/src/dfc_text.h"

#define DFC_TEXT_IMPORT_PLUGIN_ID         "dfc_text_import"
#define DFC_TEXT_EXPORT_PLUGIN_ID         "dfc_text_export"
#define DFC_TEXT_CODEC_PLUGIN_API_VERSION 1
#define DFC_TEXT_IMPORT_PLUGIN_PATH       APP_ASSETS_PATH("plugins/dfc_text_import.fal")
#define DFC_TEXT_EXPORT_PLUGIN_PATH       APP_ASSETS_PATH("plugins/dfc_text_export.fal")

// Resolve model operations in the main app; the plugin only carries text code.
typedef struct {
    __typeof__(&dfc_credential_app_auth_commands) dfc_credential_app_auth_commands;
    __typeof__(&dfc_credential_clear) dfc_credential_clear;
    __typeof__(&dfc_credential_key) dfc_credential_key;
    __typeof__(&dfc_credential_key_const) dfc_credential_key_const;
    __typeof__(&dfc_credential_key_in_set) dfc_credential_key_in_set;
    __typeof__(&dfc_credential_key_in_set_const) dfc_credential_key_in_set_const;
    __typeof__(&dfc_credential_key_length) dfc_credential_key_length;
    __typeof__(&dfc_credential_key_sets_resize) dfc_credential_key_sets_resize;
    __typeof__(&dfc_credential_key_version_in_set) dfc_credential_key_version_in_set;
    __typeof__(&dfc_credential_keys_resize) dfc_credential_keys_resize;
    __typeof__(&dfc_credential_picc_auth_commands) dfc_credential_picc_auth_commands;
    __typeof__(&dfc_credential_reset_application) dfc_credential_reset_application;
    __typeof__(&dfc_der_decode) dfc_der_decode;
    __typeof__(&dfc_der_status_name) dfc_der_status_name;
    __typeof__(&dfc_der_validate_model) dfc_der_validate_model;
    __typeof__(&dfc_file_data) dfc_file_data;
    __typeof__(&dfc_file_data_const) dfc_file_data_const;
    __typeof__(&dfc_file_resize) dfc_file_resize;
} DfcTextHostApi;

typedef struct {
    void (*bind)(const DfcTextHostApi*);
    DfcTextStatus (*parse)(DfcCredential*, const char*, size_t, DfcTextError*);
    DfcTextStatus (*write)(const DfcCredential*, char*, size_t, size_t*);
    const char* (*status_name)(DfcTextStatus);
} DfcTextCodecPlugin;
