#include "ac_gree.h"

#include <string.h>

/* Gree state layout and timings follow the public IRremoteESP8266 protocol
 * reference, reimplemented here for Flipper Zero. */
#define GREE_HEADER_MARK              9000U
#define GREE_HEADER_SPACE             4500U
#define GREE_BIT_MARK                 620U
#define GREE_ONE_SPACE                1600U
#define GREE_ZERO_SPACE               540U
#define GREE_MESSAGE_GAP              19980U
#define GREE_MID_FOOTER               0x02U
#define GREE_MID_FOOTER_BITS          3U
#define GREE_FAN_AUTO                 0U
#define GREE_FAN_MIN                  1U
#define GREE_FAN_MED                  2U
#define GREE_FAN_MAX                  3U
#define GREE_MODE_AUTO                0U
#define GREE_MODE_COOL                1U
#define GREE_MODE_DRY                 2U
#define GREE_MODE_FAN                 3U
#define GREE_MODE_HEAT                4U
#define GREE_MODE_ECONO               5U
#define GREE_MIN_TEMP_C               16U
#define GREE_MAX_TEMP_C               30U
#define GREE_PROBE_TEMP_C             24U
#define GREE_TIMING_TOLERANCE_PERCENT 30U
#define GREE_GAP_MIN                  10000U
#define GREE_GAP_MAX                  50000U

static uint8_t gree_checksum(const uint8_t frame[AC_GREE_STATE_BYTES]) {
    uint8_t sum = 10U;
    for(size_t i = 0U; i < 4U; i++)
        sum = (uint8_t)(sum + (frame[i] & 0x0FU));
    for(size_t i = 4U; i < 7U; i++)
        sum = (uint8_t)(sum + (frame[i] >> 4));
    return (uint8_t)(sum & 0x0FU);
}

static void update_checksum(uint8_t frame[AC_GREE_STATE_BYTES]) {
    frame[7] = (uint8_t)((frame[7] & 0x0FU) | (gree_checksum(frame) << 4));
}

static bool frame_valid(const uint8_t frame[AC_GREE_STATE_BYTES]) {
    if(!frame) return false;
    if((frame[3] & 0xF0U) != 0x50U || (frame[5] & 0x38U) != 0x20U) return false;
    return (frame[7] >> 4) == gree_checksum(frame);
}

static bool duration_in_range(uint32_t measured, uint32_t expected, uint32_t tolerance_percent) {
    const uint32_t tolerance = expected * tolerance_percent / 100U;
    return measured >= expected - tolerance && measured <= expected + tolerance;
}

static bool decode_byte(const InfraredRawSignal* raw, size_t* timing_index, uint8_t* value) {
    uint8_t decoded = 0U;
    for(size_t bit = 0U; bit < 8U; bit++) {
        if(*timing_index + 1U >= raw->timings_size) return false;
        if(!duration_in_range(
               raw->timings[*timing_index], GREE_BIT_MARK, GREE_TIMING_TOLERANCE_PERCENT)) {
            return false;
        }
        if(raw->timings[*timing_index + 1U] > (GREE_ONE_SPACE + GREE_ZERO_SPACE) / 2U) {
            decoded |= (uint8_t)(1U << bit);
        }
        *timing_index += 2U;
    }
    *value = decoded;
    return true;
}

static bool decode_footer(const InfraredRawSignal* raw, size_t* timing_index) {
    static const bool footer_bits[GREE_MID_FOOTER_BITS] = {false, true, false};
    for(size_t bit = 0U; bit < GREE_MID_FOOTER_BITS; bit++) {
        if(*timing_index + 1U >= raw->timings_size) return false;
        if(!duration_in_range(
               raw->timings[*timing_index], GREE_BIT_MARK, GREE_TIMING_TOLERANCE_PERCENT)) {
            return false;
        }
        const bool received_one = raw->timings[*timing_index + 1U] >
                                  (GREE_ONE_SPACE + GREE_ZERO_SPACE) / 2U;
        if(received_one != footer_bits[bit]) return false;
        *timing_index += 2U;
    }
    return true;
}

static bool decode_at(
    const InfraredRawSignal* raw,
    size_t header_index,
    uint8_t frame[AC_GREE_STATE_BYTES]) {
    const size_t required = 2U + 32U * 2U + GREE_MID_FOOTER_BITS * 2U + 2U + 32U * 2U + 2U;
    if(header_index + required > raw->timings_size) return false;
    if(!duration_in_range(
           raw->timings[header_index], GREE_HEADER_MARK, GREE_TIMING_TOLERANCE_PERCENT) ||
       !duration_in_range(
           raw->timings[header_index + 1U], GREE_HEADER_SPACE, GREE_TIMING_TOLERANCE_PERCENT)) {
        return false;
    }

    size_t timing_index = header_index + 2U;
    for(size_t i = 0U; i < 4U; i++) {
        if(!decode_byte(raw, &timing_index, &frame[i])) return false;
    }
    if(!decode_footer(raw, &timing_index)) return false;
    if(!duration_in_range(
           raw->timings[timing_index], GREE_BIT_MARK, GREE_TIMING_TOLERANCE_PERCENT) ||
       raw->timings[timing_index + 1U] < GREE_GAP_MIN ||
       raw->timings[timing_index + 1U] > GREE_GAP_MAX) {
        return false;
    }
    timing_index += 2U;
    for(size_t i = 4U; i < AC_GREE_STATE_BYTES; i++) {
        if(!decode_byte(raw, &timing_index, &frame[i])) return false;
    }
    if(!duration_in_range(
           raw->timings[timing_index], GREE_BIT_MARK, GREE_TIMING_TOLERANCE_PERCENT) ||
       raw->timings[timing_index + 1U] < GREE_GAP_MIN) {
        return false;
    }
    return frame_valid(frame);
}

bool ac_gree_decode_signal(
    const InfraredSignal* signal,
    AcGreeModel* model,
    uint8_t frame[AC_GREE_STATE_BYTES]) {
    if(!signal || !model || !frame || !infrared_signal_is_raw(signal)) return false;
    const InfraredRawSignal* raw = infrared_signal_get_raw_signal(signal);
    if(!raw || !raw->timings) return false;

    uint8_t candidate[AC_GREE_STATE_BYTES];
    const size_t required = 2U + 32U * 2U + GREE_MID_FOOTER_BITS * 2U + 2U + 32U * 2U + 2U;
    for(size_t i = 0U; i + required <= raw->timings_size; i++) {
        if(decode_at(raw, i, candidate)) {
            memcpy(frame, candidate, sizeof(candidate));
            if((frame[0] & 0x07U) == GREE_MODE_ECONO) {
                *model = AcGreeModelYx1fsf;
            } else if(frame[0] & 0x08U) {
                *model = (frame[2] & 0x40U) ? AcGreeModelYaw1f : AcGreeModelYbofb;
            } else {
                *model = AcGreeModelUnknown;
            }
            return true;
        }
    }
    return false;
}

static bool build_timings(
    const uint8_t frame[AC_GREE_STATE_BYTES],
    uint32_t* timings,
    size_t capacity,
    size_t* count) {
    const size_t required = 2U + 64U * 2U + GREE_MID_FOOTER_BITS * 2U + 2U + 2U;
    if(!frame || !timings || !count || capacity < required || !frame_valid(frame)) return false;

    size_t out = 0U;
    timings[out++] = GREE_HEADER_MARK;
    timings[out++] = GREE_HEADER_SPACE;
    for(size_t byte = 0U; byte < 4U; byte++) {
        for(size_t bit = 0U; bit < 8U; bit++) {
            timings[out++] = GREE_BIT_MARK;
            timings[out++] = (frame[byte] & (1U << bit)) ? GREE_ONE_SPACE : GREE_ZERO_SPACE;
        }
    }

    /* Three-bit 010 marker followed by the inter-block gap. */
    timings[out++] = GREE_BIT_MARK;
    timings[out++] = GREE_ZERO_SPACE;
    timings[out++] = GREE_BIT_MARK;
    timings[out++] = GREE_ONE_SPACE;
    timings[out++] = GREE_BIT_MARK;
    timings[out++] = GREE_ZERO_SPACE;
    timings[out++] = GREE_BIT_MARK;
    timings[out++] = GREE_MESSAGE_GAP;

    for(size_t byte = 4U; byte < AC_GREE_STATE_BYTES; byte++) {
        for(size_t bit = 0U; bit < 8U; bit++) {
            timings[out++] = GREE_BIT_MARK;
            timings[out++] = (frame[byte] & (1U << bit)) ? GREE_ONE_SPACE : GREE_ZERO_SPACE;
        }
    }
    timings[out++] = GREE_BIT_MARK;
    timings[out++] = GREE_MESSAGE_GAP;
    *count = out;
    return true;
}

static bool
    build_model_state(AcGreeModel model, bool power_on, uint8_t frame[AC_GREE_STATE_BYTES]) {
    if(model != AcGreeModelYaw1f && model != AcGreeModelYbofb && model != AcGreeModelYx1fsf) {
        return false;
    }
    memset(frame, 0, AC_GREE_STATE_BYTES);
    /* Keep SwingAuto and Sleep clear in byte 0 for a neutral probe state. */
    frame[0] = (uint8_t)((model == AcGreeModelYx1fsf ? GREE_MODE_ECONO : GREE_MODE_COOL) |
                         (power_on ? 0x08U : 0U));
    frame[1] = (uint8_t)(GREE_PROBE_TEMP_C - GREE_MIN_TEMP_C);
    frame[2] = (uint8_t)(0x20U | ((model == AcGreeModelYaw1f && power_on) ? 0x40U : 0U));
    frame[3] = 0x50U;
    frame[4] = 0x00U; /* Leave vertical and horizontal swing unchanged. */
    frame[5] = 0x21U; /* Display set temperature and fixed protocol bits. */
    update_checksum(frame);
    return true;
}

static bool set_raw_signal(const uint8_t frame[AC_GREE_STATE_BYTES], InfraredSignal* signal) {
    uint32_t timings[AC_GREE_RAW_MAX_TIMINGS];
    size_t count = 0U;
    if(!signal || !build_timings(frame, timings, AC_GREE_RAW_MAX_TIMINGS, &count)) return false;
    infrared_signal_set_raw_signal(signal, timings, count, 38000U, INFRARED_COMMON_DUTY_CYCLE);
    return true;
}

bool ac_gree_build_probe(AcGreeModel model, bool power_on, InfraredSignal* signal) {
    uint8_t frame[AC_GREE_STATE_BYTES];
    return build_model_state(model, power_on, frame) && set_raw_signal(frame, signal);
}

bool ac_gree_transmit(AcGreeModel model, const uint8_t frame[AC_GREE_STATE_BYTES]) {
    (void)model;
    InfraredSignal* signal = infrared_signal_alloc();
    const bool built = set_raw_signal(frame, signal);
    if(built) infrared_signal_transmit(signal);
    infrared_signal_free(signal);
    return built;
}

bool ac_gree_get_controls(
    AcGreeModel* model,
    const uint8_t frame[AC_GREE_STATE_BYTES],
    bool* power_on,
    uint8_t* temperature_c,
    uint8_t* mode_code,
    uint8_t* fan_index) {
    if(!model || !frame_valid(frame) || !power_on || !temperature_c || !mode_code || !fan_index) {
        return false;
    }
    *power_on = (frame[0] & 0x08U) != 0U;
    *mode_code = frame[0] & 0x07U;
    *temperature_c = (uint8_t)(GREE_MIN_TEMP_C + (frame[1] & 0x0FU));
    *fan_index = (frame[0] >> 4) & 0x03U;
    if(*model == AcGreeModelUnknown) {
        if(*mode_code == GREE_MODE_ECONO)
            *model = AcGreeModelYx1fsf;
        else if(*power_on)
            *model = (frame[2] & 0x40U) ? AcGreeModelYaw1f : AcGreeModelYbofb;
        else
            *model = AcGreeModelYbofb;
    }
    return true;
}

void ac_gree_set_power(AcGreeModel model, uint8_t frame[AC_GREE_STATE_BYTES], bool power_on) {
    if(power_on)
        frame[0] |= 0x08U;
    else
        frame[0] &= (uint8_t)~0x08U;
    if(power_on && model == AcGreeModelYaw1f)
        frame[2] |= 0x40U;
    else
        frame[2] &= (uint8_t)~0x40U;
    update_checksum(frame);
}

void ac_gree_set_temperature(uint8_t frame[AC_GREE_STATE_BYTES], uint8_t temperature_c) {
    if(temperature_c < GREE_MIN_TEMP_C) temperature_c = GREE_MIN_TEMP_C;
    if(temperature_c > GREE_MAX_TEMP_C) temperature_c = GREE_MAX_TEMP_C;
    frame[1] = (uint8_t)((frame[1] & 0xF0U) | (temperature_c - GREE_MIN_TEMP_C));
    update_checksum(frame);
}

void ac_gree_toggle_mode(uint8_t frame[AC_GREE_STATE_BYTES]) {
    const uint8_t current = frame[0] & 0x07U;
    const uint8_t next = current == GREE_MODE_COOL ? GREE_MODE_HEAT : GREE_MODE_COOL;
    frame[0] = (uint8_t)((frame[0] & 0xF8U) | next);
    update_checksum(frame);
}

void ac_gree_set_fan(uint8_t frame[AC_GREE_STATE_BYTES], uint8_t fan_index) {
    if(fan_index > GREE_FAN_MAX) fan_index = GREE_FAN_MAX;
    frame[0] = (uint8_t)((frame[0] & (uint8_t)~0x30U) | (fan_index << 4));
    update_checksum(frame);
}
