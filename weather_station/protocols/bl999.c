/* bl999.c */
#include "bl999.h"

#define TAG "WSProtocolBL999"

/*
 * BL999 Temperature and Humidity Sensor Decoder
 * Message Format: 36 bits
 * | Channel | PowerUUID | Battery | Temperature | Humidity | Checksum |
 * | 2 bits  | 6 bits    | 1 bit   | 12 bits     | 8 bits   | 4 bits   |
 */

static const SubGhzBlockConst ws_protocol_bl999_const = {
    .te_short = 550, // ориентир для короткого HIGH
    .te_long = 1850, // ориентир для бита '0' (см. ниже комментарии)
    .te_delta = 500, // допуск (±)
    .min_count_bit_for_found = 36,
};

struct WSProtocolDecoderBL999 {
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder; /* счетчики, parser_step, decode_data и т.п. */
    WSBlockGeneric generic; /* финальные распознанные данные (id, temp и т.п.) */
};

struct WSProtocolEncoderBL999 {
    SubGhzProtocolEncoderBase base;
    SubGhzProtocolBlockEncoder encoder;
    WSBlockGeneric generic;
};

typedef enum {
    BL999DecoderStepReset = 0,
    BL999DecoderStepGotStart,
    BL999DecoderStepGotHigh,
    BL999DecoderStepCheckBit,
} BL999DecoderStep;

const SubGhzProtocolDecoder ws_protocol_bl999_decoder = {
    .alloc = ws_protocol_decoder_bl999_alloc,
    .free = ws_protocol_decoder_bl999_free,

    .feed = ws_protocol_decoder_bl999_feed,
    .reset = ws_protocol_decoder_bl999_reset,

    .get_hash_data = ws_protocol_decoder_bl999_get_hash_data,
    .serialize = ws_protocol_decoder_bl999_serialize,
    .deserialize = ws_protocol_decoder_bl999_deserialize,
    .get_string = ws_protocol_decoder_bl999_get_string,
};

const SubGhzProtocolEncoder ws_protocol_bl999_encoder = {
    .alloc = NULL,
    .free = NULL,
    .deserialize = NULL,
    .stop = NULL,
    .yield = NULL,
};

const SubGhzProtocol ws_protocol_bl999 = {
    .name = WS_PROTOCOL_BL999_NAME,
    .type = SubGhzProtocolWeatherStation,
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_AM | SubGhzProtocolFlag_Decodable,
    .decoder = &ws_protocol_bl999_decoder,
    .encoder = &ws_protocol_bl999_encoder,
};

static bool ws_protocol_bl999_remote_controller(WSBlockGeneric* instance);

void* ws_protocol_decoder_bl999_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment); // not used, prevents compiler warning
    WSProtocolDecoderBL999* d = malloc(sizeof(WSProtocolDecoderBL999));
    d->base.protocol = &ws_protocol_bl999;
    d->generic.protocol_name = d->base.protocol->name;

    /* SubGhzBlockDecoder */
    d->decoder.parser_step = BL999DecoderStepReset;
    d->decoder.decode_data = 0ULL;
    d->decoder.decode_count_bit = 0;
    d->decoder.te_last = 0;
    return d;
}

void ws_protocol_decoder_bl999_free(void* context) {
    furi_assert(context);
    WSProtocolDecoderBL999* d = context;
    free(d);
}

void ws_protocol_decoder_bl999_reset(void* context) {
    furi_assert(context);
    WSProtocolDecoderBL999* d = context;
    d->decoder.parser_step = BL999DecoderStepReset;
    d->decoder.decode_data = 0ULL;
    d->decoder.decode_count_bit = 0;
    d->decoder.te_last = 0;
}

void ws_protocol_decoder_bl999_feed(void* context, bool level, uint32_t duration) {
    furi_assert(context);
    WSProtocolDecoderBL999* d = context;

    switch(d->decoder.parser_step) {
    case BL999DecoderStepReset:
        if(!level) {
            if(DURATION_DIFF(duration, 9000) < 1300) {
                d->decoder.parser_step = BL999DecoderStepGotStart;
                d->decoder.decode_data = 0ULL;
                d->decoder.decode_count_bit = 0;
            }
        }
        break;

    case BL999DecoderStepGotStart:
        if(level) {
            if(DURATION_DIFF(duration, ws_protocol_bl999_const.te_short) <
               ws_protocol_bl999_const.te_delta) {
                d->decoder.parser_step = BL999DecoderStepGotHigh;
            } else {
                d->decoder.parser_step = BL999DecoderStepReset;
            }
        } else {
            d->decoder.parser_step = BL999DecoderStepReset;
        }
        break;

    case BL999DecoderStepGotHigh:
        if(!level) {
            d->decoder.parser_step = BL999DecoderStepCheckBit;
            d->decoder.te_last = duration;
        } else {
            d->decoder.parser_step = BL999DecoderStepReset;
        }
        break;

    case BL999DecoderStepCheckBit: {
        uint32_t low_dur = d->decoder.te_last;
        if(DURATION_DIFF(low_dur, 1850) < 600) {
            subghz_protocol_blocks_add_bit(&d->decoder, 0);
        } else if(DURATION_DIFF(low_dur, 3900) < 700) {
            subghz_protocol_blocks_add_bit(&d->decoder, 1);
        } else {
            if(DURATION_DIFF(low_dur, 9000) < 1300) {
                if(d->decoder.decode_count_bit ==
                   ws_protocol_bl999_const.min_count_bit_for_found) {
                    d->generic.data = d->decoder.decode_data;
                    d->generic.data_count_bit = d->decoder.decode_count_bit;
                    if(!ws_protocol_bl999_remote_controller(&d->generic)) {
                        d->decoder.parser_step = BL999DecoderStepReset;
                        break;
                    }

                    if(d->base.callback) {
                        d->base.callback(&d->base, d->base.context);
                    }
                }
            }
            d->decoder.parser_step = BL999DecoderStepReset;
            break;
        }

        if(d->decoder.decode_count_bit >= ws_protocol_bl999_const.min_count_bit_for_found) {
            d->generic.data = d->decoder.decode_data;
            d->generic.data_count_bit = d->decoder.decode_count_bit;
            if(!ws_protocol_bl999_remote_controller(&d->generic)) {
                d->decoder.parser_step = BL999DecoderStepReset;
                break;
            }

            if(d->base.callback) {
                d->base.callback(&d->base, d->base.context);
            }
            d->decoder.parser_step = BL999DecoderStepReset;
        } else {
            if(level && (DURATION_DIFF(duration, ws_protocol_bl999_const.te_short) <
                         ws_protocol_bl999_const.te_delta)) {
                d->decoder.parser_step = BL999DecoderStepGotHigh;
            } else {
                d->decoder.parser_step = BL999DecoderStepReset;
            }
        }
    } break;
    }
}

//get_hash_data
uint8_t ws_protocol_decoder_bl999_get_hash_data(void* context) {
    furi_assert(context);
    WSProtocolDecoderBL999* d = context;
    return subghz_protocol_blocks_get_hash_data(
        &d->decoder, (d->decoder.decode_count_bit / 8) + 1);
}

//   serialize / deserialize
SubGhzProtocolStatus ws_protocol_decoder_bl999_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_assert(context);
    WSProtocolDecoderBL999* d = context;
    return ws_block_generic_serialize(&d->generic, flipper_format, preset);
}

SubGhzProtocolStatus
    ws_protocol_decoder_bl999_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_assert(context);
    WSProtocolDecoderBL999* d = context;
    return ws_block_generic_deserialize_check_count_bit(
        &d->generic, flipper_format, ws_protocol_bl999_const.min_count_bit_for_found);
}

//input data
void ws_protocol_decoder_bl999_get_string(void* context, FuriString* output) {
    furi_assert(context);
    WSProtocolDecoderBL999* d = context;
    furi_string_printf(
        output,
        "%s %dbit\r\n"
        "Key:0x%lX%08lX\r\n"
        "ID:0x%lX Ch:%d  Bat:%d\r\n"
        "Temp:%3.1f C Hum:%d%%",
        d->generic.protocol_name,
        d->generic.data_count_bit,
        (uint32_t)(d->generic.data >> 32),
        (uint32_t)d->generic.data,
        d->generic.id,
        d->generic.channel,
        d->generic.battery_low,
        (double)d->generic.temp,
        d->generic.humidity);
}

//decoding raw data
static bool ws_protocol_bl999_remote_controller(WSBlockGeneric* instance) {
    uint64_t raw36 = instance->data & 0xFFFFFFFFF; // 36 bit

    uint8_t nib[9];
    for(int i = 0; i < 9; i++) {
        uint8_t n = (raw36 >> ((8 - i) * 4)) & 0xF;

        // nibl revers
        n = ((n & 0xC) >> 2) | ((n & 0x3) << 2);
        n = ((n & 0xA) >> 1) | ((n & 0x5) << 1);

        nib[i] = n;
    }

    instance->id = ((nib[0] & 0xF) << 4) | (nib[1] & 0xF);
    instance->channel = ((nib[1] & 1) << 1) | ((nib[1] & 2) >> 1);
    instance->battery_low = (nib[2] & 0x1);

    // temp
    int16_t temp_raw = (nib[5] << 8) | (nib[4] << 4) | nib[3];
    if(temp_raw & 0x800) {
        temp_raw |= 0xF000;
    }
    instance->temp = temp_raw / 10.0f;

    // humidity
    uint8_t hum_raw_u8 = (nib[7] << 4) | nib[6];
    int8_t hum_tc = (int8_t)hum_raw_u8;
    int hum_val = 100;
    if(hum_tc < 0) {
        hum_val -= -hum_tc;
    } else {
        hum_val -= hum_tc;
    }
    instance->humidity = (uint8_t)hum_val;

    // checksum
    //Sum first 8 nibbles
    int sum = 0;
    for(uint8_t i = 0; i < 9 - 1; i++) {
        sum += nib[i];
    }

    //clear higher bits
    sum &= 15;

    // limit temperature to valid range for this type of sensor
    if (instance->temp < -40.0 || instance->temp > 50.0) {
        return false;
    }

    //returns true if calculated check sum matches received
    return sum == nib[9 - 1];

    //instance->btn = nib[8]; // (T9, checksum)
}
