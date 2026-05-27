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

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ZF_NFC_AID_LEN 8U
#define ZF_NFC_NDEF_AID_LEN 7U
#define ZF_NFC_STATUS_PROCESSING 0x01U
#define ZF_NFC_STATUS_UPNEEDED 0x02U
#define ZF_NFC_MAX_RX_FRAME_INF_SIZE 250U
#define ZF_NFC_MAX_TX_FRAME_INF_SIZE 250U
#define ZF_NFC_MAX_FRAME_INF_SIZE ZF_NFC_MAX_RX_FRAME_INF_SIZE
/* Keep chained TX frames below listener/phone limits even when ATS advertises FSC=256. */
#define ZF_NFC_TX_CHAIN_CHUNK_SIZE 96U
#define ZF_NFC_GET_RESPONSE_CHUNK_SIZE (ZF_NFC_TX_CHAIN_CHUNK_SIZE - 2U)
#define ZF_NFC_TX_CHAIN_THRESHOLD_SIZE 128U
#define ZF_NFC_POST_SUCCESS_COOLDOWN_MS 2500U
#define ZF_NFC_INS_CTAP_MSG 0x10U
#define ZF_NFC_INS_CTAP_GET_RESPONSE 0x11U
#define ZF_NFC_INS_CTAP_CONTROL 0x12U
#define ZF_NFC_INS_ISO_GET_RESPONSE 0xC0U
#define ZF_NFC_CTAP_MSG_P1_GET_RESPONSE 0x80U

#define ZF_NFC_SW_SUCCESS 0x9000U
#define ZF_NFC_SW_STATUS_UPDATE 0x9100U
#define ZF_NFC_SW_BYTES_REMAINING 0x6100U
#define ZF_NFC_SW_WRONG_LENGTH 0x6700U
#define ZF_NFC_SW_CONDITIONS_NOT_SATISFIED 0x6985U
#define ZF_NFC_SW_WRONG_DATA 0x6A80U
#define ZF_NFC_SW_FUNCTION_NOT_SUPPORTED 0x6A81U
#define ZF_NFC_SW_FILE_NOT_FOUND 0x6A82U
#define ZF_NFC_SW_INS_NOT_SUPPORTED 0x6D00U
#define ZF_NFC_SW_CLA_NOT_SUPPORTED 0x6E00U
#define ZF_NFC_SW_WRONG_P1P2 0x6B00U
#define ZF_NFC_SW_INTERNAL_ERROR 0x6F00U

typedef struct {
    uint8_t cla;
    uint8_t ins;
    uint8_t p1;
    uint8_t p2;
    const uint8_t *data;
    size_t data_len;
    size_t le;
    bool chained;
    bool extended;
    bool has_le;
} ZfNfcApdu;

/*
 * NFC protocol helpers parse ISO7816 APDUs, recognize the FIDO AID, normalize
 * Le values, and adapt U2F APDU payloads into the legacy U2F parser format.
 */
extern const uint8_t zf_transport_nfc_fido_aid[ZF_NFC_AID_LEN];
extern const uint8_t zf_transport_nfc_ndef_aid[ZF_NFC_NDEF_AID_LEN];
extern const uint8_t zf_transport_nfc_select_response[6];
extern const uint8_t zf_transport_nfc_fido2_select_response[8];

size_t zf_transport_nfc_normalize_le(const ZfNfcApdu *apdu);
bool zf_transport_nfc_is_fido_select_apdu(const ZfNfcApdu *apdu);
bool zf_transport_nfc_is_ndef_select_apdu(const uint8_t *buffer, size_t buffer_len);
bool zf_transport_nfc_parse_apdu(const uint8_t *buffer, size_t buffer_len, ZfNfcApdu *apdu);
uint16_t zf_transport_nfc_status_update_sw(size_t remaining);
size_t zf_transport_nfc_encode_u2f_request(const ZfNfcApdu *apdu, uint8_t *out,
                                           size_t out_capacity);
