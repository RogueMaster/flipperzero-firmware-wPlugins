#pragma once

#include "../flipper/protocol/wfr_protocol.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>

/*
 * InfraFi scancode decoder.
 * Reassembles RC-6 messages (address + command) into a WiFi credential payload.
 * Each DATA message carries one byte of the payload.
 */

typedef struct {
    uint8_t payload_buf[WFR_MAX_TOTAL_PAYLOAD + 1];
    uint8_t expected_len;   /* total payload length from START */
    uint8_t write_cursor;   /* next byte position in current pass */
    uint8_t current_pass;   /* which retransmit pass (0-3) */
    bool in_transmission;

    struct timespec start_time;
} WfrDecoder;

/* Initialize/reset the decoder. */
void wfr_decode_init(WfrDecoder* dec);

/*
 * Feed one decoded RC-6 scancode into the decoder.
 *
 * rc6_address: the RC-6 address byte
 * rc6_command: the RC-6 command byte
 * out: output buffer for the decoded WiFi string (null-terminated)
 * out_size: size of the output buffer
 *
 * Returns:
 *   >0 : payload complete, return value = string length
 *    0 : more data needed
 *   -1 : error (bad CRC, timeout, etc.)
 */
int wfr_decode_feed_scancode(
    WfrDecoder* dec,
    uint8_t rc6_address,
    uint8_t rc6_command,
    char* out,
    size_t out_size);
