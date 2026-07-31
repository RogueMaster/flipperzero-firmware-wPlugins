#include "ck42x_fido2.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void round_trip(size_t length) {
    uint8_t input[CK_FIDO2_HID_MAX_MESSAGE_SIZE];
    uint8_t output[CK_FIDO2_HID_MAX_MESSAGE_SIZE];
    uint8_t reports[CK_FIDO2_HID_MAX_FRAMES][CK_FIDO2_HID_REPORT_SIZE];
    CkFido2HidParser parser = {0};
    size_t report_count;
    size_t output_length = 0;
    uint32_t output_channel = 0;
    uint8_t output_command = 0;
    uint8_t error = 0;

    for(size_t i = 0; i < length; i++) input[i] = (uint8_t)(i * 37U + 11U);
    report_count = ck_fido2_hid_encode(0x12345678U, CK_FIDO2_HID_CMD_MSG, input, length, reports,
                                       CK_FIDO2_HID_MAX_FRAMES);
    assert(report_count > 0);
    for(size_t i = 0; i < report_count; i++) {
        bool complete = ck_fido2_hid_parser_consume(
            &parser,
            reports[i],
            output,
            sizeof(output),
            &output_length,
            &output_channel,
            &output_command,
            &error);
        if(i + 1U < report_count) assert(!complete);
        else assert(complete);
    }
    assert(error == 0);
    assert(output_length == length);
    assert(output_channel == 0x12345678U);
    assert(output_command == CK_FIDO2_HID_CMD_MSG);
    assert(memcmp(input, output, length) == 0);
}

static void rejects_bad_sequence(void) {
    uint8_t input[100] = {0};
    uint8_t output[100] = {0};
    uint8_t reports[3][CK_FIDO2_HID_REPORT_SIZE];
    CkFido2HidParser parser = {0};
    size_t length = 0;
    uint32_t channel = 0;
    uint8_t command = 0;
    uint8_t error = 0;

    assert(ck_fido2_hid_encode(7, CK_FIDO2_HID_CMD_MSG, input, sizeof(input), reports, 3) == 2);
    assert(!ck_fido2_hid_parser_consume(&parser, reports[0], output, sizeof(output), &length,
                                        &channel, &command, &error));
    reports[1][4] = 1;
    assert(!ck_fido2_hid_parser_consume(&parser, reports[1], output, sizeof(output), &length,
                                        &channel, &command, &error));
    assert(error == CK_FIDO2_HID_ERR_INVALID_SEQ);
}

static void dispatches_init_ping_cancel_and_error(void) {
    CkFido2Hid hid;
    uint8_t response[64];
    size_t response_length;
    uint8_t response_command;
    const uint8_t nonce[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    const uint8_t ping[3] = {9, 8, 7};

    ck_fido2_hid_init(&hid, NULL, 0x01020304U);
    assert(ck_fido2_hid_dispatch(
        &hid,
        CK_FIDO2_HID_BROADCAST_CID,
        CK_FIDO2_HID_CMD_INIT,
        nonce,
        sizeof(nonce),
        response,
        sizeof(response),
        &response_length,
        &response_command));
    assert(response_command == CK_FIDO2_HID_CMD_INIT);
    assert(response_length == 17U);
    assert(memcmp(response, nonce, sizeof(nonce)) == 0);
    assert(response[8] == 1U && response[9] == 2U && response[10] == 3U && response[11] == 4U);
    assert(
        response[16] ==
        (CK_FIDO2_HID_CAPABILITY_CBOR | CK_FIDO2_HID_CAPABILITY_NMSG));

    assert(ck_fido2_hid_dispatch(
        &hid, 0x01020304U, CK_FIDO2_HID_CMD_PING, ping, sizeof(ping),
        response, sizeof(response), &response_length, &response_command));
    assert(response_command == CK_FIDO2_HID_CMD_PING);
    assert(response_length == sizeof(ping) && memcmp(response, ping, sizeof(ping)) == 0);

    assert(ck_fido2_hid_dispatch(
        &hid, 0x01020304U, CK_FIDO2_HID_CMD_CANCEL, NULL, 0,
        response, sizeof(response), &response_length, &response_command));
    assert(hid.cancelled && response_length == 0U);

    assert(ck_fido2_hid_dispatch(
        &hid, 0x01020304U, 0x22U, NULL, 0,
        response, sizeof(response), &response_length, &response_command));
    assert(response_command == CK_FIDO2_HID_CMD_ERROR);
    assert(response_length == 1U && response[0] == CK_FIDO2_HID_ERR_INVALID_CMD);
}

int main(void) {
    round_trip(0);
    round_trip(1);
    round_trip(CK_FIDO2_HID_INIT_DATA_SIZE);
    round_trip(CK_FIDO2_HID_INIT_DATA_SIZE + 1U);
    round_trip(CK_FIDO2_HID_MAX_MESSAGE_SIZE);
    rejects_bad_sequence();
    dispatches_init_ping_cancel_and_error();
    puts("OK: CK42X CTAPHID framing tests passed");
    return 0;
}
