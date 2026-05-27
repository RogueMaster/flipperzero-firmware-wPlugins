/*
 * ZeroFIDO
 * Copyright (C) 2026 Alex Stoyanov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef ZF_USB_ONLY

#include "nfc_protocol.h"

#include <string.h>

const uint8_t zf_transport_nfc_fido_aid[ZF_NFC_AID_LEN] = {
    0xA0, 0x00, 0x00, 0x06, 0x47, 0x2F, 0x00, 0x01,
};

const uint8_t zf_transport_nfc_ndef_aid[ZF_NFC_NDEF_AID_LEN] = {
    0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01,
};

const uint8_t zf_transport_nfc_select_response[6] = {'U', '2', 'F', '_', 'V', '2'};
const uint8_t zf_transport_nfc_fido2_select_response[8] = {'F', 'I', 'D', 'O', '_', '2', '_', '0'};

size_t zf_transport_nfc_normalize_le(const ZfNfcApdu *apdu) {
    if (!apdu || !apdu->has_le || apdu->le == 0) {
        return apdu && apdu->extended ? 65536U : 256U;
    }

    return apdu->le;
}

bool zf_transport_nfc_is_fido_select_apdu(const ZfNfcApdu *apdu) {
    if (!apdu || apdu->cla != 0x00U || apdu->ins != 0xA4U || apdu->p1 != 0x04U ||
        (apdu->p2 != 0x00U && apdu->p2 != 0x0CU) || !apdu->data ||
        (apdu->data_len != ZF_NFC_AID_LEN && apdu->data_len != (ZF_NFC_AID_LEN + 1U))) {
        return false;
    }

    if (memcmp(apdu->data, zf_transport_nfc_fido_aid, ZF_NFC_AID_LEN) != 0) {
        return false;
    }

    return apdu->data_len == ZF_NFC_AID_LEN || apdu->data[ZF_NFC_AID_LEN] == 0x00U;
}

bool zf_transport_nfc_is_ndef_select_apdu(const uint8_t *buffer, size_t buffer_len) {
    ZfNfcApdu apdu;

    if (!zf_transport_nfc_parse_apdu(buffer, buffer_len, &apdu)) {
        return false;
    }

    return apdu.cla == 0x00U && apdu.ins == 0xA4U && apdu.p1 == 0x04U &&
           (apdu.p2 == 0x00U || apdu.p2 == 0x0CU) && apdu.data &&
           apdu.data_len == ZF_NFC_NDEF_AID_LEN &&
           memcmp(apdu.data, zf_transport_nfc_ndef_aid, ZF_NFC_NDEF_AID_LEN) == 0;
}

/*
 * Parses the ISO 7816 APDU forms ZeroFIDO accepts: short and extended Lc/Le,
 * with the CLA chaining bit stripped into apdu->chained. Any trailing bytes
 * outside the declared body/Le are rejected.
 */
bool zf_transport_nfc_parse_apdu(const uint8_t *buffer, size_t buffer_len, ZfNfcApdu *apdu) {
    size_t index = 4;

    if (!buffer || !apdu || buffer_len < 4) {
        return false;
    }

    memset(apdu, 0, sizeof(*apdu));
    apdu->cla = buffer[0];
    apdu->ins = buffer[1];
    apdu->p1 = buffer[2];
    apdu->p2 = buffer[3];
    apdu->chained = (buffer[0] & 0x10U) != 0;
    apdu->cla &= (uint8_t)~0x10U;

    if (buffer_len == 4) {
        return true;
    }

    if (buffer_len == 5) {
        apdu->has_le = true;
        apdu->le = buffer[4];
        return true;
    }

    if (buffer[index] != 0x00U) {
        size_t lc = buffer[index++];
        if (buffer_len < index + lc) {
            return false;
        }
        apdu->data = &buffer[index];
        apdu->data_len = lc;
        index += lc;
        if (buffer_len > index) {
            if (buffer_len != index + 1U) {
                return false;
            }
            apdu->has_le = true;
            apdu->le = buffer[index];
        }
        return true;
    }

    apdu->extended = true;
    if (buffer_len == 7) {
        apdu->has_le = true;
        apdu->le = ((size_t)buffer[5] << 8) | buffer[6];
        return true;
    }
    if (buffer_len < 7) {
        return false;
    }

    apdu->data_len = ((size_t)buffer[5] << 8) | buffer[6];
    index = 7;
    if (buffer_len < index + apdu->data_len) {
        return false;
    }
    apdu->data = &buffer[index];
    index += apdu->data_len;
    if (buffer_len > index) {
        if (buffer_len != index + 2U) {
            return false;
        }
        apdu->has_le = true;
        apdu->le = ((size_t)buffer[index] << 8) | buffer[index + 1U];
    }

    return true;
}

uint16_t zf_transport_nfc_status_update_sw(size_t remaining) {
    return remaining > 0xFFU ? ZF_NFC_SW_BYTES_REMAINING
                             : (uint16_t)(ZF_NFC_SW_BYTES_REMAINING | remaining);
}

size_t zf_transport_nfc_encode_u2f_request(const ZfNfcApdu *apdu, uint8_t *out,
                                           size_t out_capacity) {
    size_t offset = 4;

    if (!apdu || !out || out_capacity < 4U) {
        return 0;
    }

    out[0] = 0x00;
    out[1] = apdu->ins;
    out[2] = apdu->p1;
    out[3] = apdu->p2;

    if (apdu->data_len == 0) {
        return 4;
    }

    if (!apdu->extended) {
        if (out_capacity < 5U + apdu->data_len || apdu->data_len > 0xFFU) {
            return 0;
        }
        out[offset++] = (uint8_t)apdu->data_len;
    } else {
        if (out_capacity < 7U + apdu->data_len || apdu->data_len > 0xFFFFU) {
            return 0;
        }
        out[offset++] = 0x00;
        out[offset++] = (uint8_t)(apdu->data_len >> 8);
        out[offset++] = (uint8_t)apdu->data_len;
    }

    memmove(&out[offset], apdu->data, apdu->data_len);
    offset += apdu->data_len;
    return offset;
}

#endif
