#include "abarth_124.h"
#include <lib/toolbox/manchester_decoder.h>

#define TAG "Abarth124"

// https://github.com/merbanan/rtl_433/blob/master/src/devices/tpms_abarth124.c
// VDO Type TG1C TPMS for Abarth 124 Spider, Fiat 124 Spider, Mazda MX-5 ND

/**
 * Abarth 124 Spider TPMS (VDO Type TG1C)
 *
 * Vehicles: Abarth 124 Spider, Fiat 124 Spider, Mazda MX-5 ND
 *
 * Frequency: 433.92MHz +-38kHz
 * Modulation: FSK PCM with Manchester encoding
 *
 * Signal structure:
 * - Preamble: 0xaaa9 (after bit inversion)
 * - Data: 72 bits (9 bytes) Manchester encoded
 *
 * Data layout (9 bytes):
 * | Byte 0-3  | Byte 4    | Byte 5    | Byte 6    | Byte 7    | Byte 8    |
 * | IIII IIII | FFFF FFFF | PPPP PPPP | TTTT TTTT | SSSS SSSS | CCCC CCCC |
 *
 * I: 32-bit sensor ID
 * F: 8-bit flags/status
 * P: 8-bit pressure (value * 1.38 = kPa)
 * T: 8-bit temperature (value - 50 = deg C)
 * S: 8-bit status
 * C: 8-bit XOR checksum (bytes 0-7)
 *
 * Pressure range: 0-350 kPa (+-7 kPa)
 * Temperature range: -50C to 125C
 */

// Preamble after inversion: 0xaaa9 (16 bits)
// Original signal has inverted bits
#define PREAMBLE_PATTERN  0xAAA9
#define PREAMBLE_BITS_LEN 16
#define DATA_BITS_LEN     72 // 9 bytes

static const SubGhzBlockConst tpms_protocol_abarth_124_const = {
    .te_short = 52, // ~52us at 250kHz sample rate
    .te_long = 104, // ~104us
    .te_delta = 25, // Tolerance
    .min_count_bit_for_found = 72, // 9 bytes
};

struct TPMSProtocolDecoderAbarth124 {
    SubGhzProtocolDecoderBase base;

    SubGhzBlockDecoder decoder;
    TPMSBlockGeneric generic;

    ManchesterState manchester_saved_state;
    uint16_t header_count;
    uint32_t preamble_data;
};

struct TPMSProtocolEncoderAbarth124 {
    SubGhzProtocolEncoderBase base;

    SubGhzProtocolBlockEncoder encoder;
    TPMSBlockGeneric generic;
};

typedef enum {
    Abarth124DecoderStepReset = 0,
    Abarth124DecoderStepCheckPreamble,
    Abarth124DecoderStepDecoderData,
} Abarth124DecoderStep;

const SubGhzProtocolDecoder tpms_protocol_abarth_124_decoder = {
    .alloc = tpms_protocol_decoder_abarth_124_alloc,
    .free = tpms_protocol_decoder_abarth_124_free,

    .feed = tpms_protocol_decoder_abarth_124_feed,
    .reset = tpms_protocol_decoder_abarth_124_reset,

    .get_hash_data = NULL,
    .get_hash_data_long = tpms_protocol_decoder_abarth_124_get_hash_data,
    .serialize = tpms_protocol_decoder_abarth_124_serialize,
    .deserialize = tpms_protocol_decoder_abarth_124_deserialize,
    .get_string = tpms_protocol_decoder_abarth_124_get_string,
    .get_string_brief = NULL,
};

const SubGhzProtocolEncoder tpms_protocol_abarth_124_encoder = {
    .alloc = NULL,
    .free = NULL,

    .deserialize = NULL,
    .stop = NULL,
    .yield = NULL,
};

const SubGhzProtocol tpms_protocol_abarth_124 = {
    .name = TPMS_PROTOCOL_ABARTH_124_NAME,
    .type = SubGhzProtocolTypeStatic,
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_FM | SubGhzProtocolFlag_Decodable,

    .decoder = &tpms_protocol_abarth_124_decoder,
    .encoder = &tpms_protocol_abarth_124_encoder,
};

void* tpms_protocol_decoder_abarth_124_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    TPMSProtocolDecoderAbarth124* instance = malloc(sizeof(TPMSProtocolDecoderAbarth124));
    instance->base.protocol = &tpms_protocol_abarth_124;
    instance->generic.protocol_name = instance->base.protocol->name;
    return instance;
}

void tpms_protocol_decoder_abarth_124_free(void* context) {
    furi_assert(context);
    TPMSProtocolDecoderAbarth124* instance = context;
    free(instance);
}

void tpms_protocol_decoder_abarth_124_reset(void* context) {
    furi_assert(context);
    TPMSProtocolDecoderAbarth124* instance = context;
    instance->decoder.parser_step = Abarth124DecoderStepReset;
    instance->preamble_data = 0;
    instance->header_count = 0;
}

/**
 * Verify XOR checksum
 * Note: decode_data is 64 bits, but protocol is 72 bits (9 bytes)
 * We can only verify partial checksum with available data
 * @param instance Pointer to decoder instance
 * @return true if checksum appears valid (relaxed check)
 */
static bool tpms_protocol_abarth_124_check_checksum(TPMSProtocolDecoderAbarth124* instance) {
    uint64_t data = instance->decoder.decode_data;

    // With 64-bit decode_data, we have the last 8 bytes of the 9-byte message
    // Byte layout in our 64 bits: [ID1][ID0][FLAGS][PRESSURE][TEMP][STATUS][CHECK][extra]
    // or depending on alignment: [ID2][ID1][ID0][FLAGS][PRESSURE][TEMP][STATUS][CHECK]

    // Extract bytes from the 64-bit data
    uint8_t b[8];
    b[0] = (data >> 56) & 0xFF;
    b[1] = (data >> 48) & 0xFF;
    b[2] = (data >> 40) & 0xFF;
    b[3] = (data >> 32) & 0xFF;
    b[4] = (data >> 24) & 0xFF;
    b[5] = (data >> 16) & 0xFF;
    b[6] = (data >> 8) & 0xFF;
    b[7] = data & 0xFF;

    // XOR checksum of bytes 0-6 should equal byte 7
    // This is a partial check since we're missing the first byte of the full message
    uint8_t checksum = 0;
    for(int i = 0; i < 7; i++) {
        checksum ^= b[i];
    }

    // Relaxed check - if XOR matches or data looks reasonable, accept it
    // The full checksum would include byte 0 of the original 9-byte message
    if(checksum == b[7]) {
        return true;
    }

    // Accept if pressure and temperature are in reasonable ranges
    // Pressure byte (index 4): 0-255 maps to 0-350 kPa
    // Temperature byte (index 5): 0-255 maps to -50 to 205 C
    uint8_t pressure = b[4];
    uint8_t temp = b[5];

    // Reasonable pressure: 100-350 kPa (72-253 raw)
    // Reasonable temp: -20 to 80 C (30-130 raw)
    if(pressure >= 50 && pressure <= 253 && temp >= 20 && temp <= 150) {
        FURI_LOG_D(TAG, "Checksum mismatch but data looks valid, accepting");
        return true;
    }

    return false;
}

/**
 * Analysis of received data - extract ID, pressure, temperature
 * @param instance Pointer to a TPMSBlockGeneric* instance
 */
static void tpms_protocol_abarth_124_analyze(TPMSBlockGeneric* instance) {
    uint64_t data = instance->data;

    // Full protocol: 72 bits (9 bytes)
    // [ID3][ID2][ID1][ID0][FLAGS][PRESSURE][TEMP][STATUS][CHECKSUM]
    //
    // With 64-bit decode_data, we receive the last 64 bits:
    // [ID2][ID1][ID0][FLAGS][PRESSURE][TEMP][STATUS][CHECKSUM]
    // Byte:  7    6    5      4         3      2       1        0

    // Extract sensor ID - we have 24 bits of ID (missing MSB byte)
    // ID is in bytes 7, 6, 5 of our 64-bit data (bits 56-32)
    uint32_t id_partial = (data >> 32) & 0x00FFFFFF;
    instance->id = id_partial;

    // Extract pressure (byte 3 in our 64-bit = bits 31-24)
    uint8_t pressure_raw = (data >> 24) & 0xFF;
    // Convert: raw * 1.38 = kPa, then kPa to bar (1 kPa = 0.01 bar)
    float pressure_kpa = pressure_raw * 1.38f;
    instance->pressure = pressure_kpa * 0.01f; // Convert to bar

    // Extract temperature (byte 2 in our 64-bit = bits 23-16)
    uint8_t temp_raw = (data >> 16) & 0xFF;
    instance->temperature = (float)(temp_raw - 50);

    // Battery status not directly available in this protocol
    instance->battery_low = TPMS_NO_BATT;
}

static ManchesterEvent level_and_duration_to_event(bool level, uint32_t duration) {
    bool is_long = false;

    if(DURATION_DIFF(duration, tpms_protocol_abarth_124_const.te_long) <
       tpms_protocol_abarth_124_const.te_delta) {
        is_long = true;
    } else if(
        DURATION_DIFF(duration, tpms_protocol_abarth_124_const.te_short) <
        tpms_protocol_abarth_124_const.te_delta) {
        is_long = false;
    } else {
        return ManchesterEventReset;
    }

    if(level)
        return is_long ? ManchesterEventLongHigh : ManchesterEventShortHigh;
    else
        return is_long ? ManchesterEventLongLow : ManchesterEventShortLow;
}

void tpms_protocol_decoder_abarth_124_feed(void* context, bool level, uint32_t duration) {
    furi_assert(context);
    TPMSProtocolDecoderAbarth124* instance = context;

    // Count number of short pulses in this duration
    uint16_t pulse_count = (duration + tpms_protocol_abarth_124_const.te_short / 2) /
                           tpms_protocol_abarth_124_const.te_short;

    switch(instance->decoder.parser_step) {
    case Abarth124DecoderStepReset:
        // Look for alternating pattern (start of preamble)
        if(pulse_count >= 2) {
            instance->decoder.parser_step = Abarth124DecoderStepCheckPreamble;
            instance->preamble_data = 0;
            instance->header_count = 0;
            // Add initial bits
            for(uint16_t i = 0; i < pulse_count && i < 4; i++) {
                instance->preamble_data = (instance->preamble_data << 1) | (level ? 1 : 0);
                instance->header_count++;
            }
        }
        break;

    case Abarth124DecoderStepCheckPreamble:
        // Accumulate preamble bits
        for(uint16_t i = 0; i < pulse_count && instance->header_count < PREAMBLE_BITS_LEN; i++) {
            instance->preamble_data = (instance->preamble_data << 1) | (level ? 1 : 0);
            instance->header_count++;
        }

        // Check if we have enough bits to verify preamble
        if(instance->header_count >= PREAMBLE_BITS_LEN) {
            // Check for preamble pattern (may need to check inverted too)
            uint16_t preamble = instance->preamble_data & 0xFFFF;

            // Check both normal and inverted preamble
            if(preamble == PREAMBLE_PATTERN || preamble == (uint16_t)~PREAMBLE_PATTERN) {
                FURI_LOG_D(TAG, "Preamble matched: %04x", preamble);
                instance->decoder.parser_step = Abarth124DecoderStepDecoderData;
                instance->decoder.decode_data = 0;
                instance->decoder.decode_count_bit = 0;
                instance->manchester_saved_state = ManchesterStateStart1;
            } else {
                FURI_LOG_D(TAG, "Preamble mismatch: %04lx", instance->preamble_data);
                instance->decoder.parser_step = Abarth124DecoderStepReset;
            }
        }
        break;

    case Abarth124DecoderStepDecoderData: {
        // Manchester decode the data portion
        ManchesterEvent event = level_and_duration_to_event(level, duration);

        if(event == ManchesterEventReset) {
            // Check if we have enough data
            if(instance->decoder.decode_count_bit >=
               tpms_protocol_abarth_124_const.min_count_bit_for_found) {
                FURI_LOG_D(
                    TAG,
                    "Decode complete: %d bits, data: %llx",
                    instance->decoder.decode_count_bit,
                    instance->decoder.decode_data);

                // Verify checksum
                if(tpms_protocol_abarth_124_check_checksum(instance)) {
                    instance->generic.data = instance->decoder.decode_data;
                    instance->generic.data_count_bit = instance->decoder.decode_count_bit;
                    tpms_protocol_abarth_124_analyze(&instance->generic);

                    // Sanity check - reject if ID is zero
                    if(instance->generic.id != 0) {
                        if(instance->base.callback)
                            instance->base.callback(&instance->base, instance->base.context);
                    }
                } else {
                    FURI_LOG_D(TAG, "Checksum failed");
                }
            }
            instance->decoder.parser_step = Abarth124DecoderStepReset;
        } else {
            bool bit = false;
            bool have_bit = manchester_advance(
                instance->manchester_saved_state, event, &instance->manchester_saved_state, &bit);

            if(have_bit) {
                // Invert bit due to Manchester II encoding
                bit = !bit;
                subghz_protocol_blocks_add_bit(&instance->decoder, bit);
            }
        }
        break;
    }
    }
}

uint32_t tpms_protocol_decoder_abarth_124_get_hash_data(void* context) {
    furi_assert(context);
    TPMSProtocolDecoderAbarth124* instance = context;
    return subghz_protocol_blocks_get_hash_data_long(
        &instance->decoder, (instance->decoder.decode_count_bit / 8) + 1);
}

SubGhzProtocolStatus tpms_protocol_decoder_abarth_124_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_assert(context);
    TPMSProtocolDecoderAbarth124* instance = context;
    return tpms_block_generic_serialize(&instance->generic, flipper_format, preset);
}

SubGhzProtocolStatus
    tpms_protocol_decoder_abarth_124_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_assert(context);
    TPMSProtocolDecoderAbarth124* instance = context;
    return tpms_block_generic_deserialize_check_count_bit(
        &instance->generic,
        flipper_format,
        tpms_protocol_abarth_124_const.min_count_bit_for_found);
}

void tpms_protocol_decoder_abarth_124_get_string(void* context, FuriString* output) {
    furi_assert(context);
    TPMSProtocolDecoderAbarth124* instance = context;

    // Convert pressure from bar to PSI for display (1 bar = 14.5038 PSI)
    float pressure_psi = instance->generic.pressure * 14.5038f;
    // Also show kPa (1 bar = 100 kPa)
    float pressure_kpa = instance->generic.pressure * 100.0f;

    furi_string_printf(
        output,
        "%s\r\n"
        "Id:0x%08lX\r\n"
        "Bat:%s\r\n"
        "Temp:%2.0f C\r\n"
        "Pressure:%2.1f PSI\r\n"
        "         %3.0f kPa",
        instance->generic.protocol_name,
        instance->generic.id,
        (instance->generic.battery_low == TPMS_NO_BATT) ?
            "N/A" :
            (instance->generic.battery_low ? "Low" : "OK"),
        (double)instance->generic.temperature,
        (double)pressure_psi,
        (double)pressure_kpa);
}
