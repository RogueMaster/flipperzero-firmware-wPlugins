#include "ir_sense.h"

#include <furi_hal_infrared.h>
#include <furi_hal_adc.h>
#include <furi_hal_resources.h>
#include <furi_hal_gpio.h>
#include <string.h>

/* Update cadence. 100 ms per window gives the meter 10 updates a second: quick
 * enough to feel like it reacts to where you point it, slow enough to read. */
#define WINDOW_MS 100u

/* Probe sampling. We sample a dense burst rather than trickling reads across
 * the whole window, so that the samples are contiguous in time and ripple
 * frequency is actually recoverable. 128 samples at 250 us = a 32 ms burst,
 * which holds ~3 cycles of 100 Hz mains flicker — enough to tell mains from a
 * faster PWM. The thread then sleeps out the rest of the window. */
#define PROBE_BURST_SAMPLES 128u
#define PROBE_SAMPLE_US     250u
#define PROBE_BURST_MS      ((PROBE_BURST_SAMPLES * PROBE_SAMPLE_US) / 1000u) // 32

/* Ambient null: average this many windows at arm time to fix the baseline. */
#define PROBE_NULL_WINDOWS 8u

/* Ripple below this is indistinguishable from ADC noise, so we call it steady. */
#define PROBE_RIPPLE_FLOOR_MV 25u

/* Silence timeout handed to the IR HAL, in microseconds. */
#define ONBOARD_TIMEOUT_US 50000u

/* Full-scale and noise floor per sensitivity index (0 High, 1 Med, 2 Low). */
static const uint32_t onboard_full_scale_eps[3] = {400u, 1200u, 3000u}; // edges/sec
static const uint32_t onboard_floor_eps[3] = {20u, 60u, 150u};
static const uint32_t probe_full_scale_mv[3] = {150u, 500u, 1500u};
static const uint32_t probe_floor_mv[3] = {15u, 40u, 100u};

/* ---------------- probe pin table ---------------- */

#define MAX_PROBE_PINS 8u

static IrProbePin g_probe_pins[MAX_PROBE_PINS];
static uint8_t g_probe_pin_count = 0;
static bool g_probe_pins_built = false;

/* The SDK already knows which header pins reach an ADC channel, so read it out
 * of gpio_pins[] instead of hardcoding a table that could drift from the HAL. */
static void build_probe_pins(void) {
    if(g_probe_pins_built) return;
    for(size_t i = 0; i < gpio_pins_count && g_probe_pin_count < MAX_PROBE_PINS; i++) {
        if(gpio_pins[i].channel == FuriHalAdcChannelNone) continue;
        if(gpio_pins[i].debug) continue; // SWD pins: connecting here fights the debugger
        g_probe_pins[g_probe_pin_count].pin = gpio_pins[i].pin;
        g_probe_pins[g_probe_pin_count].name = gpio_pins[i].name;
        g_probe_pins[g_probe_pin_count].header_number = gpio_pins[i].number;
        g_probe_pins[g_probe_pin_count].channel = gpio_pins[i].channel;
        g_probe_pin_count++;
    }
    g_probe_pins_built = true;
}

const IrProbePin* ir_sense_probe_pins(void) {
    build_probe_pins();
    return g_probe_pins;
}

uint8_t ir_sense_probe_pin_count(void) {
    build_probe_pins();
    return g_probe_pin_count;
}

bool ir_sense_probe_detect(uint8_t pin_index) {
    build_probe_pins();
    if(pin_index >= g_probe_pin_count) return false;
    const GpioPin* pin = g_probe_pins[pin_index].pin;

    /* Pull-up divider test — see the header for why a low reading means loaded. */
    furi_hal_gpio_init(pin, GpioModeInput, GpioPullUp, GpioSpeedLow);
    furi_delay_ms(2);
    bool loaded = !furi_hal_gpio_read(pin);
    furi_hal_gpio_init(pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    return loaded;
}

const char* ir_sense_source_kind_str(IrSourceKind kind) {
    switch(kind) {
    case IrSourceSteady:
        return "STEADY";
    case IrSourceFlicker:
        return "FLICKER";
    case IrSourcePulsed:
        return "PULSED";
    default:
        return "--";
    }
}

/* ---------------- one-shot probe meter ---------------- */

struct IrProbeMeter {
    FuriHalAdcHandle* handle;
    FuriHalAdcChannel channel;
};

IrProbeMeter* ir_probe_meter_alloc(uint8_t pin_index) {
    build_probe_pins();
    if(pin_index >= g_probe_pin_count) pin_index = 0;

    IrProbeMeter* m = malloc(sizeof(IrProbeMeter));
    memset(m, 0, sizeof(IrProbeMeter));
    m->channel = g_probe_pins[pin_index].channel;

    furi_hal_gpio_init(g_probe_pins[pin_index].pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    m->handle = furi_hal_adc_acquire();
    if(m->handle) {
        /* Oversampled here, unlike the sweep: the setup screen wants a steady
         * number to read off, not the ripple detail. */
        furi_hal_adc_configure_ex(
            m->handle,
            FuriHalAdcScale2500,
            FuriHalAdcClockSync64,
            FuriHalAdcOversample64,
            FuriHalAdcSamplingtime247_5);
    }
    return m;
}

void ir_probe_meter_free(IrProbeMeter* m) {
    furi_assert(m);
    if(m->handle) furi_hal_adc_release(m->handle);
    free(m);
}

uint16_t ir_probe_meter_read_mv(IrProbeMeter* m) {
    furi_assert(m);
    if(!m->handle) return 0;
    float v = furi_hal_adc_convert_to_voltage(m->handle, furi_hal_adc_read(m->handle, m->channel));
    if(v < 0.0f) v = 0.0f;
    return (uint16_t)v;
}

/* ---------------- state ---------------- */

struct IrSense {
    FuriThread* thread;
    FuriMutex* mutex;
    volatile bool running;
    volatile bool reset_req;
    volatile bool renull_req;

    IrSenseMode mode; // may be Auto
    uint8_t sensitivity;
    uint8_t probe_pin_index;

    /* written from the IR capture ISR, drained under a critical section */
    volatile uint32_t isr_edges;
    volatile uint32_t isr_mark_us;

    IrStats stats; // guarded by mutex
};

static void ir_stats_clear_counters(IrStats* s) {
    s->present = false;
    s->level = 0;
    s->peak = 0;
    s->trend = 0;
    s->kind = IrSourceNone;
    s->hits = 0;
    s->last_seen_tick = 0;
    s->raw_mv = 0;
    s->ripple_mv = 0;
    s->freq_hz = 0;
    s->edges_per_sec = 0;
    s->trace_head = 0;
    memset(s->trace, 0, sizeof(s->trace));
}

/* Push one level sample and derive peak / trend / hit count. Call with the
 * mutex held. Returns the level from 300 ms ago, which is what makes the
 * "getting warmer" arrow meaningful while you pan the room. */
static void ir_stats_commit(IrSense* s, uint8_t level, bool present) {
    IrStats* st = &s->stats;

    uint8_t prev_head = st->trace_head;
    uint8_t ref_idx = (uint8_t)((prev_head + NYX_TRACE_LEN - 3u) % NYX_TRACE_LEN);
    uint8_t ref = st->trace[ref_idx];

    st->level = level;
    if(level > st->peak) st->peak = level;

    if(level > ref + 3) {
        st->trend = 1;
    } else if(ref > level + 3) {
        st->trend = -1;
    } else {
        st->trend = 0;
    }

    if(present) {
        st->last_seen_tick = furi_get_tick();
        if(!st->present) st->hits++;
    }
    st->present = present;

    st->trace_head = (uint8_t)((st->trace_head + 1u) % NYX_TRACE_LEN);
    st->trace[st->trace_head] = level;
}

/* ---------------- onboard path (TSOP-75338) ---------------- */

static void ir_sense_capture_isr(void* ctx, bool level, uint32_t duration) {
    IrSense* s = ctx;
    /* Every transition of the receiver's output is one unit of IR activity.
     * Counting edges rather than decoding is deliberate: we do not care what
     * the emitter is saying, only that it is emitting. It also keeps the
     * metric independent of the HAL's mark/space polarity. */
    s->isr_edges++;
    if(level) s->isr_mark_us += duration;
}

static void ir_sense_timeout_isr(void* ctx) {
    UNUSED(ctx); // silence just means no edges this window; the counters say so
}

static int32_t ir_sense_worker_onboard(IrSense* s) {
    if(furi_hal_infrared_is_busy()) {
        furi_mutex_acquire(s->mutex, FuriWaitForever);
        s->stats.error = IrSenseErrorIrBusy;
        s->stats.armed = false;
        furi_mutex_release(s->mutex);
        return 0;
    }

    s->isr_edges = 0;
    s->isr_mark_us = 0;

    furi_hal_infrared_async_rx_set_capture_isr_callback(ir_sense_capture_isr, s);
    furi_hal_infrared_async_rx_set_timeout_isr_callback(ir_sense_timeout_isr, s);
    furi_hal_infrared_async_rx_start();
    furi_hal_infrared_async_rx_set_timeout(ONBOARD_TIMEOUT_US);

    furi_mutex_acquire(s->mutex, FuriWaitForever);
    s->stats.armed = true;
    s->stats.error = IrSenseErrorNone;
    furi_mutex_release(s->mutex);

    uint8_t ema = 0;

    while(s->running) {
        furi_delay_ms(WINDOW_MS);

        FURI_CRITICAL_ENTER();
        uint32_t edges = s->isr_edges;
        uint32_t mark_us = s->isr_mark_us;
        s->isr_edges = 0;
        s->isr_mark_us = 0;
        FURI_CRITICAL_EXIT();

        uint32_t eps = edges * (1000u / WINDOW_MS);
        uint32_t full = onboard_full_scale_eps[s->sensitivity];
        uint32_t activity = (eps * 100u) / full;
        if(activity > 100u) activity = 100u;

        /* A receiver holding its output asserted is also a strong reading, even
         * when it produces few edges — take whichever says "louder". */
        uint32_t duty = (mark_us * 100u) / (WINDOW_MS * 1000u);
        if(duty > 100u) duty = 100u;
        uint8_t raw = (uint8_t)(activity > duty ? activity : duty);

        ema = (uint8_t)((ema * 3u + raw) / 4u);
        bool present = eps >= onboard_floor_eps[s->sensitivity];

        furi_mutex_acquire(s->mutex, FuriWaitForever);
        if(s->reset_req) {
            ir_stats_clear_counters(&s->stats);
            s->reset_req = false;
            ema = 0;
        }
        s->stats.edges_per_sec = eps;
        /* Onboard only ever sees modulation — that is the whole limitation. */
        s->stats.kind = present ? IrSourcePulsed : IrSourceNone;
        ir_stats_commit(s, ema, present);
        furi_mutex_release(s->mutex);
    }

    furi_hal_infrared_async_rx_stop();
    return 0;
}

/* ---------------- probe path (IR phototransistor on ADC) ---------------- */

/* Sample a contiguous burst and reduce it to mean / peak-to-peak / dominant
 * ripple frequency. Frequency comes from counting mean crossings, which is
 * crude but costs nothing and only has to separate "mains lamp" from "faster
 * than mains" — we are not trying to measure a spectrum. */
static void probe_sample_burst(
    FuriHalAdcHandle* handle,
    FuriHalAdcChannel channel,
    uint16_t* mean_mv,
    uint16_t* ripple_mv,
    uint16_t* freq_hz) {
    uint16_t mv[PROBE_BURST_SAMPLES];
    uint32_t sum = 0;
    uint16_t lo = 0xFFFF, hi = 0;

    for(uint32_t i = 0; i < PROBE_BURST_SAMPLES; i++) {
        uint16_t raw = furi_hal_adc_read(handle, channel);
        float v = furi_hal_adc_convert_to_voltage(handle, raw);
        if(v < 0.0f) v = 0.0f;
        mv[i] = (uint16_t)v;
        sum += mv[i];
        if(mv[i] < lo) lo = mv[i];
        if(mv[i] > hi) hi = mv[i];
        furi_delay_us(PROBE_SAMPLE_US);
    }

    uint16_t mean = (uint16_t)(sum / PROBE_BURST_SAMPLES);
    *mean_mv = mean;
    *ripple_mv = (uint16_t)(hi - lo);

    uint32_t crossings = 0;
    bool above = mv[0] > mean;
    for(uint32_t i = 1; i < PROBE_BURST_SAMPLES; i++) {
        bool now = mv[i] > mean;
        if(now != above) {
            crossings++;
            above = now;
        }
    }
    /* Two crossings per cycle. Only meaningful if there is ripple to cross. */
    *freq_hz =
        (*ripple_mv > PROBE_RIPPLE_FLOOR_MV) ? (uint16_t)((crossings * 1000u) / (2u * PROBE_BURST_MS)) : 0u;
}

static int32_t ir_sense_worker_probe(IrSense* s) {
    build_probe_pins();
    uint8_t idx = s->probe_pin_index;
    if(idx >= g_probe_pin_count) idx = 0;
    const IrProbePin* p = &g_probe_pins[idx];

    furi_hal_gpio_init(p->pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

    FuriHalAdcHandle* handle = furi_hal_adc_acquire();
    if(!handle) {
        furi_mutex_acquire(s->mutex, FuriWaitForever);
        s->stats.error = IrSenseErrorAdcBusy;
        s->stats.armed = false;
        furi_mutex_release(s->mutex);
        return 0;
    }

    /* No oversampling on purpose: the default config averages 64 samples per
     * read, which would smooth away the very ripple we use to tell a steady
     * illuminator from a flickering lamp. 2.5 V scale buys headroom before a
     * brightly-lit phototransistor pegs the input. */
    furi_hal_adc_configure_ex(
        handle,
        FuriHalAdcScale2500,
        FuriHalAdcClockSync64,
        FuriHalAdcOversampleNone,
        FuriHalAdcSamplingtime247_5);

    furi_mutex_acquire(s->mutex, FuriWaitForever);
    s->stats.armed = true;
    s->stats.error = IrSenseErrorNone;
    furi_mutex_release(s->mutex);

    uint16_t baseline = 0;
    uint8_t ema = 0;
    bool nulled = false;
    uint32_t null_acc = 0;
    uint32_t null_n = 0;

    while(s->running) {
        uint16_t mean_mv = 0, ripple_mv = 0, freq_hz = 0;
        probe_sample_burst(handle, p->channel, &mean_mv, &ripple_mv, &freq_hz);

        if(s->renull_req) {
            nulled = false;
            null_acc = 0;
            null_n = 0;
            s->renull_req = false;
        }

        /* Ambient null. Everything after this measures IR *in excess of* the
         * room as it was when you armed, which is what makes the meter mean
         * something indoors. */
        if(!nulled) {
            null_acc += mean_mv;
            null_n++;
            if(null_n >= PROBE_NULL_WINDOWS) {
                baseline = (uint16_t)(null_acc / null_n);
                nulled = true;
                ema = 0;
            }
            furi_mutex_acquire(s->mutex, FuriWaitForever);
            s->stats.baseline_mv = 0; // 0 baseline => UI shows "NULLING"
            s->stats.raw_mv = mean_mv;
            furi_mutex_release(s->mutex);
            furi_delay_ms(WINDOW_MS - PROBE_BURST_MS);
            continue;
        }

        uint16_t excess = (mean_mv > baseline) ? (uint16_t)(mean_mv - baseline) : 0u;
        uint32_t full = probe_full_scale_mv[s->sensitivity];
        uint32_t lvl = ((uint32_t)excess * 100u) / full;
        if(lvl > 100u) lvl = 100u;

        ema = (uint8_t)((ema * 3u + (uint8_t)lvl) / 4u);
        bool present = excess >= probe_floor_mv[s->sensitivity];

        IrSourceKind kind = IrSourceNone;
        if(present) {
            if(ripple_mv <= PROBE_RIPPLE_FLOOR_MV) {
                kind = IrSourceSteady; // flat DC — the night-vision signature
            } else if(freq_hz >= 70u && freq_hz <= 150u) {
                kind = IrSourceFlicker; // riding the mains — a lamp, not a camera
            } else {
                kind = IrSourcePulsed;
            }
        }

        furi_mutex_acquire(s->mutex, FuriWaitForever);
        if(s->reset_req) {
            ir_stats_clear_counters(&s->stats);
            s->reset_req = false;
            ema = 0;
        }
        s->stats.baseline_mv = baseline ? baseline : 1; // never 0 once nulled
        s->stats.raw_mv = mean_mv;
        s->stats.ripple_mv = ripple_mv;
        s->stats.freq_hz = freq_hz;
        s->stats.kind = kind;
        ir_stats_commit(s, ema, present);
        furi_mutex_release(s->mutex);

        furi_delay_ms(WINDOW_MS - PROBE_BURST_MS);
    }

    furi_hal_adc_release(handle);
    return 0;
}

static int32_t ir_sense_worker(void* context) {
    IrSense* s = context;

    IrSenseMode resolved = s->mode;
    bool probe_present = ir_sense_probe_detect(s->probe_pin_index);

    if(resolved == IrSenseModeAuto) {
        resolved = probe_present ? IrSenseModeProbe : IrSenseModeOnboard;
    } else if(resolved == IrSenseModeProbe && !probe_present) {
        /* Forced probe with nothing wired would just meter a floating pin, so
         * say so rather than showing convincing noise. */
        furi_mutex_acquire(s->mutex, FuriWaitForever);
        s->stats.error = IrSenseErrorNoProbe;
        s->stats.armed = false;
        s->stats.active_mode = IrSenseModeProbe;
        s->stats.probe_present = false;
        furi_mutex_release(s->mutex);
        return 0;
    }

    furi_mutex_acquire(s->mutex, FuriWaitForever);
    s->stats.active_mode = resolved;
    s->stats.probe_present = probe_present;
    furi_mutex_release(s->mutex);

    int32_t rc = (resolved == IrSenseModeProbe) ? ir_sense_worker_probe(s) :
                                                  ir_sense_worker_onboard(s);

    furi_mutex_acquire(s->mutex, FuriWaitForever);
    s->stats.armed = false;
    s->stats.present = false;
    furi_mutex_release(s->mutex);
    return rc;
}

/* ---------------- public API ---------------- */

IrSense* ir_sense_alloc(void) {
    IrSense* s = malloc(sizeof(IrSense));
    memset(s, 0, sizeof(IrSense));
    s->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    s->mode = IrSenseModeAuto;
    s->sensitivity = 1; // Medium
    s->probe_pin_index = 0;
    ir_stats_clear_counters(&s->stats);
    return s;
}

void ir_sense_free(IrSense* s) {
    furi_assert(s);
    ir_sense_stop(s);
    furi_mutex_free(s->mutex);
    free(s);
}

void ir_sense_set_mode(IrSense* s, IrSenseMode mode) {
    furi_assert(s);
    s->mode = mode;
}

void ir_sense_set_sensitivity(IrSense* s, uint8_t index) {
    furi_assert(s);
    if(index > 2) index = 2;
    s->sensitivity = index;
}

void ir_sense_set_probe_pin(IrSense* s, uint8_t pin_index) {
    furi_assert(s);
    s->probe_pin_index = pin_index;
}

void ir_sense_start(IrSense* s) {
    furi_assert(s);
    if(s->running) return;

    furi_mutex_acquire(s->mutex, FuriWaitForever);
    ir_stats_clear_counters(&s->stats);
    s->stats.error = IrSenseErrorNone;
    s->stats.baseline_mv = 0;
    furi_mutex_release(s->mutex);

    s->reset_req = false;
    s->renull_req = false;
    s->running = true;
    /* 4 KB: the probe path puts a 256-byte sample burst on this stack on top of
     * the ADC HAL call depth, so give it headroom over the usual 2 KB worker. */
    s->thread = furi_thread_alloc_ex("NyxIrSense", 4096, ir_sense_worker, s);
    furi_thread_start(s->thread);
}

void ir_sense_stop(IrSense* s) {
    furi_assert(s);
    if(!s->running) return;
    s->running = false;
    if(s->thread) {
        furi_thread_join(s->thread);
        furi_thread_free(s->thread);
        s->thread = NULL;
    }
}

bool ir_sense_is_running(IrSense* s) {
    furi_assert(s);
    return s->running;
}

void ir_sense_reset(IrSense* s) {
    furi_assert(s);
    if(s->running) {
        s->reset_req = true; // the worker clears on its next window
    } else {
        furi_mutex_acquire(s->mutex, FuriWaitForever);
        ir_stats_clear_counters(&s->stats);
        furi_mutex_release(s->mutex);
    }
}

void ir_sense_renull(IrSense* s) {
    furi_assert(s);
    s->renull_req = true;
}

void ir_sense_get(IrSense* s, IrStats* out) {
    furi_assert(s);
    furi_assert(out);
    furi_mutex_acquire(s->mutex, FuriWaitForever);
    *out = s->stats;
    furi_mutex_release(s->mutex);
}
