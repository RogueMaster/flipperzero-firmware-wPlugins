#pragma once

#include <lib/nfc/protocols/nfc_generic_event.h>
#include <lib/nfc/protocols/iso14443_4a/iso14443_4a_listener.h>
#include <lib/nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <lib/nfc/helpers/iso14443_crc.h>
#include <mbedtls/des.h>
#include <mbedtls/aes.h>

#include "seos_credential.h"
#include "secure_messaging.h"
#include "seos_inspect.h"

typedef struct {
    BitBuffer* tx_buffer;
    BitBuffer* rx_buffer;

    // Reader inspection state, used only in FLOW_INSPECT. inspect_3a is the 3A
    // card we present for it; inspection drives the T=CL block protocol itself
    // rather than through the 4A listener.
    SeosInspectData inspect;
    Iso14443_3aData* inspect_3a;

    /* Whether the handler already wrote a status word. The secure messaging
     * answers are complete as they stand, including a chaining or error word
     * that must not be written over. */
    bool response_complete;

    AuthParameters params;

    SecureMessaging* secure_messaging;

    SeosCredential* credential;
} SeosEmulator;

NfcCommand seos_worker_listener_callback(NfcGenericEvent event, void* context);

// The 3A card presented for inspection, and the listener that drives it.
Iso14443_3aData* seos_inspect_make_3a_data(void);

NfcCommand seos_inspect_listener_callback(NfcGenericEvent event, void* context);

SeosEmulator* seos_emulator_alloc(SeosCredential* credential);

void seos_emulator_free(SeosEmulator* seos_emulator);
