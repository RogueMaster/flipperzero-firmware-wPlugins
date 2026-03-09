#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include "protocol/picopass_protocol.h"
#include <optimized_ikeys.h>
#include <optimized_cipher.h>

#define SEADER_CRED_NAME_MAX_LEN 22
#define SEADER_APP_EXTENSION     ".credential"
#define SEADER_APP_MFC_EXTENSION ".nfc"
#define SEADER_APP_MFC_FOLDER    EXT_PATH("nfc")

typedef void (*SeaderLoadingCallback)(void* context, bool state);

typedef enum {
    SeaderCredentialTypeNone,
    SeaderCredentialTypePicopass,
    SeaderCredentialType14A,
    // Might need to make 14a into "javacard" and add Desfire
    SeaderCredentialTypeMifareClassic,
    SeaderCredentialTypeVirtual,
    SeaderCredentialTypeConfig,
} SeaderCredentialType;

typedef enum {
    SeaderPacsMediaTypeUnknown = 0,
    SeaderPacsMediaTypeDesfire = 1,
    SeaderPacsMediaTypeMifare = 2,
    SeaderPacsMediaTypePicopass = 3,
    SeaderPacsMediaTypeMifarePlus = 6,
    SeaderPacsMediaTypeSeos = 7,
} SeaderPacsMediaType;

typedef enum {
    SeaderCredentialSaveFormatAgnostic,
    SeaderCredentialSaveFormatPicopass,
    SeaderCredentialSaveFormatRFID,
    SeaderCredentialSaveFormatSR,
    SeaderCredentialSaveFormatMFC,
} SeaderCredentialSaveFormat;

typedef struct {
    Storage* storage;
    DialogsApp* dialogs;
    uint64_t credential;
    size_t bit_length;
    uint8_t sio[128];
    uint8_t sio_len;
    uint8_t diversifier[8];
    uint8_t diversifier_len;
    uint8_t sio_start_block; // for iClass SE vs iClass SR
    bool isDesfireEV2;
    bool has_pacs_media_type;
    SeaderPacsMediaType pacs_media_type;
    SeaderCredentialType type;
    SeaderCredentialSaveFormat save_format;
    char name[SEADER_CRED_NAME_MAX_LEN + 1];
    FuriString* load_path;
    SeaderLoadingCallback loading_cb;
    void* loading_cb_ctx;
} SeaderCredential;

SeaderCredential* seader_credential_alloc();

void seader_credential_free(SeaderCredential* seader_cred);

void seader_credential_set_loading_callback(
    SeaderCredential* cred,
    SeaderLoadingCallback callback,
    void* context);

void seader_credential_set_name(SeaderCredential* cred, const char* name);

bool seader_credential_save(SeaderCredential* cred, const char* name);

bool seader_file_select(SeaderCredential* cred);

void seader_credential_clear(SeaderCredential* cred);

bool seader_credential_delete(SeaderCredential* cred, bool use_load_path);

const char* seader_credential_get_type_label(const SeaderCredential* cred);
