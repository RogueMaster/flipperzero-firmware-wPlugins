#include "fdy_nfc.h"
#include <furi_hal_nfc.h>
#include <string.h>

/* ~2 ms per sample, 48 samples per window => ~10 strength updates/s: fast
 * enough to catch a reader's polling bursts, smooth enough to read. */
#define FDY_SAMPLE_PERIOD_US 2000u
#define FDY_WINDOW_SAMPLES 48u
#define FDY_PRESENT_THRESHOLD 4u // duty % that counts as "a field is here"

struct FdyNfc {
    FuriThread* thread;
    FuriMutex* mutex;
    volatile bool running;
    volatile bool reset_req;
    FdyNfcSnapshot snap; // guarded by mutex
};

static void fdy_nfc_clear(FdyNfcSnapshot* s) {
    s->present = false;
    s->strength = 0;
    s->peak = 0;
    s->history_head = 0;
    memset(s->history, 0, sizeof(s->history));
}

static int32_t fdy_nfc_worker(void* context) {
    FdyNfc* n = context;

    FuriHalNfcError err = furi_hal_nfc_acquire();
    if(err != FuriHalNfcErrorNone) {
        furi_mutex_acquire(n->mutex, FuriWaitForever);
        n->snap.error = true;
        n->snap.armed = false;
        furi_mutex_release(n->mutex);
        return 0;
    }

    furi_hal_nfc_low_power_mode_stop();
    furi_hal_nfc_field_detect_start(); // listen for an external carrier; never emit

    furi_mutex_acquire(n->mutex, FuriWaitForever);
    n->snap.armed = true;
    n->snap.error = false;
    furi_mutex_release(n->mutex);

    uint32_t hits = 0, samples = 0;
    uint8_t ema = 0;

    while(n->running) {
        if(furi_hal_nfc_field_is_present()) hits++;
        samples++;

        if(samples >= FDY_WINDOW_SAMPLES) {
            uint8_t duty = (uint8_t)((hits * 100u) / samples);
            ema = (uint8_t)((ema * 3u + duty) / 4u); // 1st-order low-pass

            furi_mutex_acquire(n->mutex, FuriWaitForever);
            FdyNfcSnapshot* s = &n->snap;
            if(n->reset_req) {
                s->peak = 0;
                n->reset_req = false;
            }
            s->strength = ema;
            s->present = ema > FDY_PRESENT_THRESHOLD;
            if(ema > s->peak) s->peak = ema;
            s->history_head = (uint8_t)((s->history_head + 1u) % FDY_HISTORY_LEN);
            s->history[s->history_head] = ema;
            furi_mutex_release(n->mutex);

            hits = 0;
            samples = 0;
        }

        furi_delay_us(FDY_SAMPLE_PERIOD_US);
    }

    furi_hal_nfc_field_detect_stop();
    furi_hal_nfc_low_power_mode_start();
    furi_hal_nfc_reset_mode();
    furi_hal_nfc_release();

    furi_mutex_acquire(n->mutex, FuriWaitForever);
    n->snap.armed = false;
    n->snap.present = false;
    furi_mutex_release(n->mutex);
    return 0;
}

FdyNfc* fdy_nfc_alloc(void) {
    FdyNfc* n = malloc(sizeof(FdyNfc));
    memset(n, 0, sizeof(FdyNfc));
    n->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    fdy_nfc_clear(&n->snap);
    return n;
}

void fdy_nfc_free(FdyNfc* n) {
    furi_assert(n);
    fdy_nfc_stop(n);
    furi_mutex_free(n->mutex);
    free(n);
}

void fdy_nfc_start(FdyNfc* n) {
    furi_assert(n);
    if(n->running) return;

    furi_mutex_acquire(n->mutex, FuriWaitForever);
    fdy_nfc_clear(&n->snap);
    n->snap.error = false;
    furi_mutex_release(n->mutex);

    n->reset_req = false;
    n->running = true;
    n->thread = furi_thread_alloc_ex("FaradayNfc", 2048, fdy_nfc_worker, n);
    furi_thread_start(n->thread);
}

void fdy_nfc_stop(FdyNfc* n) {
    furi_assert(n);
    if(!n->running) return;
    n->running = false;
    if(n->thread) {
        furi_thread_join(n->thread);
        furi_thread_free(n->thread);
        n->thread = NULL;
    }
}

bool fdy_nfc_is_running(FdyNfc* n) {
    furi_assert(n);
    return n->running;
}

void fdy_nfc_reset_peak(FdyNfc* n) {
    furi_assert(n);
    if(n->running) {
        n->reset_req = true; // worker clears on its next window
    } else {
        furi_mutex_acquire(n->mutex, FuriWaitForever);
        n->snap.peak = 0;
        furi_mutex_release(n->mutex);
    }
}

void fdy_nfc_get(FdyNfc* n, FdyNfcSnapshot* out) {
    furi_assert(n);
    furi_assert(out);
    furi_mutex_acquire(n->mutex, FuriWaitForever);
    *out = n->snap;
    furi_mutex_release(n->mutex);
}
