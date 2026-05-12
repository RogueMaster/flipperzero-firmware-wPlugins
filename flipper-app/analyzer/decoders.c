#include "decoders.h"
#include "bits.h"
#include <string.h>

#define HIST_SIZE 256

typedef struct {
    uint16_t one_hist[HIST_SIZE];
    uint16_t zero_hist[HIST_SIZE];
    uint32_t one_total;
    uint32_t zero_total;
    /* Tracked separately to detect when a run was clipped from histogram */
    uint8_t prev_value;
    uint16_t prev_run;
    bool have_prev;
} RunAgg;

static void run_agg_push(RunAgg* a, uint8_t v, uint16_t length) {
    if(v) {
        if(length < HIST_SIZE) a->one_hist[length]++;
        a->one_total++;
    } else {
        if(length < HIST_SIZE) a->zero_hist[length]++;
        a->zero_total++;
    }
}

/* Walk the inner bits of every segment and emit virtual runs as if all inner
 * bits were concatenated into one stream. (Matches analyze.py:523.) */
static void aggregate_inner_runs(const SubFile* sub, RunAgg* a) {
    memset(a, 0, sizeof(*a));
    for(uint16_t s = 0; s < sub->segment_count; s++) {
        uint16_t inner_len = sub->inner_len[s];
        if(inner_len == 0) continue;
        const uint8_t* p = sub->segment_bits[s] + sub->inner_start[s];

        for(uint16_t i = 0; i < inner_len; i++) {
            uint8_t v = p[i];
            if(!a->have_prev) {
                a->prev_value = v;
                a->prev_run = 1;
                a->have_prev = true;
            } else if(v == a->prev_value) {
                if(a->prev_run < UINT16_MAX) a->prev_run++;
            } else {
                run_agg_push(a, a->prev_value, a->prev_run);
                a->prev_value = v;
                a->prev_run = 1;
            }
        }
    }
    if(a->have_prev) run_agg_push(a, a->prev_value, a->prev_run);
}

void decoders_detect_pwm(const SubFile* sub, PWMParams* out) {
    memset(out, 0, sizeof(*out));

    /* RunAgg is ~1 KB; keep off stack. Single-threaded use. */
    static RunAgg agg;
    aggregate_inner_runs(sub, &agg);

    if(agg.one_total < 4 || agg.zero_total < 4) return;

    /* Dominant 1-run */
    uint16_t dom_pulse = 0;
    uint16_t dom_pulse_count = 0;
    for(uint16_t i = 1; i < HIST_SIZE; i++) {
        if(agg.one_hist[i] > dom_pulse_count) {
            dom_pulse_count = agg.one_hist[i];
            dom_pulse = i;
        }
    }
    if(dom_pulse_count == 0) return;
    float pulse_consistency = (float)dom_pulse_count / (float)agg.one_total;
    if(pulse_consistency < 0.60f) return;

    /* Top two 0-runs */
    uint16_t g1_len = 0, g1_count = 0;
    uint16_t g2_len = 0, g2_count = 0;
    for(uint16_t i = 1; i < HIST_SIZE; i++) {
        uint16_t c = agg.zero_hist[i];
        if(c == 0) continue;
        if(c > g1_count) {
            g2_len = g1_len;
            g2_count = g1_count;
            g1_len = i;
            g1_count = c;
        } else if(c > g2_count) {
            g2_len = i;
            g2_count = c;
        }
    }
    if(g2_count == 0) return;

    float combined = (float)(g1_count + g2_count) / (float)agg.zero_total;
    if(combined < 0.65f) return;

    uint16_t lo = g1_len < g2_len ? g1_len : g2_len;
    uint16_t hi = g1_len < g2_len ? g2_len : g1_len;
    if(lo == 0) lo = 1;
    float ratio = (float)hi / (float)lo;
    if(ratio < 1.5f || ratio > 8.0f) return;

    out->found = true;
    out->pulse_width = dom_pulse;
    out->short_gap = lo;
    out->long_gap = hi;
    out->consistency = pulse_consistency;
}

uint16_t decoders_decode_pwm(
    const uint8_t* bits,
    uint16_t len,
    const PWMParams* pwm,
    uint8_t* out,
    uint16_t out_cap) {
    if(!pwm || !pwm->found || len == 0) return 0;

    /* RLE inline. */
    uint16_t out_n = 0;
    uint16_t i = 0;

    /* Iterate consecutive runs as (val, length) pairs. */
    while(i < len) {
        uint8_t v1 = bits[i];
        uint16_t l1 = 1;
        while((i + l1) < len && bits[i + l1] == v1) l1++;
        uint16_t i_next = (uint16_t)(i + l1);

        if(i_next >= len) break;
        uint8_t v2 = bits[i_next];
        uint16_t l2 = 1;
        while((i_next + l2) < len && bits[i_next + l2] == v2) l2++;

        if(v1 == 1 && v2 == 0) {
            float l2f = (float)l2;
            if(l2f >= (float)pwm->long_gap * 0.65f) {
                if(out_n < out_cap) out[out_n++] = 0;
            } else if(l2f <= (float)pwm->short_gap * 1.5f) {
                if(out_n < out_cap) out[out_n++] = 1;
            }
            /* else: ambiguous, skip */
            i = (uint16_t)(i_next + l2);
        } else {
            i = i_next;
        }
    }
    return out_n;
}

static uint16_t try_manchester_convention(
    const uint8_t* bits,
    uint16_t len,
    bool hi_is_one,
    uint8_t* out,
    uint16_t out_cap,
    float* out_error_rate) {
    uint16_t pairs = (uint16_t)(len / 2u);
    uint16_t out_n = 0;
    uint16_t errors = 0;
    for(uint16_t i = 0; i + 1 < len; i += 2) {
        uint8_t a = bits[i];
        uint8_t b = bits[i + 1];
        if(a == 1 && b == 0) {
            if(out_n < out_cap) out[out_n++] = (uint8_t)(hi_is_one ? 1 : 0);
        } else if(a == 0 && b == 1) {
            if(out_n < out_cap) out[out_n++] = (uint8_t)(hi_is_one ? 0 : 1);
        } else {
            errors++;
        }
    }
    *out_error_rate = pairs ? (float)errors / (float)pairs : 0.0f;
    return out_n;
}

uint16_t decoders_decode_manchester(
    const uint8_t* bits,
    uint16_t len,
    uint8_t* out,
    uint16_t out_cap,
    ManchesterConvention* out_conv,
    float* out_error_rate) {
    if(len < 2) {
        *out_conv = ManchesterGEThomas;
        *out_error_rate = 0.0f;
        return 0;
    }

    /* Try both into temporary buffers, then commit the better one. */
    static uint8_t scratch_a[BITRAW_MAX_MANCH_BITS];
    static uint8_t scratch_b[BITRAW_MAX_MANCH_BITS];
    float err_a = 0.0f, err_b = 0.0f;
    uint16_t na = try_manchester_convention(
        bits, len, true, scratch_a, BITRAW_MAX_MANCH_BITS, &err_a);
    uint16_t nb = try_manchester_convention(
        bits, len, false, scratch_b, BITRAW_MAX_MANCH_BITS, &err_b);

    bool pick_a;
    if(err_a < err_b) {
        pick_a = true;
    } else if(err_b < err_a) {
        pick_a = false;
    } else {
        /* Tied: pick by first valid pair. */
        pick_a = (bits[0] == 1 && bits[1] == 0);
    }

    if(pick_a) {
        uint16_t n = na > out_cap ? out_cap : na;
        memcpy(out, scratch_a, n);
        *out_conv = ManchesterGEThomas;
        *out_error_rate = err_a;
        return n;
    } else {
        uint16_t n = nb > out_cap ? out_cap : nb;
        memcpy(out, scratch_b, n);
        *out_conv = ManchesterIEEE8023;
        *out_error_rate = err_b;
        return n;
    }
}

void decoders_detect_rolling_code(
    const SubFile* sub,
    const PWMParams* pwm,
    bool* out_rolling,
    bool* out_fixed,
    uint16_t* out_diff_positions,
    uint16_t diff_cap,
    uint16_t* out_diff_count,
    bool* out_diff_truncated) {
    *out_rolling = false;
    *out_fixed = false;
    *out_diff_count = 0;
    *out_diff_truncated = false;

    if(!pwm || !pwm->found || sub->segment_count < 2) return;

    static uint8_t decoded[BITRAW_MAX_SEGMENTS][BITRAW_MAX_DECODED_BITS];
    uint16_t lens[BITRAW_MAX_SEGMENTS] = {0};

    uint16_t min_len = UINT16_MAX;
    uint16_t max_len = 0;

    for(uint16_t s = 0; s < sub->segment_count; s++) {
        const uint8_t* inner = sub->segment_bits[s] + sub->inner_start[s];
        uint16_t inner_len = sub->inner_len[s];
        lens[s] = decoders_decode_pwm(
            inner, inner_len, pwm, decoded[s], BITRAW_MAX_DECODED_BITS);
        if(lens[s] < min_len) min_len = lens[s];
        if(lens[s] > max_len) max_len = lens[s];
    }
    if(min_len == 0) return;

    bool length_mismatch = (min_len != max_len);

    uint16_t found = 0;
    for(uint16_t pos = 0; pos < min_len; pos++) {
        uint8_t first = decoded[0][pos];
        bool diff = false;
        for(uint16_t s = 1; s < sub->segment_count; s++) {
            if(decoded[s][pos] != first) {
                diff = true;
                break;
            }
        }
        if(diff) {
            if(found < diff_cap) {
                out_diff_positions[found] = pos;
            } else {
                *out_diff_truncated = true;
            }
            found++;
        }
    }

    *out_diff_count = found > diff_cap ? diff_cap : found;
    (void)length_mismatch;
    if(found > 0) {
        *out_rolling = true;
    } else {
        *out_fixed = true;
    }
}
