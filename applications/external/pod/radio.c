#include "radio.h"

#include <furi.h>
#include <furi_hal_random.h>
#include <lib/subghz/subghz_tx_rx_worker.h>
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>
#include <string.h>

#define POD_FREQUENCY   433920000UL // 433.92 MHz ISM, region-legal
#define POD_SEND_REPEAT 3

struct PodRadio {
    SubGhzTxRxWorker* worker;
    const SubGhzDevice* device;
    bool running;
    PodMsgCallback rx_cb;
    void* rx_ctx;
    PodWire wire; // framing, CRC and dedup (pod_wire.c, host-tested)
};

// --- lifecycle ---

PodRadio* pod_radio_alloc(void) {
    PodRadio* radio = malloc(sizeof(PodRadio));
    radio->worker = subghz_tx_rx_worker_alloc();
    radio->device = NULL;
    radio->running = false;
    radio->rx_cb = NULL;
    radio->rx_ctx = NULL;
    pod_wire_reset(&radio->wire);
    return radio;
}

void pod_radio_free(PodRadio* radio) {
    if(radio->running) pod_radio_stop(radio);
    subghz_tx_rx_worker_free(radio->worker);
    free(radio);
}

void pod_radio_set_rx_callback(PodRadio* radio, PodMsgCallback cb, void* context) {
    radio->rx_cb = cb;
    radio->rx_ctx = context;
}

// --- receive ---

static void pod_radio_have_read(void* context) {
    PodRadio* radio = context;
    size_t avail = subghz_tx_rx_worker_available(radio->worker);
    while(avail > 0) {
        uint8_t tmp[64];
        size_t n = subghz_tx_rx_worker_read(radio->worker, tmp, MIN(avail, sizeof(tmp)));
        if(n == 0) break;
        pod_wire_feed(&radio->wire, tmp, n, radio->rx_cb, radio->rx_ctx);
        avail = subghz_tx_rx_worker_available(radio->worker);
    }
}

bool pod_radio_start(PodRadio* radio) {
    if(radio->running) return true;
    subghz_devices_init();
    radio->device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    if(!radio->device) {
        subghz_devices_deinit();
        return false;
    }
    if(!subghz_tx_rx_worker_start(radio->worker, radio->device, POD_FREQUENCY)) {
        subghz_devices_deinit();
        radio->device = NULL;
        return false;
    }
    pod_wire_reset(&radio->wire);
    subghz_tx_rx_worker_set_callback_have_read(radio->worker, pod_radio_have_read, radio);
    radio->running = true;
    return true;
}

void pod_radio_stop(PodRadio* radio) {
    if(!radio->running) return;
    if(subghz_tx_rx_worker_is_running(radio->worker)) {
        subghz_tx_rx_worker_stop(radio->worker);
    }
    subghz_devices_deinit();
    radio->device = NULL;
    radio->running = false;
}

bool pod_radio_is_running(PodRadio* radio) {
    return radio->running;
}

// --- send ---

static bool pod_radio_emit(PodRadio* radio, uint8_t* buf, size_t len, int repeat) {
    if(!radio->running) return false;
    bool ok = false;
    for(int r = 0; r < repeat; r++) {
        ok = subghz_tx_rx_worker_write(radio->worker, buf, len) || ok;
    }
    return ok;
}

bool pod_radio_beacon(PodRadio* radio, const PodProfile* profile, uint8_t dolphin_level) {
    uint8_t buf[POD_FRAME_MAX];
    uint16_t nonce = (uint16_t)(furi_hal_random_get() & 0xFFFF);
    size_t n = pod_wire_beacon(
        buf, sizeof(buf), profile->profile_id, profile->icon, dolphin_level, nonce, profile->name);
    return n && pod_radio_emit(radio, buf, n, 2);
}

static bool pod_radio_send_named(
    PodRadio* radio,
    uint8_t type,
    uint32_t src,
    const char* name,
    uint8_t level,
    uint32_t dst,
    uint16_t battle_id) {
    uint8_t buf[POD_FRAME_MAX];
    size_t n = pod_wire_named(buf, sizeof(buf), type, src, name, level, dst, battle_id);
    return n && pod_radio_emit(radio, buf, n, POD_SEND_REPEAT);
}

bool pod_radio_send_challenge(
    PodRadio* radio,
    uint32_t src,
    const char* name,
    uint8_t level,
    uint32_t dst,
    uint16_t battle_id) {
    return pod_radio_send_named(radio, PodMsgChallenge, src, name, level, dst, battle_id);
}

bool pod_radio_send_accept(
    PodRadio* radio,
    uint32_t src,
    const char* name,
    uint8_t level,
    uint32_t dst,
    uint16_t battle_id) {
    return pod_radio_send_named(radio, PodMsgAccept, src, name, level, dst, battle_id);
}

bool pod_radio_send_decline(PodRadio* radio, uint32_t src, uint32_t dst, uint16_t battle_id) {
    uint8_t buf[POD_FRAME_MAX];
    size_t n = pod_wire_decline(buf, sizeof(buf), src, dst, battle_id);
    return n && pod_radio_emit(radio, buf, n, POD_SEND_REPEAT);
}

bool pod_radio_send_taps(
    PodRadio* radio,
    uint32_t src,
    uint32_t dst,
    uint16_t battle_id,
    uint16_t taps) {
    uint8_t buf[POD_FRAME_MAX];
    size_t n = pod_wire_taps(buf, sizeof(buf), src, dst, battle_id, taps);
    return n && pod_radio_emit(radio, buf, n, POD_SEND_REPEAT);
}
