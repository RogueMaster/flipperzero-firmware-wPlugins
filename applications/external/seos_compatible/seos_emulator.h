#pragma once

#include <lib/nfc/protocols/nfc_generic_event.h>
#include <lib/nfc/protocols/iso14443_4a/iso14443_4a_listener.h>
#include <lib/nfc/helpers/iso14443_crc.h>
#include <mbedtls/des.h>
#include <mbedtls/aes.h>

#include "seos_credential.h"
#include "secure_messaging.h"

typedef struct {
    BitBuffer* tx_buffer;
    BitBuffer* rx_buffer;

    /* Whether the handler already wrote a status word. The secure messaging
     * answers are complete as they stand, including a chaining or error word
     * that must not be written over. */
    bool response_complete;

    AuthParameters params;

    SecureMessaging* secure_messaging;

    SeosCredential* credential;
} SeosEmulator;

NfcCommand seos_worker_listener_callback(NfcGenericEvent event, void* context);

SeosEmulator* seos_emulator_alloc(SeosCredential* credential);

void seos_emulator_free(SeosEmulator* seos_emulator);
