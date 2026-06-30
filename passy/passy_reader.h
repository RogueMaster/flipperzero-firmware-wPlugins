#pragma once

#include <lib/nfc/nfc.h>
#include <nfc/nfc_listener.h>
#include <nfc/nfc_poller.h>
#include <nfc/nfc_device.h>
#include <lib/nfc/protocols/nfc_generic_event.h>
#include <lib/nfc/protocols/iso14443_4b/iso14443_4b_poller.h>
#include <lib/nfc/protocols/iso14443_4a/iso14443_4a_poller.h>
#include <lib/nfc/helpers/iso14443_crc.h>
#include <mbedtls/des.h>

#include <lib/toolbox/stream/stream.h>
#include <lib/toolbox/stream/file_stream.h>

#include "passy_i.h"
#include "passy_common.h"
#include "secure_messaging.h"

#define PASSY_READER_MAX_BUFFER_SIZE 128

NfcCommand passy_reader_poller_callback(NfcGenericEvent event, void* context);

typedef struct {
    Passy* passy;

    Iso14443_4bPoller* iso14443_4b_poller;
    Iso14443_4aPoller* iso14443_4a_poller;
    BitBuffer* tx_buffer;
    BitBuffer* rx_buffer;

    BitBuffer* DG1;
    BitBuffer* COM;
    BitBuffer* dg_header;

    SecureMessaging* secure_messaging;

    uint16_t last_sw;
} PassyReader;

PassyReader* passy_reader_alloc(Passy* passy);

void passy_reader_free(PassyReader* passy_reader);

void passy_reader_mac(uint8_t* key, uint8_t* data, size_t data_length, uint8_t* mac);
