#include "cerberus_subghz.h"

#include <furi_hal.h>
#include <furi_hal_subghz.h> // FuriHalSubGhzPreset enum + valid-range helpers
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>

#define TAG "Cerberus"

#define CERBERUS_DWELL_MS 140 // time camped on a band before hopping
#define CERBERUS_SETTLE_US 1500 // RSSI settle time after retuning
#define CERBERUS_SAMPLE_US 2000 // gap between RSSI reads (~500 Hz)
#define CERBERUS_WORKER_STACK (2 * 1024)

// The three heads. 433.92 / 868.35 / 915.00 MHz - the common ISM bands a
// garage, car, alarm or sensor in your space is most likely to live on.
const CerberusBand cerberus_bands[CERBERUS_BAND_COUNT] = {
    {.frequency = 433920000, .mhz = 433, .label = "433"},
    {.frequency = 868350000, .mhz = 868, .label = "868"},
    {.frequency = 915000000, .mhz = 915, .label = "915"},
};

struct CerberusSubGhz {
    FuriThread* thread;
    FuriMutex* mutex; // guards config + snapshot + flags
    ViewDispatcher* view_dispatcher; // for posting alert events
    volatile bool running;

    CerberusWorkerConfig config;
    bool reset_request;

    CerberusDetector det[CERBERUS_BAND_COUNT];
    CerberusSnapshot snapshot;
};

static uint8_t cerberus_next_band(uint8_t band) {
    return (uint8_t)((band + 1) % CERBERUS_BAND_COUNT);
}

static void cerberus_tune(const SubGhzDevice* device, uint8_t band) {
    subghz_devices_idle(device);
    subghz_devices_set_frequency(device, cerberus_bands[band].frequency);
    subghz_devices_flush_rx(device);
    subghz_devices_set_rx(device);
    furi_delay_us(CERBERUS_SETTLE_US);
}

static int32_t cerberus_subghz_thread(void* context) {
    CerberusSubGhz* worker = context;

    // --- Bring the internal CC1101 up in a wide OOK receive mode ---
    subghz_devices_init();
    const SubGhzDevice* device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    subghz_devices_begin(device);
    subghz_devices_reset(device);
    subghz_devices_load_preset(device, FuriHalSubGhzPresetOok650Async, NULL);

    const uint32_t start_tick = furi_get_tick();
    uint8_t current_band = 0xFF; // force an initial tune
    uint8_t active = 0;
    uint32_t last_hop = start_tick;

    CerberusDetectorParams params;
    cerberus_detector_set_sensitivity(&params, 1);

    while(worker->running) {
        // --- Pull the latest config / flags under the lock ---
        furi_mutex_acquire(worker->mutex, FuriWaitForever);
        CerberusWorkerConfig cfg = worker->config;
        bool reset = worker->reset_request;
        worker->reset_request = false;
        furi_mutex_release(worker->mutex);

        if(reset) {
            for(size_t i = 0; i < CERBERUS_BAND_COUNT; i++) {
                cerberus_detector_reset_floor(&worker->det[i]);
            }
        }
        cerberus_detector_set_sensitivity(&params, cfg.sensitivity);

        const bool hop = (cfg.scan_mode == CerberusScanModeHop);
        if(!hop) {
            active = (uint8_t)(cfg.scan_mode - CerberusScanModeBand0);
            if(active >= CERBERUS_BAND_COUNT) active = 0;
        }

        // --- Retune only when the target band changes ---
        if(active != current_band) {
            if(subghz_devices_is_frequency_valid(device, cerberus_bands[active].frequency)) {
                cerberus_tune(device, active);
            }
            current_band = active;
        }

        // --- Sample + detect ---
        uint32_t now = furi_get_tick();
        float rssi = subghz_devices_get_rssi(device);

        CerberusThreat raised = cerberus_detector_feed(
            &worker->det[active],
            &params,
            rssi,
            now,
            cfg.detect_jam,
            cfg.detect_flood,
            cfg.detect_replay);

        // --- Publish snapshot ---
        furi_mutex_acquire(worker->mutex, FuriWaitForever);
        worker->snapshot.band[active].rssi = (int16_t)rssi;
        for(size_t i = 0; i < CERBERUS_BAND_COUNT; i++) {
            worker->snapshot.band[i].floor = (int16_t)worker->det[i].floor;
            worker->snapshot.band[i].peak = (int16_t)worker->det[i].peak;
            worker->snapshot.band[i].threshold =
                (int16_t)(worker->det[i].floor + params.margin_db);
            worker->snapshot.band[i].bursts = worker->det[i].bursts_per_sec;
            worker->snapshot.band[i].threat = worker->det[i].threat;
        }
        worker->snapshot.active_band = active;
        worker->snapshot.hop = hop;
        worker->snapshot.running = true;
        worker->snapshot.uptime_s = (now - start_tick) / 1000;
        furi_mutex_release(worker->mutex);

        // --- Raise alert to the GUI thread ---
        if(raised != CerberusThreatNone && worker->view_dispatcher) {
            view_dispatcher_send_custom_event(
                worker->view_dispatcher, CERBERUS_ALERT_EVENT(raised, active));
        }

        // --- Hop if it's time ---
        if(hop && (now - last_hop) >= CERBERUS_DWELL_MS) {
            active = cerberus_next_band(active);
            last_hop = now;
        }

        furi_delay_us(CERBERUS_SAMPLE_US);
    }

    subghz_devices_idle(device);
    subghz_devices_sleep(device);
    subghz_devices_end(device);
    subghz_devices_deinit();
    return 0;
}

CerberusSubGhz* cerberus_subghz_alloc(ViewDispatcher* view_dispatcher) {
    CerberusSubGhz* worker = malloc(sizeof(CerberusSubGhz));
    worker->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    worker->view_dispatcher = view_dispatcher;
    worker->thread = NULL;
    worker->running = false;
    worker->reset_request = false;

    // sensible defaults until set_config is called
    worker->config.scan_mode = CerberusScanModeHop;
    worker->config.sensitivity = 1;
    worker->config.detect_jam = true;
    worker->config.detect_flood = true;
    worker->config.detect_replay = true;

    memset(&worker->snapshot, 0, sizeof(worker->snapshot));
    for(size_t i = 0; i < CERBERUS_BAND_COUNT; i++) {
        cerberus_detector_init(&worker->det[i]);
        worker->snapshot.band[i].rssi = -120;
        worker->snapshot.band[i].floor = -96;
        worker->snapshot.band[i].peak = -120;
    }
    return worker;
}

void cerberus_subghz_free(CerberusSubGhz* worker) {
    furi_assert(worker);
    cerberus_subghz_stop(worker);
    furi_mutex_free(worker->mutex);
    free(worker);
}

void cerberus_subghz_set_config(CerberusSubGhz* worker, const CerberusWorkerConfig* config) {
    furi_assert(worker);
    furi_mutex_acquire(worker->mutex, FuriWaitForever);
    worker->config = *config;
    furi_mutex_release(worker->mutex);
}

void cerberus_subghz_start(CerberusSubGhz* worker) {
    furi_assert(worker);
    if(worker->running) return;
    worker->running = true;
    worker->thread = furi_thread_alloc_ex(
        "CerberusWorker", CERBERUS_WORKER_STACK, cerberus_subghz_thread, worker);
    furi_thread_start(worker->thread);
}

void cerberus_subghz_stop(CerberusSubGhz* worker) {
    furi_assert(worker);
    if(!worker->running) return;
    worker->running = false;
    furi_thread_join(worker->thread);
    furi_thread_free(worker->thread);
    worker->thread = NULL;

    furi_mutex_acquire(worker->mutex, FuriWaitForever);
    worker->snapshot.running = false;
    furi_mutex_release(worker->mutex);
}

bool cerberus_subghz_is_running(CerberusSubGhz* worker) {
    furi_assert(worker);
    return worker->running;
}

void cerberus_subghz_reset_floor(CerberusSubGhz* worker) {
    furi_assert(worker);
    furi_mutex_acquire(worker->mutex, FuriWaitForever);
    worker->reset_request = true;
    furi_mutex_release(worker->mutex);
}

void cerberus_subghz_get_snapshot(CerberusSubGhz* worker, CerberusSnapshot* out) {
    furi_assert(worker);
    furi_mutex_acquire(worker->mutex, FuriWaitForever);
    *out = worker->snapshot;
    furi_mutex_release(worker->mutex);
}
