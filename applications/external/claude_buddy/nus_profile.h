/**
 * Nordic UART Service BLE profile.
 *
 * Implements the standard Nordic UART Service (NUS) so Claude Desktop /
 * Claude Cowork's built-in BLE bridge can talk to us directly (no host-side
 * Python bridge needed).
 *
 * Service:  6e400001-b5a3-f393-e0a9-e50e24dcca9e
 * RX char:  6e400002-b5a3-f393-e0a9-e50e24dcca9e  (desktop → device, write w/o rsp)
 * TX char:  6e400003-b5a3-f393-e0a9-e50e24dcca9e  (device → desktop, notify)
 *
 * Advertised name: "Claude Flipper" (picker filters by "Claude*").
 */

#pragma once

#include <furi_ble/profile_interface.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Max payload the host should send in one write. Capped below ATT MTU-3. */
#define NUS_PROFILE_RX_MAX_LEN 240

typedef void (*NusProfileRxCallback)(const uint8_t* data, uint16_t len, void* context);

/* Profile descriptor. Pass to bt_profile_start(bt, ble_profile_nus, NULL). */
extern const FuriHalBleProfileTemplate* const ble_profile_nus;

/* Send bytes to host via TX characteristic notification.  Splits into
 * chunks <= NUS_PROFILE_RX_MAX_LEN automatically.  Safe to call from GUI
 * thread only (not from the BLE RX event callback). */
bool nus_profile_tx(FuriHalBleProfileBase* profile, const uint8_t* data, uint16_t size);

/* Register RX callback.  Callback is invoked on the BLE event thread —
 * DO NOT call nus_profile_tx or other UI functions from inside it;
 * defer to the GUI thread. */
void nus_profile_set_rx_callback(
    FuriHalBleProfileBase* profile,
    NusProfileRxCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
