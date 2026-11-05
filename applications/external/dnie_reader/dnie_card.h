#pragma once

#include <furi.h>
#include <nfc/nfc.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_poller.h>
#include <nfc/protocols/iso14443_4b/iso14443_4b_poller.h>

// El DNIe es ISO14443-4B; muchos pasaportes son 4A. El resto del codigo no distingue.
typedef struct {
    NfcProtocol protocol; // NfcProtocolIso14443_4a o _4b
    void* poller; // Iso14443_4aPoller* o Iso14443_4bPoller*
} DnieTransport;

#define DNIE_CARD_ACCESS_MAX 512

typedef enum {
    DnieCardOk,
    DnieCardErrorTransport, // fallo de radio / tarjeta retirada
    DnieCardErrorSelect, // el chip rechazo el SELECT de EF.CardAccess
    DnieCardErrorRead, // READ BINARY devolvio un SW distinto de 9000
    DnieCardErrorFormat, // no empieza por SET (0x31) o longitud invalida
    DnieCardErrorTooBig, // mas grande que nuestro buffer
} DnieCardResult;

// Lee EF.CardAccess (FID 011C). Solo se puede llamar dentro del callback del poller.
DnieCardResult dnie_card_read_card_access(
    DnieTransport* poller,
    uint8_t* out,
    size_t out_max,
    size_t* out_len,
    uint16_t* last_sw);

const char* dnie_card_result_str(DnieCardResult result);

// Envia una APDU dentro del callback del poller. En rx quedan datos + SW1SW2;
// en sw se devuelve el status word. false = error de transporte.
bool dnie_card_apdu(
    DnieTransport* poller,
    const uint8_t* apdu,
    size_t apdu_len,
    BitBuffer* tx,
    BitBuffer* rx,
    uint16_t* sw);
