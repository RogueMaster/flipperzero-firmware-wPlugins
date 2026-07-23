#include "autobaud.h"

#include <stm32wbxx.h>
#include <string.h>

/* Anything quicker than this is ringing or a probe glitch: even at 921600 a bit
 * lasts ~69 cycles, and our interrupt cannot resolve edges that tight anyway. */
#define AUTOBAUD_MIN_DELTA (32u)

/* ~11 ms. Longer than the longest run at our slowest rate (10 bits at 1200 baud
 * is ~533k cycles), so anything above this is the line sitting idle. */
#define AUTOBAUD_MAX_DELTA (700000u)

/* Longest run of identical bits that can live inside one frame. A low run is
 * bounded by the start bit and the stop bit - start plus eight zero data bits
 * is nine. High runs get one more, since the stop bit extends them. */
#define AUTOBAUD_K_MAX_LOW  (9u)
#define AUTOBAUD_K_MAX_HIGH (10u)

/* How far a segment may sit from a whole number of bits and still count.
 *
 * Measured against ONE bit time, never against the segment: the error comes
 * from interrupt latency on the segment's two edges, and that does not grow
 * with the number of bits in between. Scaling the tolerance with the segment
 * is what would let an absurdly fast rate swallow any signal by declaring
 * every pulse to be nine bits of its own. */
#define AUTOBAUD_TOLERANCE_NUM (25u)
#define AUTOBAUD_TOLERANCE_DEN (100u)

#define AUTOBAUD_REFINE_PASSES (4u)

struct Autobaud {
    const GpioPin* pin;
    FuriHalSerialHandle* serial; // held only to keep the console off our pin
    HermesPort port;
    bool running;

    /* written by the interrupt, read by the UI thread */
    volatile uint32_t count;
    uint32_t last_cycle;
    uint32_t raw[AUTOBAUD_MAX_EDGES]; // duration of each segment, in CPU cycles
    uint8_t level[AUTOBAUD_MAX_EDGES / 8]; // level *during* that segment, 1 = high

    uint32_t cycles_per_second;
};

/* ------------------------------------------------------------- capture ---- */

static void autobaud_dwt_enable(void) {
    /* The firmware already turns the cycle counter on for its own timers, but
     * claiming it here means the fit does not silently read a frozen zero if
     * that ever stops being true. */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void autobaud_isr(void* ctx) {
    Autobaud* ab = ctx;

    const uint32_t now = DWT->CYCCNT;
    const uint32_t delta = now - ab->last_cycle; // unsigned wrap is the right answer
    ab->last_cycle = now;

    const uint32_t idx = ab->count;
    if(idx >= AUTOBAUD_MAX_EDGES) return;

    ab->raw[idx] = delta;

    /* Reading the pin gives the level *after* this edge, so the segment that
     * just ended was sitting at the opposite one. */
    if(furi_hal_gpio_read(ab->pin)) {
        ab->level[idx >> 3] &= (uint8_t) ~(1u << (idx & 7u));
    } else {
        ab->level[idx >> 3] |= (uint8_t)(1u << (idx & 7u));
    }

    __DMB(); // publish raw[]/level[] before the count that advertises them
    ab->count = idx + 1;
}

Autobaud* autobaud_alloc(void) {
    Autobaud* ab = malloc(sizeof(Autobaud));
    memset(ab, 0, sizeof(Autobaud));
    ab->cycles_per_second = furi_hal_cortex_instructions_per_microsecond() * 1000000u;
    return ab;
}

void autobaud_free(Autobaud* ab) {
    furi_assert(ab);
    autobaud_stop(ab);
    free(ab);
}

bool autobaud_start(Autobaud* ab, HermesPort port) {
    furi_assert(ab);
    if(ab->running) return true;

    /* On a stock Flipper the USART is the log/CLI console. Acquiring it parks
     * that service so it cannot drive the pin out from under us mid-capture. */
    ab->serial = furi_hal_serial_control_acquire(hermes_port_serial_id(port));
    if(!ab->serial) return false;

    ab->port = port;
    ab->pin = hermes_port_rx_pin(port);
    ab->count = 0;
    memset(ab->level, 0, sizeof(ab->level));

    autobaud_dwt_enable();

    /* Pull-up so an unconnected probe reads idle-high instead of floating and
     * spraying phantom edges. */
    furi_hal_gpio_init(ab->pin, GpioModeInterruptRiseFall, GpioPullUp, GpioSpeedVeryHigh);
    furi_hal_gpio_add_int_callback(ab->pin, autobaud_isr, ab);

    ab->last_cycle = DWT->CYCCNT;
    ab->running = true;
    return true;
}

void autobaud_stop(Autobaud* ab) {
    furi_assert(ab);
    if(!ab->running) return;

    furi_hal_gpio_remove_int_callback(ab->pin);
    furi_hal_gpio_init(ab->pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

    furi_hal_serial_control_release(ab->serial);
    ab->serial = NULL;
    ab->running = false;
}

uint32_t autobaud_edge_count(const Autobaud* ab) {
    furi_assert(ab);
    return ab->count;
}

bool autobaud_is_full(const Autobaud* ab) {
    furi_assert(ab);
    return ab->count >= AUTOBAUD_MAX_EDGES;
}

bool autobaud_line_idle_high(const Autobaud* ab) {
    furi_assert(ab);
    if(!ab->running) return true;
    return furi_hal_gpio_read(ab->pin);
}

static bool autobaud_level_at(const Autobaud* ab, uint32_t idx) {
    return (ab->level[idx >> 3] >> (idx & 7u)) & 1u;
}

size_t autobaud_trace(const Autobaud* ab, AutobaudSegment* out, size_t max) {
    furi_assert(ab);
    furi_assert(out);

    const uint32_t count = ab->count;
    if(count < 2 || max == 0) return 0;

    /* Segment 0 is the gap between arming and the first edge - it says nothing
     * about the signal, so the trace starts at 1. */
    const uint32_t available = count - 1u;
    const uint32_t take = (available > max) ? (uint32_t)max : available;
    const uint32_t begin = count - take;

    for(uint32_t i = 0; i < take; i++) {
        out[i].delta = ab->raw[begin + i];
        out[i].high = autobaud_level_at(ab, begin + i);
    }
    return take;
}

/* ----------------------------------------------------------------- fit ---- */

/** Does `bit_time` explain this segment, and as how many bits?
 *
 * Every stretch of a UART line is a whole number of bit times. A candidate that
 * turns the measurements into near-integers is a candidate that fits.
 */
static bool autobaud_explains(const AutobaudSegment* seg, uint32_t bit_time, uint32_t* k_out) {
    const uint32_t k = (seg->delta + bit_time / 2) / bit_time; // nearest whole bit count
    const uint32_t k_max = seg->high ? AUTOBAUD_K_MAX_HIGH : AUTOBAUD_K_MAX_LOW;
    if(k < 1 || k > k_max) return false;

    const uint32_t ideal = k * bit_time;
    const uint32_t resid = (seg->delta > ideal) ? (seg->delta - ideal) : (ideal - seg->delta);
    if(resid * AUTOBAUD_TOLERANCE_DEN > bit_time * AUTOBAUD_TOLERANCE_NUM) return false;

    if(k_out) *k_out = k;
    return true;
}

/** How much of the capture a candidate bit time accounts for. */
static uint32_t autobaud_inliers(uint32_t bit_time, const AutobaudSegment* seg, size_t n) {
    if(bit_time == 0) return 0;

    uint32_t fitted = 0;
    for(size_t i = 0; i < n; i++) {
        if(autobaud_explains(&seg[i], bit_time, NULL)) fitted++;
    }
    return fitted;
}

/** One least-squares-ish pass: every segment is k bit times, so summing the
 * durations and dividing by the summed k lets long runs sharpen the estimate. */
static uint32_t autobaud_refine(uint32_t bit_time, const AutobaudSegment* seg, size_t n) {
    uint64_t sum_delta = 0;
    uint64_t sum_k = 0;

    for(size_t i = 0; i < n; i++) {
        uint32_t k;
        if(!autobaud_explains(&seg[i], bit_time, &k)) continue;
        sum_delta += seg[i].delta;
        sum_k += k;
    }

    if(sum_k == 0) return 0;
    return (uint32_t)(sum_delta / sum_k);
}

static uint8_t
    autobaud_confidence(uint32_t raw, uint32_t std, uint8_t fit_percent, uint32_t edges) {
    const uint32_t diff = (raw > std) ? (raw - std) : (std - raw);
    const uint32_t err_tenths = (uint32_t)(((uint64_t)diff * 1000u) / std); // 0.1% units

    /* Dead on is 100, 20% out is worthless. */
    const uint32_t closeness = (err_tenths >= 200u) ? 0u : (100u - (err_tenths / 2u));
    uint32_t conf = (fit_percent * closeness) / 100u;

    /* A tidy fit over a handful of edges is still a guess. */
    if(edges < 64u) conf = (conf * edges) / 64u;

    return (uint8_t)((conf > 100u) ? 100u : conf);
}

/** Rank the standard rates, best first.
 *
 * A rate scores on two counts: how much of the capture it explains, and how
 * close the refined measurement lands to it. Keeping the runner-ups matters -
 * a near-miss between neighbouring rates is exactly when the user wants the
 * second guess offered rather than being told the wrong answer with a
 * straight face.
 */
static void autobaud_rank_candidates(AutobaudResult* out, const uint32_t* inliers, size_t n) {
    uint8_t conf[HERMES_BAUD_COUNT];
    bool taken[HERMES_BAUD_COUNT];

    for(size_t i = 0; i < HERMES_BAUD_COUNT; i++) {
        const uint8_t inlier_pct = (uint8_t)((inliers[i] * 100u) / n);
        conf[i] = autobaud_confidence(
            out->baud_raw, hermes_baud_table[i].baud, inlier_pct, out->edges_used);
        taken[i] = false;
    }

    for(uint32_t slot = 0; slot < AUTOBAUD_CANDIDATES; slot++) {
        size_t best = HERMES_BAUD_COUNT;
        for(size_t i = 0; i < HERMES_BAUD_COUNT; i++) {
            if(taken[i]) continue;
            if(conf[i] == 0) continue; // not a candidate, just a row in a table
            if(best == HERMES_BAUD_COUNT || conf[i] > conf[best]) best = i;
        }
        if(best == HERMES_BAUD_COUNT) break;
        taken[best] = true;

        const uint32_t std = hermes_baud_table[best].baud;
        AutobaudCandidate* c = &out->candidates[out->candidate_count++];
        c->baud = std;
        c->confidence = conf[best];
        c->error_ppm =
            (int32_t)((((int64_t)out->baud_raw - (int64_t)std) * 1000000) / (int64_t)std);
    }
}

/** Pick the standard rate that best explains the capture.
 *
 * Testing each rate directly beats guessing a bit time from the shortest pulse
 * and refining: one noisy pulse can drag that seed, and everything downstream
 * follows it. Returns HERMES_BAUD_COUNT when nothing explains enough.
 *
 * Harmonics are the catch - half the true bit time also turns every measurement
 * into an integer, just twice as large a one. The cap on bits-per-run is what
 * separates them: at a harmonic, the longer runs need more bits than a frame
 * can hold, and those segments stop fitting. So among rates that score alike,
 * the slowest is the fundamental.
 */
static size_t autobaud_choose(
    const AutobaudSegment* seg,
    size_t n,
    uint32_t cycles_per_second,
    uint32_t* inliers_out) {
    uint32_t best = 0;

    for(size_t i = 0; i < HERMES_BAUD_COUNT; i++) {
        const uint32_t bit_time = cycles_per_second / hermes_baud_table[i].baud;
        inliers_out[i] = autobaud_inliers(bit_time, seg, n);
        if(inliers_out[i] > best) best = inliers_out[i];
    }

    /* Nothing accounts for even a quarter of the edges: this is not UART, or
     * it is faster than the interrupt can follow. */
    if(best * 4u < n) return HERMES_BAUD_COUNT;

    const uint32_t margin = best / 50u; // treat within 2% as a tie
    for(size_t i = 0; i < HERMES_BAUD_COUNT; i++) { // ascending: first tie is slowest
        if(inliers_out[i] + margin >= best) return i;
    }
    return HERMES_BAUD_COUNT;
}

void autobaud_analyze(const Autobaud* ab, AutobaudResult* out) {
    furi_assert(ab);
    furi_assert(out);
    memset(out, 0, sizeof(AutobaudResult));

    const uint32_t count = ab->count;
    if(count < 2) {
        out->verdict = AutobaudVerdictSilent;
        return;
    }

    AutobaudSegment* seg = malloc(sizeof(AutobaudSegment) * AUTOBAUD_MAX_EDGES);

    /* Collect the usable segments, skipping index 0 (arm-to-first-edge). */
    size_t n = 0;
    for(uint32_t i = 1; i < count; i++) {
        const uint32_t d = ab->raw[i];
        if(d < AUTOBAUD_MIN_DELTA) continue; // glitch
        if(d > AUTOBAUD_MAX_DELTA) continue; // idle gap, carries no bit timing
        seg[n].delta = d;
        seg[n].high = autobaud_level_at(ab, i);
        n++;
    }
    out->edges_used = n;

    if(n < 8) {
        out->verdict = (count < 8) ? AutobaudVerdictSilent : AutobaudVerdictTooFewEdges;
        free(seg);
        return;
    }

    uint32_t inliers[HERMES_BAUD_COUNT];
    const size_t chosen = autobaud_choose(seg, n, ab->cycles_per_second, inliers);

    if(chosen == HERMES_BAUD_COUNT) {
        out->verdict = AutobaudVerdictNoFit;
        free(seg);
        return;
    }

    /* Now that the rate is known, refine from it to read the line's *actual*
     * bit time - which is what tells an on-spec 115200 from a board whose
     * crystal is 3% out. */
    uint32_t bit_time = ab->cycles_per_second / hermes_baud_table[chosen].baud;
    const uint32_t seed = bit_time;

    for(uint32_t pass = 0; pass < AUTOBAUD_REFINE_PASSES; pass++) {
        const uint32_t next = autobaud_refine(bit_time, seg, n);
        if(next == 0) break;
        if(next == bit_time) break; // converged
        bit_time = next;
    }

    /* Refinement should nudge, not wander. If it drifted far enough to be
     * describing some other rate, trust the hypothesis that won instead. */
    const uint32_t drift = (bit_time > seed) ? (bit_time - seed) : (seed - bit_time);
    if(bit_time == 0 || drift * 5u > seed) bit_time = seed;

    out->verdict = AutobaudVerdictOk;
    out->bit_time_cycles = bit_time;
    out->baud_raw = ab->cycles_per_second / bit_time;
    out->fit_percent = (uint8_t)((inliers[chosen] * 100u) / n);
    autobaud_rank_candidates(out, inliers, n);

    free(seg);
}
