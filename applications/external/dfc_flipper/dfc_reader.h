#pragma once

#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <lib/toolbox/path.h>
#include <lib/nfc/protocols/nfc_generic_event.h>
#include <lib/nfc/protocols/iso14443_4a/iso14443_4a_poller.h>
#include <lib/nfc/helpers/iso14443_crc.h>

#include "dfc_secure_messaging.h"
#include "dfc_credential.h"

NfcCommand dfc_worker_poller_callback(NfcGenericEvent event, void* context);

typedef struct {
    Iso14443_4aPoller* iso14443_4a_poller;
    BitBuffer* tx_buffer;
    BitBuffer* rx_buffer;

    DfcSecureMessaging* secure_messaging;

    DfcCredential* credential;
} DfcReader;

DfcReader* dfc_reader_alloc(DfcCredential* credential, Iso14443_4aPoller* iso14443_4a_poller);

void dfc_reader_free(DfcReader* dfc_reader);

// Native SelectApplication (0x5A) against a 3-byte AID.
NfcCommand dfc_reader_select_application(DfcReader* reader, const uint8_t* aid);

// Full Authenticate(Legacy/ISO/AES) challenge-response with credential->keys[key_no].
// Populates reader->secure_messaging with the derived session on success.
NfcCommand dfc_reader_authenticate(DfcReader* reader, uint8_t key_no, uint8_t cipher);

// ReadData (0xBD) for a single file, storing the result into credential->files[].
NfcCommand dfc_reader_read_file(DfcReader* reader, DfcFile* file);
