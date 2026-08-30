#include "seos_sio_collect.h"

#include "seos_common.h"

#define TAG "SeosSioCollect"

void seos_sio_collect_begin(
    SeosSioCollector* collector,
    BitBuffer* assembled,
    const uint8_t* sent,
    size_t sent_len) {
    memset(collector, 0, sizeof(SeosSioCollector));
    collector->assembled = assembled;
    bit_buffer_reset(assembled);

    if(sent_len > 0 && sent_len <= sizeof(collector->sent)) {
        memcpy(collector->sent, sent, sent_len);
        collector->sent_len = sent_len;
    }
}

SeosSioCollectResult seos_sio_collect_step(
    SeosSioCollector* collector,
    const uint8_t* rx,
    size_t rx_len,
    BitBuffer* next) {
    /* A card that never says it is finished must not be able to hold the
     * reader here. */
    if(collector->frames >= SEOS_SM_MAX_CHAINED_FRAMES) {
        FURI_LOG_W(TAG, "Too many frames without an end");
        return SeosSioCollectFailed;
    }
    collector->frames++;

    if(rx_len < sizeof(uint16_t)) {
        FURI_LOG_W(TAG, "Response too short to carry a status word");
        return SeosSioCollectFailed;
    }

    uint8_t expected_length = 0;
    SeosExchangeStep step = seos_sm_next_step(
        rx[rx_len - 2], rx[rx_len - 1], collector->already_resent, &expected_length);

    if(step == SeosExchangeResend) {
        if(collector->sent_len == 0) return SeosSioCollectFailed;

        collector->already_resent = true;
        collector->sent[collector->sent_len - 1] = expected_length;
        bit_buffer_reset(next);
        bit_buffer_append_bytes(next, collector->sent, collector->sent_len);
        return SeosSioCollectSend;
    }

    if(step == SeosExchangeFailed) {
        FURI_LOG_W(TAG, "Card answered %02x%02x", rx[rx_len - 2], rx[rx_len - 1]);
        return SeosSioCollectFailed;
    }

    size_t body_len = rx_len - sizeof(uint16_t);
    if(bit_buffer_get_size_bytes(collector->assembled) + body_len > SEOS_SM_RESPONSE_MAX) {
        FURI_LOG_W(TAG, "Chained response too long to hold");
        return SeosSioCollectFailed;
    }
    bit_buffer_append_bytes(collector->assembled, rx, body_len);

    if(step == SeosExchangeContinue) {
        uint8_t get_response[SEOS_GET_RESPONSE_LEN];
        memcpy(get_response, SEOS_GET_RESPONSE, sizeof(get_response));
        get_response[SEOS_GET_RESPONSE_LEN - 1] = expected_length;

        bit_buffer_reset(next);
        bit_buffer_append_bytes(next, get_response, sizeof(get_response));
        return SeosSioCollectSend;
    }

    return SeosSioCollectComplete;
}
