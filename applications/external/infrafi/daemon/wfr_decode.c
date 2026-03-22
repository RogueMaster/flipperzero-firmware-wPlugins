#include "wfr_decode.h"
#include <string.h>
#include <syslog.h>

#define WFR_TIMEOUT_SEC 30

void wfr_decode_init(WfrDecoder* dec) {
    memset(dec, 0, sizeof(WfrDecoder));
}

static void wfr_decode_reset(WfrDecoder* dec) {
    memset(dec, 0, sizeof(WfrDecoder));
}

static bool wfr_decode_check_timeout(WfrDecoder* dec) {
    if(!dec->in_transmission) return false;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    long elapsed = now.tv_sec - dec->start_time.tv_sec;
    if(now.tv_nsec < dec->start_time.tv_nsec) elapsed--;

    if(elapsed >= WFR_TIMEOUT_SEC) {
        syslog(LOG_WARNING, "infrafid: transmission timeout after %lds", elapsed);
        wfr_decode_reset(dec);
        return true;
    }
    return false;
}

int wfr_decode_feed_scancode(
    WfrDecoder* dec,
    uint8_t rc6_address,
    uint8_t rc6_command,
    char* out,
    size_t out_size) {

    if(wfr_decode_check_timeout(dec)) return -1;

    /* Filter: must have InfraFi magic in high nibble */
    if((rc6_address & WFR_RC6_MAGIC_MASK) != WFR_RC6_MAGIC) return 0;

    uint8_t frame_type = rc6_address & WFR_RC6_TYPE_MASK;
    uint8_t pass = rc6_address & WFR_RC6_PASS_MASK;

    switch(frame_type) {
    case WFR_RC6_TYPE_START: {
        uint8_t total_len = rc6_command;
        if(total_len == 0) {
            syslog(LOG_WARNING, "infrafid: START with zero length");
            return -1;
        }

        /* If retransmit of same transmission, reset cursor for new pass */
        if(dec->in_transmission && dec->expected_len == total_len) {
            dec->write_cursor = 0;
            dec->current_pass = pass;
            clock_gettime(CLOCK_MONOTONIC, &dec->start_time);
            syslog(
                LOG_INFO,
                "infrafid: START retransmit pass %d (len=%d)",
                pass,
                total_len);
            return 0;
        }

        /* New transmission */
        wfr_decode_reset(dec);
        dec->expected_len = total_len;
        dec->current_pass = pass;
        dec->in_transmission = true;
        clock_gettime(CLOCK_MONOTONIC, &dec->start_time);
        syslog(LOG_INFO, "infrafid: START received (len=%d)", total_len);
        return 0;
    }

    case WFR_RC6_TYPE_DATA: {
        if(!dec->in_transmission) return 0;
        if(dec->write_cursor >= dec->expected_len) return 0;

        dec->payload_buf[dec->write_cursor] = rc6_command;
        dec->write_cursor++;
        syslog(
            LOG_DEBUG,
            "infrafid: DATA byte %d/%d (0x%02x '%c')",
            dec->write_cursor,
            dec->expected_len,
            rc6_command,
            (rc6_command >= 0x20 && rc6_command <= 0x7e) ? rc6_command : '.');
        return 0;
    }

    case WFR_RC6_TYPE_END: {
        if(!dec->in_transmission) return 0;

        if(dec->write_cursor != dec->expected_len) {
            syslog(
                LOG_INFO,
                "infrafid: END but only %d/%d bytes — waiting for retransmit",
                dec->write_cursor,
                dec->expected_len);
            return 0;
        }

        /* Verify CRC-8 */
        uint8_t expected_crc = rc6_command;
        uint8_t computed_crc = wfr_crc8(dec->payload_buf, dec->expected_len);
        if(expected_crc != computed_crc) {
            syslog(
                LOG_WARNING,
                "infrafid: CRC-8 mismatch (got 0x%02x, expected 0x%02x) — waiting for retransmit",
                computed_crc,
                expected_crc);
            /* Don't reset — next retransmit pass will try again */
            dec->write_cursor = 0;
            return 0;
        }

        /* Success */
        if((size_t)dec->expected_len >= out_size) {
            syslog(LOG_ERR, "infrafid: output buffer too small");
            wfr_decode_reset(dec);
            return -1;
        }
        memcpy(out, dec->payload_buf, dec->expected_len);
        out[dec->expected_len] = '\0';
        syslog(LOG_INFO, "infrafid: transmission complete (%d bytes)", dec->expected_len);

        int result_len = dec->expected_len;
        wfr_decode_reset(dec);
        return result_len;
    }

    default:
        return 0;
    }
}
