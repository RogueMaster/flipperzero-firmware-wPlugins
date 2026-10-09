// Pod sub-GHz radio — beacons (presence) + battle-control messages, all on
// 433.92 MHz via the half-duplex TX/RX worker.
#pragma once

#include "profile.h"
#include "pod_wire.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct PodRadio PodRadio;

PodRadio* pod_radio_alloc(void);
void pod_radio_free(PodRadio* radio);
void pod_radio_set_rx_callback(PodRadio* radio, PodMsgCallback cb, void* context);

// False when the region does not permit 433.92 MHz; pod_radio_start() gates on
// this, so the UI can tell a regional block apart from a hardware failure.
bool pod_radio_frequency_allowed(void);

bool pod_radio_start(PodRadio* radio);
void pod_radio_stop(PodRadio* radio);
bool pod_radio_is_running(PodRadio* radio);

// Sends (each transmitted a few times for redundancy). Return true if queued.
bool pod_radio_beacon(PodRadio* radio, const PodProfile* profile, uint8_t dolphin_level);
bool pod_radio_send_challenge(
    PodRadio* radio,
    uint32_t src,
    const char* name,
    uint8_t level,
    uint32_t dst,
    uint16_t battle_id);
bool pod_radio_send_accept(
    PodRadio* radio,
    uint32_t src,
    const char* name,
    uint8_t level,
    uint32_t dst,
    uint16_t battle_id);
bool pod_radio_send_decline(PodRadio* radio, uint32_t src, uint32_t dst, uint16_t battle_id);
bool pod_radio_send_taps(
    PodRadio* radio,
    uint32_t src,
    uint32_t dst,
    uint16_t battle_id,
    uint16_t taps);
