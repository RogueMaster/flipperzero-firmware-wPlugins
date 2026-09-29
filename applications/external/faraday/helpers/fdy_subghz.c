#include "fdy_subghz.h"

#include <furi_hal.h>
#include <furi_hal_subghz.h> // FuriHalSubGhzPreset enum + valid-range helpers
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>

#define TAG "Faraday"

#define FDY_SETTLE_MS    2 // RSSI settle after retuning
#define FDY_SAMPLE_MS    2 // gap between RSSI reads (~500 Hz)
#define FDY_WORKER_STACK (2 * 1024)

/* Noise-floor tracking. The floor snaps DOWN to any new quiet minimum
 * immediately, but only creeps back UP while the channel is actually quiet -
 * "quiet" meaning the live reading is within FDY_FLOOR_QUIET_DB of the floor.
 *
 * That gate is the whole point. Without it the floor rises on every sample,
 * carrier or not, so a fob held down for a second drags the floor up onto its
 * own carrier: peak - floor collapses, the app decides nothing is transmitting
 * and refuses to lock the baseline while the user is still pressing the fob.
 * It also quietly shrinks every attenuation figure measured against it. */
#define FDY_FLOOR_QUIET_DB      3
#define FDY_FLOOR_CREEP_SAMPLES 250 // ~500 ms per dB at the 2 ms sample rate

/* One trace slot per this many samples.
 *
 * The ring is FDY_HISTORY_LEN (64) deep and was advanced once per 2 ms sample,
 * so the Leak Hunt "rolling trace of the sweep you just made" actually showed
 * 128 ms of history - about a tenth of a second, far less than one pass along
 * a seam, which made it useless for the job it exists to do. At 60 samples a
 * slot the ring spans 64 * 60 * 2 ms = ~7.7 s, which is a real sweep. */
#define FDY_TRACE_DECIMATE 60

/* Meter scale. -100 dBm reads empty (bare noise floor), -30 dBm pegs it - the
 * span a fob's carrier sweeps as it goes from "sealed in a good pouch" to
 * "pressed against the antenna". */
#define FDY_RSSI_MIN FDY_RSSI_FLOOR_DBM
#define FDY_RSSI_MAX (-30)

/* Common ISM bands a car key, garage/gate remote or alarm fob lives on. */
const FdyBand fdy_bands[FDY_BAND_COUNT] = {
    {.frequency = 315000000, .label = "315.00"},
    {.frequency = 433920000, .label = "433.92"},
    {.frequency = 868350000, .label = "868.35"},
    {.frequency = 915000000, .label = "915.00"},
};

struct FdySubGhz {
    FuriThread* thread;
    FuriMutex* mutex; // guards config + snapshot + flags
    ViewDispatcher* view_dispatcher;
    volatile bool running;

    uint32_t frequency; // requested tune
    bool reset_request;

    FdySubGhzSnapshot snapshot;
};

uint8_t fdy_subghz_normalize(int16_t rssi_dbm) {
    if(rssi_dbm <= FDY_RSSI_MIN) return 0;
    if(rssi_dbm >= FDY_RSSI_MAX) return 100;
    return (uint8_t)(((int32_t)(rssi_dbm - FDY_RSSI_MIN) * 100) / (FDY_RSSI_MAX - FDY_RSSI_MIN));
}

static int32_t fdy_subghz_thread(void* context) {
    FdySubGhz* s = context;

    subghz_devices_init();
    const SubGhzDevice* device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    subghz_devices_begin(device);
    subghz_devices_reset(device);
    subghz_devices_load_preset(device, FuriHalSubGhzPresetOok650Async, NULL);

    uint32_t current = 0; // force initial tune
    int16_t peak = FDY_RSSI_MIN;
    int16_t floor = FDY_RSSI_MIN;
    uint16_t quiet = 0; // consecutive quiet samples, paces the floor creep
    bool primed = false; // peak/floor seeded from a real sample yet?
    uint8_t trace_div = 0; // decimates the sample rate down to the trace rate

    /* Pace the loop in kernel ticks, never in furi_delay_us(): that call is a
     * non-yielding DWT busy-wait, so it would pin the core for the whole gap
     * between samples. The GUI service then stops draining its input queue and
     * the app looks like it is ignoring the buttons. Ticks block on the kernel,
     * so the UI runs in the gaps. */
    const uint32_t sample_ticks = MAX(furi_ms_to_ticks(FDY_SAMPLE_MS), 1UL);
    const uint32_t settle_ticks = MAX(furi_ms_to_ticks(FDY_SETTLE_MS), 1UL);

    furi_mutex_acquire(s->mutex, FuriWaitForever);
    s->snapshot.valid = true;
    s->snapshot.running = true;
    furi_mutex_release(s->mutex);

    while(s->running) {
        // pull requested config + flags
        furi_mutex_acquire(s->mutex, FuriWaitForever);
        uint32_t want = s->frequency;
        bool reset = s->reset_request;
        s->reset_request = false;
        furi_mutex_release(s->mutex);

        // retune only when the band changes
        if(want != current) {
            if(subghz_devices_is_frequency_valid(device, want)) {
                subghz_devices_idle(device);
                subghz_devices_set_frequency(device, want);
                subghz_devices_flush_rx(device);
                subghz_devices_set_rx(device);
                furi_delay_tick(settle_ticks);
                current = want;
                primed = false; // relearn both trackers on the new band
                quiet = 0;
            } else {
                current = want; // skip invalid band, keep last reading
            }
        }

        int16_t rssi = (int16_t)subghz_devices_get_rssi(device);

        /* Seed from the RADIO, not from FDY_RSSI_MIN.
         *
         * FDY_RSSI_MIN is the meter's display scale, and it has nothing to do
         * with what this room reads. Seeding the trackers from it broke the
         * "did anything actually transmit" gate in both directions: on a quiet
         * band the true ambient sits BELOW -100, so the floor snapped down
         * while the peak stayed pinned at the constant and the app reported a
         * permanent phantom carrier; on a noisy band the floor started 15 dB
         * too low and the quiet-gated creep could never recover it. */
        if(!primed) {
            peak = rssi;
            floor = rssi;
            quiet = 0;
            primed = true;
        }
        if(reset) peak = rssi; // a peak-hold restarts at the current level
        if(rssi > peak) peak = rssi;

        /* Snap down to a new quiet minimum at once; creep back up only while
         * the channel is quiet, so a carrier can never raise the floor onto
         * itself. See FDY_FLOOR_QUIET_DB above. */
        if(rssi < floor) {
            floor = rssi;
            quiet = 0;
        } else if(rssi - floor <= FDY_FLOOR_QUIET_DB) {
            if(++quiet >= FDY_FLOOR_CREEP_SAMPLES) {
                quiet = 0;
                if(floor < FDY_RSSI_MAX) floor++;
            }
        } else {
            quiet = 0; // something is transmitting: hold the floor where it is
        }

        furi_mutex_acquire(s->mutex, FuriWaitForever);
        FdySubGhzSnapshot* sn = &s->snapshot;
        sn->rssi = rssi;
        sn->peak = peak;
        sn->floor = floor;
        sn->frequency = current;
        sn->level = fdy_subghz_normalize(rssi);
        sn->peak_norm = fdy_subghz_normalize(peak);
        if(++trace_div >= FDY_TRACE_DECIMATE) {
            trace_div = 0;
            sn->history_head = (uint8_t)((sn->history_head + 1) % FDY_HISTORY_LEN);
            sn->history[sn->history_head] = sn->level;
        }
        furi_mutex_release(s->mutex);

        furi_delay_tick(sample_ticks);
    }

    subghz_devices_idle(device);
    subghz_devices_sleep(device);
    subghz_devices_end(device);
    subghz_devices_deinit();

    furi_mutex_acquire(s->mutex, FuriWaitForever);
    s->snapshot.running = false;
    furi_mutex_release(s->mutex);
    return 0;
}

FdySubGhz* fdy_subghz_alloc(ViewDispatcher* view_dispatcher) {
    FdySubGhz* s = malloc(sizeof(FdySubGhz));
    memset(s, 0, sizeof(FdySubGhz));
    s->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    s->view_dispatcher = view_dispatcher;
    s->frequency = fdy_bands[1].frequency; // 433.92 default
    s->snapshot.rssi = FDY_RSSI_MIN;
    s->snapshot.peak = FDY_RSSI_MIN;
    s->snapshot.floor = FDY_RSSI_MIN;
    s->snapshot.frequency = s->frequency;
    return s;
}

void fdy_subghz_free(FdySubGhz* s) {
    furi_assert(s);
    fdy_subghz_stop(s);
    furi_mutex_free(s->mutex);
    free(s);
}

void fdy_subghz_set_freq(FdySubGhz* s, uint32_t frequency) {
    furi_assert(s);
    furi_mutex_acquire(s->mutex, FuriWaitForever);
    s->frequency = frequency;
    furi_mutex_release(s->mutex);
}

void fdy_subghz_start(FdySubGhz* s) {
    furi_assert(s);
    if(s->running) return;

    furi_mutex_acquire(s->mutex, FuriWaitForever);
    s->reset_request = false;
    s->snapshot.history_head = 0;
    memset(s->snapshot.history, 0, sizeof(s->snapshot.history));
    furi_mutex_release(s->mutex);

    s->running = true;
    s->thread = furi_thread_alloc_ex("FaradaySubGhz", FDY_WORKER_STACK, fdy_subghz_thread, s);
    /* Below the UI. A sampling worker that outranks the GUI service delays
     * every redraw and every input event behind its own loop. */
    furi_thread_set_priority(s->thread, FuriThreadPriorityLow);
    furi_thread_start(s->thread);
}

void fdy_subghz_stop(FdySubGhz* s) {
    furi_assert(s);
    if(!s->running) return;
    s->running = false;
    furi_thread_join(s->thread);
    furi_thread_free(s->thread);
    s->thread = NULL;
}

bool fdy_subghz_is_running(FdySubGhz* s) {
    furi_assert(s);
    return s->running;
}

void fdy_subghz_reset_peak(FdySubGhz* s) {
    furi_assert(s);
    furi_mutex_acquire(s->mutex, FuriWaitForever);
    s->reset_request = true;
    furi_mutex_release(s->mutex);
}

void fdy_subghz_get(FdySubGhz* s, FdySubGhzSnapshot* out) {
    furi_assert(s);
    furi_assert(out);
    furi_mutex_acquire(s->mutex, FuriWaitForever);
    *out = s->snapshot;
    furi_mutex_release(s->mutex);
}
