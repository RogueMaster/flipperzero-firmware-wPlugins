#include "hf_sam_response_view.h"

#include <string.h>

#define SEADER_ARTEMIS_HEADER_LEN          (6U)
#define SEADER_ASN_TAG_PAYLOAD_NFC_COMMAND (0xA1U)
#define SEADER_ASN_TAG_NFC_SEND            (0xA1U)
#define SEADER_ASN_TAG_NFC_SEND_DATA       (0x80U)
#define SEADER_ASN_TAG_NFC_SEND_PROTOCOL   (0x81U)
#define SEADER_ASN_TAG_NFC_SEND_TIMEOUT    (0x82U)
#define SEADER_ASN_TAG_NFC_SEND_FORMAT     (0x85U)

typedef struct {
    uint8_t tag;
    const uint8_t* value;
    size_t len;
    const uint8_t* next;
} SeaderBerTlv;

static bool seader_ber_read_tlv(const uint8_t* cursor, const uint8_t* end, SeaderBerTlv* tlv) {
    if(!cursor || !end || !tlv || cursor >= end) {
        return false;
    }

    tlv->tag = *cursor++;
    if(cursor >= end) {
        return false;
    }

    uint8_t len_byte = *cursor++;
    size_t len = 0U;
    if((len_byte & 0x80U) == 0U) {
        len = len_byte;
    } else {
        size_t len_len = len_byte & 0x7FU;
        if(len_len == 0U || len_len > sizeof(size_t) || (size_t)(end - cursor) < len_len) {
            return false;
        }

        for(size_t i = 0U; i < len_len; i++) {
            len = (len << 8) | cursor[i];
        }
        cursor += len_len;
    }

    if((size_t)(end - cursor) < len) {
        return false;
    }

    tlv->value = cursor;
    tlv->len = len;
    tlv->next = cursor + len;
    return true;
}

static bool seader_read_be_u16(const uint8_t* value, size_t len, uint16_t* out) {
    if(!value || !out || len == 0U || len > 2U) {
        return false;
    }

    uint16_t result = 0U;
    for(size_t i = 0U; i < len; i++) {
        result = (uint16_t)((result << 8) | value[i]);
    }
    *out = result;
    return true;
}

static bool seader_read_be_u32(const uint8_t* value, size_t len, uint32_t* out) {
    if(!value || !out || len == 0U || len > 4U) {
        return false;
    }

    uint32_t result = 0U;
    for(size_t i = 0U; i < len; i++) {
        result = (result << 8) | value[i];
    }
    *out = result;
    return true;
}

bool seader_hf_sam_response_view_parse_nfc_send(
    const uint8_t* response,
    size_t response_len,
    SeaderHfSamNfcSendView* out) {
    if(!response || !out || response_len <= SEADER_ARTEMIS_HEADER_LEN) {
        return false;
    }

    SeaderHfSamNfcSendView view = {0};
    const uint8_t* end = response + response_len;
    const uint8_t* cursor = response + SEADER_ARTEMIS_HEADER_LEN;
    SeaderBerTlv payload_tlv = {0};
    SeaderBerTlv nfc_command_tlv = {0};

    if(!seader_ber_read_tlv(cursor, end, &payload_tlv) ||
       payload_tlv.tag != SEADER_ASN_TAG_PAYLOAD_NFC_COMMAND || payload_tlv.next != end) {
        return false;
    }

    if(!seader_ber_read_tlv(
           payload_tlv.value, payload_tlv.value + payload_tlv.len, &nfc_command_tlv) ||
       nfc_command_tlv.tag != SEADER_ASN_TAG_NFC_SEND ||
       nfc_command_tlv.next != payload_tlv.value + payload_tlv.len) {
        return false;
    }

    bool has_data = false;
    bool has_protocol = false;
    bool has_timeout = false;
    cursor = nfc_command_tlv.value;
    const uint8_t* nfc_send_end = nfc_command_tlv.value + nfc_command_tlv.len;
    while(cursor < nfc_send_end) {
        SeaderBerTlv field = {0};
        if(!seader_ber_read_tlv(cursor, nfc_send_end, &field)) {
            return false;
        }

        switch(field.tag) {
        case SEADER_ASN_TAG_NFC_SEND_DATA:
            view.data = field.value;
            view.data_len = field.len;
            has_data = true;
            break;
        case SEADER_ASN_TAG_NFC_SEND_PROTOCOL:
            if(!seader_read_be_u16(field.value, field.len, &view.protocol)) {
                return false;
            }
            has_protocol = true;
            break;
        case SEADER_ASN_TAG_NFC_SEND_TIMEOUT:
            if(!seader_read_be_u32(field.value, field.len, &view.timeout_us)) {
                return false;
            }
            has_timeout = true;
            break;
        case SEADER_ASN_TAG_NFC_SEND_FORMAT:
            view.format = field.value;
            view.format_len = field.len;
            break;
        default:
            break;
        }

        cursor = field.next;
    }

    if(!has_data || !has_protocol || !has_timeout) {
        return false;
    }

    memcpy(out, &view, sizeof(view));
    return true;
}
