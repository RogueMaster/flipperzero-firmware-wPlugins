#include "rf_scope.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_subghz.h>
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>
#include <string.h>

#define RF_FLOOR_DBM (-95) // maps to level 0
#define RF_CEIL_DBM  (-40) // maps to RF_SCOPE_MAX
#define RF_SETTLE_US 250u // PLL settle after parking on the frequency
#define RF_SAMPLE_US 350u // per-sample dwell
#define RF_WORKER_STACK (2 * 1024)

typedef struct {
    uint32_t hz;
    const char* label;
} RfFreq;

/* Common OOK/ASK bands: 433.92 first (garage/TPMS/remotes are the usual demo). */
static const RfFreq rf_freqs[RF_SCOPE_FREQ_COUNT] = {
    {433920000, "433.92"},
    {315000000, "315.00"},
    {868350000, "868.35"},
    {915000000, "915.00"},
};

const char* rf_scope_freq_label(uint8_t index) {
    return rf_freqs[index % RF_SCOPE_FREQ_COUNT].label;
}
uint32_t rf_scope_freq_hz(uint8_t index) {
    return rf_freqs[index % RF_SCOPE_FREQ_COUNT].hz;
}

struct RfScope {
    FuriThread* thread;
    FuriMutex* mutex;
    volatile bool running;
    uint8_t freq_index;
    RfSnapshot snap;
};

static uint8_t level_from_dbm(float dbm) {
    if(dbm <= RF_FLOOR_DBM) return 0;
    if(dbm >= RF_CEIL_DBM) return RF_SCOPE_MAX;
    return (uint8_t)((dbm - RF_FLOOR_DBM) * RF_SCOPE_MAX / (RF_CEIL_DBM - RF_FLOOR_DBM));
}

static int32_t rf_worker(void* context) {
    RfScope* s = context;

    uint32_t freq = rf_scope_freq_hz(s->freq_index);

    subghz_devices_init();
    const SubGhzDevice* device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);

    if(!device || !subghz_devices_is_frequency_valid(device, freq)) {
        furi_mutex_acquire(s->mutex, FuriWaitForever);
        s->snap.present = false;
        s->snap.freq_hz = freq;
        furi_mutex_release(s->mutex);
        while(s->running) furi_delay_ms(100);
        subghz_devices_deinit();
        return 0;
    }

    subghz_devices_begin(device);
    subghz_devices_reset(device);
    subghz_devices_load_preset(device, FuriHalSubGhzPresetOok650Async, NULL);
    subghz_devices_idle(device);
    subghz_devices_set_frequency(device, freq);
    subghz_devices_flush_rx(device);
    subghz_devices_set_rx(device);
    furi_delay_us(RF_SETTLE_US);

    /* worker-owned ring; published under the mutex each sample */
    uint8_t ring[RF_SCOPE_SAMPLES] = {0};
    uint16_t head = 0;
    uint32_t floor_acc = level_from_dbm(RF_FLOOR_DBM); // running noise floor *4

    furi_mutex_acquire(s->mutex, FuriWaitForever);
    s->snap.present = true;
    s->snap.freq_hz = freq;
    furi_mutex_release(s->mutex);

    while(s->running) {
        float dbm = subghz_devices_get_rssi(device);
        uint8_t lvl = level_from_dbm(dbm);

        ring[head] = lvl;
        head = (head + 1) % RF_SCOPE_SAMPLES;

        /* Track a slow noise floor and set the carrier threshold above it. */
        floor_acc = floor_acc - (floor_acc >> 5) + lvl; // EMA * 32
        uint8_t floor = (uint8_t)(floor_acc >> 5);
        uint8_t threshold = floor + 4;
        if(threshold > RF_SCOPE_MAX) threshold = RF_SCOPE_MAX;

        furi_mutex_acquire(s->mutex, FuriWaitForever);
        /* copy ring oldest-first so the view can draw left-to-right */
        for(int i = 0; i < RF_SCOPE_SAMPLES; i++) {
            s->snap.level[i] = ring[(head + i) % RF_SCOPE_SAMPLES];
        }
        s->snap.head = head;
        s->snap.rssi_dbm = dbm;
        s->snap.threshold = threshold;
        s->snap.running = true;
        furi_mutex_release(s->mutex);

        furi_delay_us(RF_SAMPLE_US);
    }

    subghz_devices_idle(device);
    subghz_devices_sleep(device);
    subghz_devices_end(device);
    subghz_devices_deinit();
    return 0;
}

/* ---------------------------------------------------------------- public */

RfScope* rf_scope_alloc(void) {
    RfScope* s = malloc(sizeof(RfScope));
    memset(s, 0, sizeof(RfScope));
    s->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    return s;
}

void rf_scope_free(RfScope* s) {
    furi_assert(s);
    rf_scope_stop(s);
    furi_mutex_free(s->mutex);
    free(s);
}

void rf_scope_set_freq_index(RfScope* s, uint8_t index) {
    furi_assert(s);
    s->freq_index = index % RF_SCOPE_FREQ_COUNT;
}

void rf_scope_start(RfScope* s) {
    furi_assert(s);
    if(s->running) return;
    memset(&s->snap, 0, sizeof(s->snap));
    s->running = true;
    s->thread = furi_thread_alloc_ex("RosettaRf", RF_WORKER_STACK, rf_worker, s);
    furi_thread_start(s->thread);
}

void rf_scope_stop(RfScope* s) {
    furi_assert(s);
    if(!s->running) return;
    s->running = false;
    if(s->thread) {
        furi_thread_join(s->thread);
        furi_thread_free(s->thread);
        s->thread = NULL;
    }
}

bool rf_scope_is_running(RfScope* s) {
    furi_assert(s);
    return s->running;
}

void rf_scope_get(RfScope* s, RfSnapshot* out) {
    furi_assert(s);
    furi_assert(out);
    furi_mutex_acquire(s->mutex, FuriWaitForever);
    *out = s->snap;
    furi_mutex_release(s->mutex);
}
