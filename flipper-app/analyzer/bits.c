#include "bits.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

void bits_strip_padding(
    const uint8_t* bits,
    uint16_t len,
    uint16_t* out_start,
    uint16_t* out_len) {
    if(len == 0) {
        *out_start = 0;
        *out_len = 0;
        return;
    }
    uint16_t start = 0;
    while(start < len && bits[start] == 0) start++;
    if(start == len) {
        *out_start = 0;
        *out_len = 0;
        return;
    }
    uint16_t end = (uint16_t)(len - 1);
    while(end > start && bits[end] == 0) end--;
    *out_start = start;
    *out_len = (uint16_t)(end - start + 1);
}

float bits_entropy(const uint8_t* bits, uint32_t len) {
    if(len == 0) return 0.0f;
    uint32_t ones = 0;
    for(uint32_t i = 0; i < len; i++) ones += bits[i] ? 1u : 0u;
    uint32_t zeros = len - ones;
    float entropy = 0.0f;
    if(ones > 0) {
        float p = (float)ones / (float)len;
        entropy -= p * log2f(p);
    }
    if(zeros > 0) {
        float p = (float)zeros / (float)len;
        entropy -= p * log2f(p);
    }
    return entropy;
}

float bits_zero_ratio(const uint8_t* bits, uint32_t len) {
    if(len == 0) return 1.0f;
    uint32_t zeros = 0;
    for(uint32_t i = 0; i < len; i++) zeros += bits[i] ? 0u : 1u;
    return (float)zeros / (float)len;
}

void bits_for_each_run(
    const uint8_t* bits,
    uint32_t len,
    BitsRunCallback cb,
    void* ctx) {
    if(len == 0) return;
    uint8_t cur = bits[0];
    uint16_t count = 1;
    for(uint32_t i = 1; i < len; i++) {
        if(bits[i] == cur && count < UINT16_MAX) {
            count++;
        } else {
            if(!cb(cur, count, ctx)) return;
            cur = bits[i];
            count = 1;
        }
    }
    cb(cur, count, ctx);
}

size_t bits_to_hex(const uint8_t* bits, uint16_t len, char* out, size_t cap) {
    if(cap == 0) return 0;
    out[0] = 0;
    if(len == 0) return 0;
    size_t pos = 0;
    uint16_t total_bits = (uint16_t)((len + 7u) / 8u * 8u);
    for(uint16_t i = 0; i < total_bits; i += 8) {
        uint8_t byte = 0;
        for(uint8_t j = 0; j < 8; j++) {
            uint8_t bit = (uint16_t)(i + j) < len ? bits[i + j] : 0;
            byte = (uint8_t)((byte << 1) | (bit & 1u));
        }
        const char* sep = (i == 0) ? "" : " ";
        int n = snprintf(out + pos, cap - pos, "%s%02X", sep, byte);
        if(n < 0 || (size_t)n >= cap - pos) {
            out[cap - 1] = 0;
            return cap - 1;
        }
        pos += (size_t)n;
    }
    return pos;
}

PreambleInfo bits_detect_preamble(const uint8_t* bits, uint16_t len) {
    PreambleInfo info = {.found = false, .length = 0, .position = 0};
    if(len == 0) return info;

    uint16_t best_start = 0;
    uint16_t best_len = 0;
    uint16_t cur_start = 0;
    uint16_t cur_len = 1;

    for(uint16_t i = 1; i < len; i++) {
        if(bits[i] != bits[i - 1]) {
            cur_len++;
        } else {
            if(cur_len > best_len) {
                best_len = cur_len;
                best_start = cur_start;
            }
            cur_start = i;
            cur_len = 1;
        }
    }
    if(cur_len > best_len) {
        best_len = cur_len;
        best_start = cur_start;
    }
    if(best_len >= 8) {
        info.found = true;
        info.length = best_len;
        info.position = best_start;
    }
    return info;
}

bool bits_segment_similarity(const SubFile* sub, float* out_value) {
    if(sub->segment_count < 2) return false;
    const uint8_t* ref = sub->segment_bits[0];
    uint16_t ref_len = sub->segment_bit_lens[0];

    float sum = 0.0f;
    uint16_t comparisons = 0;
    for(uint16_t s = 1; s < sub->segment_count; s++) {
        const uint8_t* other = sub->segment_bits[s];
        uint16_t other_len = sub->segment_bit_lens[s];
        uint16_t min_len = ref_len < other_len ? ref_len : other_len;
        if(min_len == 0) continue;

        uint16_t mismatches = 0;
        for(uint16_t i = 0; i < min_len; i++) {
            if(ref[i] != other[i]) mismatches++;
        }
        sum += 1.0f - (float)mismatches / (float)min_len;
        comparisons++;
    }
    if(comparisons == 0) return false;
    *out_value = sum / (float)comparisons;
    return true;
}

uint16_t bits_find_repeating_subpattern_inner(
    const SubFile* sub,
    uint16_t* out_reps) {
    *out_reps = 0;

    /* Build a small temporary buffer of all inner bits concatenated. We cap
     * total inner bits to BITRAW_MAX_TOTAL_BITS at parse time, so this fits. */
    static uint8_t scratch[BITRAW_MAX_TOTAL_BITS];
    uint32_t n = 0;
    for(uint16_t s = 0; s < sub->segment_count; s++) {
        uint16_t start = sub->inner_start[s];
        uint16_t inner_len = sub->inner_len[s];
        if(inner_len == 0) continue;
        if(n + inner_len > sizeof(scratch)) inner_len = (uint16_t)(sizeof(scratch) - n);
        memcpy(scratch + n, sub->segment_bits[s] + start, inner_len);
        n += inner_len;
        if(n >= sizeof(scratch)) break;
    }

    const uint16_t min_len = 8;
    uint16_t max_len = 128;
    if(n < (uint32_t)min_len * 2u) return 0;
    uint16_t cap = (uint16_t)(n / 2u);
    if(max_len > cap) max_len = cap;

    for(uint16_t period = min_len; period <= max_len; period++) {
        uint16_t count = 1;
        uint32_t pos = period;
        while(pos + period <= n) {
            if(memcmp(scratch, scratch + pos, period) == 0) {
                count++;
                pos += period;
            } else {
                break;
            }
        }
        if(count >= 2 && (uint32_t)count * period * 2u >= n) {
            *out_reps = count;
            return period;
        }
    }
    return 0;
}
