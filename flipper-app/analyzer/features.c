#include "features.h"
#include "bits.h"
#include "decoders.h"
#include <math.h>
#include <string.h>

#define HIST_SIZE 256

static void compute_dominant_runs(const SubFile* sub, FeatureVector* fv) {
    static uint16_t one_hist[HIST_SIZE];
    static uint16_t zero_hist[HIST_SIZE];
    static uint32_t unique_lengths_seen[HIST_SIZE];
    memset(one_hist, 0, sizeof(one_hist));
    memset(zero_hist, 0, sizeof(zero_hist));
    memset(unique_lengths_seen, 0, sizeof(unique_lengths_seen));

    bool have_prev = false;
    uint8_t prev_v = 0;
    uint16_t prev_run = 0;
    uint32_t total_one_runs = 0;
    uint32_t total_zero_runs = 0;

#define PUSH_RUN(v, len)                                            \
    do {                                                            \
        if(v) {                                                     \
            if((len) < HIST_SIZE) one_hist[(len)]++;                \
            total_one_runs++;                                       \
        } else {                                                    \
            if((len) < HIST_SIZE) zero_hist[(len)]++;               \
            total_zero_runs++;                                      \
        }                                                           \
        if((len) < HIST_SIZE) unique_lengths_seen[(len)]++;         \
    } while(0)

    for(uint16_t s = 0; s < sub->segment_count; s++) {
        uint16_t inner_len = sub->inner_len[s];
        if(inner_len == 0) continue;
        const uint8_t* p = sub->segment_bits[s] + sub->inner_start[s];
        for(uint16_t i = 0; i < inner_len; i++) {
            uint8_t v = p[i];
            if(!have_prev) {
                prev_v = v;
                prev_run = 1;
                have_prev = true;
            } else if(v == prev_v) {
                if(prev_run < UINT16_MAX) prev_run++;
            } else {
                PUSH_RUN(prev_v, prev_run);
                prev_v = v;
                prev_run = 1;
            }
        }
    }
    if(have_prev) PUSH_RUN(prev_v, prev_run);
#undef PUSH_RUN

    uint16_t dom1 = 0, dom1_count = 0;
    for(uint16_t i = 1; i < HIST_SIZE; i++) {
        if(one_hist[i] > dom1_count) {
            dom1_count = one_hist[i];
            dom1 = i;
        }
    }
    uint16_t dom0 = 0, dom0_count = 0;
    for(uint16_t i = 1; i < HIST_SIZE; i++) {
        if(zero_hist[i] > dom0_count) {
            dom0_count = zero_hist[i];
            dom0 = i;
        }
    }

    uint32_t unique = 0;
    for(uint16_t i = 0; i < HIST_SIZE; i++) {
        if(unique_lengths_seen[i] > 0) unique++;
    }
    uint32_t total_runs = total_one_runs + total_zero_runs;

    fv->dominant_1run = dom1;
    fv->dominant_0run = dom0;
    fv->run_variety =
        total_runs ? (float)unique / (float)total_runs : 0.0f;
}

void features_extract(SubFile* sub, FeatureVector* fv) {
    memset(fv, 0, sizeof(*fv));

    /* Per-segment: copy raw size and compute strip_padding range. */
    fv->seg_count = sub->segment_count;
    uint32_t total_bits = 0;
    uint32_t total_inner_bits = 0;

    for(uint16_t s = 0; s < sub->segment_count; s++) {
        uint16_t raw_len = sub->segment_bit_lens[s];
        fv->seg_sizes[s] = raw_len;
        total_bits += raw_len;

        uint16_t istart = 0, ilen = 0;
        bits_strip_padding(sub->segment_bits[s], raw_len, &istart, &ilen);
        sub->inner_start[s] = istart;
        sub->inner_len[s] = ilen;
        fv->inner_sizes[s] = ilen;
        total_inner_bits += ilen;
    }

    fv->total_bits = total_bits;
    fv->total_inner_bits = total_inner_bits;
    fv->mean_inner_size = sub->segment_count
                              ? (float)total_inner_bits / (float)sub->segment_count
                              : 0.0f;

    /* Zero ratio across raw bits (all segments). */
    {
        uint32_t zeros = 0;
        for(uint16_t s = 0; s < sub->segment_count; s++) {
            uint16_t raw_len = sub->segment_bit_lens[s];
            const uint8_t* p = sub->segment_bits[s];
            for(uint16_t i = 0; i < raw_len; i++) {
                if(p[i] == 0) zeros++;
            }
        }
        fv->zero_ratio = total_bits ? (float)zeros / (float)total_bits : 1.0f;
    }

    /* Entropy across all inner bits. */
    {
        uint32_t ones = 0;
        for(uint16_t s = 0; s < sub->segment_count; s++) {
            uint16_t inner_len = sub->inner_len[s];
            if(inner_len == 0) continue;
            const uint8_t* p = sub->segment_bits[s] + sub->inner_start[s];
            for(uint16_t i = 0; i < inner_len; i++) ones += p[i] ? 1u : 0u;
        }
        fv->inner_set_bits = ones;
        uint32_t zeros = total_inner_bits - ones;
        float entropy = 0.0f;
        if(ones > 0) {
            float pp = (float)ones / (float)total_inner_bits;
            entropy -= pp * log2f(pp);
        }
        if(zeros > 0) {
            float pp = (float)zeros / (float)total_inner_bits;
            entropy -= pp * log2f(pp);
        }
        fv->entropy = entropy;
    }

    compute_dominant_runs(sub, fv);

    decoders_detect_pwm(sub, &fv->pwm_params);

    /* PWM-decode the FIRST segment's inner bits. */
    if(sub->segment_count > 0 && fv->pwm_params.found) {
        const uint8_t* p = sub->segment_bits[0] + sub->inner_start[0];
        uint16_t inner_len = sub->inner_len[0];
        fv->pwm_decoded_count = decoders_decode_pwm(
            p, inner_len, &fv->pwm_params, fv->pwm_decoded_bits, BITRAW_MAX_DECODED_BITS);
    }

    /* Preamble on first segment's inner bits. */
    if(sub->segment_count > 0) {
        const uint8_t* p = sub->segment_bits[0] + sub->inner_start[0];
        fv->preamble = bits_detect_preamble(p, sub->inner_len[0]);
    }

    fv->has_seg_similarity = bits_segment_similarity(sub, &fv->seg_similarity);

    /* Repeating subpattern across virtual concatenation of inner bits. */
    fv->repeating_subpattern_period =
        bits_find_repeating_subpattern_inner(sub, &fv->repeating_subpattern_reps);

    /* Manchester decode on first segment inner. */
    if(sub->segment_count > 0 && sub->inner_len[0] > 0) {
        const uint8_t* p = sub->segment_bits[0] + sub->inner_start[0];
        fv->manchester_decoded_count = decoders_decode_manchester(
            p,
            sub->inner_len[0],
            fv->manchester_decoded_bits,
            BITRAW_MAX_MANCH_BITS,
            &fv->manchester_convention,
            &fv->manchester_error_rate);
    } else {
        fv->manchester_convention = ManchesterGEThomas;
    }

    /* Rolling code: PWM-decode all segments and compare. */
    decoders_detect_rolling_code(
        sub,
        &fv->pwm_params,
        &fv->rolling_code,
        &fv->fixed_code,
        fv->diff_positions,
        BITRAW_MAX_DIFF_POSITIONS,
        &fv->diff_position_count,
        &fv->diff_positions_truncated);

    /* Scalar passthroughs. */
    fv->frequency = sub->frequency_hz;
    fv->te_us = (float)sub->te_us;
    fv->bitrate_bps = sub->te_us > 0 ? 1.0e6f / (float)sub->te_us : 0.0f;
    fv->lat = sub->lat;
    fv->lon = sub->lon;
    fv->has_gps = sub->has_gps;

    /* Signal quality composite. */
    {
        float components[4];
        float inner_ratio =
            fv->total_inner_bits == 0
                ? 0.0f
                : (float)fv->total_inner_bits / (float)(fv->total_bits ? fv->total_bits : 1u);
        float c0 = inner_ratio * 2.5f;
        if(c0 > 1.0f) c0 = 1.0f;
        components[0] = c0;
        components[1] = fv->pwm_params.found ? fv->pwm_params.consistency : 0.5f;
        components[2] = fv->has_seg_similarity ? fv->seg_similarity : 0.5f;
        if(fv->entropy >= 0.70f) {
            components[3] = 1.0f;
        } else if(fv->entropy >= 0.35f) {
            components[3] = fv->entropy / 0.70f;
        } else {
            components[3] = 0.15f;
        }
        float sum = 0.0f;
        for(int i = 0; i < 4; i++) sum += components[i];
        fv->signal_quality = sum / 4.0f;
        /* round to 3 decimals to match Python */
        fv->signal_quality = roundf(fv->signal_quality * 1000.0f) / 1000.0f;
    }
}
