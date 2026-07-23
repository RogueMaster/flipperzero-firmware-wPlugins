#include "auriol_hg05124a.h"

#define TAG "WSProtocolAuriolHG05124A"

/*
 * Auriol HG02832 / HG05124A-DCF / Rubicson 48957
 * temperature and humidity sensor.
 *
 * Modulation: OOK PWM
 *
 * Pulse widths:
 *   252 us - short data pulse
 *   612 us - long data pulse
 *   860 us - synchronization pulse
 *
 * Preamble:
 *   one long pulse followed by three synchronization pulses,
 *   then a 40-bit data frame.
 *
 * Data layout:
 *
 *   byte 0: sensor ID
 *   byte 1: humidity in percent
 *   byte 2:
 *       bit 7    battery low
 *       bit 6    manual TX button
 *       bits 5-4 channel, zero based
 *       bits 3-0 temperature bits 11-8
 *   byte 3: temperature bits 7-0
 *   byte 4: checksum
 *
 * Temperature is a signed 12-bit value in tenths of a degree Celsius.
 *
 * Checksum:
 *   CRC-8 polynomial 0x31, initial value 0x53,
 *   calculated over the XOR of bytes 0-3.
 */

static const SubGhzBlockConst ws_protocol_auriol_hg05124a_const = {
    .te_short = 252,
    .te_long = 612,
    .te_delta = 110,
    .min_count_bit_for_found = 40,
};

#define AURIOL_HG05124A_SYNC_US         860U
#define AURIOL_HG05124A_SYNC_DELTA_US   160U
#define AURIOL_HG05124A_SYNC_COUNT      3U
#define AURIOL_HG05124A_MAX_DATA_GAP_US 1000U

typedef enum {
    AuriolHG05124ADecoderStepReset = 0,
    AuriolHG05124ADecoderStepData,
} AuriolHG05124ADecoderStep;

struct WSProtocolDecoderAuriolHG05124A {
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder;
    WSBlockGeneric generic;
    uint8_t sync_count;
};

struct WSProtocolEncoderAuriolHG05124A {
    SubGhzProtocolEncoderBase base;
    SubGhzProtocolBlockEncoder encoder;
    WSBlockGeneric generic;
};

static uint8_t ws_protocol_auriol_hg05124a_crc8(uint8_t data) {
    uint8_t crc = 0x53 ^ data;

    for(uint8_t bit = 0; bit < 8; bit++) {
        if(crc & 0x80) {
            crc = (uint8_t)((crc << 1) ^ 0x31);
        } else {
            crc <<= 1;
        }
    }

    return crc;
}

static bool ws_protocol_auriol_hg05124a_check(WSProtocolDecoderAuriolHG05124A* instance) {
    if(instance->decoder.decode_count_bit !=
       ws_protocol_auriol_hg05124a_const.min_count_bit_for_found) {
        return false;
    }

    const uint64_t data = instance->decoder.decode_data;

    const uint8_t byte0 = (data >> 32) & 0xFF;
    const uint8_t byte1 = (data >> 24) & 0xFF;
    const uint8_t byte2 = (data >> 16) & 0xFF;
    const uint8_t byte3 = (data >> 8) & 0xFF;
    const uint8_t checksum = data & 0xFF;

    const uint8_t channel = (byte2 >> 4) & 0x03;

    if(byte1 > 100) {
        return false;
    }

    if(channel > 2) {
        return false;
    }

    const uint8_t folded = byte0 ^ byte1 ^ byte2 ^ byte3;

    return ws_protocol_auriol_hg05124a_crc8(folded) == checksum;
}

static void ws_protocol_auriol_hg05124a_remote_controller(WSBlockGeneric* instance) {
    const uint64_t data = instance->data;

    const uint8_t byte0 = (data >> 32) & 0xFF;
    const uint8_t byte1 = (data >> 24) & 0xFF;
    const uint8_t byte2 = (data >> 16) & 0xFF;
    const uint8_t byte3 = (data >> 8) & 0xFF;

    int16_t temperature_raw = (int16_t)(((byte2 & 0x0F) << 8) | byte3);

    if(temperature_raw & 0x0800) {
        temperature_raw |= (int16_t)0xF000;
    }

    instance->id = byte0;
    instance->humidity = byte1;
    instance->battery_low = (byte2 >> 7) & 0x01;
    instance->btn = (byte2 >> 6) & 0x01;
    instance->channel = ((byte2 >> 4) & 0x03) + 1;
    instance->temp = (float)temperature_raw / 10.0f;
}

const SubGhzProtocolDecoder ws_protocol_auriol_hg05124a_decoder = {
    .alloc = ws_protocol_decoder_auriol_hg05124a_alloc,
    .free = ws_protocol_decoder_auriol_hg05124a_free,

    .feed = ws_protocol_decoder_auriol_hg05124a_feed,
    .reset = ws_protocol_decoder_auriol_hg05124a_reset,

    .get_hash_data = ws_protocol_decoder_auriol_hg05124a_get_hash_data,
    .serialize = ws_protocol_decoder_auriol_hg05124a_serialize,
    .deserialize = ws_protocol_decoder_auriol_hg05124a_deserialize,
    .get_string = ws_protocol_decoder_auriol_hg05124a_get_string,
};

const SubGhzProtocolEncoder ws_protocol_auriol_hg05124a_encoder = {
    .alloc = NULL,
    .free = NULL,

    .deserialize = NULL,
    .stop = NULL,
    .yield = NULL,
};

const SubGhzProtocol ws_protocol_auriol_hg05124a = {
    .name = WS_PROTOCOL_AURIOL_HG05124A_NAME,
    .type = SubGhzProtocolWeatherStation,
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_AM | SubGhzProtocolFlag_Decodable,

    .decoder = &ws_protocol_auriol_hg05124a_decoder,
    .encoder = &ws_protocol_auriol_hg05124a_encoder,
};

void* ws_protocol_decoder_auriol_hg05124a_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);

    WSProtocolDecoderAuriolHG05124A* instance = malloc(sizeof(WSProtocolDecoderAuriolHG05124A));

    instance->base.protocol = &ws_protocol_auriol_hg05124a;
    instance->generic.protocol_name = instance->base.protocol->name;

    instance->decoder.parser_step = AuriolHG05124ADecoderStepReset;
    instance->decoder.decode_data = 0;
    instance->decoder.decode_count_bit = 0;
    instance->sync_count = 0;

    return instance;
}

void ws_protocol_decoder_auriol_hg05124a_free(void* context) {
    furi_assert(context);

    free(context);
}

void ws_protocol_decoder_auriol_hg05124a_reset(void* context) {
    furi_assert(context);

    WSProtocolDecoderAuriolHG05124A* instance = context;

    instance->decoder.parser_step = AuriolHG05124ADecoderStepReset;
    instance->decoder.decode_data = 0;
    instance->decoder.decode_count_bit = 0;
    instance->sync_count = 0;
}

void ws_protocol_decoder_auriol_hg05124a_feed(void* context, bool level, uint32_t duration) {
    furi_assert(context);

    WSProtocolDecoderAuriolHG05124A* instance = context;

    if(!level) {
        if((instance->decoder.parser_step == AuriolHG05124ADecoderStepData) &&
           (duration > AURIOL_HG05124A_MAX_DATA_GAP_US)) {
            ws_protocol_decoder_auriol_hg05124a_reset(instance);
        } else if(
            (instance->decoder.parser_step == AuriolHG05124ADecoderStepReset) &&
            (duration > 1500U)) {
            instance->sync_count = 0;
        }

        return;
    }

    if(DURATION_DIFF(duration, AURIOL_HG05124A_SYNC_US) < AURIOL_HG05124A_SYNC_DELTA_US) {
        if(instance->decoder.parser_step == AuriolHG05124ADecoderStepReset) {
            instance->sync_count++;

            if(instance->sync_count >= AURIOL_HG05124A_SYNC_COUNT) {
                instance->decoder.parser_step = AuriolHG05124ADecoderStepData;
                instance->decoder.decode_data = 0;
                instance->decoder.decode_count_bit = 0;
            }
        }

        return;
    }

    if(instance->decoder.parser_step != AuriolHG05124ADecoderStepData) {
        instance->sync_count = 0;
        return;
    }

    if(DURATION_DIFF(duration, ws_protocol_auriol_hg05124a_const.te_short) <
       ws_protocol_auriol_hg05124a_const.te_delta) {
        subghz_protocol_blocks_add_bit(&instance->decoder, 0);
    } else if(
        DURATION_DIFF(duration, ws_protocol_auriol_hg05124a_const.te_long) <
        ws_protocol_auriol_hg05124a_const.te_delta) {
        subghz_protocol_blocks_add_bit(&instance->decoder, 1);
    } else {
        ws_protocol_decoder_auriol_hg05124a_reset(instance);
        return;
    }

    if(instance->decoder.decode_count_bit ==
       ws_protocol_auriol_hg05124a_const.min_count_bit_for_found) {
        if(ws_protocol_auriol_hg05124a_check(instance)) {
            instance->generic.data = instance->decoder.decode_data;
            instance->generic.data_count_bit = instance->decoder.decode_count_bit;

            ws_protocol_auriol_hg05124a_remote_controller(&instance->generic);

            if(instance->base.callback) {
                instance->base.callback(&instance->base, instance->base.context);
            }
        }

        ws_protocol_decoder_auriol_hg05124a_reset(instance);
    }
}

uint8_t ws_protocol_decoder_auriol_hg05124a_get_hash_data(void* context) {
    furi_assert(context);

    WSProtocolDecoderAuriolHG05124A* instance = context;

    return subghz_protocol_blocks_get_hash_data(
        &instance->decoder, (instance->decoder.decode_count_bit / 8) + 1);
}

SubGhzProtocolStatus ws_protocol_decoder_auriol_hg05124a_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_assert(context);

    WSProtocolDecoderAuriolHG05124A* instance = context;

    return ws_block_generic_serialize(&instance->generic, flipper_format, preset);
}

SubGhzProtocolStatus
    ws_protocol_decoder_auriol_hg05124a_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_assert(context);

    WSProtocolDecoderAuriolHG05124A* instance = context;

    return ws_block_generic_deserialize_check_count_bit(
        &instance->generic,
        flipper_format,
        ws_protocol_auriol_hg05124a_const.min_count_bit_for_found);
}

void ws_protocol_decoder_auriol_hg05124a_get_string(void* context, FuriString* output) {
    furi_assert(context);

    WSProtocolDecoderAuriolHG05124A* instance = context;

    furi_string_printf(
        output,
        "%s %dbit\r\n"
        "Key:0x%lX%08lX\r\n"
        "ID:0x%lX Ch:%d Bat:%d Btn:%d\r\n"
        "Temp:%3.1f C Hum:%d%%",
        instance->generic.protocol_name,
        instance->generic.data_count_bit,
        (uint32_t)(instance->generic.data >> 32),
        (uint32_t)instance->generic.data,
        instance->generic.id,
        instance->generic.channel,
        instance->generic.battery_low,
        instance->generic.btn,
        (double)instance->generic.temp,
        instance->generic.humidity);
}
