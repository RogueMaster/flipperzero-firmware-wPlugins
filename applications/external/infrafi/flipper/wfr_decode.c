#include "wfr_decode.h"
#include <furi.h>
#include <string.h>

#define TAG "InfraFiACK"

void wfr_ack_decode_init(WfrAckDecoder* dec) {
    memset(dec, 0, sizeof(WfrAckDecoder));
}

static void ack_decode_reset(WfrAckDecoder* dec) {
    memset(dec, 0, sizeof(WfrAckDecoder));
}

static bool ack_check_timeout(WfrAckDecoder* dec) {
    if(!dec->in_transmission) return false;

    uint32_t elapsed = furi_get_tick() - dec->start_tick;
    if(elapsed >= furi_ms_to_ticks(WFR_ACK_TIMEOUT_SEC * 1000)) {
        FURI_LOG_W(TAG, "decode timeout");
        ack_decode_reset(dec);
        return true;
    }
    return false;
}

int wfr_ack_decode_feed(
    WfrAckDecoder* dec,
    uint8_t rc6_address,
    uint8_t rc6_command,
    char* out,
    size_t out_size) {

    if(ack_check_timeout(dec)) return -1;

    /* Must have InfraFi magic in high nibble */
    if((rc6_address & WFR_RC6_MAGIC_MASK) != WFR_RC6_MAGIC) return 0;

    uint8_t frame_type = rc6_address & WFR_RC6_TYPE_MASK;

    switch(frame_type) {
    case WFR_RC6_TYPE_START: {
        uint8_t total_len = rc6_command;
        if(total_len == 0) {
            FURI_LOG_W(TAG, "START with zero length");
            return -1;
        }

        /* Retransmit of same transmission — reset cursor */
        if(dec->in_transmission && dec->expected_len == total_len) {
            dec->write_cursor = 0;
            dec->start_tick = furi_get_tick();
            FURI_LOG_D(TAG, "START retransmit (len=%d)", total_len);
            return 0;
        }

        /* New transmission */
        ack_decode_reset(dec);
        dec->expected_len = total_len;
        dec->in_transmission = true;
        dec->start_tick = furi_get_tick();
        FURI_LOG_I(TAG, "START (len=%d)", total_len);
        return 0;
    }

    case WFR_RC6_TYPE_DATA: {
        if(!dec->in_transmission) return 0;
        if(dec->write_cursor >= dec->expected_len) return 0;

        dec->payload_buf[dec->write_cursor] = rc6_command;
        dec->write_cursor++;
        return 0;
    }

    case WFR_RC6_TYPE_END: {
        if(!dec->in_transmission) return 0;

        if(dec->write_cursor != dec->expected_len) {
            FURI_LOG_D(
                TAG, "END but %d/%d bytes — waiting", dec->write_cursor, dec->expected_len);
            return 0;
        }

        /* Verify CRC-8 */
        uint8_t expected_crc = rc6_command;
        uint8_t computed_crc = wfr_crc8(dec->payload_buf, dec->expected_len);
        if(expected_crc != computed_crc) {
            FURI_LOG_W(TAG, "CRC mismatch (0x%02x vs 0x%02x)", computed_crc, expected_crc);
            dec->write_cursor = 0;
            return 0;
        }

        /* Success */
        if((size_t)dec->expected_len >= out_size) {
            FURI_LOG_E(TAG, "output buffer too small");
            ack_decode_reset(dec);
            return -1;
        }
        memcpy(out, dec->payload_buf, dec->expected_len);
        out[dec->expected_len] = '\0';
        FURI_LOG_I(TAG, "ACK decoded: %s", out);

        int result_len = dec->expected_len;
        ack_decode_reset(dec);
        return result_len;
    }

    default:
        return 0;
    }
}
