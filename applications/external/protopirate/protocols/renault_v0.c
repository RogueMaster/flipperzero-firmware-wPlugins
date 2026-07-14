#include "renault_v0.h"
#include "protocols_common.h"

#define RENAULT_V0_MIN_BITS           0x52U
#define RENAULT_V0_DECODER_BIT_LIMIT  0x6DU
#define RENAULT_V0_SYNC_MIN_US        0x320U
#define RENAULT_V0_GAP_RESET_US       5000U
#define RENAULT_V0_END_BURST_MIN_BITS 96U
#define RENAULT_V0_END_BURST_MIN_US   1200U
#define RENAULT_V0_END_BURST_MAX_US   2000U
#define RENAULT_V0_DECODED_BITS_MAX   0x70U
#define RENAULT_V0_UPLOAD_CAPACITY    0x258U
#define RENAULT_V0_TE_DEFAULT_US      125U
#define RENAULT_V0_TE_PREAMBLE_12_US  140U
#define RENAULT_V0_ROLLING_REPEAT     1U
#define RENAULT_V0_REPLAY_REPEAT      10U

#define RENAULT_V0_KEY2_FIELD     "Key2"
#define RENAULT_V0_PREAMBLE_FIELD "Preamble"
#define RENAULT_V0_ROLLING_FIELD  "Rolling"

_Static_assert(
    RENAULT_V0_UPLOAD_CAPACITY <= PP_SHARED_UPLOAD_CAPACITY,
    "RENAULT_V0_UPLOAD_CAPACITY exceeds shared upload slab");

typedef struct {
    uint32_t low;
    uint32_t high;
} RenaultV0MatrixRow;

static const RenaultV0MatrixRow renault_v0_matrix[42] = {
    {0x00000001, 0x00000000}, {0x04000029, 0x00000000}, {0x0000001B, 0x00000000},
    {0x00000000, 0x00000000}, {0x00000001, 0x00000000}, {0x05220124, 0x00000000},
    {0x00000001, 0x00000000}, {0x00088410, 0x00000000}, {0x60132D1D, 0x00000000},
    {0x60170F87, 0x00001004}, {0x00000000, 0x00000000}, {0x002000A9, 0x00000000},
    {0x20863E01, 0x0000100C}, {0x24BB3755, 0x00000004}, {0x640199A4, 0x00000004},
    {0x24225C43, 0x00001004}, {0x607886F1, 0x0000100C}, {0x6007A101, 0x0000000C},
    {0x66672A10, 0x00000004}, {0x4651623F, 0x00001008}, {0x43380BBF, 0x00001008},
    {0x20237F84, 0x00001000}, {0x4245755E, 0x00001008}, {0x60AAF581, 0x00000004},
    {0x22722DAD, 0x0000000C}, {0x27C617F7, 0x00000000}, {0x46DE8F1B, 0x0000000C},
    {0x231DEC51, 0x00000000}, {0x03ACAA0B, 0x00000008}, {0x22D2BF81, 0x00000004},
    {0x626EF6AE, 0x0000100C}, {0x40441F95, 0x0000000C}, {0x00000001, 0x00000000},
    {0x00000000, 0x00000000}, {0x20B9A590, 0x00000008}, {0x656C8E86, 0x00001008},
    {0x60129F96, 0x0000000C}, {0x2368F667, 0x00001000}, {0x442A1A5C, 0x00000000},
    {0x04C43242, 0x0000100C}, {0x22198640, 0x00001000}, {0x23D6B958, 0x00001008},
};

static const uint8_t renault_v0_decoder_state_table[4] = {0x01, 0x91, 0x9B, 0xFB};

typedef enum {
    RenaultV0TypeUnknown = 0,
    RenaultV0Type13,
    RenaultV0Type04,
    RenaultV0Type0C,
    RenaultV0Type1A,
    RenaultV0Type3B,
    RenaultV0Type3F,
    RenaultV0TypeDynamic,
} RenaultV0TypeId;

typedef struct {
    RenaultV0TypeId id;
    uint8_t value;
    const char* name;
    uint8_t checksum_low6;
    uint8_t checksum_high2_xor;
} RenaultV0TypeEntry;

static const RenaultV0TypeEntry renault_v0_types[] = {
    {RenaultV0Type13, 0x13U, "13", 0x13U, 0x00U},
    {RenaultV0Type04, 0x04U, "04", 0x04U, 0x00U},
    {RenaultV0Type0C, 0x0CU, "0C", 0x0CU, 0x00U},
    {RenaultV0Type1A, 0x1AU, "1A", 0x1AU, 0x03U},
    {RenaultV0Type3B, 0x3BU, "3B", 0x3BU, 0x00U},
    {RenaultV0Type3F, 0x3FU, "3F", 0x3FU, 0x00U},
};

typedef struct {
    uint32_t te_short;
    uint32_t te_long;
    uint32_t te_delta;
} RenaultV0TeProfile;

static const RenaultV0TeProfile renault_v0_te_profiles[] = {
    {0x7DU, 0xFAU, 0x45U},
    {124U, 248U, 60U},
    {108U, 250U, 55U},
};

typedef enum {
    RenaultV0TeProfile125 = 0,
    RenaultV0TeProfile124 = 1,
    RenaultV0TeProfile108 = 2,
} RenaultV0TeProfileId;

typedef enum {
    RenaultV0DecoderStepReset = 0,
    RenaultV0DecoderStepData = 1,
} RenaultV0DecoderStep;

typedef struct {
    uint64_t data;
    uint32_t key2;
    uint32_t serial;
    uint8_t button;
    uint8_t counter;
    RenaultV0TypeId type_id;
    uint8_t type_tag;
    uint8_t preamble_bits;
    bool c1_ok;
    bool c2_ok;
    bool ic_ok;
} RenaultV0DecodeAttempt;

typedef struct SubGhzProtocolDecoderRenaultV0 {
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;

    uint16_t packet_bit_count;
    uint8_t check_c1;
    uint8_t check_c2;
    uint8_t check_ic;

    uint32_t key2;

    uint8_t manchester_state;
    uint8_t decoded_bits[RENAULT_V0_DECODED_BITS_MAX];
    uint8_t decoded_bit_count;
    RenaultV0TypeId type_id;
    uint8_t type_tag;
    uint8_t preamble_bits;
    bool pending_attempt_valid;
    RenaultV0DecodeAttempt pending_attempt;
} SubGhzProtocolDecoderRenaultV0;

#if PROTOPIRATE_WITH_ENCODER
typedef struct SubGhzProtocolEncoderRenaultV0 {
    SubGhzProtocolEncoderBase base;
    SubGhzProtocolBlockEncoder encoder;
    SubGhzBlockGeneric generic;

    uint16_t packet_bit_count;
    uint8_t tx_button;
    uint8_t preamble_bits;
    uint32_t key2;
} SubGhzProtocolEncoderRenaultV0;

typedef struct {
    uint8_t preamble_pairs;
    uint8_t burst_count;
    uint32_t te_short;
    uint32_t inter_burst_low;
    uint32_t final_low;
} RenaultV0UploadShape;
#endif

static void renault_v0_set_split_bit(uint32_t* low, uint32_t* high, uint8_t bit);
static uint8_t renault_v0_parity32(uint32_t value);
static bool renault_v0_type_button_valid(RenaultV0TypeId type_id, uint8_t button);
static const RenaultV0TypeEntry*
    renault_v0_find_type_by_checks(uint8_t checksum, uint32_t key2, bool* c1_ok, bool* c2_ok);
static bool renault_v0_checksum_hi2xor_valid(uint8_t hi2xor);
static bool renault_v0_button_valid_generic(uint8_t button);
static bool renault_v0_preamble_bits_valid(uint8_t preamble_bits);
static uint8_t renault_v0_default_preamble_bits(RenaultV0TypeId type_id);
static bool renault_v0_type_preamble_bits_valid(RenaultV0TypeId type_id, uint8_t preamble_bits);
static const char* renault_v0_get_button_name(RenaultV0TypeId type_id, uint8_t button);
static void
    renault_v0_parse_fields(uint64_t data, uint32_t* serial, uint8_t* button, uint8_t* counter);
static void renault_v0_build_key(
    uint32_t serial,
    uint8_t button,
    uint8_t counter,
    uint64_t* out_data,
    uint32_t* out_key2);
static bool renault_v0_classify_event_profile(
    const RenaultV0TeProfile* profile,
    uint32_t duration,
    bool level,
    uint8_t* event_code);
static bool renault_v0_classify_event(uint32_t duration, bool level, uint8_t* event_code);
static bool renault_v0_is_end_burst(bool level, uint32_t duration, uint8_t bit_count);
#if PROTOPIRATE_WITH_ENCODER
static RenaultV0TypeId renault_v0_detect_type(uint8_t checksum, uint32_t key2, uint8_t button);
#endif
static bool
    renault_v0_classify_frame(uint64_t data, uint32_t key2, RenaultV0DecodeAttempt* attempt);
static uint8_t renault_v0_checksum(uint64_t data, uint32_t key2);
static bool renault_v0_model_matches(
    uint64_t data,
    uint32_t key2,
    uint32_t serial,
    uint8_t button,
    uint8_t counter);
static bool renault_v0_attempt_at_offset(
    const SubGhzProtocolDecoderRenaultV0* instance,
    uint8_t offset,
    RenaultV0DecodeAttempt* attempt);
static bool renault_v0_confirm_attempt(
    SubGhzProtocolDecoderRenaultV0* instance,
    const RenaultV0DecodeAttempt* attempt);
#if !defined(PROTOPIRATE_PROTOCOL_RX_ONLY) || defined(PROTOPIRATE_PROTOCOL_TX_ONLY)
static bool renault_v0_get_bit_msb82(uint64_t data, uint32_t key2, uint8_t bit_index);
#endif
static void renault_v0_apply_attempt(
    SubGhzProtocolDecoderRenaultV0* instance,
    const RenaultV0DecodeAttempt* attempt);
static void renault_v0_decode_candidate(SubGhzProtocolDecoderRenaultV0* instance);
static SubGhzProtocolStatus renault_v0_write_display(
    FlipperFormat* flipper_format,
    const char* protocol_name,
    RenaultV0TypeId type_id,
    uint8_t button);
#if PROTOPIRATE_WITH_ENCODER
static bool renault_v0_upload_shape_for_type(RenaultV0TypeId type_id, RenaultV0UploadShape* shape);
static bool
    renault_v0_upload_shape_for_preamble(uint8_t preamble_bits, RenaultV0UploadShape* shape);
static uint32_t renault_v0_upload_te_for_preamble(uint8_t preamble_bits);
static bool renault_v0_emit(
    SubGhzProtocolEncoderRenaultV0* instance,
    size_t* index,
    bool level,
    uint32_t duration);
static bool renault_v0_emit_decoded_bit(
    SubGhzProtocolEncoderRenaultV0* instance,
    size_t* index,
    uint8_t* state,
    uint32_t te_short,
    bool bit);
static bool renault_v0_build_upload(
    SubGhzProtocolEncoderRenaultV0* instance,
    RenaultV0TypeId type_id,
    uint8_t preamble_bits);
#endif

const SubGhzProtocolDecoder subghz_protocol_renault_v0_decoder = {
    .alloc = subghz_protocol_decoder_renault_v0_alloc,
    .free = pp_decoder_free_default,
    .feed = subghz_protocol_decoder_renault_v0_feed,
    .reset = subghz_protocol_decoder_renault_v0_reset,
    .get_hash_data = subghz_protocol_decoder_renault_v0_get_hash_data,
    .get_string = subghz_protocol_decoder_renault_v0_get_string,
    .serialize = subghz_protocol_decoder_renault_v0_serialize,
    .deserialize = subghz_protocol_decoder_renault_v0_deserialize,
};

#if PROTOPIRATE_WITH_ENCODER
const SubGhzProtocolEncoder subghz_protocol_renault_v0_encoder = {
    .alloc = subghz_protocol_encoder_renault_v0_alloc,
    .free = pp_encoder_free,
    .deserialize = subghz_protocol_encoder_renault_v0_deserialize,
    .stop = pp_encoder_stop,
    .yield = pp_encoder_yield,
};
#else
const SubGhzProtocolEncoder subghz_protocol_renault_v0_encoder = {
    .alloc = NULL,
    .free = NULL,
    .deserialize = NULL,
    .stop = NULL,
    .yield = NULL,
};
#endif

const SubGhzProtocol renault_v0_protocol = {
    .name = RENAULT_PROTOCOL_V0_NAME,
    .type = SubGhzProtocolTypeDynamic,
    .flag = SubGhzProtocolFlag_Decodable | SubGhzProtocolFlag_315 | SubGhzProtocolFlag_433 |
            SubGhzProtocolFlag_868 | SubGhzProtocolFlag_AM | SubGhzProtocolFlag_Save |
            SubGhzProtocolFlag_Load | SubGhzProtocolFlag_Send,

#if PROTOPIRATE_WITH_ENCODER
    .encoder = &subghz_protocol_renault_v0_encoder,
#else
    .encoder = NULL,
#endif
#if PROTOPIRATE_WITH_DECODER
    .decoder = &subghz_protocol_renault_v0_decoder,
#else
    .decoder = NULL,
#endif
};

static void renault_v0_set_split_bit(uint32_t* low, uint32_t* high, uint8_t bit) {
    if(bit < 32U) {
        *low |= (1UL << bit);
    } else {
        *high |= (1UL << (bit - 32U));
    }
}

static uint8_t renault_v0_parity32(uint32_t value) {
    value ^= value >> 16U;
    value ^= value >> 8U;
    value ^= value >> 4U;
    value ^= value >> 2U;
    value ^= value >> 1U;
    return (uint8_t)(value & 1U);
}

static bool renault_v0_type_button_valid(RenaultV0TypeId type_id, uint8_t button) {
    switch(type_id) {
    case RenaultV0Type13:
        return (button == 0x06U) || (button == 0x0AU);
    case RenaultV0Type04:
        return (button >= 0x04U) && (button <= 0x0BU);
    case RenaultV0Type0C:
        return (button >= 0xC4U) && (button <= 0xCBU);
    case RenaultV0Type1A:
    case RenaultV0Type3B:
        return (button >= 0x44U) && (button <= 0x4BU);
    case RenaultV0Type3F:
        return (button >= 0xC4U) && (button <= 0xCBU);
    case RenaultV0TypeDynamic:
        return false;
    default:
        return false;
    }
}

static const RenaultV0TypeEntry*
    renault_v0_find_type_by_checks(uint8_t checksum, uint32_t key2, bool* c1_ok, bool* c2_ok) {
    const uint8_t checksum_low6 = checksum & 0x3FU;
    const uint8_t checksum_high2_xor = (uint8_t)(((checksum >> 6U) & 0x03U) ^ (key2 & 0x03U));

    if(c1_ok) {
        *c1_ok = false;
    }
    if(c2_ok) {
        *c2_ok = false;
    }

    for(size_t i = 0; i < COUNT_OF(renault_v0_types); i++) {
        const RenaultV0TypeEntry* type = &renault_v0_types[i];
        if(checksum_low6 != type->checksum_low6) {
            continue;
        }

        if(c1_ok) {
            *c1_ok = true;
        }
        if(checksum_high2_xor != type->checksum_high2_xor) {
            return NULL;
        }

        if(c2_ok) {
            *c2_ok = true;
        }
        return type;
    }

    return NULL;
}

static bool renault_v0_checksum_hi2xor_valid(uint8_t hi2xor) {
    return (hi2xor == 0x00U) || (hi2xor == 0x03U);
}

static bool renault_v0_button_valid_generic(uint8_t button) {
    if((button == 0x06U) || (button == 0x0AU)) {
        return true;
    }
    if((button >= 0x04U) && (button <= 0x0BU)) {
        return true;
    }
    if((button >= 0x44U) && (button <= 0x4BU)) {
        return true;
    }
    if((button >= 0xC4U) && (button <= 0xCBU)) {
        return true;
    }
    return false;
}

static bool renault_v0_preamble_bits_valid(uint8_t preamble_bits) {
    switch(preamble_bits) {
    case 12U:
    case 16U:
    case 18U:
    case 20U:
        return true;
    default:
        return false;
    }
}

static uint8_t renault_v0_default_preamble_bits(RenaultV0TypeId type_id) {
    switch(type_id) {
    case RenaultV0Type13:
    case RenaultV0Type04:
        return 16U;
    case RenaultV0Type0C:
        return 18U;
    case RenaultV0Type1A:
        return 12U;
    case RenaultV0Type3B:
    case RenaultV0Type3F:
        return 20U;
    default:
        return 0U;
    }
}

static bool renault_v0_type_preamble_bits_valid(RenaultV0TypeId type_id, uint8_t preamble_bits) {
    if(type_id == RenaultV0TypeDynamic) {
        return renault_v0_preamble_bits_valid(preamble_bits);
    }

    return preamble_bits == renault_v0_default_preamble_bits(type_id);
}

static const char* renault_v0_get_button_name(RenaultV0TypeId type_id, uint8_t button) {
    if(type_id == RenaultV0Type13) {
        switch(button) {
        case 0x06:
            return "Lock";
        case 0x0A:
            return "Unlock";
        default:
            return "??";
        }
    }

    const uint8_t low_nibble = button & 0x0FU;
    if((low_nibble >= 0x04U) && (low_nibble <= 0x07U)) {
        return "Lock";
    }
    if((low_nibble >= 0x08U) && (low_nibble <= 0x0BU)) {
        return "Unlock";
    }
    return "??";
}

static void
    renault_v0_parse_fields(uint64_t data, uint32_t* serial, uint8_t* button, uint8_t* counter) {
    if(serial) {
        *serial = (uint32_t)(data >> 40U);
    }
    if(button) {
        *button = (uint8_t)(data >> 32U);
    }
    if(counter) {
        *counter = (uint8_t)(((uint32_t)data >> 24U) & 0xFFU);
    }
}

static void renault_v0_build_key(
    uint32_t serial,
    uint8_t button,
    uint8_t counter,
    uint64_t* out_data,
    uint32_t* out_key2) {
    uint8_t vars[7];
    uint8_t parity_bits[42];

    vars[0] = (button == 0x0AU) ? 1U : 0U;
    for(uint8_t bit = 0; bit < 6U; bit++) {
        vars[bit + 1U] = (counter >> bit) & 1U;
    }

    uint32_t mask_low = 1U;
    uint32_t mask_high = 0U;
    uint8_t mask_bit = 1U;

    for(uint8_t i = 0; i < 7U; i++, mask_bit++) {
        if(vars[i]) {
            renault_v0_set_split_bit(&mask_low, &mask_high, mask_bit);
        }
    }

    for(uint8_t i = 0; i < 6U; i++) {
        for(uint8_t j = i + 1U; j < 7U; j++, mask_bit++) {
            if(vars[i] & vars[j]) {
                renault_v0_set_split_bit(&mask_low, &mask_high, mask_bit);
            }
        }
    }

    for(uint8_t i = 0; i < 6U; i++) {
        for(uint8_t j = i + 1U; j < 7U; j++) {
            for(uint8_t k = j + 1U; k < 7U; k++, mask_bit++) {
                if(vars[i] & vars[j] & vars[k]) {
                    renault_v0_set_split_bit(&mask_low, &mask_high, mask_bit);
                }
            }
        }
    }

    for(size_t row = 0; row < COUNT_OF(renault_v0_matrix); row++) {
        const uint32_t mixed = (renault_v0_matrix[row].low & mask_low) ^
                               (renault_v0_matrix[row].high & mask_high);
        parity_bits[row] = renault_v0_parity32(mixed);
    }

    if(counter & 0x40U) {
        parity_bits[41] ^= 1U;
    }
    if((counter >> 7U) != 0U) {
        parity_bits[40] ^= 1U;
    }

    uint32_t data_low = ((uint32_t)counter) << 24U;
    uint32_t data_high = (serial << 8U) | (uint32_t)button;

    for(uint8_t i = 0; i < 24U; i++) {
        if(parity_bits[i]) {
            data_low |= 1UL << (23U - i);
        }
    }

    uint32_t key2 = 0U;
    for(uint8_t i = 24U; i < 42U; i++) {
        if(parity_bits[i]) {
            key2 |= 1UL << (41U - i);
        }
    }

    if(out_data) {
        *out_data = ((uint64_t)data_high << 32U) | data_low;
    }
    if(out_key2) {
        *out_key2 = key2;
    }
}

static bool renault_v0_classify_event_profile(
    const RenaultV0TeProfile* profile,
    uint32_t duration,
    bool level,
    uint8_t* event_code) {
    furi_check(event_code);
    furi_check(profile);

    const uint32_t te_short = profile->te_short;
    const uint32_t te_long = profile->te_long;
    const uint32_t te_delta = profile->te_delta;

    if(duration <= (te_short - 1U)) {
        if((te_short - duration) > te_delta) {
            return false;
        }
        *event_code = (uint8_t)(((level ? 1U : 0U) ^ 1U) << 1U);
        return true;
    }

    if(duration <= (te_long - 1U)) {
        const uint32_t short_delta = duration - te_short;
        const uint32_t long_inv_delta = te_long - duration;

        if(short_delta <= te_delta) {
            if(long_inv_delta > te_delta) {
                *event_code = (uint8_t)(((level ? 1U : 0U) ^ 1U) << 1U);
            } else {
                *event_code = level ? 4U : 6U;
            }
            return true;
        }

        if(long_inv_delta <= te_delta) {
            *event_code = level ? 4U : 6U;
            return true;
        }

        return false;
    }

    if((duration - te_long) > te_delta) {
        return false;
    }

    *event_code = level ? 4U : 6U;
    return true;
}

static bool renault_v0_classify_event(uint32_t duration, bool level, uint8_t* event_code) {
    for(size_t i = 0; i < COUNT_OF(renault_v0_te_profiles); i++) {
        if(renault_v0_classify_event_profile(
               &renault_v0_te_profiles[i], duration, level, event_code)) {
            return true;
        }
    }
    return false;
}

static bool renault_v0_is_end_burst(bool level, uint32_t duration, uint8_t bit_count) {
    return (!level) && (bit_count >= RENAULT_V0_END_BURST_MIN_BITS) &&
           (duration >= RENAULT_V0_END_BURST_MIN_US) && (duration <= RENAULT_V0_END_BURST_MAX_US);
}

#if PROTOPIRATE_WITH_ENCODER
static RenaultV0TypeId renault_v0_detect_type(uint8_t checksum, uint32_t key2, uint8_t button) {
    const RenaultV0TypeEntry* type = renault_v0_find_type_by_checks(checksum, key2, NULL, NULL);
    if(type && renault_v0_type_button_valid(type->id, button)) {
        return type->id;
    }

    return RenaultV0TypeUnknown;
}
#endif

static bool
    renault_v0_classify_frame(uint64_t data, uint32_t key2, RenaultV0DecodeAttempt* attempt) {
    furi_check(attempt);

    uint32_t serial = 0U;
    uint8_t button = 0U;
    uint8_t counter = 0U;
    renault_v0_parse_fields(data, &serial, &button, &counter);

    const uint8_t checksum = renault_v0_checksum(data, key2);
    const uint8_t checksum_low6 = checksum & 0x3FU;
    const uint8_t checksum_high2_xor = (uint8_t)(((checksum >> 6U) & 0x03U) ^ (key2 & 0x03U));
    bool c1_ok = false;
    bool c2_ok = false;
    const RenaultV0TypeEntry* type =
        renault_v0_find_type_by_checks(checksum, key2, &c1_ok, &c2_ok);
    const bool ic_ok = renault_v0_model_matches(data, key2, serial, button, counter);

    attempt->data = data;
    attempt->key2 = key2;
    attempt->serial = serial;
    attempt->button = button;
    attempt->counter = counter;
    attempt->c1_ok = c1_ok;
    attempt->c2_ok = c2_ok;
    attempt->ic_ok = ic_ok;

    if(type && c1_ok && c2_ok && renault_v0_type_button_valid(type->id, button)) {
        attempt->type_id = type->id;
        attempt->type_tag = type->value;
        return true;
    }

    const bool dynamic_c1_ok = renault_v0_button_valid_generic(button) && (serial != 0U) &&
                               (serial <= 0xFFFFFFU);
    const bool dynamic_c2_ok = renault_v0_checksum_hi2xor_valid(checksum_high2_xor);
    attempt->c1_ok = dynamic_c1_ok;
    attempt->c2_ok = dynamic_c2_ok;

    if(dynamic_c1_ok && dynamic_c2_ok) {
        attempt->type_id = RenaultV0TypeDynamic;
        attempt->type_tag = checksum_low6;
        return true;
    }

    return false;
}

static uint8_t renault_v0_checksum(uint64_t data, uint32_t key2) {
    uint8_t bytes[10];
    pp_u64_to_bytes_be(data, bytes);
    bytes[8] = (uint8_t)((key2 >> 10U) & 0xFFU);
    bytes[9] = (uint8_t)((key2 >> 2U) & 0xFFU);

    uint8_t checksum = 0U;
    for(size_t i = 0; i < COUNT_OF(bytes); i++) {
        checksum ^= bytes[i];
    }
    return checksum;
}

static bool renault_v0_model_matches(
    uint64_t data,
    uint32_t key2,
    uint32_t serial,
    uint8_t button,
    uint8_t counter) {
    uint64_t rebuilt_data = 0ULL;
    uint32_t rebuilt_key2 = 0U;
    renault_v0_build_key(serial, button, counter, &rebuilt_data, &rebuilt_key2);
    return (rebuilt_data == data) && (rebuilt_key2 == key2);
}

static bool renault_v0_attempt_at_offset(
    const SubGhzProtocolDecoderRenaultV0* instance,
    uint8_t offset,
    RenaultV0DecodeAttempt* attempt) {
    furi_check(attempt);

    if(((uint32_t)offset + 82U) > instance->decoded_bit_count) {
        return false;
    }

    uint64_t data = 0ULL;
    for(uint8_t i = 0; i < 64U; i++) {
        data = (data << 1U) | (uint64_t)(instance->decoded_bits[offset + i] & 1U);
    }

    uint32_t key2 = 0U;
    for(uint8_t i = 0; i < 18U; i++) {
        key2 = (key2 << 1U) | (uint32_t)(instance->decoded_bits[offset + 64U + i] & 1U);
    }

    if(!renault_v0_classify_frame(data, key2, attempt)) {
        return false;
    }

    attempt->preamble_bits = offset;
    if(!renault_v0_type_preamble_bits_valid(attempt->type_id, attempt->preamble_bits)) {
        return false;
    }

    return true;
}

static bool renault_v0_confirm_attempt(
    SubGhzProtocolDecoderRenaultV0* instance,
    const RenaultV0DecodeAttempt* attempt) {
    if(attempt->type_id != RenaultV0TypeDynamic) {
        instance->pending_attempt_valid = false;
        return true;
    }

    if(instance->pending_attempt_valid && (instance->pending_attempt.data == attempt->data) &&
       (instance->pending_attempt.key2 == attempt->key2) &&
       (instance->pending_attempt.type_id == attempt->type_id) &&
       (instance->pending_attempt.type_tag == attempt->type_tag) &&
       (instance->pending_attempt.preamble_bits == attempt->preamble_bits)) {
        instance->pending_attempt_valid = false;
        return true;
    }

    instance->pending_attempt = *attempt;
    instance->pending_attempt_valid = true;
    return false;
}

#if !defined(PROTOPIRATE_PROTOCOL_RX_ONLY) || defined(PROTOPIRATE_PROTOCOL_TX_ONLY)
static bool renault_v0_get_bit_msb82(uint64_t data, uint32_t key2, uint8_t bit_index) {
    if(bit_index <= 0x3FU) {
        return ((data >> (63U - bit_index)) & 1ULL) != 0ULL;
    }

    return ((key2 >> (0x51U - bit_index)) & 1U) != 0U;
}
#endif

static void renault_v0_apply_attempt(
    SubGhzProtocolDecoderRenaultV0* instance,
    const RenaultV0DecodeAttempt* attempt) {
    instance->generic.data = attempt->data;
    instance->decoder.decode_data = attempt->data;
    instance->packet_bit_count = RENAULT_V0_MIN_BITS;
    instance->decoder.decode_count_bit = RENAULT_V0_MIN_BITS;
    instance->key2 = attempt->key2;
    instance->generic.data_count_bit = RENAULT_V0_MIN_BITS;
    instance->generic.serial = attempt->serial;
    instance->generic.cnt = attempt->counter;
    instance->generic.btn = attempt->button;
    instance->type_id = attempt->type_id;
    instance->type_tag = attempt->type_tag;
    instance->preamble_bits = attempt->preamble_bits;
    instance->check_c1 = !attempt->c1_ok;
    instance->check_c2 = !attempt->c2_ok;
    instance->check_ic = !attempt->ic_ok;
}

static void renault_v0_decode_candidate(SubGhzProtocolDecoderRenaultV0* instance) {
    if(instance->decoded_bit_count <= 0x51U) {
        return;
    }

    const uint8_t tail_offset = instance->decoded_bit_count - RENAULT_V0_MIN_BITS;
    if(!renault_v0_preamble_bits_valid(tail_offset)) {
        instance->pending_attempt_valid = false;
        instance->packet_bit_count = 0U;
        instance->generic.data_count_bit = 0U;
        return;
    }

    RenaultV0DecodeAttempt attempt = {0};
    if(!renault_v0_attempt_at_offset(instance, tail_offset, &attempt)) {
        instance->pending_attempt_valid = false;
        instance->packet_bit_count = 0U;
        instance->generic.data_count_bit = 0U;
        return;
    }

    if(!renault_v0_confirm_attempt(instance, &attempt)) {
        instance->decoded_bit_count = 0U;
        return;
    }

    if((instance->packet_bit_count != 0U) && (instance->generic.data == attempt.data) &&
       (instance->key2 == attempt.key2)) {
        instance->decoded_bit_count = 0U;
        return;
    }

    renault_v0_apply_attempt(instance, &attempt);
    instance->decoded_bit_count = 0U;

    if(instance->packet_bit_count && instance->base.callback) {
        instance->base.callback(&instance->base, instance->base.context);
    }
}

static SubGhzProtocolStatus renault_v0_write_display(
    FlipperFormat* flipper_format,
    const char* protocol_name,
    RenaultV0TypeId type_id,
    uint8_t button) {
    return pp_write_display(
        flipper_format, protocol_name, renault_v0_get_button_name(type_id, button));
}

#if PROTOPIRATE_WITH_ENCODER

static bool
    renault_v0_upload_shape_for_type(RenaultV0TypeId type_id, RenaultV0UploadShape* shape) {
    furi_check(shape);

    switch(type_id) {
    case RenaultV0Type13:
        shape->preamble_pairs = 16U;
        shape->burst_count = 3U;
        shape->te_short = RENAULT_V0_TE_DEFAULT_US;
        shape->inter_burst_low = 0x61A8U;
        shape->final_low = 250U;
        return true;
    case RenaultV0Type04:
    case RenaultV0Type0C:
    case RenaultV0Type1A:
    case RenaultV0Type3B:
    case RenaultV0Type3F:
        return renault_v0_upload_shape_for_preamble(
            renault_v0_default_preamble_bits(type_id), shape);
    default:
        return false;
    }
}

static bool
    renault_v0_upload_shape_for_preamble(uint8_t preamble_bits, RenaultV0UploadShape* shape) {
    furi_check(shape);

    if(!renault_v0_preamble_bits_valid(preamble_bits)) {
        return false;
    }

    shape->preamble_pairs = preamble_bits;
    shape->burst_count = 3U;
    shape->te_short = renault_v0_upload_te_for_preamble(preamble_bits);
    shape->inter_burst_low = 1500U;
    shape->final_low = 1500U;
    return true;
}

static uint32_t renault_v0_upload_te_for_preamble(uint8_t preamble_bits) {
    if(preamble_bits == 12U) {
        return RENAULT_V0_TE_PREAMBLE_12_US;
    }
    return RENAULT_V0_TE_DEFAULT_US;
}

static bool renault_v0_emit(
    SubGhzProtocolEncoderRenaultV0* instance,
    size_t* index,
    bool level,
    uint32_t duration) {
    furi_check(instance);
    furi_check(index);

    const size_t prev = *index;
    *index =
        pp_emit_merge(instance->encoder.upload, prev, RENAULT_V0_UPLOAD_CAPACITY, level, duration);
    if(*index > prev) {
        return true;
    }
    if(prev > 0U && level_duration_get_level(instance->encoder.upload[prev - 1U]) == level) {
        return true;
    }
    return false;
}

static bool renault_v0_emit_decoded_bit(
    SubGhzProtocolEncoderRenaultV0* instance,
    size_t* index,
    uint8_t* state,
    uint32_t te_short,
    bool bit) {
    furi_check(state);

    const uint32_t te_long = te_short * 2U;

    if(*state == 1U) {
        if(bit) {
            return renault_v0_emit(instance, index, false, te_short) &&
                   renault_v0_emit(instance, index, true, te_short);
        }

        *state = 2U;
        return renault_v0_emit(instance, index, false, te_long);
    }

    if(bit) {
        *state = 1U;
        return renault_v0_emit(instance, index, true, te_long);
    }

    return renault_v0_emit(instance, index, true, te_short) &&
           renault_v0_emit(instance, index, false, te_short);
}

static bool renault_v0_build_upload(
    SubGhzProtocolEncoderRenaultV0* instance,
    RenaultV0TypeId type_id,
    uint8_t preamble_bits) {
    furi_check(instance);

    RenaultV0UploadShape shape;
    if(type_id == RenaultV0Type13) {
        if(!renault_v0_upload_shape_for_type(type_id, &shape)) {
            return false;
        }
    } else if(renault_v0_preamble_bits_valid(preamble_bits)) {
        if(!renault_v0_upload_shape_for_preamble(preamble_bits, &shape)) {
            return false;
        }
    } else {
        if(!renault_v0_upload_shape_for_type(type_id, &shape)) {
            return false;
        }
    }

    size_t write_index = 0U;
    for(uint8_t burst = 0U; burst < shape.burst_count; burst++) {
        if(!renault_v0_emit(instance, &write_index, true, 1000U)) {
            return false;
        }

        uint8_t state = 1U;
        for(uint8_t pair = 0U; pair < shape.preamble_pairs; pair++) {
            if(!renault_v0_emit_decoded_bit(instance, &write_index, &state, shape.te_short, true)) {
                return false;
            }
        }

        for(uint8_t bit_index = 0U; bit_index < RENAULT_V0_MIN_BITS; bit_index++) {
            const bool bit =
                renault_v0_get_bit_msb82(instance->generic.data, instance->key2, bit_index);
            if(!renault_v0_emit_decoded_bit(instance, &write_index, &state, shape.te_short, bit)) {
                return false;
            }
        }

        if(state == 2U) {
            if(!renault_v0_emit(instance, &write_index, true, shape.te_short)) {
                return false;
            }
        }

        const uint32_t trailing_low = (burst + 1U < shape.burst_count) ? shape.inter_burst_low :
                                                                         shape.final_low;
        if(!renault_v0_emit(instance, &write_index, false, trailing_low)) {
            return false;
        }
    }

    instance->encoder.size_upload = write_index;
    instance->encoder.front = 0U;
    return true;
}

void* subghz_protocol_encoder_renault_v0_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);

    SubGhzProtocolEncoderRenaultV0* instance = calloc(1, sizeof(SubGhzProtocolEncoderRenaultV0));
    furi_check(instance);

    instance->base.protocol = &renault_v0_protocol;
    instance->generic.protocol_name = instance->base.protocol->name;
    instance->encoder.repeat = 1U;
    instance->encoder.front = 0;
    instance->encoder.is_running = false;
    pp_encoder_buffer_ensure(instance, RENAULT_V0_UPLOAD_CAPACITY);

    return instance;
}

SubGhzProtocolStatus
    subghz_protocol_encoder_renault_v0_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_assert(context);

    SubGhzProtocolEncoderRenaultV0* instance = context;
    SubGhzProtocolStatus ret = SubGhzProtocolStatusError;

    instance->encoder.is_running = false;
    instance->encoder.front = 0;

    do {
        if(pp_verify_protocol_name(flipper_format, instance->base.protocol->name) !=
           SubGhzProtocolStatusOk) {
            break;
        }

        flipper_format_rewind(flipper_format);
        SubGhzProtocolStatus load_st = subghz_block_generic_deserialize_check_count_bit(
            &instance->generic, flipper_format, RENAULT_V0_MIN_BITS);
        if(load_st != SubGhzProtocolStatusOk) {
            break;
        }

        if(!flipper_format_rewind(flipper_format)) {
            break;
        }

        uint32_t key2 = 0U;
        if(!flipper_format_read_uint32(flipper_format, RENAULT_V0_KEY2_FIELD, &key2, 1)) {
            break;
        }
        instance->key2 = key2;
        const uint64_t captured_data = instance->generic.data;
        const uint32_t captured_key2 = instance->key2;

        uint32_t preamble_bits = 0U;
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(
               flipper_format, RENAULT_V0_PREAMBLE_FIELD, &preamble_bits, 1) &&
           renault_v0_preamble_bits_valid((uint8_t)preamble_bits)) {
            instance->preamble_bits = (uint8_t)preamble_bits;
        } else {
            instance->preamble_bits = 0U;
        }

        uint32_t serial = 0;
        uint8_t button = 0;
        uint8_t counter = 0;
        renault_v0_parse_fields(instance->generic.data, &serial, &button, &counter);
        const uint32_t captured_serial = serial;
        const uint8_t captured_button = button;
        const uint8_t captured_counter = counter;

        RenaultV0DecodeAttempt captured_attempt = {0};
        if(!renault_v0_classify_frame(captured_data, captured_key2, &captured_attempt)) {
            break;
        }
        const RenaultV0TypeId captured_type = captured_attempt.type_id;
        if(instance->preamble_bits == 0U) {
            instance->preamble_bits = renault_v0_default_preamble_bits(captured_type);
        }
        captured_attempt.preamble_bits = instance->preamble_bits;
        if((captured_type == RenaultV0TypeDynamic) &&
           !renault_v0_preamble_bits_valid(instance->preamble_bits)) {
            break;
        }
        const bool rolling_supported = (captured_type == RenaultV0Type13) &&
                                       captured_attempt.ic_ok;

        if(rolling_supported) {
            uint32_t serial_u32 = serial;
            uint32_t btn_u32 = button;
            uint32_t cnt_u32 = counter;
            pp_encoder_read_fields(flipper_format, &serial_u32, &btn_u32, &cnt_u32, NULL);

            instance->tx_button = (uint8_t)btn_u32;
            if(!renault_v0_type_button_valid(captured_type, instance->tx_button)) {
                break;
            }
            counter = (uint8_t)(cnt_u32 & 0xFFU);
            serial = serial_u32 & 0x00FFFFFFU;

            renault_v0_build_key(
                serial, instance->tx_button, counter, &instance->generic.data, &instance->key2);
            const uint8_t generated_checksum =
                renault_v0_checksum(instance->generic.data, instance->key2);
            if(renault_v0_detect_type(generated_checksum, instance->key2, instance->tx_button) !=
               RenaultV0Type13) {
                break;
            }
        } else {
            instance->tx_button = captured_button;
            serial = captured_serial;
            counter = captured_counter;
            instance->generic.data = captured_data;
            instance->key2 = captured_key2;
        }

        instance->packet_bit_count = RENAULT_V0_MIN_BITS;
        instance->generic.data_count_bit = RENAULT_V0_MIN_BITS;
        instance->generic.serial = serial;
        instance->generic.btn = instance->tx_button;
        instance->generic.cnt = counter;

        const uint32_t default_repeat = rolling_supported ? RENAULT_V0_ROLLING_REPEAT :
                                                            RENAULT_V0_REPLAY_REPEAT;
        uint32_t tx_repeat = pp_encoder_read_repeat(flipper_format, default_repeat);
        if(tx_repeat == 0U) {
            tx_repeat = default_repeat;
        }
        if(!rolling_supported && (tx_repeat < RENAULT_V0_REPLAY_REPEAT)) {
            tx_repeat = RENAULT_V0_REPLAY_REPEAT;
        }
        instance->encoder.repeat = tx_repeat;

        if(!renault_v0_build_upload(instance, captured_type, instance->preamble_bits)) {
            break;
        }
        if(instance->encoder.size_upload == 0) {
            break;
        }

        if(rolling_supported) {
            flipper_format_rewind(flipper_format);
            uint8_t key_data[8];
            pp_u64_to_bytes_be(instance->generic.data, key_data);
            if(!flipper_format_update_hex(flipper_format, FF_KEY, key_data, sizeof(key_data))) {
                flipper_format_rewind(flipper_format);
                if(!flipper_format_insert_or_update_hex(
                       flipper_format, FF_KEY, key_data, sizeof(key_data))) {
                    break;
                }
            }

            flipper_format_rewind(flipper_format);
            if(!flipper_format_update_uint32(
                   flipper_format, RENAULT_V0_KEY2_FIELD, &instance->key2, 1)) {
                flipper_format_rewind(flipper_format);
                if(!flipper_format_insert_or_update_uint32(
                       flipper_format, RENAULT_V0_KEY2_FIELD, &instance->key2, 1)) {
                    break;
                }
            }
        }

        instance->encoder.is_running = true;
        ret = SubGhzProtocolStatusOk;
    } while(false);

    return ret;
}

#endif

void* subghz_protocol_decoder_renault_v0_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);

    SubGhzProtocolDecoderRenaultV0* instance = calloc(1, sizeof(SubGhzProtocolDecoderRenaultV0));
    furi_check(instance);

    instance->base.protocol = &renault_v0_protocol;
    instance->generic.protocol_name = instance->base.protocol->name;
    instance->manchester_state = 1U;

    return instance;
}

void subghz_protocol_decoder_renault_v0_reset(void* context) {
    furi_assert(context);

    SubGhzProtocolDecoderRenaultV0* instance = context;
    instance->decoder.parser_step = RenaultV0DecoderStepReset;
    instance->manchester_state = 1U;
    instance->decoded_bit_count = 0U;
    instance->key2 = 0U;
    instance->type_id = RenaultV0TypeUnknown;
    instance->type_tag = 0U;
    instance->preamble_bits = 0U;
    instance->pending_attempt_valid = false;
}

void subghz_protocol_decoder_renault_v0_feed(void* context, bool level, uint32_t duration) {
    furi_assert(context);

    SubGhzProtocolDecoderRenaultV0* instance = context;
    uint8_t event_code = 0U;

    if(instance->decoder.parser_step == RenaultV0DecoderStepReset) {
        if(level && (duration >= RENAULT_V0_SYNC_MIN_US)) {
            instance->decoder.parser_step = RenaultV0DecoderStepData;
            instance->decoded_bit_count = 0U;
            instance->manchester_state = 1U;
        }
        return;
    }

    if(renault_v0_is_end_burst(level, duration, instance->decoded_bit_count)) {
        renault_v0_decode_candidate(instance);
        instance->decoder.parser_step = RenaultV0DecoderStepReset;
        return;
    }

    if(duration >= RENAULT_V0_GAP_RESET_US) {
        renault_v0_decode_candidate(instance);
        instance->decoder.parser_step = RenaultV0DecoderStepReset;
        if(level && (duration >= RENAULT_V0_SYNC_MIN_US)) {
            instance->decoder.parser_step = RenaultV0DecoderStepData;
            instance->decoded_bit_count = 0U;
            instance->manchester_state = 1U;
        }
        return;
    }

    if(instance->decoded_bit_count > RENAULT_V0_DECODER_BIT_LIMIT) {
        renault_v0_decode_candidate(instance);
        instance->decoder.parser_step = RenaultV0DecoderStepReset;
        return;
    }

    if(!renault_v0_classify_event(duration, level, &event_code)) {
        const bool starts_next_burst = level && (duration >= RENAULT_V0_SYNC_MIN_US);
        renault_v0_decode_candidate(instance);
        if(starts_next_burst) {
            instance->decoder.parser_step = RenaultV0DecoderStepData;
            instance->decoded_bit_count = 0U;
            instance->manchester_state = 1U;
        } else {
            instance->decoder.parser_step = RenaultV0DecoderStepReset;
        }
        return;
    }

    const uint8_t state = instance->manchester_state & 0x03U;
    uint8_t next_state = (renault_v0_decoder_state_table[state] >> event_code) & 0x03U;
    if(next_state == state) {
        return;
    }

    instance->manchester_state = next_state;
    if((next_state == 1U) || (next_state == 2U)) {
        const uint8_t bit = (next_state == 1U) ? 1U : 0U;
        const uint8_t bit_offset = instance->decoded_bit_count;
        instance->decoded_bit_count = bit_offset + 1U;
        instance->decoded_bits[bit_offset] = bit;
    }
}

static uint32_t renault_v0_arm_lsl(uint32_t value, uint32_t shift) {
    shift &= 0xFFU;
    if(shift >= 32U) {
        return 0U;
    }
    return value << shift;
}

static uint32_t renault_v0_arm_lsr(uint32_t value, uint32_t shift) {
    shift &= 0xFFU;
    if(shift >= 32U) {
        return 0U;
    }
    return value >> shift;
}

uint8_t subghz_protocol_decoder_renault_v0_get_hash_data(void* context) {
    furi_assert(context);

    SubGhzProtocolDecoderRenaultV0* instance = context;
    const uint32_t low = (uint32_t)instance->decoder.decode_data;
    const uint32_t high = (uint32_t)(instance->decoder.decode_data >> 32U);

    uint32_t hash = 0U;
    for(uint32_t shift = 0U; shift < 0x38U; shift += 8U) {
        uint32_t mixed = renault_v0_arm_lsr(low, shift);
        mixed |= renault_v0_arm_lsl(high, 32U - shift);
        mixed |= renault_v0_arm_lsr(high, shift - 32U);

        hash ^= mixed;
        hash = ((hash << 1U) & 0xFEU) | ((hash >> 7U) & 1U);
    }

    const uint32_t key2_mix = instance->key2 ^ (instance->key2 >> 2U) ^ (instance->key2 >> 10U);
    return (uint8_t)(hash ^ key2_mix);
}

void subghz_protocol_decoder_renault_v0_get_string(void* context, FuriString* output) {
    furi_assert(context);

    SubGhzProtocolDecoderRenaultV0* instance = context;

    furi_string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Key:%016llX\r\n"
        "Key2:%05lX Sn:%06lX\r\n"
        "Btn:%01X [%s] Cnt:%02lX\r\n"
        "C1:[%s] C2:[%s]\r\n"
        "IC:[%s]",
        instance->generic.protocol_name,
        instance->packet_bit_count,
        instance->generic.data,
        instance->key2,
        instance->generic.serial,
        instance->generic.btn,
        renault_v0_get_button_name(instance->type_id, instance->generic.btn),
        instance->generic.cnt,
        instance->check_c1 ? "ERR" : "OK",
        instance->check_c2 ? "ERR" : "OK",
        instance->check_ic ? "MISS" : "MATCH");
}

bool renault_v0_flipper_is_rolling(FlipperFormat* flipper_format) {
    if(!flipper_format) {
        return false;
    }

    uint32_t rolling = 0U;
    flipper_format_rewind(flipper_format);
    if(flipper_format_read_uint32(flipper_format, RENAULT_V0_ROLLING_FIELD, &rolling, 1)) {
        return rolling != 0U;
    }

    SubGhzBlockGeneric generic = {0};
    flipper_format_rewind(flipper_format);
    if(subghz_block_generic_deserialize_check_count_bit(
           &generic, flipper_format, RENAULT_V0_MIN_BITS) != SubGhzProtocolStatusOk) {
        return false;
    }

    uint32_t key2 = 0U;
    flipper_format_rewind(flipper_format);
    if(!flipper_format_read_uint32(flipper_format, RENAULT_V0_KEY2_FIELD, &key2, 1)) {
        return false;
    }

    RenaultV0DecodeAttempt attempt = {0};
    if(!renault_v0_classify_frame(generic.data, key2, &attempt)) {
        return false;
    }

    return (attempt.type_id == RenaultV0Type13) && attempt.ic_ok;
}

SubGhzProtocolStatus subghz_protocol_decoder_renault_v0_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_assert(context);

    SubGhzProtocolDecoderRenaultV0* instance = context;
    instance->generic.data_count_bit = instance->packet_bit_count;

    SubGhzProtocolStatus status =
        subghz_block_generic_serialize(&instance->generic, flipper_format, preset);
    if(status != SubGhzProtocolStatusOk) {
        return status;
    }

    status = renault_v0_write_display(
        flipper_format, instance->generic.protocol_name, instance->type_id, instance->generic.btn);
    if(status != SubGhzProtocolStatusOk) {
        return status;
    }

    if(!flipper_format_write_uint32(flipper_format, RENAULT_V0_KEY2_FIELD, &instance->key2, 1)) {
        return SubGhzProtocolStatusErrorParserOthers;
    }

    const uint32_t preamble_bits = instance->preamble_bits;
    if(!flipper_format_write_uint32(flipper_format, RENAULT_V0_PREAMBLE_FIELD, &preamble_bits, 1)) {
        return SubGhzProtocolStatusErrorParserOthers;
    }

    status = pp_serialize_fields(
        flipper_format,
        PP_FIELD_SERIAL | PP_FIELD_BTN | PP_FIELD_CNT,
        instance->generic.serial,
        instance->generic.btn,
        instance->generic.cnt,
        0U);
    if(status != SubGhzProtocolStatusOk) {
        return status;
    }

    const uint32_t rolling = ((instance->type_id == RenaultV0Type13) && !instance->check_ic) ? 1U :
                                                                                               0U;
    if(!flipper_format_write_uint32(flipper_format, RENAULT_V0_ROLLING_FIELD, &rolling, 1)) {
        return SubGhzProtocolStatusErrorParserOthers;
    }

    return SubGhzProtocolStatusOk;
}

SubGhzProtocolStatus
    subghz_protocol_decoder_renault_v0_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_assert(context);

    SubGhzProtocolDecoderRenaultV0* instance = context;
    SubGhzProtocolStatus status = subghz_block_generic_deserialize_check_count_bit(
        &instance->generic, flipper_format, RENAULT_V0_MIN_BITS);
    if(status != SubGhzProtocolStatusOk) {
        return status;
    }

    uint32_t key2 = 0U;
    if(!flipper_format_read_uint32(flipper_format, RENAULT_V0_KEY2_FIELD, &key2, 1)) {
        return SubGhzProtocolStatusError;
    }

    instance->key2 = key2;
    uint32_t preamble_bits = 0U;
    flipper_format_rewind(flipper_format);
    if(flipper_format_read_uint32(flipper_format, RENAULT_V0_PREAMBLE_FIELD, &preamble_bits, 1) &&
       renault_v0_preamble_bits_valid((uint8_t)preamble_bits)) {
        instance->preamble_bits = (uint8_t)preamble_bits;
    } else {
        instance->preamble_bits = 0U;
    }

    instance->packet_bit_count = RENAULT_V0_MIN_BITS;
    instance->generic.data_count_bit = RENAULT_V0_MIN_BITS;
    instance->decoder.decode_data = instance->generic.data;
    instance->decoder.decode_count_bit = RENAULT_V0_MIN_BITS;

    uint8_t button = 0;
    uint32_t serial = 0;
    uint8_t counter = 0;
    renault_v0_parse_fields(instance->generic.data, &serial, &button, &counter);
    instance->generic.serial = serial;
    instance->generic.btn = button;
    instance->generic.cnt = counter;

    RenaultV0DecodeAttempt attempt = {0};
    if(renault_v0_classify_frame(instance->generic.data, instance->key2, &attempt)) {
        instance->type_id = attempt.type_id;
        instance->type_tag = attempt.type_tag;
        if(instance->preamble_bits == 0U) {
            instance->preamble_bits = renault_v0_default_preamble_bits(attempt.type_id);
        }
        instance->check_c1 = !attempt.c1_ok;
        instance->check_c2 = !attempt.c2_ok;
        instance->check_ic = !attempt.ic_ok;
    } else {
        instance->type_id = RenaultV0TypeUnknown;
        instance->type_tag = 0U;
        instance->check_c1 = true;
        instance->check_c2 = true;
        instance->check_ic = true;
    }

    return status;
}
