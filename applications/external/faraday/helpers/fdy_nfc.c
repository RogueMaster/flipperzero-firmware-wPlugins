#include "fdy_nfc.h"
#include <furi_hal_nfc.h>
#include <string.h>

/* ~2 ms per sample, 48 samples per window => ~10 strength updates/s: fast
 * enough to catch a reader's polling bursts, smooth enough to read. */
#define FDY_SAMPLE_PERIOD_MS  2u
#define FDY_WINDOW_SAMPLES    48u
#define FDY_PRESENT_THRESHOLD 4u // duty % that counts as "a field is here"
#define FDY_WORKER_STACK      2048u

/* One window is FDY_WINDOW_SAMPLES * FDY_SAMPLE_PERIOD_MS = ~96 ms. */
#define FDY_WINDOW_MS       (FDY_WINDOW_SAMPLES * FDY_SAMPLE_PERIOD_MS)
#define FDY_ARM_WINDOWS     52u /* ~5 s to seal the Flipper in the pouch */
#define FDY_MEASURE_WINDOWS 42u /* ~4 s of sealed measurement            */

struct FdyNfc {
    FuriThread* thread;
    FuriMutex* mutex;
    volatile bool running;
    volatile bool reset_req;
    volatile bool shield_req; // start the timed shielded capture
    FdyNfcSnapshot snap; // guarded by mutex
};

static void fdy_nfc_clear(FdyNfcSnapshot* s) {
    s->present = false;
    s->strength = 0;
    s->peak = 0;
    s->capture = (uint8_t)FdyNfcCaptureLive;
    s->seconds = 0;
    s->history_head = 0;
    memset(s->history, 0, sizeof(s->history));
}

static int32_t fdy_nfc_worker(void* context) {
    FdyNfc* n = context;

    /* Keep asking for the chip instead of giving up on the first refusal.
     *
     * furi_hal_nfc_acquire() returns busy while another app still holds the
     * radio. Returning here left n->running true with no thread behind it, so
     * snap.error could never clear and fdy_nfc_start() early-returned on the
     * stale flag - the "NFC busy" screen was a dead end that no key could
     * leave except Back, and nothing on screen said so. Retrying on a yielding
     * delay means the screen heals itself the moment the other app lets go. */
    while(n->running) {
        if(furi_hal_nfc_acquire() == FuriHalNfcErrorNone) break;
        furi_mutex_acquire(n->mutex, FuriWaitForever);
        n->snap.error = true;
        n->snap.armed = false;
        furi_mutex_release(n->mutex);
        furi_delay_tick(MAX(furi_ms_to_ticks(250), 1UL));
    }
    if(!n->running) return 0; // asked to stop while waiting for the chip

    furi_mutex_acquire(n->mutex, FuriWaitForever);
    n->snap.error = false;
    furi_mutex_release(n->mutex);

    furi_hal_nfc_low_power_mode_stop();
    furi_hal_nfc_field_detect_start(); // listen for an external carrier; never emit

    furi_mutex_acquire(n->mutex, FuriWaitForever);
    n->snap.armed = true;
    n->snap.error = false;
    furi_mutex_release(n->mutex);

    uint32_t hits = 0, samples = 0;
    uint8_t ema = 0;
    uint16_t stage = 0; // windows left in the current capture stage
    FdyNfcCapture capture = FdyNfcCaptureLive;

    /* Kernel ticks, not furi_delay_us(). That call is a non-yielding DWT busy
     * wait, so pacing this loop with it would hold the core for the whole gap
     * between samples, starve the GUI service and leave the app looking like
     * it is ignoring the buttons. */
    const uint32_t sample_ticks = MAX(furi_ms_to_ticks(FDY_SAMPLE_PERIOD_MS), 1UL);

    while(n->running) {
        if(furi_hal_nfc_field_is_present()) hits++;
        samples++;

        if(samples >= FDY_WINDOW_SAMPLES) {
            uint8_t duty = (uint8_t)((hits * 100u) / samples);
            ema = (uint8_t)((ema * 3u + duty) / 4u); // 1st-order low-pass

            furi_mutex_acquire(n->mutex, FuriWaitForever);
            FdyNfcSnapshot* s = &n->snap;

            if(n->shield_req) {
                /* Begin the timed shielded capture. */
                n->shield_req = false;
                n->reset_req = false;
                ema = 0;
                s->peak = 0;
                s->strength = 0;
                s->present = false;
                stage = FDY_ARM_WINDOWS;
                capture = FdyNfcCaptureArming;
            } else if(n->reset_req) {
                /* A plain peak reset must also drop the LOW-PASS, not just the
                 * peak. ema is a worker local carrying several windows of the
                 * PREVIOUS phase's field, so publishing a peak from it in the
                 * same window silently re-latched the baseline - which is
                 * exactly what made every NFC pouch grade F. */
                n->reset_req = false;
                ema = 0;
                s->peak = 0;
                s->strength = 0;
                s->present = false;
                stage = 0;
                capture = FdyNfcCaptureLive;
            } else {
                s->strength = ema;
                s->present = ema > FDY_PRESENT_THRESHOLD;

                switch(capture) {
                case FdyNfcCaptureArming:
                    /* Deliberately NOT accumulating: the Flipper is in the
                     * user's hand, passing through the reader field on its way
                     * into the pouch. */
                    if(stage) stage--;
                    if(!stage) {
                        stage = FDY_MEASURE_WINDOWS;
                        capture = FdyNfcCaptureMeasuring;
                    }
                    break;
                case FdyNfcCaptureMeasuring:
                    if(ema > s->peak) s->peak = ema;
                    if(stage) stage--;
                    if(!stage) capture = FdyNfcCaptureFrozen;
                    break;
                case FdyNfcCaptureFrozen:
                    /* Held. Taking the Flipper back out of the pouch to press
                     * OK must not overwrite what was measured inside it. */
                    break;
                default:
                    if(ema > s->peak) s->peak = ema;
                    break;
                }
            }

            s->capture = (uint8_t)capture;
            s->seconds = (uint8_t)(((uint32_t)stage * FDY_WINDOW_MS + 999u) / 1000u);
            s->history_head = (uint8_t)((s->history_head + 1u) % FDY_HISTORY_LEN);
            s->history[s->history_head] = ema;
            furi_mutex_release(n->mutex);

            hits = 0;
            samples = 0;
        }

        furi_delay_tick(sample_ticks);
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
    n->shield_req = false;
    n->running = true;
    n->thread = furi_thread_alloc_ex("FaradayNfc", FDY_WORKER_STACK, fdy_nfc_worker, n);
    /* Below the UI, for the same reason as the Sub-GHz worker. */
    furi_thread_set_priority(n->thread, FuriThreadPriorityLow);
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

void fdy_nfc_begin_shielded(FdyNfc* n) {
    furi_assert(n);
    if(n->running) {
        n->shield_req = true; // the worker picks it up on its next window
    }
}

void fdy_nfc_get(FdyNfc* n, FdyNfcSnapshot* out) {
    furi_assert(n);
    furi_assert(out);
    furi_mutex_acquire(n->mutex, FuriWaitForever);
    *out = n->snap;
    furi_mutex_release(n->mutex);
}
