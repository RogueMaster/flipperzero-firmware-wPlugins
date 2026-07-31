#include "ck42x_fido2.h"

#include <string.h>

static void ck_put_u32_be(uint8_t* out, uint32_t value) {
    out[0] = (uint8_t)(value >> 24);
    out[1] = (uint8_t)(value >> 16);
    out[2] = (uint8_t)(value >> 8);
    out[3] = (uint8_t)value;
}

static uint32_t ck_get_u32_be(const uint8_t* in) {
    return ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16) | ((uint32_t)in[2] << 8) |
           (uint32_t)in[3];
}

static void ck_put_u16_be(uint8_t* out, uint16_t value) {
    out[0] = (uint8_t)(value >> 8);
    out[1] = (uint8_t)value;
}

static uint16_t ck_get_u16_be(const uint8_t* in) {
    return (uint16_t)(((uint16_t)in[0] << 8) | in[1]);
}

static bool ck_fido2_hid_fail(uint8_t* error_code, uint8_t code) {
    if(error_code) *error_code = code;
    return false;
}

void ck_fido2_hid_parser_reset(CkFido2HidParser* parser) {
    if(parser) memset(parser, 0, sizeof(*parser));
}

bool ck_fido2_hid_parser_consume(
    CkFido2HidParser* parser,
    const uint8_t report[CK_FIDO2_HID_REPORT_SIZE],
    uint8_t* message_out,
    size_t message_capacity,
    size_t* message_length,
    uint32_t* message_channel,
    uint8_t* message_command,
    uint8_t* error_code) {
    uint32_t channel;
    if(message_length) *message_length = 0;
    if(error_code) *error_code = 0;
    if(!parser || !report || !message_out || !message_length || !message_channel ||
       !message_command) {
        return ck_fido2_hid_fail(error_code, CK_FIDO2_HID_ERR_INVALID_PAR);
    }

    channel = ck_get_u32_be(report);
    if(report[4] & CK_FIDO2_HID_CMD_MASK) {
        uint16_t declared = ck_get_u16_be(report + 5);
        uint8_t command = (uint8_t)(report[4] & (uint8_t)~CK_FIDO2_HID_CMD_MASK);
        if(declared > CK_FIDO2_HID_MAX_MESSAGE_SIZE || declared > message_capacity) {
            ck_fido2_hid_parser_reset(parser);
            return ck_fido2_hid_fail(error_code, CK_FIDO2_HID_ERR_INVALID_LEN);
        }
        if(declared > CK_FIDO2_HID_INIT_DATA_SIZE && message_capacity < declared) {
            ck_fido2_hid_parser_reset(parser);
            return ck_fido2_hid_fail(error_code, CK_FIDO2_HID_ERR_INVALID_LEN);
        }
        parser->channel = channel;
        parser->command = command;
        parser->length = declared;
        parser->received = 0;
        parser->next_sequence = 0;
        parser->active = true;
        size_t copied = declared < CK_FIDO2_HID_INIT_DATA_SIZE ? declared : CK_FIDO2_HID_INIT_DATA_SIZE;
        memcpy(message_out, report + CK_FIDO2_HID_INIT_HEADER_SIZE, copied);
        parser->received = (uint16_t)copied;
        if(parser->received == parser->length) {
            *message_length = parser->received;
            *message_channel = parser->channel;
            *message_command = parser->command;
            ck_fido2_hid_parser_reset(parser);
            return true;
        }
        return false;
    }

    if(!parser->active || channel != parser->channel) {
        return ck_fido2_hid_fail(error_code, CK_FIDO2_HID_ERR_INVALID_CHANNEL);
    }
    if(report[4] != parser->next_sequence) {
        ck_fido2_hid_parser_reset(parser);
        return ck_fido2_hid_fail(error_code, CK_FIDO2_HID_ERR_INVALID_SEQ);
    }

    size_t remaining = parser->length - parser->received;
    size_t copied = remaining < CK_FIDO2_HID_CONT_DATA_SIZE ? remaining : CK_FIDO2_HID_CONT_DATA_SIZE;
    memcpy(message_out + parser->received, report + CK_FIDO2_HID_CONT_HEADER_SIZE, copied);
    parser->received = (uint16_t)(parser->received + copied);
    parser->next_sequence++;
    if(parser->received != parser->length) return false;

    *message_length = parser->received;
    *message_channel = parser->channel;
    *message_command = parser->command;
    ck_fido2_hid_parser_reset(parser);
    return true;
}

size_t ck_fido2_hid_encode(
    uint32_t channel,
    uint8_t command,
    const uint8_t* message,
    size_t message_length,
    uint8_t reports[][CK_FIDO2_HID_REPORT_SIZE],
    size_t report_capacity) {
    if(!message || !reports || message_length > CK_FIDO2_HID_MAX_MESSAGE_SIZE ||
       message_length > UINT16_MAX || report_capacity == 0 || (command & CK_FIDO2_HID_CMD_MASK)) {
        return 0;
    }

    size_t required = 1;
    if(message_length > CK_FIDO2_HID_INIT_DATA_SIZE) {
        required += (message_length - CK_FIDO2_HID_INIT_DATA_SIZE + CK_FIDO2_HID_CONT_DATA_SIZE - 1U) /
                    CK_FIDO2_HID_CONT_DATA_SIZE;
    }
    if(required > report_capacity) return 0;

    memset(reports, 0, required * CK_FIDO2_HID_REPORT_SIZE);
    ck_put_u32_be(reports[0], channel);
    reports[0][4] = command | CK_FIDO2_HID_CMD_MASK;
    ck_put_u16_be(reports[0] + 5, (uint16_t)message_length);
    size_t copied = message_length < CK_FIDO2_HID_INIT_DATA_SIZE ? message_length : CK_FIDO2_HID_INIT_DATA_SIZE;
    memcpy(reports[0] + CK_FIDO2_HID_INIT_HEADER_SIZE, message, copied);

    size_t offset = copied;
    for(size_t i = 1; i < required; i++) {
        ck_put_u32_be(reports[i], channel);
        reports[i][4] = (uint8_t)(i - 1U);
        size_t remaining = message_length - offset;
        copied = remaining < CK_FIDO2_HID_CONT_DATA_SIZE ? remaining : CK_FIDO2_HID_CONT_DATA_SIZE;
        memcpy(reports[i] + CK_FIDO2_HID_CONT_HEADER_SIZE, message + offset, copied);
        offset += copied;
    }
    return required;
}

void ck_fido2_hid_init(CkFido2Hid* hid, CkCtap2* ctap2, uint32_t first_channel) {
    if(!hid) return;
    hid->ctap2 = ctap2;
    hid->next_channel =
        (first_channel == 0U || first_channel == CK_FIDO2_HID_BROADCAST_CID) ? 1U : first_channel;
    hid->cancelled = false;
}

static bool ck_hid_error(
    uint8_t error,
    uint8_t* response,
    size_t capacity,
    size_t* length,
    uint8_t* command) {
    if(capacity == 0U) return false;
    response[0] = error;
    *length = 1U;
    *command = CK_FIDO2_HID_CMD_ERROR;
    return true;
}

bool ck_fido2_hid_dispatch(
    CkFido2Hid* hid,
    uint32_t channel,
    uint8_t command,
    const uint8_t* request,
    size_t request_length,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_length,
    uint8_t* response_command) {
    if(!hid || !response || !response_length || !response_command ||
       (request_length != 0U && !request)) {
        return false;
    }
    *response_length = 0;
    if(command == CK_FIDO2_HID_CMD_INIT) {
        uint32_t assigned;
        if(request_length != 8U)
            return ck_hid_error(
                CK_FIDO2_HID_ERR_INVALID_LEN,
                response,
                response_capacity,
                response_length,
                response_command);
        if(response_capacity < 17U) return false;
        assigned = channel == CK_FIDO2_HID_BROADCAST_CID ? hid->next_channel++ : channel;
        if(hid->next_channel == 0U || hid->next_channel == CK_FIDO2_HID_BROADCAST_CID)
            hid->next_channel = 1U;
        memcpy(response, request, 8U);
        ck_put_u32_be(response + 8U, assigned);
        response[12] = 2U;
        response[13] = 1U;
        response[14] = 0U;
        response[15] = 0U;
        response[16] = CK_FIDO2_HID_CAPABILITY_CBOR | CK_FIDO2_HID_CAPABILITY_NMSG;
        *response_length = 17U;
        *response_command = CK_FIDO2_HID_CMD_INIT;
        return true;
    }
    if(channel == CK_FIDO2_HID_BROADCAST_CID || channel == 0U)
        return ck_hid_error(
            CK_FIDO2_HID_ERR_INVALID_CHANNEL,
            response,
            response_capacity,
            response_length,
            response_command);
    if(command == CK_FIDO2_HID_CMD_PING) {
        if(request_length > response_capacity)
            return ck_hid_error(
                CK_FIDO2_HID_ERR_INVALID_LEN,
                response,
                response_capacity,
                response_length,
                response_command);
        if(request_length != 0U) memcpy(response, request, request_length);
        *response_length = request_length;
        *response_command = CK_FIDO2_HID_CMD_PING;
        return true;
    }
    if(command == CK_FIDO2_HID_CMD_CBOR) {
        if(!hid->ctap2)
            return ck_hid_error(
                CK_FIDO2_HID_ERR_INVALID_CMD,
                response,
                response_capacity,
                response_length,
                response_command);
        hid->cancelled = false;
        *response_command = CK_FIDO2_HID_CMD_CBOR;
        return ck_ctap2_handle(
            hid->ctap2,
            request,
            request_length,
            response,
            response_capacity,
            response_length);
    }
    if(command == CK_FIDO2_HID_CMD_CANCEL) {
        if(request_length != 0U)
            return ck_hid_error(
                CK_FIDO2_HID_ERR_INVALID_LEN,
                response,
                response_capacity,
                response_length,
                response_command);
        hid->cancelled = true;
        *response_command = CK_FIDO2_HID_CMD_CANCEL;
        return true;
    }
    return ck_hid_error(
        CK_FIDO2_HID_ERR_INVALID_CMD,
        response,
        response_capacity,
        response_length,
        response_command);
}
