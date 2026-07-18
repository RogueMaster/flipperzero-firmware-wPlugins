#include "fdy_subghz.h"

#include <furi_hal.h>
#include <furi_hal_subghz.h> // FuriHalSubGhzPreset enum + valid-range helpers
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>

#define TAG "Faraday"

#define FDY_SETTLE_US 1500 // RSSI settle after retuning
#define FDY_SAMPLE_US 2000 // gap between RSSI reads (~500 Hz)
#define FDY_WORKER_STACK (2 * 1024)

/* Meter scale. -100 dBm reads empty (bare noise floor), -30 dBm pegs it - the
 * span a fob's carrier sweeps as it goes from "sealed in a good pouch" to
 * "pressed against the antenna". */
#define FDY_RSSI_MIN (-100)
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

        if(reset) {
            peak = FDY_RSSI_MIN;
        }

        // retune only when the band changes
        if(want != current) {
            if(subghz_devices_is_frequency_valid(device, want)) {
                subghz_devices_idle(device);
                subghz_devices_set_frequency(device, want);
                subghz_devices_flush_rx(device);
                subghz_devices_set_rx(device);
                furi_delay_us(FDY_SETTLE_US);
                current = want;
                floor = FDY_RSSI_MIN; // relearn the floor on the new band
            } else {
                current = want; // skip invalid band, keep last reading
            }
        }

        int16_t rssi = (int16_t)subghz_devices_get_rssi(device);
        if(rssi > peak) peak = rssi;

        // Track the noise floor: snap down fast to a new quiet minimum, drift
        // up slowly so a burst doesn't permanently raise it.
        if(rssi < floor)
            floor = rssi;
        else if((furi_get_tick() & 0x3F) == 0 && floor < FDY_RSSI_MAX)
            floor++;

        furi_mutex_acquire(s->mutex, FuriWaitForever);
        FdySubGhzSnapshot* sn = &s->snapshot;
        sn->rssi = rssi;
        sn->peak = peak;
        sn->floor = floor;
        sn->frequency = current;
        sn->level = fdy_subghz_normalize(rssi);
        sn->peak_norm = fdy_subghz_normalize(peak);
        sn->history_head = (uint8_t)((sn->history_head + 1) % FDY_HISTORY_LEN);
        sn->history[sn->history_head] = sn->level;
        furi_mutex_release(s->mutex);

        furi_delay_us(FDY_SAMPLE_US);
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
