#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ck42x_ctap2.h"

/* USB CTAPHID packet geometry for a full-size HID report. */
#define CK_FIDO2_HID_REPORT_SIZE 64U
#define CK_FIDO2_HID_INIT_HEADER_SIZE 7U
#define CK_FIDO2_HID_CONT_HEADER_SIZE 5U
#define CK_FIDO2_HID_INIT_DATA_SIZE \
    (CK_FIDO2_HID_REPORT_SIZE - CK_FIDO2_HID_INIT_HEADER_SIZE)
#define CK_FIDO2_HID_CONT_DATA_SIZE \
    (CK_FIDO2_HID_REPORT_SIZE - CK_FIDO2_HID_CONT_HEADER_SIZE)
#define CK_FIDO2_HID_BROADCAST_CID 0xffffffffU
#define CK_FIDO2_HID_CMD_MASK 0x80U

#define CK_FIDO2_HID_CMD_INIT 0x06U
#define CK_FIDO2_HID_CMD_PING 0x01U
#define CK_FIDO2_HID_CMD_MSG 0x03U
#define CK_FIDO2_HID_CMD_CBOR 0x10U
#define CK_FIDO2_HID_CMD_CANCEL 0x11U
#define CK_FIDO2_HID_CMD_KEEPALIVE 0x3bU
#define CK_FIDO2_HID_CMD_ERROR 0x3fU

#define CK_FIDO2_HID_CAPABILITY_CBOR 0x04U
#define CK_FIDO2_HID_CAPABILITY_NMSG 0x08U

#define CK_FIDO2_HID_ERR_INVALID_CMD 0x01U
#define CK_FIDO2_HID_ERR_INVALID_PAR 0x02U
#define CK_FIDO2_HID_ERR_INVALID_LEN 0x03U
#define CK_FIDO2_HID_ERR_INVALID_SEQ 0x04U
#define CK_FIDO2_HID_ERR_MSG_TIMEOUT 0x05U
#define CK_FIDO2_HID_ERR_CHANNEL_BUSY 0x06U
#define CK_FIDO2_HID_ERR_LOCK_REQUIRED 0x0aU
#define CK_FIDO2_HID_ERR_INVALID_CHANNEL 0x0bU
#define CK_FIDO2_KEEPALIVE_STATUS_UPNEEDED 0x02U

#define CK_FIDO2_HID_MAX_MESSAGE_SIZE 7609U
#define CK_FIDO2_HID_MAX_FRAMES \
    (1U + ((CK_FIDO2_HID_MAX_MESSAGE_SIZE - CK_FIDO2_HID_INIT_DATA_SIZE + \
            CK_FIDO2_HID_CONT_DATA_SIZE - 1U) / CK_FIDO2_HID_CONT_DATA_SIZE))

typedef struct {
    uint32_t channel;
    uint8_t command;
    uint16_t length;
    uint16_t received;
    uint8_t next_sequence;
    bool active;
} CkFido2HidParser;

typedef struct {
    CkCtap2* ctap2;
    uint32_t next_channel;
    bool cancelled;
} CkFido2Hid;

void ck_fido2_hid_parser_reset(CkFido2HidParser* parser);

/*
 * Consume one 64-byte CTAPHID report. The parser validates channel, command,
 * sequence, and declared length. It returns true only when a complete message
 * has been received; payload is then available in message_out.
 */
bool ck_fido2_hid_parser_consume(
    CkFido2HidParser* parser,
    const uint8_t report[CK_FIDO2_HID_REPORT_SIZE],
    uint8_t* message_out,
    size_t message_capacity,
    size_t* message_length,
    uint32_t* message_channel,
    uint8_t* message_command,
    uint8_t* error_code);

/* Encode one complete message into CTAPHID reports. */
size_t ck_fido2_hid_encode(
    uint32_t channel,
    uint8_t command,
    const uint8_t* message,
    size_t message_length,
    uint8_t reports[][CK_FIDO2_HID_REPORT_SIZE],
    size_t report_capacity);

void ck_fido2_hid_init(CkFido2Hid* hid, CkCtap2* ctap2, uint32_t first_channel);
bool ck_fido2_hid_dispatch(
    CkFido2Hid* hid,
    uint32_t channel,
    uint8_t command,
    const uint8_t* request,
    size_t request_length,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_length,
    uint8_t* response_command);
