#pragma once

#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <lib/toolbox/path.h>
#include <lib/nfc/protocols/nfc_generic_event.h>
#include <lib/nfc/protocols/iso14443_4a/iso14443_4a_poller.h>
#include <lib/nfc/helpers/iso14443_crc.h>
#include <mbedtls/des.h>
#include <mbedtls/aes.h>

#include "secure_messaging.h"

NfcCommand seos_worker_poller_callback(NfcGenericEvent event, void* context);

typedef struct {
    Iso14443_4aPoller* iso14443_4a_poller;
    BitBuffer* tx_buffer;
    BitBuffer* rx_buffer;

    AuthParameters params;
    SecureMessaging* secure_messaging;

    SeosCredential* credential;

} SeosReader;

SeosReader* seos_reader_alloc(SeosCredential* credential, Iso14443_4aPoller* iso14443_4a_poller);

void seos_reader_free(SeosReader* seos_reader);

bool seos_reader_save(SeosReader* seos_reader, const char* dev_name);

bool seos_reader_select_adf_response(
    BitBuffer* rx_buffer,
    size_t offset,
    SeosCredential* credential,
    AuthParameters* params);

void seos_reader_generate_cryptogram(
    SeosCredential* credential,
    AuthParameters* params,
    uint8_t* cryptogram);

bool seos_reader_verify_cryptogram(AuthParameters* params, const uint8_t* cryptogram);
