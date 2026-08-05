// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
/**
 * @file tracker_rules.h
 * Shared, dependency-free rules for BLE tracker advertisements.
 *
 * This header lives beside the Arduino sketch because Arduino copies sketches
 * to a temporary build directory and cannot resolve includes outside it. The
 * Flipper-side helpers/tracker_rules.h is a thin wrapper around this file, so
 * both toolchains still compile the same byte-layout and eligibility rules.
 * The rules identify tracker-shaped evidence; they do not prove ownership or
 * stalking.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* BLE categories carried on the companion wire protocol. */
#define BLE_TRACKER_CAT_AIRTAG      2u
#define BLE_TRACKER_CAT_TILE        3u
#define BLE_TRACKER_CAT_SMARTTAG    4u
#define BLE_TRACKER_CAT_FIND_MY_DEV 5u

/** Do not use a weak, distant advert as evidence that a tracker travels with us. */
#define BLE_TRACKER_MIN_RSSI (-85)

/* Apple manufacturer-data layout, after the AD length/type bytes. */
#define APPLE_FIND_MY_COMPANY_ID            0x004Cu
#define APPLE_FIND_MY_PAYLOAD_TYPE          0x12u
#define APPLE_FIND_MY_NEARBY_PAYLOAD_LEN    0x02u
#define APPLE_FIND_MY_SEPARATED_PAYLOAD_LEN 0x19u

/* Status-byte device type values (bits 4-5). */
#define APPLE_FIND_MY_DEVICE_AIRTAG      0x01u
#define APPLE_FIND_MY_DEVICE_THIRD_PARTY 0x02u

typedef struct {
    bool valid; /**< company/type/payload length were structurally valid */
    bool separated; /**< 0x19 payload: the accessory is advertising separated state */
    bool owner_connected_recently; /**< status bit 2: owner connected this key period */
    uint8_t status;
    uint8_t device_type; /**< status bits 4-5: AirTag, Apple device, etc. */
    uint8_t battery_level; /**< status bits 6-7: full through critically low */
    uint8_t payload_len; /**< 0x02 nearby or 0x19 separated */
} AppleFindMyAdvert;

/** Clear an output without requiring string/memory library support. */
static inline void apple_find_my_advert_clear(AppleFindMyAdvert* out) {
    if(!out) return;
    out->valid = false;
    out->separated = false;
    out->owner_connected_recently = false;
    out->status = 0;
    out->device_type = 0;
    out->battery_level = 0;
    out->payload_len = 0;
}

/**
 * Decode the Apple Find My manufacturer payload.
 *
 * `mfg` starts with the little-endian company id, as returned by the ESP32 BLE
 * library. The following bytes are 0x12, payload length, status, then payload.
 * A valid decode does not claim that the device is an AirTag; callers must
 * inspect `device_type` through apple_find_my_is_tracker().
 */
static inline bool apple_find_my_decode(const uint8_t* mfg, size_t len, AppleFindMyAdvert* out) {
    apple_find_my_advert_clear(out);
    if(!mfg || len < 5) return false;
    if(mfg[0] != (uint8_t)(APPLE_FIND_MY_COMPANY_ID & 0xFFu) ||
       mfg[1] != (uint8_t)(APPLE_FIND_MY_COMPANY_ID >> 8) ||
       mfg[2] != APPLE_FIND_MY_PAYLOAD_TYPE) {
        return false;
    }

    uint8_t payload_len = mfg[3];
    if(payload_len != APPLE_FIND_MY_NEARBY_PAYLOAD_LEN &&
       payload_len != APPLE_FIND_MY_SEPARATED_PAYLOAD_LEN) {
        return false;
    }
    if(len < (size_t)4u + payload_len) return false;

    uint8_t status = mfg[4];
    if(out) {
        out->valid = true;
        out->separated = payload_len == APPLE_FIND_MY_SEPARATED_PAYLOAD_LEN;
        out->owner_connected_recently = (status & 0x04u) != 0;
        out->status = status;
        out->device_type = (uint8_t)((status >> 4) & 0x03u);
        out->battery_level = (uint8_t)((status >> 6) & 0x03u);
        out->payload_len = payload_len;
    }
    return true;
}

/** AirTag or a licensed third-party Find My accessory, not an iPhone/Mac/AirPods. */
static inline bool apple_find_my_is_tracker(const AppleFindMyAdvert* advert) {
    return advert && advert->valid &&
           (advert->device_type == APPLE_FIND_MY_DEVICE_AIRTAG ||
            advert->device_type == APPLE_FIND_MY_DEVICE_THIRD_PARTY);
}

/** Categories that are legitimate tracker candidates for the following gate. */
static inline bool ble_tracker_category_is_known(uint8_t cat) {
    return cat == BLE_TRACKER_CAT_AIRTAG || cat == BLE_TRACKER_CAT_TILE ||
           cat == BLE_TRACKER_CAT_SMARTTAG || cat == BLE_TRACKER_CAT_FIND_MY_DEV;
}

/** A tracker advert is useful for anti-stalking only when it is close enough. */
static inline bool ble_tracker_rssi_is_usable(int8_t rssi) {
    return rssi >= BLE_TRACKER_MIN_RSSI;
}

#ifdef __cplusplus
static_assert(APPLE_FIND_MY_COMPANY_ID == 0x004C, "Apple company id must stay little-endian");
#endif
