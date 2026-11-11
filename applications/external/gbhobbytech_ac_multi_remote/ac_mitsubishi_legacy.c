#include "ac_mitsubishi_legacy.h"

#include <string.h>

#define MITSU112_BYTES        14U
#define MITSU136_BYTES        17U
#define MITSU112_BITS         (MITSU112_BYTES * 8U)
#define MITSU136_BITS         (MITSU136_BYTES * 8U)
#define MITSU112_HEADER_MARK  3450U
#define MITSU112_HEADER_SPACE 1696U
#define MITSU112_BIT_MARK     450U
#define MITSU112_ONE_SPACE    1250U
#define MITSU112_ZERO_SPACE   385U
#define MITSU136_HEADER_MARK  3324U
#define MITSU136_HEADER_SPACE 1474U
#define MITSU136_BIT_MARK     467U
#define MITSU136_ONE_SPACE    1137U
#define MITSU136_ZERO_SPACE   351U
#define PROBE_TEMPERATURE_C   24U

static const uint8_t mitsu112_reset[MITSU112_BYTES] = {
    0x23U,
    0xCBU,
    0x26U,
    0x01U,
    0x00U,
    0x24U,
    0x03U,
    0x0BU,
    0x10U,
    0x00U,
    0x00U,
    0x00U,
    0x30U,
    0x00U,
};

static const uint8_t mitsu136_reset[MITSU136_BYTES] = {
    0x23U,
    0xCBU,
    0x26U,
    0x21U,
    0x00U,
    0x40U,
    0xC2U,
    0xC7U,
    0x04U,
    0x00U,
    0x00U,
    0xBFU,
    0x3DU,
    0x38U,
    0xFBU,
    0xFFU,
    0xFFU,
};

/* Native fan order is quiet/minimum, low, medium, maximum. 112-bit has
 * no distinct auto fan code; the app presents its minimum as QUIET. */
static const uint8_t mitsu112_fan_codes[] = {2U, 3U, 5U, 0U};
static const uint8_t mitsu136_fan_codes[] = {0U, 1U, 2U, 3U};

static void update_checksum(AcMitsubishiLegacyVariant variant, uint8_t* frame) {
    if(variant == AcMitsubishiLegacy112) {
        uint8_t sum = 0U;
        for(size_t i = 0U; i < MITSU112_BYTES - 1U; i++) {
            sum = (uint8_t)(sum + frame[i]);
        }
        frame[MITSU112_BYTES - 1U] = sum;
    } else if(variant == AcMitsubishiLegacy136) {
        for(size_t i = 0U; i < 6U; i++) {
            frame[11U + i] = (uint8_t)~frame[5U + i];
        }
    }
}

static bool frame_valid(AcMitsubishiLegacyVariant variant, const uint8_t* frame) {
    if(variant == AcMitsubishiLegacy112) {
        static const uint8_t signature[] = {0x23U, 0xCBU, 0x26U, 0x01U, 0x00U};
        if(memcmp(frame, signature, sizeof(signature)) != 0) return false;
        uint8_t sum = 0U;
        for(size_t i = 0U; i < MITSU112_BYTES - 1U; i++) {
            sum = (uint8_t)(sum + frame[i]);
        }
        return sum == frame[MITSU112_BYTES - 1U];
    }
    if(variant == AcMitsubishiLegacy136) {
        static const uint8_t signature[] = {0x23U, 0xCBU, 0x26U};
        if(memcmp(frame, signature, sizeof(signature)) != 0) return false;
        for(size_t i = 0U; i < 6U; i++) {
            if(frame[11U + i] != (uint8_t)~frame[5U + i]) return false;
        }
        return true;
    }
    return false;
}

static bool decode_frame_at(
    const InfraredRawSignal* raw,
    size_t header,
    AcMitsubishiLegacyVariant variant,
    uint8_t* output) {
    const size_t bytes = (variant == AcMitsubishiLegacy112) ? MITSU112_BYTES : MITSU136_BYTES;
    const size_t bits = bytes * 8U;
    const uint32_t hdr_mark = (variant == AcMitsubishiLegacy112) ? MITSU112_HEADER_MARK :
                                                                   MITSU136_HEADER_MARK;
    const uint32_t hdr_space = (variant == AcMitsubishiLegacy112) ? MITSU112_HEADER_SPACE :
                                                                    MITSU136_HEADER_SPACE;
    const uint32_t bit_mark = (variant == AcMitsubishiLegacy112) ? MITSU112_BIT_MARK :
                                                                   MITSU136_BIT_MARK;
    const uint32_t one_space = (variant == AcMitsubishiLegacy112) ? MITSU112_ONE_SPACE :
                                                                    MITSU136_ONE_SPACE;
    const size_t required = 2U + bits * 2U;
    if(header + required > raw->timings_size) return false;

    const uint32_t mark = raw->timings[header];
    const uint32_t space = raw->timings[header + 1U];
    if(mark < hdr_mark * 80U / 100U || mark > hdr_mark * 120U / 100U ||
       space < hdr_space * 75U / 100U || space > hdr_space * 125U / 100U) {
        return false;
    }

    uint8_t candidate[AC_MITSUBISHI_LEGACY_MAX_FRAME_BYTES] = {0U};
    const uint32_t mark_min = bit_mark * 60U / 100U;
    const uint32_t mark_max = bit_mark * 140U / 100U;
    const uint32_t threshold =
        (one_space +
         ((variant == AcMitsubishiLegacy112) ? MITSU112_ZERO_SPACE : MITSU136_ZERO_SPACE)) /
        2U;

    for(size_t bit = 0U; bit < bits; bit++) {
        const size_t timing_index = header + 2U + bit * 2U;
        const uint32_t data_mark = raw->timings[timing_index];
        if(data_mark < mark_min || data_mark > mark_max) return false;
        if(raw->timings[timing_index + 1U] > threshold) {
            candidate[bit / 8U] |= (uint8_t)(1U << (bit % 8U));
        }
    }

    if(!frame_valid(variant, candidate)) return false;
    memcpy(output, candidate, bytes);
    return true;
}

bool ac_mitsubishi_legacy_decode(
    const InfraredSignal* signal,
    AcMitsubishiLegacyVariant* variant,
    uint8_t frame[AC_MITSUBISHI_LEGACY_MAX_FRAME_BYTES]) {
    if(!signal || !variant || !frame || !infrared_signal_is_raw(signal)) return false;
    const InfraredRawSignal* raw = infrared_signal_get_raw_signal(signal);
    if(!raw || !raw->timings) return false;

    for(size_t i = 0U; i + 2U + MITSU112_BITS * 2U <= raw->timings_size; i++) {
        if(decode_frame_at(raw, i, AcMitsubishiLegacy112, frame)) {
            *variant = AcMitsubishiLegacy112;
            return true;
        }
    }
    for(size_t i = 0U; i + 2U + MITSU136_BITS * 2U <= raw->timings_size; i++) {
        if(decode_frame_at(raw, i, AcMitsubishiLegacy136, frame)) {
            *variant = AcMitsubishiLegacy136;
            return true;
        }
    }
    return false;
}

static bool build_timings(
    AcMitsubishiLegacyVariant variant,
    const uint8_t* frame,
    uint32_t* timings,
    size_t capacity,
    size_t* count) {
    const size_t bytes = (variant == AcMitsubishiLegacy112) ? MITSU112_BYTES : MITSU136_BYTES;
    const size_t bits = bytes * 8U;
    const size_t required = 2U + bits * 2U + 1U;
    const uint32_t hdr_mark = (variant == AcMitsubishiLegacy112) ? MITSU112_HEADER_MARK :
                                                                   MITSU136_HEADER_MARK;
    const uint32_t hdr_space = (variant == AcMitsubishiLegacy112) ? MITSU112_HEADER_SPACE :
                                                                    MITSU136_HEADER_SPACE;
    const uint32_t bit_mark = (variant == AcMitsubishiLegacy112) ? MITSU112_BIT_MARK :
                                                                   MITSU136_BIT_MARK;
    const uint32_t one_space = (variant == AcMitsubishiLegacy112) ? MITSU112_ONE_SPACE :
                                                                    MITSU136_ONE_SPACE;
    const uint32_t zero_space = (variant == AcMitsubishiLegacy112) ? MITSU112_ZERO_SPACE :
                                                                     MITSU136_ZERO_SPACE;
    if(!frame || !timings || !count || capacity < required || !frame_valid(variant, frame))
        return false;

    size_t out = 0U;
    timings[out++] = hdr_mark;
    timings[out++] = hdr_space;
    for(size_t bit = 0U; bit < bits; bit++) {
        timings[out++] = bit_mark;
        timings[out++] = (frame[bit / 8U] & (1U << (bit % 8U))) ? one_space : zero_space;
    }
    timings[out++] = bit_mark;
    *count = out;
    return true;
}

static bool transmit_frame(
    AcMitsubishiLegacyVariant variant,
    const uint8_t* frame,
    InfraredSignal* signal) {
    uint32_t timings[AC_MITSUBISHI_LEGACY_MAX_RAW_TIMINGS];
    size_t count = 0U;
    if(!build_timings(variant, frame, timings, AC_MITSUBISHI_LEGACY_MAX_RAW_TIMINGS, &count)) {
        return false;
    }
    infrared_signal_set_raw_signal(signal, timings, count, 38000U, INFRARED_COMMON_DUTY_CYCLE);
    return true;
}

bool ac_mitsubishi_legacy_transmit(
    AcMitsubishiLegacyVariant variant,
    const uint8_t frame[AC_MITSUBISHI_LEGACY_MAX_FRAME_BYTES]) {
    InfraredSignal* signal = infrared_signal_alloc();
    const bool built = transmit_frame(variant, frame, signal);
    if(built) infrared_signal_transmit(signal);
    infrared_signal_free(signal);
    return built;
}

bool ac_mitsubishi_legacy_build_probe(
    AcMitsubishiLegacyVariant variant,
    bool power_on,
    InfraredSignal* signal) {
    if(!signal) return false;
    uint8_t frame[AC_MITSUBISHI_LEGACY_MAX_FRAME_BYTES] = {0U};
    if(variant == AcMitsubishiLegacy112) {
        memcpy(frame, mitsu112_reset, sizeof(mitsu112_reset));
        frame[5] = (uint8_t)(0x20U | (power_on ? 0x04U : 0x00U));
        frame[6] = 0x03U; /* Cool */
        frame[7] = (uint8_t)(31U - PROBE_TEMPERATURE_C);
        frame[8] = 0x02U; /* Quiet/minimum fan */
        if(!power_on) frame[5] &= (uint8_t)~0x04U;
    } else if(variant == AcMitsubishiLegacy136) {
        memcpy(frame, mitsu136_reset, sizeof(mitsu136_reset));
        frame[5] = 0U;
        frame[6] = 0U;
        frame[7] = 0xC0U;
        if(power_on) frame[5] |= 0x40U;
        frame[6] = (uint8_t)(0x01U | ((PROBE_TEMPERATURE_C - 16U) << 4));
    } else {
        return false;
    }
    update_checksum(variant, frame);
    return transmit_frame(variant, frame, signal);
}

bool ac_mitsubishi_legacy_get_controls(
    AcMitsubishiLegacyVariant variant,
    const uint8_t frame[AC_MITSUBISHI_LEGACY_MAX_FRAME_BYTES],
    bool* power_on,
    uint8_t* temperature_c,
    uint8_t* mode_code,
    uint8_t* fan_index) {
    if(!frame_valid(variant, frame) || !power_on || !temperature_c || !mode_code || !fan_index) {
        return false;
    }
    if(variant == AcMitsubishiLegacy112) {
        *power_on = (frame[5] & 0x04U) != 0U;
        *mode_code = frame[6] & 0x07U;
        *temperature_c = (uint8_t)(31U - (frame[7] & 0x0FU));
        const uint8_t fan_code = frame[8] & 0x07U;
        *fan_index = (fan_code == 3U) ? 1U : (fan_code == 5U) ? 2U : (fan_code == 0U) ? 3U : 0U;
    } else if(variant == AcMitsubishiLegacy136) {
        *power_on = (frame[5] & 0x40U) != 0U;
        *mode_code = frame[6] & 0x07U;
        *temperature_c = (uint8_t)(16U + ((frame[6] >> 4) & 0x0FU));
        *fan_index = (frame[7] >> 1) & 0x03U;
    } else {
        return false;
    }
    return true;
}

void ac_mitsubishi_legacy_set_power(
    AcMitsubishiLegacyVariant variant,
    uint8_t frame[AC_MITSUBISHI_LEGACY_MAX_FRAME_BYTES],
    bool power_on) {
    if(variant == AcMitsubishiLegacy112) {
        if(power_on)
            frame[5] |= 0x04U;
        else
            frame[5] &= (uint8_t)~0x04U;
    } else if(variant == AcMitsubishiLegacy136) {
        if(power_on)
            frame[5] |= 0x40U;
        else
            frame[5] &= (uint8_t)~0x40U;
    }
    update_checksum(variant, frame);
}

void ac_mitsubishi_legacy_set_temperature(
    AcMitsubishiLegacyVariant variant,
    uint8_t frame[AC_MITSUBISHI_LEGACY_MAX_FRAME_BYTES],
    uint8_t temperature_c) {
    if(variant == AcMitsubishiLegacy112) {
        if(temperature_c < 16U) temperature_c = 16U;
        if(temperature_c > 31U) temperature_c = 31U;
        frame[7] = (uint8_t)((frame[7] & 0xF0U) | (31U - temperature_c));
    } else if(variant == AcMitsubishiLegacy136) {
        if(temperature_c < 17U) temperature_c = 17U;
        if(temperature_c > 30U) temperature_c = 30U;
        frame[6] = (uint8_t)((frame[6] & 0x0FU) | ((temperature_c - 16U) << 4));
    }
    update_checksum(variant, frame);
}

void ac_mitsubishi_legacy_toggle_mode(
    AcMitsubishiLegacyVariant variant,
    uint8_t frame[AC_MITSUBISHI_LEGACY_MAX_FRAME_BYTES]) {
    if(variant == AcMitsubishiLegacy112) {
        const uint8_t current = frame[6] & 0x07U;
        const uint8_t next = current == 3U ? 1U : 3U;
        frame[6] = (uint8_t)((frame[6] & 0xF8U) | next);
    } else if(variant == AcMitsubishiLegacy136) {
        const uint8_t current = frame[6] & 0x07U;
        const uint8_t next = current == 1U ? 2U : 1U;
        frame[6] = (uint8_t)((frame[6] & 0xF8U) | next);
    }
    update_checksum(variant, frame);
}

void ac_mitsubishi_legacy_set_fan(
    AcMitsubishiLegacyVariant variant,
    uint8_t frame[AC_MITSUBISHI_LEGACY_MAX_FRAME_BYTES],
    uint8_t fan_index) {
    if(variant == AcMitsubishiLegacy112) {
        if(fan_index > 3U) fan_index = 3U;
        frame[8] = (uint8_t)((frame[8] & 0xF8U) | mitsu112_fan_codes[fan_index]);
    } else if(variant == AcMitsubishiLegacy136) {
        if(fan_index > 3U) fan_index = 3U;
        frame[7] = (uint8_t)((frame[7] & (uint8_t)~0x06U) |
                             (uint8_t)(mitsu136_fan_codes[fan_index] << 1));
    }
    update_checksum(variant, frame);
}

uint8_t ac_mitsubishi_legacy_min_temperature(AcMitsubishiLegacyVariant variant) {
    return (variant == AcMitsubishiLegacy112) ? 16U : 17U;
}

uint8_t ac_mitsubishi_legacy_max_temperature(AcMitsubishiLegacyVariant variant) {
    return (variant == AcMitsubishiLegacy112) ? 31U : 30U;
}

uint8_t ac_mitsubishi_legacy_fan_count(AcMitsubishiLegacyVariant variant) {
    return (variant == AcMitsubishiLegacy112 || variant == AcMitsubishiLegacy136) ? 4U : 5U;
}
