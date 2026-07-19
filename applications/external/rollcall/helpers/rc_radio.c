#include "rc_radio.h"

#include <furi_hal_subghz.h> // FuriHalSubGhzPreset enum
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>
#include <lib/subghz/environment.h>
#include <lib/subghz/receiver.h>
#include <lib/subghz/subghz_worker.h>
#include <lib/subghz/subghz_protocol_registry.h>
#include <lib/subghz/protocols/base.h>

#define TAG "RollCall"

/* A held button re-sends the SAME frame every few ms. We collapse those into a
 * single press, and only register a NEW press when the decoded parcel changes
 * OR enough quiet time has passed for a human to have released and re-pressed.
 * This is what lets us count presses honestly for BOTH fixed codes (identical
 * parcel every press) and rolling codes (a fresh parcel every press). */
#define RC_PRESS_GAP_MS 500

/* The common ISM bands a garage, gate, car or alarm fob is most likely to use. */
const RcBand rc_bands[RC_BAND_COUNT] = {
    {.frequency = 300000000, .label = "300.00"},
    {.frequency = 315000000, .label = "315.00"},
    {.frequency = 390000000, .label = "390.00"},
    {.frequency = 433920000, .label = "433.92"},
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

    SubGhzEnvironment* environment;
    SubGhzReceiver* receiver;
    SubGhzWorker* worker;
    const SubGhzDevice* device;

    FuriMutex* mutex; // guards the capture log + press bookkeeping
    volatile bool running;

    uint32_t frequency;
    uint8_t preset;

    RcCapture captures[RC_MAX_CAPTURES];
    uint8_t count;

    /* press-collapsing bookkeeping */
    uint64_t last_fp;
    uint32_t last_tick;
    bool have_last;
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
    if(subghz_protocol_decoder_base_get_string(decoder, dump) && furi_string_size(dump) > 0) {
        fp = rc_fnv64(furi_string_get_cstr(dump));
    } else {
        /* Fallback: 8-bit rolling-parcel hash mixed with the bit length. */
        fp = ((uint64_t)subghz_protocol_decoder_base_get_hash_data(decoder) << 8);
    }
    furi_string_free(dump);

    uint32_t now = furi_get_tick();

    furi_mutex_acquire(radio->mutex, FuriWaitForever);

    bool new_press = !radio->have_last || (fp != radio->last_fp) ||
                     ((now - radio->last_tick) > RC_PRESS_GAP_MS);

    if(new_press && radio->count < RC_MAX_CAPTURES) {
        RcCapture* c = &radio->captures[radio->count];
        strncpy(c->protocol, proto->name ? proto->name : "?", sizeof(c->protocol) - 1);
        c->protocol[sizeof(c->protocol) - 1] = '\0';
        c->cls = rc_class_from_type(proto->type);
        c->bits = 0;
        c->fingerprint = fp;
        c->tick = now;
        radio->count++;

        if(radio->view_dispatcher) {
            view_dispatcher_send_custom_event(radio->view_dispatcher, RC_EVENT_CAPTURE);
        }
    }

    radio->last_fp = fp;
    radio->last_tick = now;
    radio->have_last = true;

    furi_mutex_release(radio->mutex);

    subghz_receiver_reset(receiver);
}

RcRadio* rc_radio_alloc(ViewDispatcher* view_dispatcher) {
    RcRadio* radio = malloc(sizeof(RcRadio));
    memset(radio, 0, sizeof(RcRadio));

    radio->view_dispatcher = view_dispatcher;
    radio->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    radio->frequency = 433920000;
    radio->preset = FuriHalSubGhzPresetOok650Async;
    radio->running = false;

    /* Decoder stack. The registry gives us every built-in protocol; the
     * classification we care about (static vs dynamic) needs no keystore. */
    radio->environment = subghz_environment_alloc();
    subghz_environment_set_protocol_registry(radio->environment, (void*)&subghz_protocol_registry);

    radio->receiver = subghz_receiver_alloc_init(radio->environment);
    subghz_receiver_set_filter(radio->receiver, SubGhzProtocolFlag_Decodable);
    subghz_receiver_set_rx_callback(radio->receiver, rc_on_decode, radio);

    radio->worker = subghz_worker_alloc();
    subghz_worker_set_overrun_callback(
        radio->worker, (SubGhzWorkerOverrunCallback)subghz_receiver_reset);
    subghz_worker_set_pair_callback(
        radio->worker, (SubGhzWorkerPairCallback)subghz_receiver_decode);
    subghz_worker_set_context(radio->worker, radio->receiver);

    return radio;
}

void rc_radio_free(RcRadio* radio) {
    furi_assert(radio);
    rc_radio_stop(radio);
    subghz_receiver_free(radio->receiver);
    subghz_environment_free(radio->environment);
    subghz_worker_free(radio->worker);
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

void rc_radio_start(RcRadio* radio) {
    furi_assert(radio);
    if(radio->running) return;

    /* fresh capture log for this run */
    furi_mutex_acquire(radio->mutex, FuriWaitForever);
    radio->count = 0;
    radio->have_last = false;
    radio->last_fp = 0;
    radio->last_tick = 0;
    uint32_t freq = radio->frequency;
    uint8_t preset = radio->preset;
    furi_mutex_release(radio->mutex);

    subghz_devices_init();
    radio->device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    subghz_devices_begin(radio->device);
    subghz_devices_reset(radio->device);
    subghz_devices_load_preset(radio->device, preset, NULL);

    if(!subghz_devices_is_frequency_valid(radio->device, freq)) {
        freq = 433920000;
    }
    subghz_devices_set_frequency(radio->device, freq);

    subghz_receiver_reset(radio->receiver);
    subghz_worker_start(radio->worker);
    subghz_devices_start_async_rx(radio->device, (void*)subghz_worker_rx_callback, radio->worker);

    radio->running = true;
}

void rc_radio_stop(RcRadio* radio) {
    furi_assert(radio);
    if(!radio->running) return;
    radio->running = false;

    subghz_devices_stop_async_rx(radio->device);
    subghz_worker_stop(radio->worker);

    subghz_devices_idle(radio->device);
    subghz_devices_sleep(radio->device);
    subghz_devices_end(radio->device);
    subghz_devices_deinit();
    radio->device = NULL;
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
    for(uint8_t i = 0; i < n; i++)
        out[i] = radio->captures[i];
    furi_mutex_release(radio->mutex);
    return n;
}
