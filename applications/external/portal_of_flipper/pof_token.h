#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <lib/nfc/nfc_device.h>
#include <lib/nfc/protocols/mf_classic/mf_classic.h>

#define POF_TOKEN_NAME_MAX_LEN 129

typedef void (*PoFLoadingCallback)(void* context, bool state);

typedef struct {
    Storage* storage;
    DialogsApp* dialogs;
    FuriString* load_path;
    PoFLoadingCallback loading_cb;
    void* loading_cb_ctx;
    char dev_name[POF_TOKEN_NAME_MAX_LEN];
    bool change;
    bool loaded;
    NfcDevice* nfc_device;
    uint8_t UID[4];
} PoFToken;

PoFToken* pof_token_alloc();

void pof_token_free(PoFToken* pof_token);

void pof_token_set_name(PoFToken* pof_token, const char* name);

bool pof_file_select(PoFToken* pof_token);

void pof_token_clear(PoFToken* pof_token, bool save);

void pof_token_set_loading_callback(PoFToken* dev, PoFLoadingCallback callback, void* context);
