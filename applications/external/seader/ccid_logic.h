#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    SeaderCcidDecodedCommandStatusProcessed = 0,
    SeaderCcidDecodedCommandStatusFailed = 1,
    SeaderCcidDecodedCommandStatusTimeExtension = 2,
} SeaderCcidDecodedCommandStatus;

typedef struct {
    uint8_t icc_status;
    SeaderCcidDecodedCommandStatus command_status;
} SeaderCcidStatus;

typedef enum {
    SeaderCcidDataRouteSamT0 = 0,
    SeaderCcidDataRouteSamT1 = 1,
    SeaderCcidDataRouteAtrRecognition = 2,
    SeaderCcidDataRouteWrongSlotError = 3,
} SeaderCcidDataRoute;

uint8_t seader_ccid_sequence_advance(uint8_t* sequence);
bool seader_ccid_payload_fits_frame(size_t payload_len, size_t uart_buf_size, size_t header_len);
bool seader_ccid_data_in_scratchpad(
    const uint8_t* tx_buf,
    size_t tx_buf_size,
    size_t header_len,
    const uint8_t* data,
    size_t payload_len);
SeaderCcidStatus seader_ccid_decode_status(uint8_t status);
bool seader_ccid_response_matches_pending(bool pending, uint8_t expected_seq, uint8_t response_seq);
size_t seader_ccid_find_frame_start(
    const uint8_t* data,
    size_t len,
    uint8_t sync,
    uint8_t ctrl,
    uint8_t nak);
bool seader_ccid_pending_timed_out(
    bool pending,
    uint32_t pending_since_tick,
    uint32_t now_tick,
    uint32_t timeout_ticks);
SeaderCcidDataRoute seader_ccid_route_data_block(
    bool has_sam,
    uint8_t sam_slot,
    uint8_t message_slot,
    uint8_t protocol_t);
