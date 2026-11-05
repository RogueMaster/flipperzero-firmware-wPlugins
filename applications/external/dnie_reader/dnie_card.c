#include "dnie_card.h"

#define TAG             "DnieCard"
#define DNIE_READ_CHUNK 0x80 // bytes por READ BINARY (conservador para el tamano de trama)
#define SW_OK           0x9000

// Envia una APDU y devuelve el status word (SW1SW2). Los datos quedan en rx (sin el SW).
bool dnie_card_apdu(
    DnieTransport* poller,
    const uint8_t* apdu,
    size_t apdu_len,
    BitBuffer* tx,
    BitBuffer* rx,
    uint16_t* sw) {
    bit_buffer_reset(tx);
    bit_buffer_reset(rx);
    bit_buffer_append_bytes(tx, apdu, apdu_len);

    int err;
    if(poller->protocol == NfcProtocolIso14443_4a) {
        err = iso14443_4a_poller_send_block(poller->poller, tx, rx);
        if(err != Iso14443_4aErrorNone) {
            FURI_LOG_E(TAG, "send_block 4A error %d", err);
            return false;
        }
    } else {
        err = iso14443_4b_poller_send_block(poller->poller, tx, rx);
        if(err != Iso14443_4bErrorNone) {
            FURI_LOG_E(TAG, "send_block 4B error %d", err);
            return false;
        }
    }

    size_t n = bit_buffer_get_size_bytes(rx);
    if(n < 2) return false;
    *sw = (uint16_t)(bit_buffer_get_byte(rx, n - 2) << 8) | bit_buffer_get_byte(rx, n - 1);
    FURI_LOG_D(TAG, "APDU %02X %02X -> SW %04X (%u bytes)", apdu[1], apdu[2], *sw, n - 2);
    return true;
}

// READ BINARY de 'le' bytes. p1p2 = offset (o SFI si p1 lleva el bit 0x80)
static bool dnie_read_binary(
    DnieTransport* poller,
    uint8_t p1,
    uint8_t p2,
    uint8_t le,
    BitBuffer* tx,
    BitBuffer* rx,
    uint16_t* sw) {
    const uint8_t apdu[] = {0x00, 0xB0, p1, p2, le};
    return dnie_card_apdu(poller, apdu, sizeof(apdu), tx, rx, sw);
}

static size_t dnie_append_data(BitBuffer* rx, uint8_t* out, size_t pos, size_t out_max) {
    size_t n = bit_buffer_get_size_bytes(rx) - 2; // quitamos el SW
    for(size_t i = 0; i < n && pos < out_max; i++) {
        out[pos++] = bit_buffer_get_byte(rx, i);
    }
    return pos;
}

DnieCardResult dnie_card_read_card_access(
    DnieTransport* poller,
    uint8_t* out,
    size_t out_max,
    size_t* out_len,
    uint16_t* last_sw) {
    furi_assert(poller && out && out_len && last_sw);

    BitBuffer* tx = bit_buffer_alloc(32);
    BitBuffer* rx = bit_buffer_alloc(DNIE_READ_CHUNK + 16);
    DnieCardResult result = DnieCardOk;
    uint16_t sw = 0;
    *out_len = 0;

    do {
        // 1. SELECT MF (3F00). Algunos chips no lo necesitan: si falla, seguimos.
        const uint8_t select_mf[] = {0x00, 0xA4, 0x00, 0x0C, 0x02, 0x3F, 0x00};
        if(!dnie_card_apdu(poller, select_mf, sizeof(select_mf), tx, rx, &sw)) {
            result = DnieCardErrorTransport;
            break;
        }

        // 2. SELECT EF.CardAccess (011C). Si lo rechaza, probamos READ BINARY por SFI.
        const uint8_t select_ca[] = {0x00, 0xA4, 0x02, 0x0C, 0x02, 0x01, 0x1C};
        if(!dnie_card_apdu(poller, select_ca, sizeof(select_ca), tx, rx, &sw)) {
            result = DnieCardErrorTransport;
            break;
        }
        bool use_sfi = (sw != SW_OK);
        if(use_sfi) FURI_LOG_W(TAG, "SELECT 011C -> %04X, probando SFI 0x1C", sw);

        // 3. Leer la cabecera TLV (4 bytes) para saber la longitud total
        //    Con SFI: P1 = 0x80 | 0x1C = 0x9C. Ademas deja el EF seleccionado.
        if(!dnie_read_binary(poller, use_sfi ? 0x9C : 0x00, 0x00, 4, tx, rx, &sw)) {
            result = DnieCardErrorTransport;
            break;
        }
        if(sw != SW_OK) {
            result = use_sfi ? DnieCardErrorSelect : DnieCardErrorRead;
            break;
        }
        size_t pos = dnie_append_data(rx, out, 0, out_max);
        if(pos < 2 || out[0] != 0x31) { // EF.CardAccess es un SET OF SecurityInfo
            result = DnieCardErrorFormat;
            break;
        }

        size_t total;
        if(out[1] < 0x80) {
            total = 2 + out[1];
        } else if(out[1] == 0x81 && pos >= 3) {
            total = 3 + out[2];
        } else if(out[1] == 0x82 && pos >= 4) {
            total = 4 + ((size_t)out[2] << 8 | out[3]);
        } else {
            result = DnieCardErrorFormat;
            break;
        }
        if(total > out_max) {
            result = DnieCardErrorTooBig;
            break;
        }

        // 4. Leer el resto por trozos usando offset (el EF ya esta seleccionado)
        while(pos < total) {
            size_t remaining = total - pos;
            uint8_t le = remaining > DNIE_READ_CHUNK ? DNIE_READ_CHUNK : (uint8_t)remaining;
            if(!dnie_read_binary(
                   poller, (uint8_t)(pos >> 8), (uint8_t)(pos & 0xFF), le, tx, rx, &sw)) {
                result = DnieCardErrorTransport;
                break;
            }
            if(sw != SW_OK) {
                result = DnieCardErrorRead;
                break;
            }
            size_t before = pos;
            pos = dnie_append_data(rx, out, pos, total);
            if(pos == before) { // el chip no devolvio datos: evitamos bucle infinito
                result = DnieCardErrorRead;
                break;
            }
        }
        *out_len = pos;
    } while(false);

    *last_sw = sw;
    bit_buffer_free(tx);
    bit_buffer_free(rx);
    return result;
}

const char* dnie_card_result_str(DnieCardResult result) {
    switch(result) {
    case DnieCardOk:
        return "OK";
    case DnieCardErrorTransport:
        return "Radio error\n(don't move the ID)";
    case DnieCardErrorSelect:
        return "Chip rejected\nEF.CardAccess";
    case DnieCardErrorRead:
        return "READ BINARY error";
    case DnieCardErrorFormat:
        return "Unexpected format\n(not SET 0x31)";
    case DnieCardErrorTooBig:
        return "EF.CardAccess\ntoo large";
    }
    return "Unknown error";
}
