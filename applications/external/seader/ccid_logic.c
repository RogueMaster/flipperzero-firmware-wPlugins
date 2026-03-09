#include "ccid_logic.h"

uint8_t seader_ccid_sequence_advance(uint8_t* sequence) {
    return (*sequence)++;
}

bool seader_ccid_payload_fits_frame(size_t payload_len, size_t uart_buf_size, size_t header_len) {
    if(header_len > uart_buf_size) {
        return false;
    }
    return payload_len <= (uart_buf_size - header_len);
}

bool seader_ccid_data_in_scratchpad(
    const uint8_t* tx_buf,
    size_t tx_buf_size,
    size_t header_len,
    const uint8_t* data,
    size_t payload_len) {
    if(data < tx_buf + header_len || data > tx_buf + tx_buf_size) {
        return false;
    }

    size_t available = (size_t)((tx_buf + tx_buf_size) - data);
    return payload_len <= available;
}

SeaderCcidStatus seader_ccid_decode_status(uint8_t status) {
    SeaderCcidStatus decoded = {
        .icc_status = status & 0x03,
        .command_status = (SeaderCcidDecodedCommandStatus)((status >> 6) & 0x03),
    };
    return decoded;
}

bool seader_ccid_response_matches_pending(bool pending, uint8_t expected_seq, uint8_t response_seq) {
    return !pending || (expected_seq == response_seq);
}

size_t seader_ccid_find_frame_start(
    const uint8_t* data,
    size_t len,
    uint8_t sync,
    uint8_t ctrl,
    uint8_t nak) {
    size_t i = 0;

    while(i + 1 < len) {
        if(i + 2 < len && data[i] == sync && data[i + 1] == nak) {
            i += 3;
            continue;
        }

        if(data[i] == sync && data[i + 1] == ctrl) {
            return i;
        }

        i++;
    }

    return len;
}

bool seader_ccid_pending_timed_out(
    bool pending,
    uint32_t pending_since_tick,
    uint32_t now_tick,
    uint32_t timeout_ticks) {
    if(!pending || timeout_ticks == 0 || pending_since_tick == 0) {
        return false;
    }

    return (now_tick - pending_since_tick) > timeout_ticks;
}

SeaderCcidDataRoute seader_ccid_route_data_block(
    bool has_sam,
    uint8_t sam_slot,
    uint8_t message_slot,
    uint8_t protocol_t) {
    if(!has_sam) {
        return SeaderCcidDataRouteAtrRecognition;
    }

    if(message_slot != sam_slot) {
        return SeaderCcidDataRouteWrongSlotError;
    }

    if(protocol_t == 0) {
        return SeaderCcidDataRouteSamT0;
    }

    return SeaderCcidDataRouteSamT1;
}
