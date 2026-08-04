#include "rc_radio.h"
#include "rc_parse.h"

#include <furi_hal_subghz.h> // FuriHalSubGhzPreset enum
#include <storage/storage.h> // EXT_PATH
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>
#include <lib/subghz/environment.h>
#include <lib/subghz/receiver.h>
#include <lib/subghz/subghz_worker.h>
#include <lib/subghz/subghz_protocol_registry.h>
#include <lib/subghz/protocols/base.h>

#include <string.h>

#define TAG "RollCall"

/* Where the firmware keeps the manufacture keys and rainbow tables. Loading
 * them is optional - rolling-vs-fixed never needs a key - but with them the
 * KeeLoq / Nice Flor-S / CAME Atomo decoders emit a much richer parcel, which
 * makes our per-press fingerprint sharper. */
#define RC_KEYSTORE_PATH      EXT_PATH("subghz/assets/keeloq_mfcodes")
#define RC_KEYSTORE_USER_PATH EXT_PATH("subghz/assets/keeloq_mfcodes_user")
#define RC_RT_NICE_FLOR_S     EXT_PATH("subghz/assets/nice_flor_s")
#define RC_RT_CAME_ATOMO      EXT_PATH("subghz/assets/came_atomo")
#define RC_RT_ALUTECH_AT_4N   EXT_PATH("subghz/assets/alutech_at_4n")

/* Band-hunt sweep timing. One band costs settle + samples, so a full sweep of
 * RC_BAND_COUNT bands lands well inside the ~1s a held fob keeps transmitting. */
#define RC_HUNT_SETTLE_MS  2
#define RC_HUNT_SAMPLES    8
#define RC_HUNT_SAMPLE_US  500

/* The common ISM bands a garage, gate, car or alarm fob is most likely to use.
 * RC_BAND_DEFAULT must stay pointing at 433.92. */
const RcBand rc_bands[RC_BAND_COUNT] = {
    {.frequency = 300000000, .label = "300.00"},
    {.frequency = 303875000, .label = "303.87"},
    {.frequency = 310000000, .label = "310.00"},
    {.frequency = 315000000, .label = "315.00"},
    {.frequency = 318000000, .label = "318.00"},
    {.frequency = 330000000, .label = "330.00"},
    {.frequency = 345000000, .label = "345.00"},
    {.frequency = 390000000, .label = "390.00"},
    {.frequency = 418000000, .label = "418.00"},
    {.frequency = 433920000, .label = "433.92"},
    {.frequency = 434420000, .label = "434.42"},
    {.frequency = 434775000, .label = "434.77"},
    {.frequency = 868350000, .label = "868.35"},
    {.frequency = 915000000, .label = "915.00"},
};

const RcMod rc_mods[RC_MOD_COUNT] = {
    {.label = "AM650", .preset = FuriHalSubGhzPresetOok650Async},
    {.label = "AM270", .preset = FuriHalSubGhzPresetOok270Async},
    {.label = "FM238", .preset = FuriHalSubGhzPreset2FSKDev238Async},
    {.label = "FM476", .preset = FuriHalSubGhzPreset2FSKDev476Async},
};

struct RcRadio {
    ViewDispatcher* view_dispatcher;
    uint32_t capture_event;

    SubGhzEnvironment* environment;
    SubGhzReceiver* receiver;
    SubGhzWorker* worker;
    const SubGhzDevice* device;

    FuriMutex* mutex; // guards the capture log + press bookkeeping
    FuriMutex* dev_mutex; // serialises SPI access to the CC1101
    volatile bool running;

    uint32_t frequency;
    uint8_t preset;
    uint32_t press_gap_ms;

    RcCapture captures[RC_MAX_CAPTURES];
    uint8_t count;

    /* press-collapsing bookkeeping */
    uint64_t last_fp;
    uint32_t last_tick;
    bool have_last;

    /* live diagnostics */
    volatile uint32_t edges; // raw demodulator transitions since start()

    /* band hunt */
    FuriThread* hunt_thread;
    volatile bool hunt_running;
    RcHuntBand hunt[RC_BAND_COUNT];
    uint32_t hunt_sweeps;
};

/* 64-bit FNV-1a over a decoded parcel string. Two presses of a fixed code hash
 * identically; two presses of a rolling code differ (the counter/hop changed). */
static uint64_t rc_fnv64(const char* s) {
    uint64_t h = 1469598103934665603ULL;
    while(*s) {
        h ^= (uint8_t)(*s++);
        h *= 1099511628211ULL;
    }
    return h;
}

static int8_t rc_dbm_clamp(float rssi) {
    if(rssi > 0.0f) return 0;
    if(rssi < -127.0f) return -127;
    return (int8_t)rssi;
}

static RcCodeClass rc_class_from_type(SubGhzProtocolType type) {
    switch(type) {
    case SubGhzProtocolTypeStatic:
        return RcCodeStatic;
    case SubGhzProtocolTypeDynamic:
        return RcCodeDynamic;
    default:
        return RcCodeUnknown;
    }
}

/* Read RSSI without colliding with whoever else is talking to the CC1101. */
static float rc_read_rssi(RcRadio* radio) {
    float rssi = -127.0f;
    furi_mutex_acquire(radio->dev_mutex, FuriWaitForever);
    if(radio->device) rssi = subghz_devices_get_rssi(radio->device);
    furi_mutex_release(radio->dev_mutex);
    return rssi;
}

/* Fired by the receiver on every successful decode (in the worker thread). */
static void rc_on_decode(SubGhzReceiver* receiver, SubGhzProtocolDecoderBase* decoder, void* ctx) {
    RcRadio* radio = ctx;
    const SubGhzProtocol* proto = decoder->protocol;
    if(!proto) {
        subghz_receiver_reset(receiver);
        return;
    }

    /* Build a fingerprint of THIS parcel. The textual dump contains the key,
     * serial and (for rolling codes) the counter/hop, so identical presses
     * hash the same and advancing presses hash differently. */
    FuriString* dump = furi_string_alloc();
    uint64_t fp;
    uint16_t bits = 0;
    if(subghz_protocol_decoder_base_get_string(decoder, dump) && furi_string_size(dump) > 0) {
        const char* text = furi_string_get_cstr(dump);
        fp = rc_fnv64(text);
        bits = rc_bits_from_dump(text);
    } else {
        /* Fallback: 8-bit rolling-parcel hash mixed with the bit length. */
        fp = ((uint64_t)subghz_protocol_decoder_base_get_hash_data(decoder) << 8);
    }
    furi_string_free(dump);

    /* Sample the carrier before we take the log lock - it is an SPI round trip. */
    int8_t rssi = rc_dbm_clamp(rc_read_rssi(radio));
    uint32_t now = furi_get_tick();

    furi_mutex_acquire(radio->mutex, FuriWaitForever);

    bool new_press = !radio->have_last || (fp != radio->last_fp) ||
                     ((now - radio->last_tick) > radio->press_gap_ms);

    if(new_press && radio->count < RC_MAX_CAPTURES) {
        RcCapture* c = &radio->captures[radio->count];
        strncpy(c->protocol, proto->name ? proto->name : "?", sizeof(c->protocol) - 1);
        c->protocol[sizeof(c->protocol) - 1] = '\0';
        c->cls = rc_class_from_type(proto->type);
        c->bits = bits;
        c->fingerprint = fp;
        c->tick = now;
        c->rssi = rssi;
        radio->count++;

        if(radio->view_dispatcher) {
            view_dispatcher_send_custom_event(radio->view_dispatcher, radio->capture_event);
        }
    }

    radio->last_fp = fp;
    radio->last_tick = now;
    radio->have_last = true;

    furi_mutex_release(radio->mutex);

    subghz_receiver_reset(receiver);
}

/* Worker pair callback. Counting every level transition on the way through to
 * the decoders is what powers the "the radio IS hearing something" indicator. */
static void rc_on_pair(void* ctx, bool level, uint32_t duration) {
    RcRadio* radio = ctx;
    radio->edges++;
    subghz_receiver_decode(radio->receiver, level, duration);
}

static void rc_on_overrun(void* ctx) {
    RcRadio* radio = ctx;
    subghz_receiver_reset(radio->receiver);
}

/* ------------------------------------------------------- device bring-up -- */

/* Power the CC1101 up and park it on `frequency` with `preset` loaded. */
static void rc_device_up(RcRadio* radio, uint32_t frequency, uint8_t preset) {
    furi_hal_power_suppress_charge_enter(); // the charger is a noise source

    subghz_devices_init();
    radio->device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    subghz_devices_begin(radio->device);
    subghz_devices_reset(radio->device);
    subghz_devices_load_preset(radio->device, preset, NULL);

    if(!subghz_devices_is_frequency_valid(radio->device, frequency)) {
        frequency = rc_bands[RC_BAND_DEFAULT].frequency;
    }
    subghz_devices_set_frequency(radio->device, frequency);
}

static void rc_device_down(RcRadio* radio) {
    furi_mutex_acquire(radio->dev_mutex, FuriWaitForever);
    subghz_devices_idle(radio->device);
    subghz_devices_sleep(radio->device);
    subghz_devices_end(radio->device);
    subghz_devices_deinit();
    radio->device = NULL;
    furi_mutex_release(radio->dev_mutex);

    furi_hal_power_suppress_charge_exit();
}

/* ------------------------------------------------------------ lifecycle --- */

RcRadio* rc_radio_alloc(ViewDispatcher* view_dispatcher, uint32_t capture_event) {
    RcRadio* radio = malloc(sizeof(RcRadio));
    memset(radio, 0, sizeof(RcRadio));

    radio->view_dispatcher = view_dispatcher;
    radio->capture_event = capture_event;
    radio->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    radio->dev_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    radio->frequency = rc_bands[RC_BAND_DEFAULT].frequency;
    radio->preset = FuriHalSubGhzPresetOok650Async;
    radio->press_gap_ms = 500;
    radio->running = false;

    /* Decoder stack. The registry gives us every built-in protocol; the
     * classification we care about (static vs dynamic) needs no keystore, but
     * loading one makes the decoded parcels - and so our fingerprints - richer. */
    radio->environment = subghz_environment_alloc();
    subghz_environment_set_protocol_registry(
        radio->environment, (void*)&subghz_protocol_registry);
    subghz_environment_set_nice_flor_s_rainbow_table_file_name(
        radio->environment, RC_RT_NICE_FLOR_S);
    subghz_environment_set_came_atomo_rainbow_table_file_name(
        radio->environment, RC_RT_CAME_ATOMO);
    subghz_environment_set_alutech_at_4n_rainbow_table_file_name(
        radio->environment, RC_RT_ALUTECH_AT_4N);
    if(!subghz_environment_load_keystore(radio->environment, RC_KEYSTORE_PATH)) {
        FURI_LOG_W(TAG, "no keystore at " RC_KEYSTORE_PATH ", decoding anyway");
    }
    subghz_environment_load_keystore(radio->environment, RC_KEYSTORE_USER_PATH);

    radio->receiver = subghz_receiver_alloc_init(radio->environment);
    subghz_receiver_set_filter(radio->receiver, SubGhzProtocolFlag_Decodable);
    subghz_receiver_set_rx_callback(radio->receiver, rc_on_decode, radio);

    radio->worker = subghz_worker_alloc();
    subghz_worker_set_overrun_callback(radio->worker, rc_on_overrun);
    subghz_worker_set_pair_callback(radio->worker, rc_on_pair);
    subghz_worker_set_context(radio->worker, radio);

    return radio;
}

void rc_radio_free(RcRadio* radio) {
    furi_assert(radio);
    rc_radio_hunt_stop(radio);
    rc_radio_stop(radio);
    subghz_receiver_free(radio->receiver);
    subghz_environment_free(radio->environment);
    subghz_worker_free(radio->worker);
    furi_mutex_free(radio->dev_mutex);
    furi_mutex_free(radio->mutex);
    free(radio);
}

void rc_radio_configure(RcRadio* radio, uint32_t frequency, uint8_t preset) {
    furi_assert(radio);
    furi_mutex_acquire(radio->mutex, FuriWaitForever);
    radio->frequency = frequency;
    radio->preset = preset;
    furi_mutex_release(radio->mutex);
}

void rc_radio_set_press_gap(RcRadio* radio, uint32_t gap_ms) {
    furi_assert(radio);
    furi_mutex_acquire(radio->mutex, FuriWaitForever);
    radio->press_gap_ms = gap_ms;
    furi_mutex_release(radio->mutex);
}

void rc_radio_start(RcRadio* radio) {
    furi_assert(radio);
    if(radio->running || radio->hunt_running) return;

    /* fresh capture log for this run */
    furi_mutex_acquire(radio->mutex, FuriWaitForever);
    radio->count = 0;
    radio->have_last = false;
    radio->last_fp = 0;
    radio->last_tick = 0;
    uint32_t freq = radio->frequency;
    uint8_t preset = radio->preset;
    furi_mutex_release(radio->mutex);

    radio->edges = 0;

    rc_device_up(radio, freq, preset);

    subghz_receiver_reset(radio->receiver);
    subghz_worker_start(radio->worker);
    subghz_devices_start_async_rx(
        radio->device, (void*)subghz_worker_rx_callback, radio->worker);

    radio->running = true;
}

void rc_radio_stop(RcRadio* radio) {
    furi_assert(radio);
    if(!radio->running) return;
    radio->running = false;

    /* The worker thread can still be inside a decode - and so inside an RSSI
     * read - right now, so tearing the RX down takes the same lock it does. */
    furi_mutex_acquire(radio->dev_mutex, FuriWaitForever);
    subghz_devices_stop_async_rx(radio->device);
    furi_mutex_release(radio->dev_mutex);

    subghz_worker_stop(radio->worker);

    rc_device_down(radio);
}

bool rc_radio_is_running(RcRadio* radio) {
    furi_assert(radio);
    return radio->running;
}

uint8_t rc_radio_count(RcRadio* radio) {
    furi_assert(radio);
    furi_mutex_acquire(radio->mutex, FuriWaitForever);
    uint8_t n = radio->count;
    furi_mutex_release(radio->mutex);
    return n;
}

uint8_t rc_radio_snapshot(RcRadio* radio, RcCapture* out, uint8_t max) {
    furi_assert(radio);
    furi_mutex_acquire(radio->mutex, FuriWaitForever);
    uint8_t n = radio->count < max ? radio->count : max;
    for(uint8_t i = 0; i < n; i++) out[i] = radio->captures[i];
    furi_mutex_release(radio->mutex);
    return n;
}

/* ----------------------------------------------------------- diagnostics -- */

float rc_radio_rssi(RcRadio* radio) {
    furi_assert(radio);
    if(!radio->running && !radio->hunt_running) return -127.0f;
    return rc_read_rssi(radio);
}

uint32_t rc_radio_edges(RcRadio* radio) {
    furi_assert(radio);
    return radio->edges;
}

/* -------------------------------------------------------------- band hunt -- */

static int32_t rc_hunt_thread(void* ctx) {
    RcRadio* radio = ctx;

    while(radio->hunt_running) {
        for(uint8_t i = 0; i < RC_BAND_COUNT && radio->hunt_running; i++) {
            furi_mutex_acquire(radio->dev_mutex, FuriWaitForever);
            if(!radio->device) {
                furi_mutex_release(radio->dev_mutex);
                break;
            }
            subghz_devices_idle(radio->device);
            subghz_devices_set_frequency(radio->device, rc_bands[i].frequency);
            subghz_devices_set_rx(radio->device);
            furi_mutex_release(radio->dev_mutex);

            furi_delay_ms(RC_HUNT_SETTLE_MS);

            /* Peak-hold across the dwell: a fob keying the carrier on and off
             * spends part of the window silent, so the max is what matters. */
            float peak = -127.0f;
            for(uint8_t s = 0; s < RC_HUNT_SAMPLES; s++) {
                float r = rc_read_rssi(radio);
                if(r > peak) peak = r;
                furi_delay_us(RC_HUNT_SAMPLE_US);
            }

            int8_t dbm = rc_dbm_clamp(peak);

            furi_mutex_acquire(radio->mutex, FuriWaitForever);
            RcHuntBand* b = &radio->hunt[i];
            if(!b->seen) {
                b->seen = true;
                b->floor_dbm = dbm;
                b->peak_dbm = dbm;
            } else {
                if(dbm > b->peak_dbm) b->peak_dbm = dbm;
                if(dbm < b->floor_dbm) b->floor_dbm = dbm;
            }
            b->last_dbm = dbm;
            furi_mutex_release(radio->mutex);
        }

        furi_mutex_acquire(radio->mutex, FuriWaitForever);
        radio->hunt_sweeps++;
        furi_mutex_release(radio->mutex);
    }

    return 0;
}

void rc_radio_hunt_start(RcRadio* radio) {
    furi_assert(radio);
    if(radio->hunt_running || radio->running) return;

    furi_mutex_acquire(radio->mutex, FuriWaitForever);
    memset(radio->hunt, 0, sizeof(radio->hunt));
    radio->hunt_sweeps = 0;
    furi_mutex_release(radio->mutex);

    /* Widest OOK filter for the sweep: we are measuring raw carrier power, not
     * demodulating, so a wide window catches fobs whose exact centre is off. */
    rc_device_up(radio, rc_bands[RC_BAND_DEFAULT].frequency, FuriHalSubGhzPresetOok650Async);
    subghz_devices_set_rx(radio->device);

    radio->hunt_running = true;
    radio->hunt_thread = furi_thread_alloc_ex("RcHunt", 1024, rc_hunt_thread, radio);
    furi_thread_start(radio->hunt_thread);
}

void rc_radio_hunt_stop(RcRadio* radio) {
    furi_assert(radio);
    if(!radio->hunt_running) return;
    radio->hunt_running = false;

    furi_thread_join(radio->hunt_thread);
    furi_thread_free(radio->hunt_thread);
    radio->hunt_thread = NULL;

    rc_device_down(radio);
}

bool rc_radio_hunt_is_running(RcRadio* radio) {
    furi_assert(radio);
    return radio->hunt_running;
}

uint8_t rc_radio_hunt_snapshot(RcRadio* radio, RcHuntBand* out, uint8_t max) {
    furi_assert(radio);
    furi_mutex_acquire(radio->mutex, FuriWaitForever);
    uint8_t n = RC_BAND_COUNT < max ? RC_BAND_COUNT : max;
    for(uint8_t i = 0; i < n; i++) out[i] = radio->hunt[i];
    furi_mutex_release(radio->mutex);
    return n;
}

uint32_t rc_radio_hunt_sweeps(RcRadio* radio) {
    furi_assert(radio);
    furi_mutex_acquire(radio->mutex, FuriWaitForever);
    uint32_t n = radio->hunt_sweeps;
    furi_mutex_release(radio->mutex);
    return n;
}

int8_t rc_radio_hunt_best(RcRadio* radio) {
    furi_assert(radio);
    int8_t best = -1;
    int16_t best_delta = RC_HUNT_MIN_DELTA_DB - 1;

    furi_mutex_acquire(radio->mutex, FuriWaitForever);
    for(uint8_t i = 0; i < RC_BAND_COUNT; i++) {
        const RcHuntBand* b = &radio->hunt[i];
        if(!b->seen) continue;
        int16_t delta = (int16_t)b->peak_dbm - (int16_t)b->floor_dbm;
        if(delta > best_delta) {
            best_delta = delta;
            best = (int8_t)i;
        }
    }
    furi_mutex_release(radio->mutex);

    return best;
}
