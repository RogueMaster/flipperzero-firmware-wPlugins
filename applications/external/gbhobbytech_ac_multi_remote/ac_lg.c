#include "ac_lg.h"

#include <stddef.h>
#include <stdint.h>
#include <infrared/signal/infrared_signal.h>

/* LG A/C timings, in microseconds. Reference framing: IRremoteESP8266
 * ir_LG.cpp. The wizard offers one neutral Cool 24 C / Auto probe per format. */
#define LG_HEADER_MARK   8500U
#define LG_HEADER_SPACE  4250U
#define LG2_HEADER_MARK  3200U
#define LG2_HEADER_SPACE 9900U
#define LG_BIT_MARK      550U
#define LG2_BIT_MARK     480U
#define LG_ONE_SPACE     1600U
#define LG_ZERO_SPACE    550U
#define LG_FOOTER_GAP    39750U
#define LG_FRAME_BITS    28U
#define LG_RAW_TIMINGS   (2U + (LG_FRAME_BITS * 2U) + 2U)
#define LG_FRAME_OFF     0x88C0051U
#define LG_SIGNATURE     0x88U

static bool duration_in_range(uint32_t measured, uint32_t expected) {
    const uint32_t tolerance = expected * 30U / 100U;
    return measured >= expected - tolerance && measured <= expected + tolerance;
}

static uint8_t checksum(uint32_t frame) {
    frame >>= 4U;
    uint8_t sum = 0U;
    for(size_t i = 0U; i < 4U; i++) {
        sum = (uint8_t)(sum + (frame & 0x0FU));
        frame >>= 4U;
    }
    return (uint8_t)(sum & 0x0FU);
}

static bool valid_frame(uint32_t frame) {
    return (uint8_t)(frame >> 20U) == LG_SIGNATURE && (frame & 0x0FU) == checksum(frame);
}

static bool protocol_timing(
    AcLgVariant variant,
    uint32_t* header_mark,
    uint32_t* header_space,
    uint32_t* bit_mark) {
    if(!header_mark || !header_space || !bit_mark) return false;
    switch(variant) {
    case AcLgVariantLg:
        *header_mark = LG_HEADER_MARK;
        *header_space = LG_HEADER_SPACE;
        *bit_mark = LG_BIT_MARK;
        return true;
    case AcLgVariantLg2:
        *header_mark = LG2_HEADER_MARK;
        *header_space = LG2_HEADER_SPACE;
        *bit_mark = LG2_BIT_MARK;
        return true;
    default:
        return false;
    }
}

static bool decode_frame(const InfraredRawSignal* raw, AcLgVariant variant, uint32_t* frame_out) {
    if(!raw || !raw->timings || !frame_out) return false;

    uint32_t header_mark = 0U;
    uint32_t header_space = 0U;
    uint32_t bit_mark = 0U;
    if(!protocol_timing(variant, &header_mark, &header_space, &bit_mark)) return false;

    const size_t required = 2U + (LG_FRAME_BITS * 2U) + 1U;
    if(raw->timings_size < required) return false;

    for(size_t start = 0U; start + required <= raw->timings_size; start++) {
        if(!duration_in_range(raw->timings[start], header_mark) ||
           !duration_in_range(raw->timings[start + 1U], header_space)) {
            continue;
        }

        uint32_t frame = 0U;
        size_t index = start + 2U;
        bool valid = true;
        for(int bit = (int)LG_FRAME_BITS - 1; bit >= 0; bit--) {
            if(!duration_in_range(raw->timings[index], bit_mark)) {
                valid = false;
                break;
            }
            const uint32_t space = raw->timings[index + 1U];
            if(duration_in_range(space, LG_ONE_SPACE)) {
                frame |= 1UL << bit;
            } else if(!duration_in_range(space, LG_ZERO_SPACE)) {
                valid = false;
                break;
            }
            index += 2U;
        }
        if(valid && duration_in_range(raw->timings[index], bit_mark) && valid_frame(frame)) {
            *frame_out = frame;
            return true;
        }
    }
    return false;
}

bool ac_lg_decode_signal(const InfraredSignal* signal, AcLgVariant* variant, uint32_t* frame) {
    if(!signal || !variant || !frame || !infrared_signal_is_raw(signal)) return false;
    const InfraredRawSignal* raw = infrared_signal_get_raw_signal(signal);
    if(decode_frame(raw, AcLgVariantLg, frame)) {
        *variant = AcLgVariantLg;
        return true;
    }
    if(decode_frame(raw, AcLgVariantLg2, frame)) {
        *variant = AcLgVariantLg2;
        return true;
    }
    return false;
}

bool ac_lg_matches_variant(const InfraredSignal* signal, AcLgVariant variant) {
    AcLgVariant found = AcLgVariantLg;
    uint32_t frame = 0U;
    return ac_lg_decode_signal(signal, &found, &frame) && found == variant;
}

/* LG 28-bit A/C state:
 * [27:20] signature 0x88, [19:18] power, [14:12] mode,
 * [11:8] temperature minus 15, [7:4] fan, [3:0] checksum.
 * Off is a dedicated command rather than an ordinary state frame.
 * Fan codes follow the common LG A/C reference; models may differ.
 */
bool ac_lg_read_controls(
    uint32_t frame,
    bool* power_on,
    uint8_t* temperature_c,
    uint8_t* mode_code,
    uint8_t* fan_index) {
    if(!valid_frame(frame) || !power_on || !temperature_c || !mode_code || !fan_index)
        return false;
    if(frame == LG_FRAME_OFF) {
        *power_on = false;
        *temperature_c = 24U;
        *mode_code = AC_LG_MODE_COOL;
        *fan_index = 0U;
        return true;
    }
    if(((frame >> 18U) & 3U) != 0U) return false;
    const uint8_t mode = (frame >> 12U) & 7U;
    const uint8_t temp = 15U + ((frame >> 8U) & 15U);
    if((mode != AC_LG_MODE_COOL && mode != AC_LG_MODE_HEAT) || temp < 16U || temp > 30U)
        return false;
    const uint8_t fan = (frame >> 4U) & 15U;
    *fan_index = fan == 5U                 ? 0U :
                 fan == 1U                 ? 1U :
                 fan == 2U                 ? 2U :
                 (fan == 4U || fan == 10U) ? 3U :
                                             0U;
    *power_on = true;
    *temperature_c = temp;
    *mode_code = mode;
    return true;
}

bool ac_lg_build_state(
    bool power_on,
    uint8_t temperature_c,
    uint8_t mode_code,
    uint8_t fan_index,
    uint32_t* frame) {
    if(!frame || (mode_code != AC_LG_MODE_COOL && mode_code != AC_LG_MODE_HEAT) ||
       temperature_c < 16U || temperature_c > 30U || fan_index > 3U)
        return false;
    if(!power_on) {
        *frame = LG_FRAME_OFF;
        return true;
    }
    static const uint8_t fan_codes[4] = {5U, 1U, 2U, 4U};
    uint32_t result = (LG_SIGNATURE << 20U) | ((uint32_t)mode_code << 12U) |
                      ((uint32_t)(temperature_c - 15U) << 8U) |
                      ((uint32_t)fan_codes[fan_index] << 4U);
    *frame = result | checksum(result);
    return valid_frame(*frame);
}

bool ac_lg_build_probe(AcLgVariant variant, bool power_on, InfraredSignal* signal) {
    if(!signal) return false;

    uint32_t header_mark = 0U;
    uint32_t header_space = 0U;
    uint32_t bit_mark = 0U;
    if(!protocol_timing(variant, &header_mark, &header_space, &bit_mark)) return false;

    uint32_t frame = 0U;
    if(!ac_lg_build_state(power_on, 24U, AC_LG_MODE_COOL, 0U, &frame)) return false;

    uint32_t timings[LG_RAW_TIMINGS];
    size_t count = 0U;
    timings[count++] = header_mark;
    timings[count++] = header_space;
    for(int bit = (int)LG_FRAME_BITS - 1; bit >= 0; bit--) {
        timings[count++] = bit_mark;
        timings[count++] = (frame & (1UL << bit)) ? LG_ONE_SPACE : LG_ZERO_SPACE;
    }
    timings[count++] = bit_mark;
    timings[count++] = LG_FOOTER_GAP;

    infrared_signal_set_raw_signal(signal, timings, count, 38000U, INFRARED_COMMON_DUTY_CYCLE);
    return true;
}

bool ac_lg_transmit(
    AcLgVariant variant,
    bool power_on,
    uint8_t temperature_c,
    uint8_t mode_code,
    uint8_t fan_index) {
    uint32_t frame = 0U;
    if(!ac_lg_build_state(power_on, temperature_c, mode_code, fan_index, &frame)) return false;
    uint32_t header_mark = 0U, header_space = 0U, bit_mark = 0U;
    if(!protocol_timing(variant, &header_mark, &header_space, &bit_mark)) return false;
    uint32_t timings[LG_RAW_TIMINGS];
    size_t count = 0U;
    timings[count++] = header_mark;
    timings[count++] = header_space;
    for(int bit = (int)LG_FRAME_BITS - 1; bit >= 0; bit--) {
        timings[count++] = bit_mark;
        timings[count++] = (frame & (1UL << bit)) ? LG_ONE_SPACE : LG_ZERO_SPACE;
    }
    timings[count++] = bit_mark;
    timings[count++] = LG_FOOTER_GAP;
    InfraredSignal* signal = infrared_signal_alloc();
    if(!signal) return false;
    infrared_signal_set_raw_signal(signal, timings, count, 38000U, INFRARED_COMMON_DUTY_CYCLE);
    infrared_signal_transmit(signal);
    infrared_signal_free(signal);
    return true;
}
