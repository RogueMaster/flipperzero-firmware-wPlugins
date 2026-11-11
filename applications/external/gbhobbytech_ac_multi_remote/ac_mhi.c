#include "ac_mhi.h"

#define MHI_HEADER_MARK_US    3140U
#define MHI_HEADER_SPACE_US   1630U
#define MHI_BIT_MARK_US       370U
#define MHI_ONE_SPACE_US      420U
#define MHI_ZERO_SPACE_US     1220U
#define MHI_MIN_TEMP_C        17U
#define MHI_MAX_TEMP_C        31U
#define MHI_MAX_HEADER_OFFSET 128U

/* MHI frame layout and timings were independently implemented from public
 * protocol documentation and reference captures; no upstream library code is
 * included here. */
static size_t mhi_state_size(AcMhiVariant variant) {
    switch(variant) {
    case AcMhiVariant88:
        return AC_MHI_88_STATE_BYTES;
    case AcMhiVariant152:
        return AC_MHI_152_STATE_BYTES;
    default:
        return 0U;
    }
}

static bool mhi_frame_valid(const uint8_t* frame, size_t size, AcMhiVariant* variant_out) {
    static const uint8_t signature_88[] = {0xADU, 0x51U, 0x3CU, 0xD9U, 0x26U};
    static const uint8_t signature_152[] = {0xADU, 0x51U, 0x3CU, 0xE5U, 0x1AU};

    if(!frame) return false;
    const AcMhiVariant variant = size == AC_MHI_88_STATE_BYTES  ? AcMhiVariant88 :
                                 size == AC_MHI_152_STATE_BYTES ? AcMhiVariant152 :
                                                                  AcMhiVariantUnknown;
    if(variant == AcMhiVariantUnknown) return false;

    const uint8_t* signature = variant == AcMhiVariant88 ? signature_88 : signature_152;
    for(size_t i = 0; i < sizeof(signature_88); i++) {
        if(frame[i] != signature[i]) return false;
    }

    /* MHI stores each payload byte with its bitwise inverse beside it. */
    for(size_t i = 3U; i + 1U < size; i += 2U) {
        if((uint8_t)(frame[i] ^ frame[i + 1U]) != 0xFFU) return false;
    }

    if(variant_out) *variant_out = variant;
    return true;
}

bool ac_mhi_decode_signal(
    const InfraredSignal* signal,
    AcMhiVariant* variant,
    uint8_t state[AC_MHI_MAX_STATE_BYTES]) {
    if(!signal || !variant || !state || !infrared_signal_is_raw(signal)) return false;

    const InfraredRawSignal* raw = infrared_signal_get_raw_signal(signal);
    if(!raw || !raw->timings) return false;

    const AcMhiVariant variants[] = {AcMhiVariant88, AcMhiVariant152};
    for(size_t v = 0; v < sizeof(variants) / sizeof(variants[0]); v++) {
        const AcMhiVariant candidate_variant = variants[v];
        const size_t state_size = mhi_state_size(candidate_variant);
        const size_t bit_count = state_size * 8U;
        const size_t required = 2U + bit_count * 2U + 1U;
        if(raw->timings_size < required) continue;

        for(size_t header = 0;
            header + required <= raw->timings_size && header <= MHI_MAX_HEADER_OFFSET;
            header++) {
            if(raw->timings[header] < 2800U || raw->timings[header] > 3500U ||
               raw->timings[header + 1U] < 1400U || raw->timings[header + 1U] > 1850U) {
                continue;
            }

            uint8_t candidate[AC_MHI_MAX_STATE_BYTES] = {0};
            bool valid = true;
            for(size_t bit = 0; bit < bit_count; bit++) {
                const size_t mark_index = header + 2U + bit * 2U;
                const uint32_t mark = raw->timings[mark_index];
                const uint32_t space = raw->timings[mark_index + 1U];
                if(mark < 260U || mark > 520U) {
                    valid = false;
                    break;
                }
                if(space >= 250U && space <= 700U) {
                    candidate[bit / 8U] |= (uint8_t)(1U << (bit % 8U));
                } else if(space < 850U || space > 1550U) {
                    valid = false;
                    break;
                }
            }

            const uint32_t footer_mark = raw->timings[header + 2U + bit_count * 2U];
            if(!valid || footer_mark < 260U || footer_mark > 520U ||
               !mhi_frame_valid(candidate, state_size, NULL)) {
                continue;
            }

            for(size_t i = 0; i < state_size; i++)
                state[i] = candidate[i];
            *variant = candidate_variant;
            return true;
        }
    }
    return false;
}

static void mhi_invert_payload_pairs(uint8_t* state, size_t size) {
    for(size_t i = 5U; i + 1U < size; i += 2U) {
        state[i + 1U] = (uint8_t)~state[i];
    }
}

bool ac_mhi_build_state(
    AcMhiVariant variant,
    bool power_on,
    uint8_t temperature_c,
    AcMhiMode mode,
    uint8_t fan_code,
    uint8_t state[AC_MHI_MAX_STATE_BYTES]) {
    if(!state || (variant != AcMhiVariant88 && variant != AcMhiVariant152) ||
       temperature_c < MHI_MIN_TEMP_C || temperature_c > MHI_MAX_TEMP_C || mode > AcMhiModeHeat) {
        return false;
    }

    const size_t size = mhi_state_size(variant);
    for(size_t i = 0; i < AC_MHI_MAX_STATE_BYTES; i++)
        state[i] = 0U;

    if(variant == AcMhiVariant88) {
        const uint8_t signature[] = {0xADU, 0x51U, 0x3CU, 0xD9U, 0x26U};
        for(size_t i = 0; i < sizeof(signature); i++)
            state[i] = signature[i];

        /* 88-bit variant: fan is in byte 7; mode, power and temp share byte 9. */
        state[7] = (uint8_t)((fan_code & 0x07U) << 5);
        state[9] = (uint8_t)mode | (power_on ? 0x08U : 0U) |
                   (uint8_t)((temperature_c - MHI_MIN_TEMP_C) << 4);
    } else {
        const uint8_t signature[] = {0xADU, 0x51U, 0x3CU, 0xE5U, 0x1AU};
        for(size_t i = 0; i < sizeof(signature); i++)
            state[i] = signature[i];

        /* 152-bit variant fields are in the odd bytes of each payload pair. */
        state[5] = (uint8_t)mode | (power_on ? 0x08U : 0U);
        state[7] = (uint8_t)(temperature_c - MHI_MIN_TEMP_C);
        state[9] = (uint8_t)(fan_code & 0x0FU);
        state[17] = 0x80U;
    }

    mhi_invert_payload_pairs(state, size);
    return mhi_frame_valid(state, size, NULL);
}

bool ac_mhi_build_raw_timings(
    AcMhiVariant variant,
    const uint8_t state[AC_MHI_MAX_STATE_BYTES],
    uint32_t* timings,
    size_t timings_capacity,
    size_t* timings_count) {
    const size_t size = mhi_state_size(variant);
    if(!size || !state || !timings || !timings_count || !mhi_frame_valid(state, size, NULL)) {
        return false;
    }

    const size_t needed = 2U + size * 8U * 2U + 1U;
    if(timings_capacity < needed) return false;

    size_t out = 0U;
    timings[out++] = MHI_HEADER_MARK_US;
    timings[out++] = MHI_HEADER_SPACE_US;
    for(size_t bit = 0; bit < size * 8U; bit++) {
        timings[out++] = MHI_BIT_MARK_US;
        timings[out++] = (state[bit / 8U] & (uint8_t)(1U << (bit % 8U))) ? MHI_ONE_SPACE_US :
                                                                           MHI_ZERO_SPACE_US;
    }
    timings[out++] = MHI_BIT_MARK_US;
    *timings_count = out;
    return true;
}
