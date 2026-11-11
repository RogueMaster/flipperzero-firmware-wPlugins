#include <furi.h>
#include "u2f_nfc.h"
#include "u2f.h"

#include <furi_hal_random.h>
#include <toolbox/bit_buffer.h>

#include <nfc/nfc.h>
#include <nfc/nfc_listener.h>
#include <nfc/protocols/nfc_generic_event.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_listener.h>

#define TAG "U2fNfc"

// FIDO U2F application identifier (A0000006472F0001)
static const uint8_t u2f_nfc_aid[] = {0xA0, 0x00, 0x00, 0x06, 0x47, 0x2F, 0x00, 0x01};
static const uint8_t u2f_nfc_version[] = {'U', '2', 'F', '_', 'V', '2'};

// ISO7816 instruction bytes
#define U2F_INS_REGISTER     0x01
#define U2F_INS_AUTHENTICATE 0x02
#define U2F_INS_VERSION      0x03
#define U2F_INS_SELECT       0xA4
#define U2F_INS_GET_RESPONSE 0xC0

// Response chunk sizing. The ISO-DEP layer does NOT chain outbound frames and its
// listener buffer is 256 B, so large responses (Register > 256 B) are split with
// ISO7816 61xx / GET RESPONSE chaining. 240 B leaves room for PCB + SW + CRC.
#define U2F_NFC_MAX_CHUNK 240U

// Heap buffer holding the normalized request and, in-place, the U2F response
// (body + SW). Realistic Register responses (~570 B with the 365 B stock cert)
// fit comfortably; sized generously for user certificates too.
#define U2F_NFC_BUF_SIZE 2048U

struct U2fNfc {
    Nfc* nfc;
    NfcListener* listener;
    Iso14443_4aData* dev_data;
    U2fData* u2f;
    BitBuffer* tx;
    uint8_t* buf;
    uint16_t resp_len; // total cached response length (body + SW), 0 = none pending
    uint16_t resp_sent; // body bytes already transmitted (excludes final SW)
    bool selected;
};

// Parse an ISO7816 APDU (short or extended) starting at CLA. Fills lc/data/le.
// Returns false on a malformed/truncated frame.
static bool u2f_nfc_parse_apdu(
    const uint8_t* apdu,
    size_t len,
    size_t* out_lc,
    const uint8_t** out_data,
    size_t* out_le) {
    size_t lc = 0;
    size_t le = 0;
    const uint8_t* data = NULL;

    const size_t body = len - 4; // bytes after CLA INS P1 P2
    const uint8_t* b = apdu + 4;

    if(body == 0) {
        // No Lc, no Le
    } else if(body == 1) {
        // Short Le only
        le = b[0];
        if(le == 0) le = 256;
    } else if(b[0] != 0) {
        // Short Lc
        lc = b[0];
        if(body < 1 + lc) return false;
        data = b + 1;
        const size_t rem = body - 1 - lc;
        if(rem == 1) {
            le = b[1 + lc];
            if(le == 0) le = 256;
        } else if(rem != 0) {
            return false;
        }
    } else {
        // Extended (leading 0x00)
        if(body < 3) return false;
        if(body == 3) {
            // Extended Le only
            le = ((size_t)b[1] << 8) | b[2];
            if(le == 0) le = 65536;
        } else {
            lc = ((size_t)b[1] << 8) | b[2];
            if(body < 3 + lc) return false;
            data = b + 3;
            const size_t rem = body - 3 - lc;
            if(rem == 2) {
                le = ((size_t)b[3 + lc] << 8) | b[4 + lc];
                if(le == 0) le = 65536;
            } else if(rem != 0) {
                return false;
            }
        }
    }

    *out_lc = lc;
    *out_data = data;
    *out_le = le;
    return true;
}

static void u2f_nfc_send_sw(
    U2fNfc* u2f_nfc,
    Iso14443_4aListener* listener,
    uint8_t sw1,
    uint8_t sw2) {
    bit_buffer_reset(u2f_nfc->tx);
    bit_buffer_append_byte(u2f_nfc->tx, sw1);
    bit_buffer_append_byte(u2f_nfc->tx, sw2);
    iso14443_4a_listener_send_block(listener, u2f_nfc->tx);
}

// Send the next chunk of the cached response, applying 61xx chaining when more
// data remains. `le` is the length the reader is willing to accept (0 -> 256).
static void u2f_nfc_send_chunk(U2fNfc* u2f_nfc, Iso14443_4aListener* listener, size_t le) {
    bit_buffer_reset(u2f_nfc->tx);

    const uint16_t data_len = u2f_nfc->resp_len - 2; // exclude final SW
    uint16_t remaining = data_len - u2f_nfc->resp_sent;

    if(le == 0) le = 256;
    size_t chunk = remaining;
    if(chunk > U2F_NFC_MAX_CHUNK) chunk = U2F_NFC_MAX_CHUNK;
    if(chunk > le) chunk = le;

    if(chunk > 0) {
        bit_buffer_append_bytes(u2f_nfc->tx, u2f_nfc->buf + u2f_nfc->resp_sent, chunk);
        u2f_nfc->resp_sent += chunk;
    }

    const uint16_t now_remaining = data_len - u2f_nfc->resp_sent;
    if(now_remaining > 0) {
        // More data follows: status 61 XX (XX = remaining, capped at 255)
        const uint8_t xx = now_remaining > 255 ? 255 : (uint8_t)now_remaining;
        bit_buffer_append_byte(u2f_nfc->tx, 0x61);
        bit_buffer_append_byte(u2f_nfc->tx, xx);
    } else {
        // Final chunk: append the real status word cached at the end of buf
        bit_buffer_append_byte(u2f_nfc->tx, u2f_nfc->buf[data_len]);
        bit_buffer_append_byte(u2f_nfc->tx, u2f_nfc->buf[data_len + 1]);
        u2f_nfc->resp_len = 0;
        u2f_nfc->resp_sent = 0;
    }

    iso14443_4a_listener_send_block(listener, u2f_nfc->tx);
}

static void u2f_nfc_handle_msg(
    U2fNfc* u2f_nfc,
    Iso14443_4aListener* listener,
    uint8_t cla,
    uint8_t ins,
    uint8_t p1,
    uint8_t p2,
    size_t lc,
    const uint8_t* data,
    size_t le) {
    // Guard against oversized requests overflowing the working buffer.
    if(lc + 7 > U2F_NFC_BUF_SIZE) {
        u2f_nfc_send_sw(u2f_nfc, listener, 0x67, 0x00); // Wrong length
        return;
    }

    // Auto-approve user presence on tap (correct FIDO NFC behaviour).
    if(ins == U2F_INS_REGISTER || ins == U2F_INS_AUTHENTICATE) {
        u2f_confirm_user_present(u2f_nfc->u2f);
    }

    // Rebuild the request into the extended-APDU layout u2f_msg_parse expects:
    // CLA INS P1 P2, 3-byte Lc, data at offset 7.
    uint8_t* bb = u2f_nfc->buf;
    bb[0] = cla;
    bb[1] = ins;
    bb[2] = p1;
    bb[3] = p2;
    bb[4] = 0x00;
    bb[5] = (uint8_t)((lc >> 8) & 0xFF);
    bb[6] = (uint8_t)(lc & 0xFF);
    if(lc > 0 && data != NULL) {
        memcpy(bb + 7, data, lc);
    }

    const uint16_t rlen = u2f_msg_parse(u2f_nfc->u2f, bb, (uint16_t)(7 + lc));
    if(rlen < 2) {
        // Core did not produce a valid response (not ready / malformed).
        u2f_nfc_send_sw(u2f_nfc, listener, 0x6D, 0x00); // Instruction not supported
        return;
    }

    u2f_nfc->resp_len = rlen;
    u2f_nfc->resp_sent = 0;
    u2f_nfc_send_chunk(u2f_nfc, listener, le);
}

static void
    u2f_nfc_handle_apdu(U2fNfc* u2f_nfc, Iso14443_4aListener* listener, const BitBuffer* rx) {
    const size_t rx_len = bit_buffer_get_size_bytes(rx);
    // Ignore empty / header-only-incomplete frames.
    if(rx_len < 4) return;

    const uint8_t* rx_data = bit_buffer_get_data(rx);
    const uint8_t cla = rx_data[0];
    const uint8_t ins = rx_data[1];
    const uint8_t p1 = rx_data[2];
    const uint8_t p2 = rx_data[3];

    size_t lc = 0;
    size_t le = 0;
    const uint8_t* data = NULL;
    if(!u2f_nfc_parse_apdu(rx_data, rx_len, &lc, &data, &le)) {
        u2f_nfc_send_sw(u2f_nfc, listener, 0x67, 0x00); // Wrong length
        return;
    }

    switch(ins) {
    case U2F_INS_SELECT:
        if(data != NULL && lc == sizeof(u2f_nfc_aid) &&
           memcmp(data, u2f_nfc_aid, sizeof(u2f_nfc_aid)) == 0) {
            u2f_nfc->selected = true;
            u2f_nfc->resp_len = 0;
            u2f_nfc->resp_sent = 0;
            bit_buffer_reset(u2f_nfc->tx);
            bit_buffer_append_bytes(u2f_nfc->tx, u2f_nfc_version, sizeof(u2f_nfc_version));
            bit_buffer_append_byte(u2f_nfc->tx, 0x90);
            bit_buffer_append_byte(u2f_nfc->tx, 0x00);
            iso14443_4a_listener_send_block(listener, u2f_nfc->tx);
        } else {
            u2f_nfc_send_sw(u2f_nfc, listener, 0x6A, 0x82); // File/application not found
        }
        break;

    case U2F_INS_REGISTER:
    case U2F_INS_AUTHENTICATE:
    case U2F_INS_VERSION:
        u2f_nfc_handle_msg(u2f_nfc, listener, cla, ins, p1, p2, lc, data, le);
        break;

    case U2F_INS_GET_RESPONSE:
        if(u2f_nfc->resp_len == 0) {
            u2f_nfc_send_sw(u2f_nfc, listener, 0x6A, 0x88); // Referenced data not found
        } else {
            u2f_nfc_send_chunk(u2f_nfc, listener, le);
        }
        break;

    default:
        u2f_nfc_send_sw(u2f_nfc, listener, 0x6D, 0x00); // Instruction not supported
        break;
    }
}

static NfcCommand u2f_nfc_listener_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    furi_assert(event.protocol == NfcProtocolIso14443_4a);
    furi_assert(event.event_data);

    U2fNfc* u2f_nfc = context;
    Iso14443_4aListenerEvent* iso_event = event.event_data;
    Iso14443_4aListener* listener = event.instance;

    switch(iso_event->type) {
    case Iso14443_4aListenerEventTypeReceivedData:
        u2f_nfc_handle_apdu(u2f_nfc, listener, iso_event->data->buffer);
        break;
    case Iso14443_4aListenerEventTypeHalted:
    case Iso14443_4aListenerEventTypeFieldOff:
        // Reader gone: drop selection and any half-sent chained response.
        u2f_nfc->selected = false;
        u2f_nfc->resp_len = 0;
        u2f_nfc->resp_sent = 0;
        break;
    }

    return NfcCommandContinue;
}

U2fNfc* u2f_nfc_start(U2fData* u2f_inst) {
    U2fNfc* u2f_nfc = malloc(sizeof(U2fNfc));
    u2f_nfc->u2f = u2f_inst;
    u2f_nfc->buf = malloc(U2F_NFC_BUF_SIZE);
    u2f_nfc->tx = bit_buffer_alloc(U2F_NFC_BUF_SIZE);
    u2f_nfc->resp_len = 0;
    u2f_nfc->resp_sent = 0;
    u2f_nfc->selected = false;

    // Build ISO14443-4A device data programmatically (no file on disk).
    u2f_nfc->dev_data = iso14443_4a_alloc();
    iso14443_4a_reset(u2f_nfc->dev_data);

    Iso14443_3aData* iso3 = iso14443_4a_get_base_data(u2f_nfc->dev_data);
    uint8_t uid[7];
    furi_hal_random_fill_buf(uid, sizeof(uid));
    uid[0] = 0x04; // avoid the reserved cascade-tag value (0x88) as first byte
    iso14443_3a_set_uid(iso3, uid, sizeof(uid));
    const uint8_t atqa[2] = {0x44, 0x00};
    iso14443_3a_set_atqa(iso3, atqa);
    iso14443_3a_set_sak(iso3, 0x20); // ISO14443-4 compliant

    // ATS: T0 = TA1|TB1 present, FSCI=8 (256 B frames so extended-APDU Authenticate
    // requests arrive in a single frame). TB1 high FWI gives a long response window
    // for the slow on-device ECDSA signature.
    Iso14443_4aAtsData* ats = &u2f_nfc->dev_data->ats_data;
    ats->tl = 4; // TL + T0 + TA1 + TB1
    ats->t0 = 0x38; // TA1(0x10) | TB1(0x20) | FSCI=8(0x08)
    ats->ta_1 = 0x80; // 106 kbit/s both directions only
    ats->tb_1 = 0xE0; // FWI=14 (~4.9 s), SFGI=0
    ats->tc_1 = 0x00;

    // Reflect "connected" state to the UI (there is no hardware connect event here).
    u2f_set_state(u2f_inst, 1);

    u2f_nfc->nfc = nfc_alloc();
    u2f_nfc->listener =
        nfc_listener_alloc(u2f_nfc->nfc, NfcProtocolIso14443_4a, u2f_nfc->dev_data);
    nfc_listener_start(u2f_nfc->listener, u2f_nfc_listener_callback, u2f_nfc);

    return u2f_nfc;
}

void u2f_nfc_stop(U2fNfc* u2f_nfc) {
    furi_assert(u2f_nfc);

    nfc_listener_stop(u2f_nfc->listener);
    nfc_listener_free(u2f_nfc->listener);
    nfc_free(u2f_nfc->nfc);
    iso14443_4a_free(u2f_nfc->dev_data);
    bit_buffer_free(u2f_nfc->tx);
    free(u2f_nfc->buf);
    free(u2f_nfc);
}
