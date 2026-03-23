#pragma once

#include "protocol/wfr_protocol.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
 * InfraFi ACK decoder for Flipper Zero.
 * Reassembles RC-6 scancodes into a payload string.
 * Used to decode ACK responses from the daemon ("OK:<ip>" or "FAIL").
 */

typedef struct {
    uint8_t payload_buf[WFR_MAX_TOTAL_PAYLOAD + 1];
    uint8_t expected_len;
    uint8_t write_cursor;
    bool in_transmission;
    uint32_t start_tick;
} WfrAckDecoder;

/* Initialize/reset the decoder. */
void wfr_ack_decode_init(WfrAckDecoder* dec);

/*
 * Feed one decoded RC-6 scancode.
 *
 * Returns:
 *   >0 : payload complete (return value = string length, null-terminated in out)
 *    0 : more data needed
 *   -1 : error (bad CRC, timeout, etc.)
 */
int wfr_ack_decode_feed(
    WfrAckDecoder* dec,
    uint8_t rc6_address,
    uint8_t rc6_command,
    char* out,
    size_t out_size);
