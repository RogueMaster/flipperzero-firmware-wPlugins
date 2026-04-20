/**
 * allstar_firefly.c  --  Allstar Firefly 318ALD31K native SubGHz protocol
 *
 * Implements the SubGhzProtocol vtable for the Supertex ED-9 based gate remote.
 * Uses subghz_block_generic_serialize / deserialize for standard-format .sub
 * files, encoding the 9-position trinary DIP code as a uint64 (base-3, MSB
 * first: '+' = 2, '0' = 1, '-' = 0).
 *
 * Saved file format:
 *   Filetype: Flipper SubGhz Key File
 *   Version: 1
 *   Frequency: 318000000
 *   Preset: FuriHalSubGhzPresetOok650Async
 *   Protocol: Allstar Firefly
 *   Key: 00 00 00 00 00 00 34 B9
 *   Bit: 9
 *
 * See allstar_firefly.h for full protocol documentation.
 */

#include "allstar_firefly.h"

#include <lib/subghz/protocols/base.h>
#include <lib/subghz/blocks/generic.h>

#include <furi.h>
#include <furi_hal.h>
#include <flipper_format/flipper_format.h>
#include <string.h>

#define TAG "AllstarFirefly"

/* --- Decoder instance ------------------------------------------------------ */

typedef enum {
    AfRxState_WaitGap,
    AfRxState_Receiving,
} AfRxState;

typedef struct {
    SubGhzProtocolDecoderBase base;

    SubGhzBlockGeneric generic; /* holds data (uint64) and data_count_bit (9) */

    AfRxState rx_state;
    uint8_t   rx_syms[AF_SYM_COUNT];
    uint8_t   rx_count;

    char dip[AF_BIT_COUNT + 1]; /* '+', '-', '0', null-terminated */
} SubGhzProtocolDecoderAllstarFirefly;

/* --- Encoder instance ------------------------------------------------------ */

typedef struct {
    SubGhzProtocolEncoderBase base;

    SubGhzBlockGeneric generic; /* holds data (uint64) and data_count_bit (9) */

    char     dip[AF_BIT_COUNT + 1];
    uint32_t tx_buf[AF_TX_BUF_SIZE];
    uint32_t tx_size;
    uint32_t tx_pos;
} SubGhzProtocolEncoderAllstarFirefly;

/* =========================================================================== */
/* Internal helpers                                                             */
/* =========================================================================== */

/**
 * Decode 18 raw symbols into a 9-char DIP string.
 * Returns true on success; false if any invalid (L,H) pair found.
 */
static bool af_decode_symbols(const uint8_t* syms, char* dip) {
    for(uint8_t i = 0; i < AF_BIT_COUNT; i++) {
        uint8_t a = syms[i * 2];
        uint8_t b = syms[i * 2 + 1];
        if      (a == 1 && b == 1) dip[i] = '+';
        else if (a == 0 && b == 0) dip[i] = '-';
        else if (a == 1 && b == 0) dip[i] = '0';
        else return false;
    }
    dip[AF_BIT_COUNT] = '\0';
    return true;
}

/**
 * Validate a DIP string: exactly AF_BIT_COUNT chars, all '+'/'-'/'0'.
 */
static bool af_dip_valid(const char* dip) {
    if(!dip) return false;
    for(uint8_t i = 0; i < AF_BIT_COUNT; i++) {
        if(dip[i] != '+' && dip[i] != '-' && dip[i] != '0') return false;
    }
    return (dip[AF_BIT_COUNT] == '\0');
}

/**
 * Encode DIP string as uint64 (base-3, MSB first).
 * '+' = 2,  '0' = 1,  '-' = 0
 * Max value for 9 positions: 3^9 - 1 = 19682 = 0x4CE2
 */
static uint64_t af_dip_to_uint64(const char* dip) {
    uint64_t val = 0;
    for(uint8_t i = 0; i < AF_BIT_COUNT; i++) {
        val *= 3;
        if     (dip[i] == '+') val += 2;
        else if(dip[i] == '0') val += 1;
    }
    return val;
}

/**
 * Decode uint64 back to a 9-char DIP string (inverse of above).
 */
static void af_uint64_to_dip(uint64_t val, char* dip) {
    for(int8_t i = AF_BIT_COUNT - 1; i >= 0; i--) {
        uint8_t rem = (uint8_t)(val % 3);
        val /= 3;
        if     (rem == 2) dip[i] = '+';
        else if(rem == 1) dip[i] = '0';
        else              dip[i] = '-';
    }
    dip[AF_BIT_COUNT] = '\0';
}

/**
 * Build the flat TX pulse/gap duration buffer from a DIP string.
 * Buffer alternates HIGH/LOW (even = HIGH, odd = LOW).
 * Per bit:
 *   '+' -> [LONG_PULSE, SHORT_GAP, LONG_PULSE, SHORT_GAP]
 *   '-' -> [SHORT_PULSE, LONG_GAP, SHORT_PULSE, LONG_GAP]
 *   '0' -> [LONG_PULSE, SHORT_GAP, SHORT_PULSE, LONG_GAP]
 * Last gap of each frame stretched to AF_INTERFRAME_US.
 */
static uint32_t af_build_tx_buf(const char* dip, uint32_t* buf) {
    uint32_t pos = 0;
    for(uint32_t rep = 0; rep < AF_TX_REPEAT; rep++) {
        for(uint32_t bit = 0; bit < AF_BIT_COUNT; bit++) {
            bool     last = (bit == AF_BIT_COUNT - 1);
            char     c    = dip[bit];
            uint32_t p0, g0, p1, g1;

            if(c == '+') {
                p0 = AF_LONG_PULSE_US;  g0 = AF_SHORT_GAP_US;
                p1 = AF_LONG_PULSE_US;  g1 = AF_SHORT_GAP_US;
            } else if(c == '-') {
                p0 = AF_SHORT_PULSE_US; g0 = AF_LONG_GAP_US;
                p1 = AF_SHORT_PULSE_US; g1 = AF_LONG_GAP_US;
            } else { /* '0' */
                p0 = AF_LONG_PULSE_US;  g0 = AF_SHORT_GAP_US;
                p1 = AF_SHORT_PULSE_US; g1 = AF_LONG_GAP_US;
            }

            if(last) g1 = AF_INTERFRAME_US;

            buf[pos++] = p0;
            buf[pos++] = g0;
            buf[pos++] = p1;
            buf[pos++] = g1;
        }
    }
    return pos;
}

/* =========================================================================== */
/* Decoder implementation                                                       */
/* =========================================================================== */

static void* subghz_protocol_decoder_allstar_firefly_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolDecoderAllstarFirefly* instance =
        malloc(sizeof(SubGhzProtocolDecoderAllstarFirefly));
    instance->base.protocol          = &subghz_protocol_allstar_firefly;
    instance->generic.protocol_name  = SUBGHZ_PROTOCOL_ALLSTAR_FIREFLY_NAME;
    instance->generic.data           = 0;
    instance->generic.data_count_bit = AF_SYM_COUNT; /* 18 OOK symbols = the binary storage width */
    instance->rx_state               = AfRxState_WaitGap;
    instance->rx_count               = 0;
    memset(instance->dip, '-', AF_BIT_COUNT);
    instance->dip[AF_BIT_COUNT]      = '\0';
    return instance;
}

static void subghz_protocol_decoder_allstar_firefly_free(void* context) {
    furi_assert(context);
    free(context);
}

static void subghz_protocol_decoder_allstar_firefly_reset(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderAllstarFirefly* instance = context;
    instance->rx_state = AfRxState_WaitGap;
    instance->rx_count = 0;
}

/**
 * Edge callback (Flipper SubGHz native convention):
 *   level = true  -> HIGH just ended, duration = pulse width
 *   level = false -> LOW  just ended, duration = gap   width
 */
static void subghz_protocol_decoder_allstar_firefly_feed(
    void*    context,
    bool     level,
    uint32_t duration) {

    furi_assert(context);
    SubGhzProtocolDecoderAllstarFirefly* instance = context;

    if(level) {
        /* HIGH just ended: duration = pulse width */
        if(instance->rx_state == AfRxState_Receiving) {
            uint8_t sym;
            if(duration >= AF_LONG_PULSE_MIN && duration <= AF_LONG_PULSE_MAX) {
                sym = 1u; /* H */
            } else if(duration >= AF_SHORT_PULSE_MIN && duration <= AF_SHORT_PULSE_MAX) {
                sym = 0u; /* L */
            } else {
                instance->rx_state = AfRxState_WaitGap;
                instance->rx_count = 0;
                return;
            }
            if(instance->rx_count < AF_SYM_COUNT) {
                instance->rx_syms[instance->rx_count++] = sym;
            }
        }
    } else {
        /* LOW just ended: duration = gap width */
        if(duration >= AF_FRAME_THRESH_US) {
            if(instance->rx_state == AfRxState_Receiving &&
               instance->rx_count == AF_SYM_COUNT) {

                char decoded[AF_BIT_COUNT + 1];
                if(af_decode_symbols(instance->rx_syms, decoded)) {
                    memcpy(instance->dip, decoded, AF_BIT_COUNT + 1);
                    instance->generic.data           = af_dip_to_uint64(decoded);
                    instance->generic.data_count_bit = AF_SYM_COUNT;

                    if(instance->base.callback) {
                        instance->base.callback(
                            &instance->base,
                            instance->base.context);
                    }
                }
                instance->rx_state = AfRxState_WaitGap;
                instance->rx_count = 0;

            } else if(instance->rx_state == AfRxState_WaitGap) {
                instance->rx_state = AfRxState_Receiving;
                instance->rx_count = 0;

            } else {
                instance->rx_state = AfRxState_WaitGap;
                instance->rx_count = 0;
            }
        }
    }
}

static uint8_t subghz_protocol_decoder_allstar_firefly_get_hash_data(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderAllstarFirefly* instance = context;
    uint8_t hash = 0;
    for(uint8_t i = 0; i < AF_BIT_COUNT; i++) hash ^= (uint8_t)instance->dip[i];
    return hash;
}

/**
 * Serialize to a .sub file.
 * Delegates to subghz_block_generic_serialize which writes the standard
 * Filetype/Version/Frequency/Preset/Protocol/Key/Bit fields correctly,
 * including the proper preset name conversion (AM650 -> FuriHalSubGhzPresetOok650Async).
 */
static SubGhzProtocolStatus subghz_protocol_decoder_allstar_firefly_serialize(
    void*              context,
    FlipperFormat*     flipper_format,
    SubGhzRadioPreset* preset) {

    furi_assert(context);
    SubGhzProtocolDecoderAllstarFirefly* instance = context;
    return subghz_block_generic_serialize(&instance->generic, flipper_format, preset);
}

/**
 * Deserialize from a .sub file.
 * The framework has already consumed Frequency/Preset/Protocol before calling us.
 * subghz_block_generic_deserialize reads the Key (uint64) and Bit (uint32) fields,
 * then we decode the uint64 back into our DIP string.
 */
static SubGhzProtocolStatus subghz_protocol_decoder_allstar_firefly_deserialize(
    void*          context,
    FlipperFormat* flipper_format) {

    furi_assert(context);
    SubGhzProtocolDecoderAllstarFirefly* instance = context;

    SubGhzProtocolStatus status = subghz_block_generic_deserialize_check_count_bit(
        &instance->generic, flipper_format, AF_SYM_COUNT);
    if(status != SubGhzProtocolStatusOk) {
        FURI_LOG_E(TAG, "Decoder deserialize failed: %d", status);
        return status;
    }

    af_uint64_to_dip(instance->generic.data, instance->dip);

    if(!af_dip_valid(instance->dip)) {
        FURI_LOG_E(TAG, "Decoded invalid DIP: %s", instance->dip);
        return SubGhzProtocolStatusErrorParserOthers;
    }

    return SubGhzProtocolStatusOk;
}

static void subghz_protocol_decoder_allstar_firefly_get_string(
    void*       context,
    FuriString* output) {

    furi_assert(context);
    SubGhzProtocolDecoderAllstarFirefly* instance = context;

    /* cat_printf appends to the output string.  The SubGHz app
     * pre-populates it with the timestamp before calling us.
     * First line = what the history list displays.                */
    furi_string_cat_printf(
        output,
        "%s\r\n0x%04lX\r\n"
        "Freq: 318MHz  OOK\r\n"
        "1 2 3 4 5 6 7 8 9\r\n"
        "%c %c %c %c %c %c %c %c %c",
        SUBGHZ_PROTOCOL_ALLSTAR_FIREFLY_NAME,
        (unsigned long)(instance->generic.data),
        instance->dip[0], instance->dip[1], instance->dip[2],
        instance->dip[3], instance->dip[4], instance->dip[5],
        instance->dip[6], instance->dip[7], instance->dip[8]);
}

/* =========================================================================== */
/* Encoder implementation                                                       */
/* =========================================================================== */

static void* subghz_protocol_encoder_allstar_firefly_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolEncoderAllstarFirefly* instance =
        malloc(sizeof(SubGhzProtocolEncoderAllstarFirefly));
    instance->base.protocol          = &subghz_protocol_allstar_firefly;
    instance->generic.protocol_name  = SUBGHZ_PROTOCOL_ALLSTAR_FIREFLY_NAME;
    instance->generic.data           = 0;
    instance->generic.data_count_bit = AF_SYM_COUNT; /* 18 OOK symbols = the binary storage width */
    memset(instance->dip, '-', AF_BIT_COUNT);
    instance->dip[AF_BIT_COUNT]      = '\0';
    instance->tx_size                = 0;
    instance->tx_pos                 = 0;
    return instance;
}

static void subghz_protocol_encoder_allstar_firefly_free(void* context) {
    furi_assert(context);
    free(context);
}

static void subghz_protocol_encoder_allstar_firefly_stop(void* context) {
    UNUSED(context);
}

/**
 * Load DIP code from a .sub file for retransmit.
 * subghz_block_generic_deserialize reads Key (uint64) and Bit (uint32),
 * then we decode the uint64 to a DIP string and build the TX buffer.
 */
static SubGhzProtocolStatus subghz_protocol_encoder_allstar_firefly_deserialize(
    void*          context,
    FlipperFormat* flipper_format) {

    furi_assert(context);
    SubGhzProtocolEncoderAllstarFirefly* instance = context;

    SubGhzProtocolStatus status = subghz_block_generic_deserialize_check_count_bit(
        &instance->generic, flipper_format, AF_SYM_COUNT);
    if(status != SubGhzProtocolStatusOk) {
        FURI_LOG_E(TAG, "Encoder deserialize failed: %d", status);
        return status;
    }

    af_uint64_to_dip(instance->generic.data, instance->dip);

    if(!af_dip_valid(instance->dip)) {
        FURI_LOG_E(TAG, "Encoder: invalid DIP: %s", instance->dip);
        return SubGhzProtocolStatusErrorParserOthers;
    }

    instance->tx_size = af_build_tx_buf(instance->dip, instance->tx_buf);
    instance->tx_pos  = 0;

    return SubGhzProtocolStatusOk;
}

/**
 * Yield the next (level, duration) pair for transmission.
 * Even index = HIGH, odd index = LOW.
 */
static LevelDuration subghz_protocol_encoder_allstar_firefly_yield(void* context) {
    furi_assert(context);
    SubGhzProtocolEncoderAllstarFirefly* instance = context;

    if(instance->tx_pos >= instance->tx_size) {
        return level_duration_reset();
    }

    bool     lv  = (instance->tx_pos % 2 == 0);
    uint32_t dur = instance->tx_buf[instance->tx_pos++];
    return level_duration_make(lv, dur);
}

/* =========================================================================== */
/* Protocol vtable                                                              */
/* =========================================================================== */

const SubGhzProtocolDecoder subghz_protocol_decoder_allstar_firefly = {
    .alloc         = subghz_protocol_decoder_allstar_firefly_alloc,
    .free          = subghz_protocol_decoder_allstar_firefly_free,
    .feed          = subghz_protocol_decoder_allstar_firefly_feed,
    .reset         = subghz_protocol_decoder_allstar_firefly_reset,
    .get_hash_data = subghz_protocol_decoder_allstar_firefly_get_hash_data,
    .serialize     = subghz_protocol_decoder_allstar_firefly_serialize,
    .deserialize   = subghz_protocol_decoder_allstar_firefly_deserialize,
    .get_string    = subghz_protocol_decoder_allstar_firefly_get_string,
};

const SubGhzProtocolEncoder subghz_protocol_encoder_allstar_firefly = {
    .alloc       = subghz_protocol_encoder_allstar_firefly_alloc,
    .free        = subghz_protocol_encoder_allstar_firefly_free,
    .deserialize = subghz_protocol_encoder_allstar_firefly_deserialize,
    .stop        = subghz_protocol_encoder_allstar_firefly_stop,
    .yield       = subghz_protocol_encoder_allstar_firefly_yield,
};

const SubGhzProtocol subghz_protocol_allstar_firefly = {
    .name    = SUBGHZ_PROTOCOL_ALLSTAR_FIREFLY_NAME,
    .type    = SubGhzProtocolTypeStatic,
    .flag    = SubGhzProtocolFlag_AM        |
               SubGhzProtocolFlag_Decodable |
               SubGhzProtocolFlag_Load      |
               SubGhzProtocolFlag_Save      |
               SubGhzProtocolFlag_Send,
    .decoder = &subghz_protocol_decoder_allstar_firefly,
    .encoder = &subghz_protocol_encoder_allstar_firefly,
};