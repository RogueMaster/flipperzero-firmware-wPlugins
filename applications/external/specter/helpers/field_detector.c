#include "field_detector.h"
#include <furi_hal_nfc.h>
#include <string.h>

/* Sampling: ~2 ms per sample, ~48 samples per window => ~10 strength updates/s.
 * That's fast enough to catch the short polling bursts a reader emits while
 * still giving the meter a smooth, readable cadence. SPECTER_SAMPLE_MS in
 * emitter_classify.h must agree with this - it is what the classifier treats as
 * its resolution floor.
 *
 * The wait between samples MUST yield to the scheduler. furi_delay_us() does
 * not: it is a DWT busy-loop, so pacing this thread with it pinned a core at
 * 100% for the whole scan. Everything else on the system - the GUI thread, the
 * input service, storage - then had to fight this thread for time, and once the
 * view dispatcher stopped draining its input queue fast enough the GUI thread
 * blocked posting into it and took the whole UI down with it: dead buttons and
 * a Flipper that needed a reboot. furi_delay_tick() sleeps properly instead. */
#define WINDOW_SAMPLES 48u

/* The pulse trace compresses 4 samples (~8 ms) into one column, so the 128-column
 * buffer spans ~1.0 s - enough to show four or five cycles of a typical 200 ms
 * reader poll, and it maps one column to one screen pixel with no rescaling. */
#define TRACE_SLICE_SAMPLES 4u

/* How many recent burst/gap/period triples the cadence figures average over. */
#define CADENCE_RING 16u

/* Calibration lifts the threshold this far above the measured ambient floor. */
#define CALIBRATION_MARGIN 3u
#define CALIBRATION_MAX    60u

struct FieldDetector {
    FuriThread* thread;
    FuriMutex* mutex;
    volatile bool running;
    volatile bool reset_req;
    volatile bool calib_req;
    volatile bool calib_cancel_req;
    volatile uint32_t calib_duration_ms;
    uint8_t threshold; // duty-cycle noise floor (%)
    volatile uint8_t full_scale; // raw duty that displays as 100%
    FieldStats stats; // guarded by mutex
};

/* Rolling window of recent carrier timings, owned by the worker thread. */
typedef struct {
    uint16_t burst[CADENCE_RING];
    uint16_t gap[CADENCE_RING];
    uint16_t period[CADENCE_RING];
    uint8_t count; // entries filled, saturates at CADENCE_RING
    uint8_t head; // next slot to write
    uint32_t total; // complete cycles seen since the last reset
} CadenceRing;

static void cadence_ring_clear(CadenceRing* r) {
    memset(r, 0, sizeof(*r));
}

static uint16_t clamp_u16(uint32_t v) {
    return (uint16_t)(v > UINT16_MAX ? UINT16_MAX : v);
}

static void cadence_ring_push(CadenceRing* r, uint32_t burst_ms, uint32_t gap_ms) {
    r->burst[r->head] = clamp_u16(burst_ms);
    r->gap[r->head] = clamp_u16(gap_ms);
    r->period[r->head] = clamp_u16(burst_ms + gap_ms);
    r->head = (uint8_t)((r->head + 1u) % CADENCE_RING);
    if(r->count < CADENCE_RING) r->count++;
    r->total++;
}

/* Condense the ring into the means the classifier wants. Jitter is the mean
 * absolute deviation of the period - a plain, explainable measure of how steady
 * the emitter's rhythm is, and one that does not need a square root. */
static void cadence_ring_summarise(const CadenceRing* r, CadenceStats* out, uint8_t duty) {
    memset(out, 0, sizeof(*out));
    out->duty = duty;
    out->bursts = clamp_u16(r->total);
    if(r->count == 0) return;

    uint32_t n = r->count;
    uint32_t burst_sum = 0, gap_sum = 0, period_sum = 0;
    for(uint32_t i = 0; i < n; i++) {
        burst_sum += r->burst[i];
        gap_sum += r->gap[i];
        period_sum += r->period[i];
    }
    out->burst_ms = clamp_u16(burst_sum / n);
    out->gap_ms = clamp_u16(gap_sum / n);
    out->period_ms = clamp_u16(period_sum / n);

    uint32_t mean = period_sum / n;
    uint32_t dev_sum = 0;
    for(uint32_t i = 0; i < n; i++) {
        uint32_t p = r->period[i];
        dev_sum += (p > mean) ? (p - mean) : (mean - p);
    }
    out->jitter_ms = clamp_u16(dev_sum / n);
}

static void field_stats_clear(FieldStats* s) {
    s->present = false;
    s->strength = 0;
    s->strength_raw = 0;
    s->saturated = false;
    s->peak = 0;
    s->average = 0;
    s->contacts = 0;
    s->last_seen_tick = 0;
    s->elapsed_ms = 0;
    s->in_field_ms = 0;
    s->history_head = 0;
    memset(s->history, 0, sizeof(s->history));
    s->trace_head = 0;
    memset(s->trace, 0, sizeof(s->trace));
    memset(&s->cadence, 0, sizeof(s->cadence));
    s->calibrating = false;
    s->calibration_ready = false;
    s->calibration_floor = 0;
    s->calibration_suggest = 0;
    s->calibration_progress = 0;
}

static int32_t field_detector_worker(void* context) {
    FieldDetector* fd = context;

    FuriHalNfcError err = furi_hal_nfc_acquire();
    if(err != FuriHalNfcErrorNone) {
        furi_mutex_acquire(fd->mutex, FuriWaitForever);
        fd->stats.error = true;
        fd->stats.armed = false;
        furi_mutex_release(fd->mutex);
        return 0;
    }

    furi_hal_nfc_low_power_mode_stop();
    furi_hal_nfc_field_detect_start(); // listen for an external carrier; we never emit

    uint32_t armed_tick = furi_get_tick();
    furi_mutex_acquire(fd->mutex, FuriWaitForever);
    fd->stats.armed = true;
    fd->stats.error = false;
    fd->stats.armed_tick = armed_tick;
    furi_mutex_release(fd->mutex);

    /* Derived from the real tick rate rather than assuming 1 kHz, and never
     * zero - a zero-tick delay would put us straight back to a spin loop. */
    uint32_t tick_hz = furi_kernel_get_tick_frequency();
    uint32_t sample_ticks = (SPECTER_SAMPLE_MS * tick_hz) / 1000u;
    if(sample_ticks == 0u) sample_ticks = 1u;

    uint32_t hits = 0, samples = 0;
    uint8_t ema = 0; // smoothed strength
    bool was_present = false;
    uint32_t window_start = armed_tick;

    /* running mean of the windowed strength, for the session average */
    uint32_t strength_sum = 0, strength_n = 0;

    /* Presence is latched rather than taken window-by-window - a polling
     * reader legitimately goes quiet between bursts. See present_hold.h. */
    PresentHold hold;
    present_hold_reset(&hold);

    /* per-sample edge tracking */
    bool raw_prev = false;
    uint32_t run_start = armed_tick;
    bool have_burst = false;
    uint32_t last_burst_ms = 0;
    CadenceRing ring;
    cadence_ring_clear(&ring);

    /* pulse trace accumulation */
    uint32_t slice_hits = 0, slice_samples = 0;

    /* calibration */
    bool calibrating = false;
    uint32_t calib_end = 0, calib_start = 0, calib_duration = 0;
    uint8_t calib_max = 0;

    while(fd->running) {
        bool raw = furi_hal_nfc_field_is_present();
        uint32_t now = furi_get_tick();

        /* ---- edges: time each contiguous carrier-on and carrier-off run ---- */
        if(raw != raw_prev) {
            uint32_t run_ms = now - run_start;
            if(raw_prev) {
                /* an ON run just ended */
                last_burst_ms = run_ms;
                have_burst = true;
            } else if(have_burst) {
                /* an OFF run just ended and we have its burst: one full cycle */
                cadence_ring_push(&ring, last_burst_ms, run_ms);
            }
            run_start = now;
            raw_prev = raw;
        }

        /* ---- pulse trace ---- */
        if(raw) slice_hits++;
        slice_samples++;
        if(slice_samples >= TRACE_SLICE_SAMPLES) {
            furi_mutex_acquire(fd->mutex, FuriWaitForever);
            FieldStats* s = &fd->stats;
            s->trace_head = (uint8_t)((s->trace_head + 1u) % SPECTER_TRACE_LEN);
            s->trace[s->trace_head] = (uint8_t)slice_hits;
            furi_mutex_release(fd->mutex);
            slice_hits = 0;
            slice_samples = 0;
        }

        /* ---- strength window ---- */
        if(raw) hits++;
        samples++;

        if(samples >= WINDOW_SAMPLES) {
            uint8_t duty = (uint8_t)((hits * 100u) / samples);
            ema = (uint8_t)((ema * 3u + duty) / 4u); // 1st-order low-pass
            uint32_t window_ms = now - window_start;

            furi_mutex_acquire(fd->mutex, FuriWaitForever);
            FieldStats* s = &fd->stats;

            if(fd->reset_req) {
                field_stats_clear(s);
                fd->reset_req = false;
                ema = 0;
                was_present = false;
                have_burst = false;
                present_hold_reset(&hold);
                strength_sum = 0;
                strength_n = 0;
                calibrating = false;
                cadence_ring_clear(&ring);
                armed_tick = now;
                s->armed_tick = now;
                /* This window straddles the reset, so none of it belongs to the
                 * new session's in-field total. */
                window_ms = 0;
            }

            if(fd->calib_cancel_req) {
                fd->calib_cancel_req = false;
                calibrating = false;
                s->calibrating = false;
                s->calibration_ready = false;
                s->calibration_progress = 0;
            }

            if(fd->calib_req) {
                fd->calib_req = false;
                calibrating = true;
                calib_start = now;
                calib_duration = fd->calib_duration_ms ? fd->calib_duration_ms : 1u;
                calib_end = now + calib_duration;
                calib_max = 0;
                s->calibrating = true;
                s->calibration_ready = false;
                s->calibration_progress = 0;
            }

            /* Cadence first: the hold below is sized from the measured polling
             * period, so it has to be up to date before presence is decided. */
            cadence_ring_summarise(&ring, &s->cadence, ema);

            /* Detection stays on the raw duty: the noise floor is a statement
             * about the signal, not about how the gauge is drawn. The verdict is
             * then latched, so the gaps in a reader's polling cycle do not read
             * as the reader disappearing - which used to inflate the contact
             * count and machine-gun the alert notifications - but the latch is
             * only as long as this reader's own rhythm needs. */
            bool present = present_hold_update(
                &hold, duty > fd->threshold, now, present_hold_ms_for(s->cadence.period_ms));
            s->present = present;
            s->strength_raw = ema;

            /* Everything the user reads is mapped onto a full-scale meter. */
            uint8_t shown = field_scale_apply(ema, fd->full_scale);
            s->strength = shown;
            s->saturated = field_scale_is_saturated(ema, fd->full_scale);
            if(shown > s->peak) s->peak = shown;

            /* Halve both sides long before the sum could overflow - the mean is
             * preserved, and a sweep left running for days still reads sanely. */
            if(strength_n >= 0x400000u) {
                strength_sum >>= 1;
                strength_n >>= 1;
            }
            strength_sum += shown;
            strength_n++;
            s->average = (uint8_t)(strength_sum / strength_n);

            s->elapsed_ms = now - armed_tick;
            s->in_field_ms += (window_ms * hits) / samples;

            if(present) {
                s->last_seen_tick = now;
                if(!was_present) s->contacts++;
            }
            /* The waveform is part of the meter, so it follows the same scale. */
            s->history_head = (uint8_t)((s->history_head + 1u) % SPECTER_HISTORY_LEN);
            s->history[s->history_head] = shown;

            /* ---- calibration pass ---- */
            if(calibrating) {
                if(duty > calib_max) calib_max = duty;
                uint32_t done = now - calib_start;
                uint32_t pct = (done * 100u) / calib_duration;
                s->calibration_progress = (uint8_t)(pct > 100u ? 100u : pct);

                if((int32_t)(now - calib_end) >= 0) {
                    calibrating = false;
                    uint32_t suggest = (uint32_t)calib_max + CALIBRATION_MARGIN;
                    if(suggest > CALIBRATION_MAX) suggest = CALIBRATION_MAX;
                    s->calibration_floor = calib_max;
                    s->calibration_suggest = (uint8_t)suggest;
                    s->calibration_progress = 100;
                    s->calibrating = false;
                    s->calibration_ready = true;
                }
            }

            furi_mutex_release(fd->mutex);
            was_present = present;

            hits = 0;
            samples = 0;
            window_start = now;
        }

        furi_delay_tick(sample_ticks);
    }

    furi_hal_nfc_field_detect_stop();
    furi_hal_nfc_low_power_mode_start();
    furi_hal_nfc_reset_mode();
    furi_hal_nfc_release();

    furi_mutex_acquire(fd->mutex, FuriWaitForever);
    fd->stats.armed = false;
    fd->stats.present = false;
    fd->stats.calibrating = false;
    furi_mutex_release(fd->mutex);
    return 0;
}

FieldDetector* field_detector_alloc(void) {
    FieldDetector* fd = malloc(sizeof(FieldDetector));
    memset(fd, 0, sizeof(FieldDetector));
    fd->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    fd->threshold = 0; // default: most sensitive
    fd->full_scale = SPECTER_FULL_SCALE_DUTY;
    field_stats_clear(&fd->stats);
    return fd;
}

void field_detector_free(FieldDetector* fd) {
    furi_assert(fd);
    field_detector_stop(fd);
    furi_mutex_free(fd->mutex);
    free(fd);
}

void field_detector_set_threshold(FieldDetector* fd, uint8_t duty_threshold) {
    furi_assert(fd);
    fd->threshold = duty_threshold;
}

void field_detector_set_full_scale(FieldDetector* fd, uint8_t full_scale) {
    furi_assert(fd);
    /* Safe to change while the worker is running: it is a single byte read once
     * per window, and the only consequence of racing is one window drawn at the
     * old scale. */
    fd->full_scale = full_scale ? full_scale : SPECTER_SCALE_RAW;
}

void field_detector_start(FieldDetector* fd) {
    furi_assert(fd);
    if(fd->running) return;

    furi_mutex_acquire(fd->mutex, FuriWaitForever);
    field_stats_clear(&fd->stats);
    fd->stats.error = false;
    furi_mutex_release(fd->mutex);

    fd->reset_req = false;
    fd->calib_req = false;
    fd->calib_cancel_req = false;
    fd->running = true;
    fd->thread = furi_thread_alloc_ex("SpecterSniffer", 2048, field_detector_worker, fd);
    /* Below the GUI and input services on purpose. Sampling a bit late is
     * invisible; a laggy or unresponsive UI is not. */
    furi_thread_set_priority(fd->thread, FuriThreadPriorityLow);
    furi_thread_start(fd->thread);
}

void field_detector_stop(FieldDetector* fd) {
    furi_assert(fd);
    if(!fd->running) return;
    fd->running = false;
    if(fd->thread) {
        furi_thread_join(fd->thread);
        furi_thread_free(fd->thread);
        fd->thread = NULL;
    }
}

bool field_detector_is_running(FieldDetector* fd) {
    furi_assert(fd);
    return fd->running;
}

void field_detector_reset(FieldDetector* fd) {
    furi_assert(fd);
    if(fd->running) {
        fd->reset_req = true; // the worker clears on its next window
    } else {
        furi_mutex_acquire(fd->mutex, FuriWaitForever);
        field_stats_clear(&fd->stats);
        furi_mutex_release(fd->mutex);
    }
}

void field_detector_calibrate_begin(FieldDetector* fd, uint32_t duration_ms) {
    furi_assert(fd);
    if(!fd->running) return;
    fd->calib_duration_ms = duration_ms;
    fd->calib_req = true;
}

void field_detector_calibrate_cancel(FieldDetector* fd) {
    furi_assert(fd);
    fd->calib_req = false;
    fd->calib_cancel_req = true;
}

void field_detector_get(FieldDetector* fd, FieldStats* out) {
    furi_assert(fd);
    furi_assert(out);
    furi_mutex_acquire(fd->mutex, FuriWaitForever);
    *out = fd->stats;
    furi_mutex_release(fd->mutex);
}
