/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * Command and response APDUs, and the handful of commands ICAO Doc 9303
 * builds out of them.
 *
 * Every builder only fills a descriptor; nothing is serialised until
 * emrtd_apdu_encode() is called, which is what lets Secure Messaging take the
 * same descriptor, protect it and encode the result instead.
 */

#include "emrtd_apdu.h"

#include <string.h>

/** ICAO Doc 9303 part 11 section 4.2. */
const uint8_t EMRTD_AID[7] = {0xA0, 0x00, 0x00, 0x02, 0x47, 0x10, 0x01};

/**
 * Turn a wanted byte count into an Le the transport can actually carry.
 *
 * The Flipper announces FSD = 256 in RATS and its ISO 14443-4 layer does not
 * reassemble a chained response, so an answer longer than one frame never
 * arrives whatever the chip is asked for. Zero is read as "as much as
 * possible" for the same reason: it is the only sense in which it is useful.
 * See docs/platform.md.
 */
static int emrtd_apdu_clamp_le(size_t length) {
    if(length == 0 || length > (size_t)EMRTD_LE_MAX) {
        return EMRTD_LE_MAX;
    }
    return (int)length;
}

EmrtdError emrtd_apdu_encode(
    const EmrtdCommandApdu* command,
    uint8_t* out,
    size_t out_size,
    size_t* out_len) {
    if(command == NULL || out == NULL || out_len == NULL) {
        return EmrtdErrorInvalidInput;
    }
    if(command->data_len > 0 && command->data == NULL) {
        return EmrtdErrorInvalidInput;
    }
    /* ISO/IEC 7816-4 allows Le up to 65536; 0 has no meaning in this struct. */
    if(command->le != EMRTD_LE_NONE && (command->le < 1 || command->le > 65536)) {
        return EmrtdErrorInvalidInput;
    }
    /* Lc is three bytes at most, so the data field is bounded by 65535. */
    if(command->data_len > 0xFFFF) {
        return EmrtdErrorInvalidInput;
    }

    const bool has_data = command->data_len > 0;
    const bool has_le = command->le != EMRTD_LE_NONE;
    const bool extended = command->data_len > 0xFF || command->le > EMRTD_LE_MAX;

    size_t needed = 4;
    if(extended) {
        /* Case 2E carries no Lc, so its Le field is three bytes, not two. */
        needed += has_data ? (3 + command->data_len + (has_le ? 2 : 0)) : (has_le ? 3 : 0);
    } else {
        needed += (has_data ? 1 + command->data_len : 0) + (has_le ? 1 : 0);
    }
    if(needed > out_size) {
        return EmrtdErrorBufferTooSmall;
    }

    size_t pos = 0;
    out[pos++] = command->cla;
    out[pos++] = command->ins;
    out[pos++] = command->p1;
    out[pos++] = command->p2;

    if(!extended) {
        if(has_data) {
            out[pos++] = (uint8_t)command->data_len;
            memcpy(out + pos, command->data, command->data_len);
            pos += command->data_len;
        }
        if(has_le) {
            /* 256 is requested as a zero byte (ISO/IEC 7816-4 table 6). */
            out[pos++] = (uint8_t)(command->le == EMRTD_LE_MAX ? 0x00 : command->le);
        }
    } else {
        out[pos++] = 0x00; /* Extended length marker. */
        if(has_data) {
            out[pos++] = (uint8_t)(command->data_len >> 8);
            out[pos++] = (uint8_t)command->data_len;
            memcpy(out + pos, command->data, command->data_len);
            pos += command->data_len;
        }
        if(has_le) {
            /* 65536 is requested as two zero bytes, for the same reason. */
            const uint32_t le = (command->le >= 65536) ? 0u : (uint32_t)command->le;
            out[pos++] = (uint8_t)(le >> 8);
            out[pos++] = (uint8_t)le;
        }
    }

    *out_len = pos;
    return EmrtdErrorNone;
}

EmrtdError emrtd_apdu_decode(const uint8_t* raw, size_t raw_len, EmrtdResponseApdu* out) {
    if(raw == NULL || out == NULL) {
        return EmrtdErrorInvalidInput;
    }
    /* Every response ends in SW1 SW2; anything shorter is not one. */
    if(raw_len < 2) {
        return EmrtdErrorProtocol;
    }

    out->data = raw;
    out->data_len = raw_len - 2;
    out->sw = (uint16_t)((raw[raw_len - 2] << 8) | raw[raw_len - 1]);
    return EmrtdErrorNone;
}

/** Fill a descriptor with the fields every builder sets. */
static void emrtd_apdu_init(
    EmrtdCommandApdu* out,
    uint8_t cla,
    uint8_t ins,
    uint8_t p1,
    uint8_t p2,
    const uint8_t* data,
    size_t data_len,
    int le) {
    out->cla = cla;
    out->ins = ins;
    out->p1 = p1;
    out->p2 = p2;
    out->data = data;
    out->data_len = data_len;
    out->le = le;
}

void emrtd_apdu_select_application(EmrtdCommandApdu* out, const uint8_t* aid, size_t aid_len) {
    if(out == NULL) {
        return;
    }
    /*
     * P1 = 04 selects by name, P2 = 0C asks for no response data, which is
     * what ICAO 9303-11 section 4.2 specifies. The descriptor points at the
     * caller's identifier, so it has to outlive the exchange.
     */
    emrtd_apdu_init(out, 0x00, 0xA4, 0x04, 0x0C, aid, aid_len, EMRTD_LE_NONE);
}

void emrtd_apdu_select_file(EmrtdCommandApdu* out, uint8_t fid[2]) {
    if(out == NULL) {
        return;
    }
    /* P1 = 02 selects an elementary file under the current application. */
    emrtd_apdu_init(out, 0x00, 0xA4, 0x02, 0x0C, fid, 2, EMRTD_LE_NONE);
}

void emrtd_apdu_get_challenge(EmrtdCommandApdu* out) {
    if(out == NULL) {
        return;
    }
    /* BAC needs exactly eight bytes of RND.IC (9303-11 section 4.3.2). */
    emrtd_apdu_init(out, 0x00, 0x84, 0x00, 0x00, NULL, 0, 8);
}

void emrtd_apdu_external_authenticate(EmrtdCommandApdu* out, const uint8_t* data, size_t len) {
    if(out == NULL) {
        return;
    }
    /*
     * The chip answers with as many bytes as it was sent: E.IC || M.IC, the
     * mirror of the E.IFD || M.IFD it received.
     */
    emrtd_apdu_init(out, 0x00, 0x82, 0x00, 0x00, data, len, emrtd_apdu_clamp_le(len));
}

void emrtd_apdu_read_binary(EmrtdCommandApdu* out, uint16_t offset, size_t length) {
    if(out == NULL) {
        return;
    }
    /* Bit 8 of P1 marks the short identifier form, so the offset keeps 15 bits. */
    const uint8_t p1 = (uint8_t)((offset >> 8) & 0x7F);
    const uint8_t p2 = (uint8_t)(offset & 0xFF);
    emrtd_apdu_init(out, 0x00, 0xB0, p1, p2, NULL, 0, emrtd_apdu_clamp_le(length));
}

void emrtd_apdu_read_binary_sfi(EmrtdCommandApdu* out, uint8_t sfi, uint8_t offset, size_t length) {
    if(out == NULL) {
        return;
    }
    /*
     * With bit 8 of P1 set, bits 1 to 5 hold the short file identifier and P2
     * becomes the offset, which is why this form only reaches 255 bytes. It
     * saves the SELECT, so it is used for the first read of a file.
     */
    const uint8_t p1 = (uint8_t)(0x80 | (sfi & 0x1F));
    emrtd_apdu_init(out, 0x00, 0xB0, p1, offset, NULL, 0, emrtd_apdu_clamp_le(length));
}

EmrtdError emrtd_apdu_read_binary_odd(
    EmrtdCommandApdu* out,
    uint32_t offset,
    size_t length,
    uint8_t* offset_do,
    size_t offset_do_size) {
    if(out == NULL || offset_do == NULL) {
        return EmrtdErrorInvalidInput;
    }

    /* DO '54' holds the offset big endian; two bytes suffice below 64 KiB. */
    const size_t width = (offset > 0xFFFFu) ? 3 : 2;
    if(offset_do_size < 2 + width) {
        return EmrtdErrorBufferTooSmall;
    }

    offset_do[0] = 0x54;
    offset_do[1] = (uint8_t)width;
    for(size_t i = 0; i < width; i++) {
        offset_do[2 + i] = (uint8_t)(offset >> ((width - 1 - i) * 8));
    }

    /* P1-P2 at zero addresses the currently selected elementary file. */
    emrtd_apdu_init(
        out, 0x00, 0xB1, 0x00, 0x00, offset_do, 2 + width, emrtd_apdu_clamp_le(length));
    return EmrtdErrorNone;
}

void emrtd_apdu_mse_set_at(EmrtdCommandApdu* out, const uint8_t* data, size_t len) {
    if(out == NULL) {
        return;
    }
    /*
     * P1 = C1 sets the computation template, P2 = A4 the authentication
     * template: the pair that starts PACE or Chip Authentication
     * (ICAO 9303-11 sections 4.4.4 and 6.2).
     */
    emrtd_apdu_init(out, 0x00, 0x22, 0xC1, 0xA4, data, len, EMRTD_LE_NONE);
}

void emrtd_apdu_general_authenticate(
    EmrtdCommandApdu* out,
    const uint8_t* data,
    size_t len,
    bool chaining) {
    if(out == NULL) {
        return;
    }
    /*
     * Every step of PACE but the last is one link of a chain, marked by bit 5
     * of the class byte (ISO/IEC 7816-4 section 5.1.1).
     */
    const uint8_t cla = chaining ? 0x10 : 0x00;
    emrtd_apdu_init(out, cla, 0x86, 0x00, 0x00, data, len, EMRTD_LE_MAX);
}
