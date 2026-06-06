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

/* Differential Manchester: each pair (a,b) must have a != b; the data bit is
 * encoded by whether a transition occurs between consecutive pairs. */
static uint16_t try_manchester_diff(
    const uint8_t* bits,
    uint16_t len,
    bool transition_is_one,
    uint8_t* out,
    uint16_t out_cap,
    float* out_error_rate) {
    uint16_t pairs = (uint16_t)(len / 2u);
    uint16_t out_n = 0;
    uint16_t errors = 0;
    bool have_prev = false;
    uint8_t prev_end = 0;
    for(uint16_t i = 0; i + 1 < len; i += 2) {
        uint8_t a = bits[i];
        uint8_t b = bits[i + 1];
        if(a == b) {
            errors++;
            prev_end = b;
            have_prev = true;
            continue;
        }
        if(have_prev) {
            bool transition = (a != prev_end);
            uint8_t bit_val =
                (uint8_t)((transition == transition_is_one) ? 1u : 0u);
            if(out_n < out_cap) out[out_n++] = bit_val;
        }
        prev_end = b;
        have_prev = true;
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

    /* Try all four conventions into temporary buffers, then commit the
     * best (standard preferred on tie). */
    static uint8_t scratch_a[SUBHOUND_MAX_MANCH_BITS];
    static uint8_t scratch_b[SUBHOUND_MAX_MANCH_BITS];
    static uint8_t scratch_c[SUBHOUND_MAX_MANCH_BITS];
    static uint8_t scratch_d[SUBHOUND_MAX_MANCH_BITS];
    float err_a = 0.0f, err_b = 0.0f, err_c = 0.0f, err_d = 0.0f;
    uint16_t na = try_manchester_convention(
        bits, len, true, scratch_a, SUBHOUND_MAX_MANCH_BITS, &err_a);
    uint16_t nb = try_manchester_convention(
        bits, len, false, scratch_b, SUBHOUND_MAX_MANCH_BITS, &err_b);
    uint16_t nc = try_manchester_diff(
        bits, len, true, scratch_c, SUBHOUND_MAX_MANCH_BITS, &err_c);
    uint16_t nd = try_manchester_diff(
        bits, len, false, scratch_d, SUBHOUND_MAX_MANCH_BITS, &err_d);

    /* Standard tie-break (matches analyze.py behaviour). */
    bool pick_a_over_b;
    if(err_a < err_b) {
        pick_a_over_b = true;
    } else if(err_b < err_a) {
        pick_a_over_b = false;
    } else {
        pick_a_over_b = (bits[0] == 1 && bits[1] == 0);
    }
    float std_err = pick_a_over_b ? err_a : err_b;
    uint16_t std_n = pick_a_over_b ? na : nb;
    const uint8_t* std_buf = pick_a_over_b ? scratch_a : scratch_b;
    ManchesterConvention std_conv =
        pick_a_over_b ? ManchesterGEThomas : ManchesterIEEE8023;

    /* Pick lower-error diff convention. */
    float diff_err;
    uint16_t diff_n;
    const uint8_t* diff_buf;
    ManchesterConvention diff_conv;
    if(err_d < err_c) {
        diff_err = err_d;
        diff_n = nd;
        diff_buf = scratch_d;
        diff_conv = ManchesterDiffTransitionIsZero;
    } else {
        diff_err = err_c;
        diff_n = nc;
        diff_buf = scratch_c;
        diff_conv = ManchesterDiffTransitionIsOne;
    }

    /* Prefer standard unless differential is strictly lower-error. */
    bool use_diff = (diff_err < std_err);
    uint16_t n = use_diff ? diff_n : std_n;
    if(n > out_cap) n = out_cap;
    memcpy(out, use_diff ? diff_buf : std_buf, n);
    *out_conv = use_diff ? diff_conv : std_conv;
    *out_error_rate = use_diff ? diff_err : std_err;
    return n;
}

bool decoders_detect_pwm3(const SubFile* sub, uint16_t* out_symbol_count) {
    if(out_symbol_count) *out_symbol_count = 0;

    static RunAgg agg;
    aggregate_inner_runs(sub, &agg);
    if(agg.one_total < 6 || agg.zero_total < 6) return false;

    /* Dominant 1-run must carry >=60% of one-runs. */
    uint16_t dom_pulse_count = 0;
    for(uint16_t i = 1; i < HIST_SIZE; i++) {
        if(agg.one_hist[i] > dom_pulse_count) {
            dom_pulse_count = agg.one_hist[i];
        }
    }
    if(dom_pulse_count == 0) return false;
    if((float)dom_pulse_count / (float)agg.one_total < 0.60f) return false;

    /* Top three zero-runs. */
    uint16_t g1l = 0, g1c = 0, g2l = 0, g2c = 0, g3l = 0, g3c = 0;
    for(uint16_t i = 1; i < HIST_SIZE; i++) {
        uint16_t c = agg.zero_hist[i];
        if(c == 0) continue;
        if(c > g1c) {
            g3l = g2l; g3c = g2c;
            g2l = g1l; g2c = g1c;
            g1l = i;  g1c = c;
        } else if(c > g2c) {
            g3l = g2l; g3c = g2c;
            g2l = i;  g2c = c;
        } else if(c > g3c) {
            g3l = i; g3c = c;
        }
    }
    if(g3c == 0) return false;
    if((float)g3c / (float)agg.zero_total < 0.10f) return false;

    /* Sort three bucket lengths ascending. */
    uint16_t lens[3] = {g1l, g2l, g3l};
    for(int i = 0; i < 2; i++) {
        for(int j = i + 1; j < 3; j++) {
            if(lens[j] < lens[i]) {
                uint16_t t = lens[i]; lens[i] = lens[j]; lens[j] = t;
            }
        }
    }
    uint16_t short_g = lens[0], mid_g = lens[1], long_g = lens[2];
    if(short_g == 0) short_g = 1;
    if((float)mid_g / (float)short_g < 1.4f) return false;
    if((float)long_g / (float)mid_g < 1.4f) return false;

    /* Count tri-state symbols across runs (one-run + matching zero-run). */
    uint16_t tol_s = short_g / 4u; if(tol_s < 1) tol_s = 1;
    uint16_t tol_m = mid_g / 4u;   if(tol_m < 1) tol_m = 1;
    uint16_t tol_l = long_g / 4u;  if(tol_l < 1) tol_l = 1;
    uint16_t symbols = 0;

    for(uint16_t s = 0; s < sub->segment_count; s++) {
        uint16_t inner_len = sub->inner_len[s];
        if(inner_len == 0) continue;
        const uint8_t* p = sub->segment_bits[s] + sub->inner_start[s];
        uint16_t i = 0;
        while(i < inner_len) {
            uint8_t v1 = p[i];
            uint16_t l1 = 1;
            while((i + l1) < inner_len && p[i + l1] == v1) l1++;
            uint16_t inext = (uint16_t)(i + l1);
            if(inext >= inner_len) break;
            uint8_t v2 = p[inext];
            uint16_t l2 = 1;
            while((inext + l2) < inner_len && p[inext + l2] == v2) l2++;
            if(v1 == 1 && v2 == 0) {
                int diff_s = (int)l2 - (int)short_g; if(diff_s < 0) diff_s = -diff_s;
                int diff_m = (int)l2 - (int)mid_g;   if(diff_m < 0) diff_m = -diff_m;
                int diff_l = (int)l2 - (int)long_g;  if(diff_l < 0) diff_l = -diff_l;
                if((uint16_t)diff_s <= tol_s || (uint16_t)diff_m <= tol_m ||
                   (uint16_t)diff_l <= tol_l) {
                    symbols++;
                }
                i = (uint16_t)(inext + l2);
            } else {
                i = inext;
            }
        }
    }

    if(out_symbol_count) *out_symbol_count = symbols;
    return true;
}

/* CRC helpers --------------------------------------------------------------*/

static uint8_t crc8(const uint8_t* data, size_t n, uint8_t poly) {
    uint8_t crc = 0;
    for(size_t i = 0; i < n; i++) {
        crc = (uint8_t)(crc ^ data[i]);
        for(int b = 0; b < 8; b++) {
            crc = (crc & 0x80u) ? (uint8_t)((crc << 1) ^ poly) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

static uint16_t crc16_ccitt(const uint8_t* data, size_t n) {
    uint16_t crc = 0xFFFFu;
    for(size_t i = 0; i < n; i++) {
        crc = (uint16_t)(crc ^ ((uint16_t)data[i] << 8));
        for(int b = 0; b < 8; b++) {
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

bool decoders_detect_crc(const uint8_t* decoded_bits, uint16_t len, uint8_t* out_kind) {
    if(out_kind) *out_kind = 0;
    uint16_t nbytes = (uint16_t)(len / 8u);
    if(nbytes < 2) return false;

    static uint8_t bytes[SUBHOUND_MAX_DECODED_BITS / 8 + 1];
    for(uint16_t i = 0; i < nbytes; i++) {
        uint8_t v = 0;
        for(int j = 0; j < 8; j++) {
            v = (uint8_t)((v << 1) | (decoded_bits[i * 8 + j] & 1u));
        }
        bytes[i] = v;
    }

    /* CRC-8 with poly 0x07 or 0x31 over all-but-last byte. */
    if(nbytes >= 2) {
        if(crc8(bytes, nbytes - 1, 0x07) == bytes[nbytes - 1]) {
            if(out_kind) *out_kind = 1;
            return true;
        }
        if(crc8(bytes, nbytes - 1, 0x31) == bytes[nbytes - 1]) {
            if(out_kind) *out_kind = 1;
            return true;
        }
    }
    if(nbytes >= 3) {
        uint16_t expected = crc16_ccitt(bytes, nbytes - 2);
        uint16_t trailer = (uint16_t)((bytes[nbytes - 2] << 8) | bytes[nbytes - 1]);
        if(expected == trailer) {
            if(out_kind) *out_kind = 2;
            return true;
        }
    }
    return false;
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

    static uint8_t decoded[SUBHOUND_MAX_SEGMENTS][SUBHOUND_MAX_DECODED_BITS];
    uint16_t lens[SUBHOUND_MAX_SEGMENTS] = {0};

    uint16_t min_len = UINT16_MAX;
    uint16_t max_len = 0;

    for(uint16_t s = 0; s < sub->segment_count; s++) {
        const uint8_t* inner = sub->segment_bits[s] + sub->inner_start[s];
        uint16_t inner_len = sub->inner_len[s];
        lens[s] = decoders_decode_pwm(
            inner, inner_len, pwm, decoded[s], SUBHOUND_MAX_DECODED_BITS);
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
