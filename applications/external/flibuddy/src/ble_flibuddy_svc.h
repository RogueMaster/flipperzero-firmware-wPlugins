/* The FliBuddy BLE profile: one GATT service with RX (the host writes,
 * with or without response) and TX (the Flipper notifies), up to 243 bytes
 * each, advertised by its 128-bit UUID (ble_wire.h). No bonding and no
 * pairing: the attributes need no encryption unless
 * FLIBUDDY_BLE_REQUIRE_ENCRYPTION is 1, which lets a central try "just
 * works" pairing.
 *
 * Start it with bt_profile_start(bt, flibuddy_ble_profile, &callbacks);
 * the callbacks run on the BLE event thread, must be quick and stay valid
 * until the profile stops (bt_profile_restore_default).
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <furi_ble/profile_interface.h>

#ifndef FLIBUDDY_BLE_REQUIRE_ENCRYPTION
#define FLIBUDDY_BLE_REQUIRE_ENCRYPTION 0
#endif

typedef struct {
    void (*on_rx)(const uint8_t* data, size_t len, void* ctx); /* written to RX */
    void (*on_subscribe)(bool subscribed, void* ctx); /* TX notifications on or off */
    void (*on_mtu)(uint16_t mtu, void* ctx); /* the ATT MTU exchange finished */
    void (*on_disconnect)(void* ctx);
    void* ctx;
} FliBuddyBleCallbacks;

extern const FuriHalBleProfileTemplate* const flibuddy_ble_profile;

/* Notifies `len` (1..243) bytes on TX; false when the stack did not take
 * them (it is busy even after the SDK's own retries). */
bool flibuddy_ble_notify(FuriHalBleProfileBase* profile, const uint8_t* data, size_t len);
