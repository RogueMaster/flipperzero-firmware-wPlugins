#include "tpms_renault.h"

#include <furi.h>

/* The stream from the CC1101 arrives as a sequence of "level + duration"
 * intervals. Durations are split into 50 us chips, and chips are grouped
 * into Manchester pairs.
 *
 * The demodulator polarity is not known in advance. What is more, on real
 * sensors the polarity of the sync word does not have to match the
 * Manchester convention used in the data, so we try them independently:
 * four state machines covering every combination. The right one is the
 * one whose CRC checks out.
 */

typedef struct {
    bool invert_sync; /**< look for an inverted sync word */
    bool invert_data; /**< "10" means one rather than "01" */

    uint32_t sync_reg; /**< shift register for the sync search */
    uint8_t bytes[TPMS_RENAULT_FRAME_BYTES];
    uint16_t bit_count;
    bool collecting;
    bool have_pending;
    bool pending_chip;
} TpmsChannel;

#define TPMS_CHANNEL_COUNT 4

struct TpmsRenaultDecoder {
    TpmsChannel channel[TPMS_CHANNEL_COUNT];
    TpmsRenaultCallback callback;
    void* context;
};

uint8_t tpms_renault_crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0x00;
    for(size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for(uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

bool tpms_renault_parse(const uint8_t* raw, TpmsRenaultFrame* frame) {
    furi_check(raw);
    furi_check(frame);

    if(tpms_renault_crc8(raw, 8) != raw[8]) return false;

    memcpy(frame->raw, raw, TPMS_RENAULT_FRAME_BYTES);
    frame->flags = raw[0] >> 2;
    frame->pressure_raw = (uint16_t)(((raw[0] & 0x03) << 8) | raw[1]);
    frame->temperature_c = (int16_t)raw[2] - 30;
    frame->id = ((uint32_t)raw[5] << 16) | ((uint32_t)raw[4] << 8) | raw[3];
    frame->unknown = (uint16_t)((raw[7] << 8) | raw[6]);
    return true;
}

static void tpms_channel_reset(TpmsChannel* channel) {
    channel->sync_reg = 0;
    channel->bit_count = 0;
    channel->collecting = false;
    channel->have_pending = false;
    memset(channel->bytes, 0, sizeof(channel->bytes));
}

static void tpms_channel_feed_chip(TpmsRenaultDecoder* decoder, TpmsChannel* channel, bool raw_chip) {
    if(!channel->collecting) {
        const bool chip = channel->invert_sync ? !raw_chip : raw_chip;
        channel->sync_reg = ((channel->sync_reg << 1) | (chip ? 1U : 0U)) & TPMS_SYNC_MASK;
        if(channel->sync_reg == TPMS_SYNC_PATTERN) {
            channel->collecting = true;
            channel->bit_count = 0;
            channel->have_pending = false;
            memset(channel->bytes, 0, sizeof(channel->bytes));
        }
        return;
    }

    const bool chip = channel->invert_data ? !raw_chip : raw_chip;

    if(!channel->have_pending) {
        channel->pending_chip = chip;
        channel->have_pending = true;
        return;
    }

    channel->have_pending = false;
    if(channel->pending_chip == chip) {
        /* "00" or "11": the frame broke, go back to searching for sync. */
        tpms_channel_reset(channel);
        return;
    }

    /* "01" -> 1, "10" -> 0 */
    const bool bit = !channel->pending_chip;
    const uint16_t index = channel->bit_count / 8;
    channel->bytes[index] = (uint8_t)((channel->bytes[index] << 1) | (bit ? 1U : 0U));
    channel->bit_count++;

    if(channel->bit_count == TPMS_RENAULT_FRAME_BITS) {
        if(tpms_renault_crc8(channel->bytes, 8) == channel->bytes[8]) {
            if(decoder->callback) decoder->callback(channel->bytes, decoder->context);
        }
        tpms_channel_reset(channel);
    }
}

TpmsRenaultDecoder* tpms_renault_decoder_alloc(TpmsRenaultCallback callback, void* context) {
    TpmsRenaultDecoder* decoder = malloc(sizeof(TpmsRenaultDecoder));
    memset(decoder, 0, sizeof(TpmsRenaultDecoder));
    decoder->callback = callback;
    decoder->context = context;

    for(size_t i = 0; i < TPMS_CHANNEL_COUNT; i++) {
        decoder->channel[i].invert_sync = (i & 1) != 0;
        decoder->channel[i].invert_data = (i & 2) != 0;
    }
    return decoder;
}

void tpms_renault_decoder_free(TpmsRenaultDecoder* decoder) {
    furi_check(decoder);
    free(decoder);
}

void tpms_renault_decoder_reset(TpmsRenaultDecoder* decoder) {
    furi_check(decoder);
    for(size_t i = 0; i < TPMS_CHANNEL_COUNT; i++) {
        tpms_channel_reset(&decoder->channel[i]);
    }
}

void tpms_renault_decoder_feed(TpmsRenaultDecoder* decoder, bool level, uint32_t duration) {
    furi_check(decoder);

    if(duration > TPMS_MAX_PULSE_US) {
        tpms_renault_decoder_reset(decoder);
        return;
    }

    uint32_t chips = (duration + TPMS_CHIP_US / 2) / TPMS_CHIP_US;
    if(chips == 0) chips = 1;

    for(uint32_t i = 0; i < chips; i++) {
        for(size_t c = 0; c < TPMS_CHANNEL_COUNT; c++) {
            tpms_channel_feed_chip(decoder, &decoder->channel[c], level);
        }
    }
}
