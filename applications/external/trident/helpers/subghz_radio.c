#include "subghz_radio.h"

#include <furi_hal.h>
#include <furi_hal_subghz.h>
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>
#include <string.h>
#include <stdio.h>

// The cc1101_ext driver (Unleashed/RogueMaster/Momentum) registers under this
// name; declared literally so we don't depend on the applications/ include path.
#define TRIDENT_CC1101_EXT_NAME "cc1101_ext"

#define SUBGHZ_BINS         60 // frequency steps across the band (sweep)
#define SUBGHZ_SETTLE_US    700 // PLL settle before reading RSSI
#define SUBGHZ_CAMP_US      20000 // sample gap while camped (~50 Hz)
#define SUBGHZ_FLOOR_DBM    (-100) // maps to level 0
#define SUBGHZ_CEIL_DBM     (-30) // maps to level 100
#define SUBGHZ_WORKER_STACK (2 * 1024)

const SubghzBand trident_subghz_bands[TRIDENT_SUBGHZ_BAND_COUNT] = {
    {.lo_hz = 300000000,
     .hi_hz = 348000000,
     .label = "300-348",
     .lo_label = "300",
     .hi_label = "348"},
    {.lo_hz = 387000000,
     .hi_hz = 464000000,
     .label = "387-464",
     .lo_label = "387",
     .hi_label = "464"},
    {.lo_hz = 779000000,
     .hi_hz = 928000000,
     .label = "779-928",
     .lo_label = "779",
     .hi_label = "928"},
};

const SubghzPreset trident_subghz_presets[TRIDENT_SUBGHZ_PRESET_COUNT] = {
    {.hz = 315000000, .label = "315.00"},
    {.hz = 390000000, .label = "390.00"},
    {.hz = 418000000, .label = "418.00"},
    {.hz = 433920000, .label = "433.92"},
    {.hz = 868350000, .label = "868.35"},
    {.hz = 915000000, .label = "915.00"},
};

const char* trident_subghz_band_label(uint8_t index) {
    return trident_subghz_bands[index % TRIDENT_SUBGHZ_BAND_COUNT].label;
}

struct SubghzRadio {
    FuriThread* thread;
    FuriMutex* mutex; // guards snap + meter + flags
    volatile bool running;
    volatile bool reset_req;
    volatile uint8_t band; // active band index (sweep)
    volatile uint32_t camp_freq; // camped frequency (camp)
    uint8_t mode; // SubghzMode
    bool want_external; // latched at start

    uint8_t hold[SUBGHZ_BINS]; // sweep max-hold, worker-owned
    int16_t dbm[SUBGHZ_BINS]; // sweep last dBm, worker-owned
    uint8_t camp_peak; // camp peak-hold level
    SpectrumSnapshot snap;
    MeterSnapshot meter;
};

static uint8_t subghz_level_from_dbm(int16_t dbm) {
    if(dbm <= SUBGHZ_FLOOR_DBM) return 0;
    if(dbm >= SUBGHZ_CEIL_DBM) return 100;
    return (uint8_t)(((int32_t)dbm - SUBGHZ_FLOOR_DBM) * 100 /
                     (SUBGHZ_CEIL_DBM - SUBGHZ_FLOOR_DBM));
}

/* ---------------- sweep publish ---------------- */

static void subghz_publish_sweep(SubghzRadio* radio, const char* title, bool present) {
    SpectrumSnapshot s;
    memset(&s, 0, sizeof(s));
    s.running = radio->running;
    s.present = present;
    s.count = SUBGHZ_BINS;
    strncpy(s.title, title, sizeof(s.title) - 1);
    strncpy(s.unit, "dBm", sizeof(s.unit) - 1);

    uint8_t b = radio->band % TRIDENT_SUBGHZ_BAND_COUNT;
    const SubghzBand* band = &trident_subghz_bands[b];
    strncpy(s.lo_label, band->lo_label, sizeof(s.lo_label) - 1);
    strncpy(s.hi_label, band->hi_label, sizeof(s.hi_label) - 1);

    int peak_bin = -1;
    uint8_t peak_lvl = 0;
    for(int i = 0; i < SUBGHZ_BINS; i++) {
        s.level[i] = radio->hold[i];
        if(radio->hold[i] > peak_lvl) {
            peak_lvl = radio->hold[i];
            peak_bin = i;
        }
    }
    s.peak_bin = peak_bin;
    if(peak_bin >= 0 && peak_lvl > 0) {
        uint32_t span = band->hi_hz - band->lo_hz;
        uint32_t f = band->lo_hz + (uint32_t)((uint64_t)span * peak_bin / (SUBGHZ_BINS - 1));
        uint32_t mhz = (f / 1000000u) % 1000u;
        uint32_t tenth = (f % 1000000u) / 100000u;
        int shown = radio->dbm[peak_bin];
        if(shown < -199) shown = -199;
        if(shown > 99) shown = 99;
        s.peak_value = radio->dbm[peak_bin];
        snprintf(
            s.peak_label,
            sizeof(s.peak_label),
            "%lu.%luM %ddBm",
            (unsigned long)mhz,
            (unsigned long)tenth,
            shown);
    } else {
        strncpy(s.peak_label, "scanning...", sizeof(s.peak_label) - 1);
    }

    furi_mutex_acquire(radio->mutex, FuriWaitForever);
    uint32_t sweeps = radio->snap.sweeps;
    radio->snap = s;
    radio->snap.sweeps = sweeps;
    furi_mutex_release(radio->mutex);
}

/* ---------------- camp publish ---------------- */

static void subghz_publish_camp(SubghzRadio* radio, const char* title, uint32_t f, int16_t dbm) {
    uint8_t lvl = subghz_level_from_dbm(dbm);
    uint8_t peak = radio->camp_peak;
    peak = (uint8_t)(peak - (peak >> 4)); // slow decay
    if(lvl > peak) peak = lvl;
    radio->camp_peak = peak;

    MeterSnapshot m;
    memset(&m, 0, sizeof(m));
    m.running = radio->running;
    m.present = true;
    strncpy(m.title, title, sizeof(m.title) - 1);
    m.level = lvl;
    m.peak = peak;
    int shown = dbm;
    if(shown < -199) shown = -199;
    if(shown > 99) shown = 99;
    snprintf(m.value, sizeof(m.value), "%d", shown);
    strncpy(m.unit, "dBm", sizeof(m.unit) - 1);
    uint32_t mhz = (f / 1000000u) % 1000u;
    uint32_t hun = (f % 1000000u) / 10000u; // 2 decimals
    snprintf(m.sub, sizeof(m.sub), "%lu.%02lu MHz", (unsigned long)mhz, (unsigned long)hun);
    strncpy(m.foot, "<>tune ^vstep OK0", sizeof(m.foot) - 1);

    furi_mutex_acquire(radio->mutex, FuriWaitForever);
    radio->meter = m;
    furi_mutex_release(radio->mutex);
}

/* ---------------- worker ---------------- */

static int32_t subghz_worker(void* context) {
    SubghzRadio* radio = context;

    subghz_devices_init();
    const SubGhzDevice* device = NULL;
    bool external = false;
    if(radio->want_external) {
        device = subghz_devices_get_by_name(TRIDENT_CC1101_EXT_NAME);
        external = (device != NULL);
    }
    if(!device) {
        device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
        external = false;
    }

    char title[20];
    if(radio->mode == SubghzModeCamp) {
        snprintf(title, sizeof(title), "CC1101 Finder");
    } else {
        snprintf(title, sizeof(title), "CC1101 %s", external ? "ext" : "int");
    }

    if(!device) {
        furi_mutex_acquire(radio->mutex, FuriWaitForever);
        memset(&radio->snap, 0, sizeof(radio->snap));
        memset(&radio->meter, 0, sizeof(radio->meter));
        radio->snap.present = false;
        radio->meter.present = false;
        strncpy(radio->snap.title, title, sizeof(radio->snap.title) - 1);
        strncpy(radio->meter.title, title, sizeof(radio->meter.title) - 1);
        furi_mutex_release(radio->mutex);
        while(radio->running)
            furi_delay_ms(100);
        subghz_devices_deinit();
        return 0;
    }

    subghz_devices_begin(device);
    subghz_devices_reset(device);
    subghz_devices_load_preset(device, FuriHalSubGhzPresetOok650Async, NULL);

    if(radio->mode == SubghzModeCamp) {
        uint32_t current = 0;
        while(radio->running) {
            if(radio->reset_req) {
                radio->reset_req = false;
                radio->camp_peak = 0;
            }
            uint32_t f = radio->camp_freq;
            if(f != current) {
                if(subghz_devices_is_frequency_valid(device, f)) {
                    subghz_devices_idle(device);
                    subghz_devices_set_frequency(device, f);
                    subghz_devices_flush_rx(device);
                    subghz_devices_set_rx(device);
                    current = f;
                    radio->camp_peak = 0;
                    furi_delay_us(SUBGHZ_SETTLE_US);
                }
            }
            int16_t dbm = (int16_t)subghz_devices_get_rssi(device);
            subghz_publish_camp(radio, title, current, dbm);
            furi_delay_us(SUBGHZ_CAMP_US);
        }
    } else {
        while(radio->running) {
            if(radio->reset_req) {
                radio->reset_req = false;
                memset(radio->hold, 0, sizeof(radio->hold));
            }
            uint8_t b = radio->band % TRIDENT_SUBGHZ_BAND_COUNT;
            const SubghzBand* band = &trident_subghz_bands[b];
            uint32_t span = band->hi_hz - band->lo_hz;

            for(int i = 0; i < SUBGHZ_BINS && radio->running; i++) {
                uint32_t f = band->lo_hz + (uint32_t)((uint64_t)span * i / (SUBGHZ_BINS - 1));
                int16_t dbm = SUBGHZ_FLOOR_DBM;
                if(subghz_devices_is_frequency_valid(device, f)) {
                    subghz_devices_idle(device);
                    subghz_devices_set_frequency(device, f);
                    subghz_devices_flush_rx(device);
                    subghz_devices_set_rx(device);
                    furi_delay_us(SUBGHZ_SETTLE_US);
                    dbm = (int16_t)subghz_devices_get_rssi(device);
                }
                radio->dbm[i] = dbm;
                uint8_t lvl = subghz_level_from_dbm(dbm);
                uint8_t held = radio->hold[i];
                held = (uint8_t)(held - (held >> 3));
                radio->hold[i] = (lvl > held) ? lvl : held;
            }

            furi_mutex_acquire(radio->mutex, FuriWaitForever);
            radio->snap.sweeps++;
            furi_mutex_release(radio->mutex);
            subghz_publish_sweep(radio, title, true);
        }
    }

    subghz_devices_idle(device);
    subghz_devices_sleep(device);
    subghz_devices_end(device);
    subghz_devices_deinit();
    return 0;
}

/* ---------------- public API ---------------- */

SubghzRadio* subghz_radio_alloc(void) {
    SubghzRadio* radio = malloc(sizeof(SubghzRadio));
    memset(radio, 0, sizeof(SubghzRadio));
    radio->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    radio->camp_freq = 433920000;
    return radio;
}

void subghz_radio_free(SubghzRadio* radio) {
    furi_assert(radio);
    subghz_radio_stop(radio);
    furi_mutex_free(radio->mutex);
    free(radio);
}

void subghz_radio_configure(SubghzRadio* radio, uint8_t band_index, bool external) {
    furi_assert(radio);
    radio->band = band_index % TRIDENT_SUBGHZ_BAND_COUNT;
    radio->want_external = external;
}

void subghz_radio_set_mode(SubghzRadio* radio, SubghzMode mode) {
    furi_assert(radio);
    radio->mode = (uint8_t)mode;
}

void subghz_radio_set_band(SubghzRadio* radio, uint8_t band_index) {
    furi_assert(radio);
    radio->band = band_index % TRIDENT_SUBGHZ_BAND_COUNT;
    radio->reset_req = true;
}

void subghz_radio_set_camp_freq(SubghzRadio* radio, uint32_t hz) {
    furi_assert(radio);
    radio->camp_freq = hz;
}

uint32_t subghz_radio_get_camp_freq(SubghzRadio* radio) {
    furi_assert(radio);
    return radio->camp_freq;
}

void subghz_radio_start(SubghzRadio* radio) {
    furi_assert(radio);
    if(radio->running) return;
    memset(radio->hold, 0, sizeof(radio->hold));
    radio->camp_peak = 0;
    memset(&radio->snap, 0, sizeof(radio->snap));
    memset(&radio->meter, 0, sizeof(radio->meter));
    radio->reset_req = false;
    radio->running = true;
    radio->thread =
        furi_thread_alloc_ex("TridentSubghz", SUBGHZ_WORKER_STACK, subghz_worker, radio);
    furi_thread_start(radio->thread);
}

void subghz_radio_stop(SubghzRadio* radio) {
    furi_assert(radio);
    if(!radio->running) return;
    radio->running = false;
    if(radio->thread) {
        furi_thread_join(radio->thread);
        furi_thread_free(radio->thread);
        radio->thread = NULL;
    }
}

bool subghz_radio_is_running(SubghzRadio* radio) {
    furi_assert(radio);
    return radio->running;
}

void subghz_radio_reset(SubghzRadio* radio) {
    furi_assert(radio);
    radio->reset_req = true;
}

void subghz_radio_get_snapshot(SubghzRadio* radio, SpectrumSnapshot* out) {
    furi_assert(radio);
    furi_assert(out);
    furi_mutex_acquire(radio->mutex, FuriWaitForever);
    *out = radio->snap;
    furi_mutex_release(radio->mutex);
}

void subghz_radio_get_meter(SubghzRadio* radio, MeterSnapshot* out) {
    furi_assert(radio);
    furi_assert(out);
    furi_mutex_acquire(radio->mutex, FuriWaitForever);
    *out = radio->meter;
    furi_mutex_release(radio->mutex);
}
