#include "ac_mitsubishi.h"

#include <string.h>

#define MITSUBISHI_HEADER_MARK_MIN     3000U
#define MITSUBISHI_HEADER_MARK_MAX     3800U
#define MITSUBISHI_HEADER_SPACE_MIN    1400U
#define MITSUBISHI_HEADER_SPACE_MAX    2000U
#define MITSUBISHI_BIT_MARK_MIN        300U
#define MITSUBISHI_BIT_MARK_MAX        700U
#define MITSUBISHI_ONE_SPACE_THRESHOLD 850U
#define MITSUBISHI_HEADER_MARK         3400U
#define MITSUBISHI_HEADER_SPACE        1750U
#define MITSUBISHI_BIT_MARK            450U
#define MITSUBISHI_ONE_SPACE           1300U
#define MITSUBISHI_ZERO_SPACE          420U

static bool mitsubishi_frame_valid(const uint8_t* frame) {
    static const uint8_t signature[] = {0x23, 0xCB, 0x26, 0x01, 0x00};
    if(memcmp(frame, signature, sizeof(signature)) != 0) return false;

    uint8_t checksum = 0;
    for(size_t i = 0; i < AC_MITSUBISHI_FRAME_BYTES - 1U; i++) {
        checksum = (uint8_t)(checksum + frame[i]);
    }
    return checksum == frame[AC_MITSUBISHI_FRAME_BYTES - 1U];
}

void ac_mitsubishi_update_checksum(uint8_t frame[AC_MITSUBISHI_FRAME_BYTES]) {
    uint8_t checksum = 0;
    for(size_t i = 0; i < AC_MITSUBISHI_FRAME_BYTES - 1U; i++) {
        checksum = (uint8_t)(checksum + frame[i]);
    }
    frame[AC_MITSUBISHI_FRAME_BYTES - 1U] = checksum;
}

bool ac_mitsubishi_decode_profile(
    const InfraredSignal* signal,
    uint8_t frame[AC_MITSUBISHI_FRAME_BYTES],
    uint32_t* prefix,
    size_t* prefix_count) {
    if(!signal || !frame || !prefix || !prefix_count || !infrared_signal_is_raw(signal)) {
        return false;
    }

    const InfraredRawSignal* raw = infrared_signal_get_raw_signal(signal);
    const size_t required = 2U + (AC_MITSUBISHI_FRAME_BYTES * 8U * 2U);
    if(!raw || !raw->timings || raw->timings_size < required) return false;

    for(size_t header = 0; header + required <= raw->timings_size; header++) {
        const uint32_t mark = raw->timings[header];
        const uint32_t space = raw->timings[header + 1U];
        if(mark < MITSUBISHI_HEADER_MARK_MIN || mark > MITSUBISHI_HEADER_MARK_MAX ||
           space < MITSUBISHI_HEADER_SPACE_MIN || space > MITSUBISHI_HEADER_SPACE_MAX) {
            continue;
        }
        if(header > AC_MITSUBISHI_PREFIX_MAX) continue;

        uint8_t candidate[AC_MITSUBISHI_FRAME_BYTES] = {0};
        bool timings_valid = true;
        for(size_t bit = 0; bit < AC_MITSUBISHI_FRAME_BYTES * 8U; bit++) {
            const size_t mark_index = header + 2U + bit * 2U;
            const uint32_t bit_mark = raw->timings[mark_index];
            const uint32_t bit_space = raw->timings[mark_index + 1U];
            if(bit_mark < MITSUBISHI_BIT_MARK_MIN || bit_mark > MITSUBISHI_BIT_MARK_MAX) {
                timings_valid = false;
                break;
            }
            if(bit_space > MITSUBISHI_ONE_SPACE_THRESHOLD) {
                candidate[bit / 8U] |= (uint8_t)(1U << (bit % 8U));
            }
        }

        if(timings_valid && mitsubishi_frame_valid(candidate)) {
            memcpy(frame, candidate, sizeof(candidate));
            memcpy(prefix, raw->timings, header * sizeof(uint32_t));
            *prefix_count = header;
            return true;
        }
    }

    return false;
}

bool ac_mitsubishi_build_signal(
    const uint8_t frame[AC_MITSUBISHI_FRAME_BYTES],
    const uint32_t* prefix,
    size_t prefix_count,
    uint32_t* timings,
    size_t timings_capacity,
    size_t* timings_count) {
    const size_t frame_timings = 2U + AC_MITSUBISHI_FRAME_BYTES * 8U * 2U + 1U;
    if(!frame || !timings || !timings_count || prefix_count > AC_MITSUBISHI_PREFIX_MAX ||
       (prefix_count > 0U && !prefix) || prefix_count + frame_timings > timings_capacity ||
       !mitsubishi_frame_valid(frame)) {
        return false;
    }

    size_t out = 0;
    if(prefix_count > 0U) {
        memcpy(timings, prefix, prefix_count * sizeof(uint32_t));
        out = prefix_count;
    }

    timings[out++] = MITSUBISHI_HEADER_MARK;
    timings[out++] = MITSUBISHI_HEADER_SPACE;
    for(size_t bit = 0; bit < AC_MITSUBISHI_FRAME_BYTES * 8U; bit++) {
        timings[out++] = MITSUBISHI_BIT_MARK;
        timings[out++] = (frame[bit / 8U] & (1U << (bit % 8U))) ? MITSUBISHI_ONE_SPACE :
                                                                  MITSUBISHI_ZERO_SPACE;
    }
    timings[out++] = MITSUBISHI_BIT_MARK;
    *timings_count = out;
    return true;
}
