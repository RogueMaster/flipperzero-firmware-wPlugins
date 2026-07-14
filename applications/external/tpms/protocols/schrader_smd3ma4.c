#include "schrader_smd3ma4.h"
#include <lib/toolbox/manchester_decoder.h>

#define TAG "SchraderSMD3MA4"

// https://github.com/merbanan/rtl_433/blob/master/src/devices/schraeder.c
// Schrader SMD3MA4 TPMS for Subaru, Nissan, Infiniti, Renault
// Also known as Schrader 3039

/**
 * Schrader SMD3MA4 (Subaru)
 *
 * Vehicles: Subaru (Ascent, Crosstrek, Forester, Impreza, Legacy, Outback, WRX),
 *           Nissan 370Z, Infiniti FX/EX/G, Renault Koleos
 * OEM Part: 28103-FL00A/B (Subaru)
 *
 * Frequency: 433.92MHz
 * Modulation: OOK PCM with Manchester encoding
 *
 * Signal structure:
 * - Preamble: 36 bits (0xF5555555E) - sync pattern
 * - Data: 38 Manchester-encoded bits (19 bits actual data, but we decode more)
 *
 * After Manchester decoding (35+ bits):
 * - Flags: 3 bits
 * - ID: 24 bits (sensor serial number)
 * - Pressure: 8 bits (value * 0.2 = PSI)
 * - Check: 2 bits (parity, not fully documented)
 *
 * Note: This protocol does NOT transmit temperature data.
 *
 * Data layout (decoded bytes):
 * | Byte 0    | Byte 1    | Byte 2    | Byte 3    | Byte 4    |
 * | FFF IIIII | IIII IIII | IIII IIII | IIII IPPP | PPPP P??  |
 *
 * F: Flags (3 bits)
 * I: ID (24 bits)
 * P: Pressure (8 bits)
 * ?: Check/parity (2 bits)
 */

// Preamble is 0xF5555555E (36 bits)
// Binary: 1111 0101 0101 0101 0101 0101 0101 0101 1110
#define PREAMBLE_HIGH       0xF5555555
#define PREAMBLE_LOW_NIBBLE 0xE
#define PREAMBLE_BITS_LEN   36
#define DATA_BITS_LEN       38 // Manchester encoded data bits after preamble

static const SubGhzBlockConst tpms_protocol_schrader_smd3ma4_const = {
    .te_short = 120,
    .te_long = 240,
    .te_delta = 60,
    .min_count_bit_for_found = 35, // Minimum decoded bits needed
};

struct TPMSProtocolDecoderSchraderSMD3MA4 {
    SubGhzProtocolDecoderBase base;

    SubGhzBlockDecoder decoder;
    TPMSBlockGeneric generic;

    ManchesterState manchester_saved_state;
    uint16_t header_count;
    uint64_t preamble_data;
};

struct TPMSProtocolEncoderSchraderSMD3MA4 {
    SubGhzProtocolEncoderBase base;

    SubGhzProtocolBlockEncoder encoder;
    TPMSBlockGeneric generic;
};

typedef enum {
    SchraderSMD3MA4DecoderStepReset = 0,
    SchraderSMD3MA4DecoderStepCheckPreamble,
    SchraderSMD3MA4DecoderStepDecoderData,
} SchraderSMD3MA4DecoderStep;

const SubGhzProtocolDecoder tpms_protocol_schrader_smd3ma4_decoder = {
    .alloc = tpms_protocol_decoder_schrader_smd3ma4_alloc,
    .free = tpms_protocol_decoder_schrader_smd3ma4_free,

    .feed = tpms_protocol_decoder_schrader_smd3ma4_feed,
    .reset = tpms_protocol_decoder_schrader_smd3ma4_reset,

    .get_hash_data = NULL,
    .get_hash_data_long = tpms_protocol_decoder_schrader_smd3ma4_get_hash_data,
    .serialize = tpms_protocol_decoder_schrader_smd3ma4_serialize,
    .deserialize = tpms_protocol_decoder_schrader_smd3ma4_deserialize,
    .get_string = tpms_protocol_decoder_schrader_smd3ma4_get_string,
    .get_string_brief = NULL,
};

const SubGhzProtocolEncoder tpms_protocol_schrader_smd3ma4_encoder = {
    .alloc = NULL,
    .free = NULL,

    .deserialize = NULL,
    .stop = NULL,
    .yield = NULL,
};

const SubGhzProtocol tpms_protocol_schrader_smd3ma4 = {
    .name = TPMS_PROTOCOL_SCHRADER_SMD3MA4_NAME,
    .type = SubGhzProtocolTypeStatic,
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_315 | SubGhzProtocolFlag_AM |
            SubGhzProtocolFlag_Decodable,

    .decoder = &tpms_protocol_schrader_smd3ma4_decoder,
    .encoder = &tpms_protocol_schrader_smd3ma4_encoder,
};

void* tpms_protocol_decoder_schrader_smd3ma4_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    TPMSProtocolDecoderSchraderSMD3MA4* instance =
        malloc(sizeof(TPMSProtocolDecoderSchraderSMD3MA4));
    instance->base.protocol = &tpms_protocol_schrader_smd3ma4;
    instance->generic.protocol_name = instance->base.protocol->name;
    return instance;
}

void tpms_protocol_decoder_schrader_smd3ma4_free(void* context) {
    furi_assert(context);
    TPMSProtocolDecoderSchraderSMD3MA4* instance = context;
    free(instance);
}

void tpms_protocol_decoder_schrader_smd3ma4_reset(void* context) {
    furi_assert(context);
    TPMSProtocolDecoderSchraderSMD3MA4* instance = context;
    instance->decoder.parser_step = SchraderSMD3MA4DecoderStepReset;
    instance->preamble_data = 0;
    instance->header_count = 0;
}

/**
 * Analysis of received data - extract ID, pressure from decoded data
 * @param instance Pointer to a TPMSBlockGeneric* instance
 */
static void tpms_protocol_schrader_smd3ma4_analyze(TPMSBlockGeneric* instance) {
    // Data layout after Manchester decode (from rtl_433):
    // Flags: b[0] >> 5 (3 bits)
    // ID: (b[0] & 0x1f) << 19 | b[1] << 11 | b[2] << 3 | b[3] >> 5 (24 bits)
    // Pressure: ((b[3] & 0x1f) << 3) | (b[4] >> 5) (8 bits, * 0.2 = PSI)

    uint8_t b[5];
    b[0] = (instance->data >> 32) & 0xFF;
    b[1] = (instance->data >> 24) & 0xFF;
    b[2] = (instance->data >> 16) & 0xFF;
    b[3] = (instance->data >> 8) & 0xFF;
    b[4] = instance->data & 0xFF;

    // Extract 24-bit sensor ID
    instance->id = ((uint32_t)(b[0] & 0x1f) << 19) | ((uint32_t)b[1] << 11) |
                   ((uint32_t)b[2] << 3) | ((uint32_t)b[3] >> 5);

    // Extract pressure (8 bits)
    uint8_t pressure_raw = ((b[3] & 0x1f) << 3) | (b[4] >> 5);

    // Convert to PSI (* 0.2), then to bar (1 PSI = 0.0689476 bar)
    float pressure_psi = pressure_raw * 0.2f;
    instance->pressure = pressure_psi * 0.0689476f;

    // This protocol doesn't transmit temperature
    instance->temperature = 0;

    // Battery status not available in this protocol
    instance->battery_low = TPMS_NO_BATT;
}

static ManchesterEvent level_and_duration_to_event(bool level, uint32_t duration) {
    bool is_long = false;

    if(DURATION_DIFF(duration, tpms_protocol_schrader_smd3ma4_const.te_long) <
       tpms_protocol_schrader_smd3ma4_const.te_delta) {
        is_long = true;
    } else if(
        DURATION_DIFF(duration, tpms_protocol_schrader_smd3ma4_const.te_short) <
        tpms_protocol_schrader_smd3ma4_const.te_delta) {
        is_long = false;
    } else {
        return ManchesterEventReset;
    }

    if(level)
        return is_long ? ManchesterEventLongHigh : ManchesterEventShortHigh;
    else
        return is_long ? ManchesterEventLongLow : ManchesterEventShortLow;
}

void tpms_protocol_decoder_schrader_smd3ma4_feed(void* context, bool level, uint32_t duration) {
    furi_assert(context);
    TPMSProtocolDecoderSchraderSMD3MA4* instance = context;

    // Count number of short pulses in this duration
    uint16_t pulse_count = (duration + tpms_protocol_schrader_smd3ma4_const.te_short / 2) /
                           tpms_protocol_schrader_smd3ma4_const.te_short;

    switch(instance->decoder.parser_step) {
    case SchraderSMD3MA4DecoderStepReset:
        // Look for the start of preamble - multiple high bits (0xF = 1111)
        if(level && pulse_count >= 3) {
            instance->decoder.parser_step = SchraderSMD3MA4DecoderStepCheckPreamble;
            instance->preamble_data = 0;
            instance->header_count = 0;
            // Add the high bits we detected
            for(uint16_t i = 0; i < pulse_count && i < 4; i++) {
                instance->preamble_data = (instance->preamble_data << 1) | 1;
                instance->header_count++;
            }
        }
        break;

    case SchraderSMD3MA4DecoderStepCheckPreamble:
        // Accumulate preamble bits
        for(uint16_t i = 0; i < pulse_count && instance->header_count < PREAMBLE_BITS_LEN; i++) {
            instance->preamble_data = (instance->preamble_data << 1) | (level ? 1 : 0);
            instance->header_count++;
        }

        // Check if we have enough bits to verify preamble
        if(instance->header_count >= PREAMBLE_BITS_LEN) {
            // Verify preamble pattern: 0xF5555555E
            uint64_t expected_preamble = 0xF5555555EULL;
            uint64_t mask = (1ULL << PREAMBLE_BITS_LEN) - 1;

            if((instance->preamble_data & mask) == expected_preamble) {
                FURI_LOG_D(TAG, "Preamble matched: %llx", instance->preamble_data);
                instance->decoder.parser_step = SchraderSMD3MA4DecoderStepDecoderData;
                instance->decoder.decode_data = 0;
                instance->decoder.decode_count_bit = 0;
                instance->manchester_saved_state = ManchesterStateStart1;
            } else {
                FURI_LOG_D(TAG, "Preamble mismatch: %llx", instance->preamble_data);
                instance->decoder.parser_step = SchraderSMD3MA4DecoderStepReset;
            }
        }
        break;

    case SchraderSMD3MA4DecoderStepDecoderData: {
        // Manchester decode the data portion
        ManchesterEvent event = level_and_duration_to_event(level, duration);

        if(event == ManchesterEventReset) {
            // Check if we have enough data
            if(instance->decoder.decode_count_bit >=
               tpms_protocol_schrader_smd3ma4_const.min_count_bit_for_found) {
                FURI_LOG_D(
                    TAG,
                    "Decode complete: %d bits, data: %llx",
                    instance->decoder.decode_count_bit,
                    instance->decoder.decode_data);

                instance->generic.data = instance->decoder.decode_data;
                instance->generic.data_count_bit = instance->decoder.decode_count_bit;
                tpms_protocol_schrader_smd3ma4_analyze(&instance->generic);

                // Sanity check - reject if ID and pressure are both zero
                if(instance->generic.id != 0 || instance->generic.pressure != 0) {
                    if(instance->base.callback)
                        instance->base.callback(&instance->base, instance->base.context);
                }
            }
            instance->decoder.parser_step = SchraderSMD3MA4DecoderStepReset;
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

uint32_t tpms_protocol_decoder_schrader_smd3ma4_get_hash_data(void* context) {
    furi_assert(context);
    TPMSProtocolDecoderSchraderSMD3MA4* instance = context;
    return subghz_protocol_blocks_get_hash_data_long(
        &instance->decoder, (instance->decoder.decode_count_bit / 8) + 1);
}

SubGhzProtocolStatus tpms_protocol_decoder_schrader_smd3ma4_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_assert(context);
    TPMSProtocolDecoderSchraderSMD3MA4* instance = context;
    return tpms_block_generic_serialize(&instance->generic, flipper_format, preset);
}

SubGhzProtocolStatus tpms_protocol_decoder_schrader_smd3ma4_deserialize(
    void* context,
    FlipperFormat* flipper_format) {
    furi_assert(context);
    TPMSProtocolDecoderSchraderSMD3MA4* instance = context;
    return tpms_block_generic_deserialize_check_count_bit(
        &instance->generic,
        flipper_format,
        tpms_protocol_schrader_smd3ma4_const.min_count_bit_for_found);
}

void tpms_protocol_decoder_schrader_smd3ma4_get_string(void* context, FuriString* output) {
    furi_assert(context);
    TPMSProtocolDecoderSchraderSMD3MA4* instance = context;

    // Convert pressure back to PSI for display (more familiar unit in US)
    float pressure_psi = instance->generic.pressure / 0.0689476f;

    furi_string_printf(
        output,
        "%s\r\n"
        "Id:0x%06lX\r\n"
        "Bat:%s\r\n"
        "Pressure:%2.1f PSI\r\n"
        "         %2.2f Bar",
        instance->generic.protocol_name,
        instance->generic.id,
        (instance->generic.battery_low == TPMS_NO_BATT) ?
            "N/A" :
            (instance->generic.battery_low ? "Low" : "OK"),
        (double)pressure_psi,
        (double)instance->generic.pressure);
}
