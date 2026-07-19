#include "field_detector.h"
#include <furi_hal_nfc.h>
#include <string.h>

/* Sampling: ~2 ms per sample, ~48 samples per window => ~10 strength updates/s.
 * That's fast enough to catch the short polling bursts a reader emits while
 * still giving the meter a smooth, readable cadence. */
#define SAMPLE_PERIOD_US 2000u
#define WINDOW_SAMPLES   48u

struct FieldDetector {
    FuriThread* thread;
    FuriMutex* mutex;
    volatile bool running;
    volatile bool reset_req;
    uint8_t threshold; // duty-cycle noise floor (%)
    FieldStats stats; // guarded by mutex
};

static void field_stats_clear(FieldStats* s) {
    s->present = false;
    s->strength = 0;
    s->peak = 0;
    s->contacts = 0;
    s->last_seen_tick = 0;
    s->history_head = 0;
    memset(s->history, 0, sizeof(s->history));
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

    furi_mutex_acquire(fd->mutex, FuriWaitForever);
    fd->stats.armed = true;
    fd->stats.error = false;
    fd->stats.armed_tick = furi_get_tick();
    furi_mutex_release(fd->mutex);

    uint32_t hits = 0, samples = 0;
    uint8_t ema = 0; // smoothed strength
    bool was_present = false;

    while(fd->running) {
        if(furi_hal_nfc_field_is_present()) hits++;
        samples++;

        if(samples >= WINDOW_SAMPLES) {
            uint8_t duty = (uint8_t)((hits * 100u) / samples);
            ema = (uint8_t)((ema * 3u + duty) / 4u); // 1st-order low-pass

            furi_mutex_acquire(fd->mutex, FuriWaitForever);
            FieldStats* s = &fd->stats;

            if(fd->reset_req) {
                field_stats_clear(s);
                fd->reset_req = false;
                ema = 0;
                was_present = false;
            }

            bool present = duty > fd->threshold;
            s->strength = ema;
            s->present = present;
            if(ema > s->peak) s->peak = ema;
            if(present) {
                s->last_seen_tick = furi_get_tick();
                if(!was_present) s->contacts++;
            }
            s->history_head = (uint8_t)((s->history_head + 1u) % SPECTER_HISTORY_LEN);
            s->history[s->history_head] = ema;

            furi_mutex_release(fd->mutex);
            was_present = present;

            hits = 0;
            samples = 0;
        }

        furi_delay_us(SAMPLE_PERIOD_US);
    }

    furi_hal_nfc_field_detect_stop();
    furi_hal_nfc_low_power_mode_start();
    furi_hal_nfc_reset_mode();
    furi_hal_nfc_release();

    furi_mutex_acquire(fd->mutex, FuriWaitForever);
    fd->stats.armed = false;
    fd->stats.present = false;
    furi_mutex_release(fd->mutex);
    return 0;
}

FieldDetector* field_detector_alloc(void) {
    FieldDetector* fd = malloc(sizeof(FieldDetector));
    memset(fd, 0, sizeof(FieldDetector));
    fd->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    fd->threshold = 0; // default: most sensitive
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

void field_detector_start(FieldDetector* fd) {
    furi_assert(fd);
    if(fd->running) return;

    furi_mutex_acquire(fd->mutex, FuriWaitForever);
    field_stats_clear(&fd->stats);
    fd->stats.error = false;
    furi_mutex_release(fd->mutex);

    fd->reset_req = false;
    fd->running = true;
    fd->thread = furi_thread_alloc_ex("SpecterSniffer", 2048, field_detector_worker, fd);
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

void field_detector_get(FieldDetector* fd, FieldStats* out) {
    furi_assert(fd);
    furi_assert(out);
    furi_mutex_acquire(fd->mutex, FuriWaitForever);
    *out = fd->stats;
    furi_mutex_release(fd->mutex);
}
