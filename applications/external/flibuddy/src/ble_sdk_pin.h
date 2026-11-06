/* The SDK API version the BLE workarounds were verified against.
 *
 * The SDK offers FAP code no API-version macro, so this is a hand-kept
 * pin, checked by tools/tests/test_sdk_pin.py against the installed ufbt
 * SDK's api_symbols.csv (the `Version` row). After a firmware / SDK bump
 * that test fails until you re-verify both workarounds on the new SDK and
 * update the constant:
 *
 *   - ble_flibuddy_svc.c, flibuddy_ble_notify(): the result of
 *     ble_gatt_characteristic_update() is inverted (1.5.1 returns true when
 *     the update FAILED). If a newer SDK returns true on success, drop the
 *     `!`, or notifications are reported as failed.
 *   - ble_wire.h, BleWireLink: firmware 1.5.1's gap.c raises
 *     GapEventTypeConnected only on pairing complete, so a connected host
 *     is detected by its TX subscription. A newer gap.c may report the
 *     connection itself.
 */
#pragma once

#define FLIBUDDY_VERIFIED_SDK_API "88.2"
