/* Credential persistence for the Flipper application.
 *
 * The engine holds no filesystem. Call dfc_credential_storage_init once at
 * startup and dfc_credential_storage_deinit at shutdown; the save, load, and
 * delete entry points use the handles those open.
 */
#pragma once

#include <stdbool.h>

#include <furi.h>

#include "dfc_credential.h"

// Why a load failed, in the terms the user interface reports.
typedef enum {
    DfcCredentialLoadStatusOk,
    DfcCredentialLoadStatusOpenFailed,
    DfcCredentialLoadStatusUnsupportedFormat,
    DfcCredentialLoadStatusMalformedFile,
    DfcCredentialLoadStatusUnsupportedAuthMode,
    // Well formed and understood, but larger than this build can hold. Distinct
    // from malformed so a valid credential is never reported as a broken file.
    DfcCredentialLoadStatusCapacity,
} DfcCredentialLoadStatus;

// The path the loaded credential came from, empty when none was loaded. Owned
// by the storage module; the caller reads it and does not free it.
FuriString* dfc_credential_storage_load_path(void);

// Shows a storage error to the user through the application's dialog handle.
void dfc_credential_storage_show_error(const char* message);

void dfc_credential_storage_init(void);
void dfc_credential_storage_deinit(void);

bool dfc_credential_save(DfcCredential* credential, const char* dev_name);
bool dfc_credential_save_loaded(DfcCredential* credential);
bool dfc_credential_load_selected_file(DfcCredential* credential, DfcCredentialLoadStatus* status);
bool dfc_credential_delete(DfcCredential* credential, bool use_load_path);
