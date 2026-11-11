#include "ac_fujitsu.h"

#include <string.h>

#define FUJITSU_HDR_MARK   3324U
#define FUJITSU_HDR_SPACE  1574U
#define FUJITSU_BIT_MARK   448U
#define FUJITSU_ONE_SPACE  1182U
#define FUJITSU_ZERO_SPACE 390U
#define FUJITSU_GAP        8100U
#define FUJITSU_CARRIER_HZ 38000U
#define FUJITSU_DUTY_CYCLE INFRARED_COMMON_DUTY_CYCLE
#define FUJITSU_CMD_ON     0xFEU
#define FUJITSU_CMD_OFF    0x02U
#define FUJITSU_MODE_AUTO  0U
#define FUJITSU_MODE_COOL  1U
#define FUJITSU_MODE_DRY   2U
#define FUJITSU_MODE_FAN   3U
#define FUJITSU_MODE_HEAT  4U
#define FUJITSU_FAN_AUTO   0U
#define FUJITSU_FAN_HIGH   1U
#define FUJITSU_FAN_MED    2U
#define FUJITSU_FAN_LOW    3U
#define FUJITSU_FAN_QUIET  4U
#define FUJITSU_SWING_MASK 0x30U

static size_t state_length(AcFujitsuVariant variant) {
    return variant == AcFujitsuVariantCompact ? 15U : 16U;
}

uint8_t ac_fujitsu_min_temperature(void) {
    return 16U;
}

uint8_t ac_fujitsu_max_temperature(void) {
    return 30U;
}

void ac_fujitsu_update_checksum(
    AcFujitsuVariant variant,
    uint8_t frame[AC_FUJITSU_MAX_STATE_BYTES]) {
    if(variant == AcFujitsuVariantCompact) {
        uint8_t sum = 0U;
        for(size_t i = 0U; i < 14U; i++)
            sum = (uint8_t)(sum + frame[i]);
        frame[14] = (uint8_t)(0x9BU - sum);
    } else if(variant == AcFujitsuVariantLong30 || variant == AcFujitsuVariantLong31) {
        if(variant == AcFujitsuVariantLong30)
            frame[14] |= 0x20U;
        else
            frame[14] &= (uint8_t)~0x20U;
        uint8_t sum = 0U;
        for(size_t i = 7U; i < 15U; i++)
            sum = (uint8_t)(sum + frame[i]);
        frame[15] = (uint8_t)(0U - sum);
    }
}

static bool frame_valid(AcFujitsuVariant variant, const uint8_t* frame) {
    if(!frame || variant == AcFujitsuVariantUnknown || frame[0] != 0x14U || frame[1] != 0x63U ||
       frame[3] != 0x10U || frame[4] != 0x10U) {
        return false;
    }

    if(variant == AcFujitsuVariantCompact) {
        if(frame[5] != 0xFCU || frame[6] != 8U) return false;
        uint8_t sum = 0U;
        for(size_t i = 0U; i < 14U; i++)
            sum = (uint8_t)(sum + frame[i]);
        return frame[14] == (uint8_t)(0x9BU - sum);
    }

    if(frame[5] != FUJITSU_CMD_ON || frame[6] != 9U) return false;
    if((variant == AcFujitsuVariantLong30 && frame[7] != 0x30U) ||
       (variant == AcFujitsuVariantLong31 && frame[7] != 0x31U)) {
        return false;
    }
    uint8_t sum = 0U;
    for(size_t i = 7U; i < 16U; i++)
        sum = (uint8_t)(sum + frame[i]);
    return sum == 0U;
}

static bool decode_frame_at(
    const InfraredRawSignal* raw,
    size_t start,
    AcFujitsuVariant variant,
    uint8_t output[AC_FUJITSU_MAX_STATE_BYTES]) {
    const size_t bytes = state_length(variant);
    const size_t bits = bytes * 8U;
    const size_t required = 2U + bits * 2U;
    if(start + required > raw->timings_size) return false;

    const uint32_t header_mark = raw->timings[start];
    const uint32_t header_space = raw->timings[start + 1U];
    if(header_mark < FUJITSU_HDR_MARK * 75U / 100U ||
       header_mark > FUJITSU_HDR_MARK * 125U / 100U ||
       header_space < FUJITSU_HDR_SPACE * 75U / 100U ||
       header_space > FUJITSU_HDR_SPACE * 125U / 100U) {
        return false;
    }

    uint8_t candidate[AC_FUJITSU_MAX_STATE_BYTES] = {0U};
    for(size_t bit = 0U; bit < bits; bit++) {
        const size_t timing = start + 2U + bit * 2U;
        const uint32_t mark = raw->timings[timing];
        const uint32_t space = raw->timings[timing + 1U];
        if(mark < FUJITSU_BIT_MARK * 65U / 100U || mark > FUJITSU_BIT_MARK * 140U / 100U) {
            return false;
        }
        if(space > FUJITSU_ONE_SPACE * 65U / 100U) {
            candidate[bit / 8U] |= (uint8_t)(1U << (bit % 8U));
        } else if(space > FUJITSU_ZERO_SPACE * 55U / 100U && space < FUJITSU_ZERO_SPACE * 155U / 100U) {
            /* zero bit */
        } else {
            return false;
        }
    }

    if(!frame_valid(variant, candidate)) return false;
    memcpy(output, candidate, bytes);
    return true;
}

bool ac_fujitsu_decode_signal(
    const InfraredSignal* signal,
    AcFujitsuVariant* variant,
    uint8_t frame[AC_FUJITSU_MAX_STATE_BYTES]) {
    if(!signal || !variant || !frame || !infrared_signal_is_raw(signal)) return false;
    const InfraredRawSignal* raw = infrared_signal_get_raw_signal(signal);
    if(!raw || !raw->timings) return false;

    for(size_t i = 0U; i + 2U + 15U * 8U * 2U <= raw->timings_size; i++) {
        if(decode_frame_at(raw, i, AcFujitsuVariantCompact, frame)) {
            *variant = AcFujitsuVariantCompact;
            return true;
        }
    }
    for(size_t i = 0U; i + 2U + 16U * 8U * 2U <= raw->timings_size; i++) {
        if(raw->timings[i] < FUJITSU_HDR_MARK * 75U / 100U ||
           raw->timings[i] > FUJITSU_HDR_MARK * 125U / 100U ||
           raw->timings[i + 1U] < FUJITSU_HDR_SPACE * 75U / 100U ||
           raw->timings[i + 1U] > FUJITSU_HDR_SPACE * 125U / 100U) {
            continue;
        }
        /* The protocol byte identifies the 0x30/0x31 long-state family. */
        uint8_t candidate[AC_FUJITSU_MAX_STATE_BYTES] = {0U};
        if(decode_frame_at(raw, i, AcFujitsuVariantLong30, candidate)) {
            memcpy(frame, candidate, sizeof(candidate));
            *variant = AcFujitsuVariantLong30;
            return true;
        }
        if(decode_frame_at(raw, i, AcFujitsuVariantLong31, candidate)) {
            memcpy(frame, candidate, sizeof(candidate));
            *variant = AcFujitsuVariantLong31;
            return true;
        }
    }
    return false;
}

bool ac_fujitsu_build_state(
    AcFujitsuVariant variant,
    bool power_on,
    uint8_t temperature_c,
    uint8_t mode,
    uint8_t fan_code,
    uint8_t frame[AC_FUJITSU_MAX_STATE_BYTES]) {
    if(!frame ||
       (variant != AcFujitsuVariantLong30 && variant != AcFujitsuVariantCompact &&
        variant != AcFujitsuVariantLong31) ||
       mode > FUJITSU_MODE_HEAT || fan_code > FUJITSU_FAN_QUIET) {
        return false;
    }

    memset(frame, 0, AC_FUJITSU_MAX_STATE_BYTES);
    frame[0] = 0x14U;
    frame[1] = 0x63U;
    frame[3] = 0x10U;
    frame[4] = 0x10U;
    frame[5] = variant == AcFujitsuVariantCompact ? 0xFCU : FUJITSU_CMD_ON;
    frame[6] = variant == AcFujitsuVariantCompact ? 8U : 9U;
    frame[7] = variant == AcFujitsuVariantLong31 ? 0x31U : 0x30U;
    const uint8_t min_temp = ac_fujitsu_min_temperature();
    if(temperature_c < min_temp) temperature_c = min_temp;
    if(temperature_c > ac_fujitsu_max_temperature()) {
        temperature_c = ac_fujitsu_max_temperature();
    }
    const uint8_t temp_field = variant == AcFujitsuVariantLong31 ?
                                   (uint8_t)((temperature_c - 8U) * 2U) :
                                   (uint8_t)((temperature_c - 16U) * 4U);
    frame[8] = (uint8_t)((power_on ? 1U : 0U) | (temp_field << 2));
    frame[9] = mode & 0x07U;
    frame[10] = (uint8_t)((fan_code & 0x07U) | 0U); /* Swing bits always off. */
    frame[14] = variant == AcFujitsuVariantLong30 ? 0x20U : 0U;
    ac_fujitsu_update_checksum(variant, frame);
    return true;
}

static bool build_timings(
    AcFujitsuVariant variant,
    const uint8_t* frame,
    bool power_on,
    uint32_t timings[AC_FUJITSU_MAX_RAW_TIMINGS],
    size_t* timings_count) {
    const size_t long_length = state_length(variant);
    uint8_t command[AC_FUJITSU_MAX_STATE_BYTES] = {0U};
    size_t length = long_length;

    if(!frame_valid(variant, frame)) return false;
    if(!power_on) {
        length = variant == AcFujitsuVariantCompact ? 6U : 7U;
        memcpy(command, frame, length);
        command[5] = FUJITSU_CMD_OFF;
        if(length == 7U) command[6] = (uint8_t)~command[5];
    } else {
        memcpy(command, frame, long_length);
    }

    size_t out = 0U;
    timings[out++] = FUJITSU_HDR_MARK;
    timings[out++] = FUJITSU_HDR_SPACE;
    for(size_t bit = 0U; bit < length * 8U; bit++) {
        timings[out++] = FUJITSU_BIT_MARK;
        timings[out++] = (command[bit / 8U] & (uint8_t)(1U << (bit % 8U))) ? FUJITSU_ONE_SPACE :
                                                                             FUJITSU_ZERO_SPACE;
    }
    timings[out++] = FUJITSU_BIT_MARK;
    *timings_count = out;
    return true;
}

bool ac_fujitsu_build_probe(AcFujitsuVariant variant, bool power_on, InfraredSignal* signal) {
    if(!signal) return false;
    uint8_t frame[AC_FUJITSU_MAX_STATE_BYTES];
    uint32_t timings[AC_FUJITSU_MAX_RAW_TIMINGS];
    size_t timings_count = 0U;
    if(!ac_fujitsu_build_state(variant, true, 24U, FUJITSU_MODE_COOL, FUJITSU_FAN_AUTO, frame) ||
       !build_timings(variant, frame, power_on, timings, &timings_count)) {
        return false;
    }
    infrared_signal_set_raw_signal(
        signal, timings, timings_count, FUJITSU_CARRIER_HZ, FUJITSU_DUTY_CYCLE);
    return true;
}

bool ac_fujitsu_transmit(AcFujitsuVariant variant, const uint8_t frame[AC_FUJITSU_MAX_STATE_BYTES]) {
    if(!frame) return false;
    uint32_t timings[AC_FUJITSU_MAX_RAW_TIMINGS];
    size_t timings_count = 0U;
    const bool power_on = (frame[8] & 0x01U) != 0U;
    if(!build_timings(variant, frame, power_on, timings, &timings_count)) return false;

    InfraredSignal* signal = infrared_signal_alloc();
    infrared_signal_set_raw_signal(
        signal, timings, timings_count, FUJITSU_CARRIER_HZ, FUJITSU_DUTY_CYCLE);
    infrared_signal_transmit(signal);
    infrared_signal_free(signal);
    return true;
}
