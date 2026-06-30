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

    AuthParameters params;

    SecureMessaging* secure_messaging;

    SeosCredential* credential;
} SeosEmulator;

NfcCommand seos_worker_listener_callback(NfcGenericEvent event, void* context);

SeosEmulator* seos_emulator_alloc(SeosCredential* credential);

void seos_emulator_free(SeosEmulator* seos_emulator);

void seos_emulator_general_authenticate_1(BitBuffer* tx_buffer, AuthParameters params);
bool seos_emulator_general_authenticate_2(
    const uint8_t* buffer,
    size_t buffer_len,
    SeosCredential* credential,
    AuthParameters* params,
    BitBuffer* tx_buffer);

void seos_emulator_select_aid(BitBuffer* tx_buffer, const uint8_t* aid, size_t aid_len);
bool seos_emulator_select_adf(
    const uint8_t* oid_list,
    size_t oid_list_len,
    AuthParameters* params,
    SeosCredential* credential,
    BitBuffer* tx_buffer);
