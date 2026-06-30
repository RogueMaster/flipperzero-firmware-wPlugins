#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb.h>
#include <furi_hal_usb_hid.h>
#include <bt/bt_service/bt.h>
#include <extra_profiles/hid_profile.h>
#include "flipper_wedge_keyboard_layout.h"

#define FLIPPER_WEDGE_BT_KEYS_STORAGE_NAME ".flipper_wedge_bt.keys"

typedef struct FlipperWedgeHid FlipperWedgeHid;

typedef void (*FlipperWedgeHidConnectionCallback)(bool usb_connected, bool bt_connected, void* context);

/** Allocate HID helper
 *
 * @return FlipperWedgeHid instance
 */
FlipperWedgeHid* flipper_wedge_hid_alloc(void);

/** Free HID helper
 *
 * @param instance FlipperWedgeHid instance
 */
void flipper_wedge_hid_free(FlipperWedgeHid* instance);

/** Initialize USB HID interface
 * Like Bad USB pattern - call at app start or when switching to USB mode
 *
 * @param instance FlipperWedgeHid instance
 */
void flipper_wedge_hid_init_usb(FlipperWedgeHid* instance);

/** Deinitialize USB HID interface
 * Like Bad USB pattern - call when switching away from USB mode or at app exit
 *
 * @param instance FlipperWedgeHid instance
 */
void flipper_wedge_hid_deinit_usb(FlipperWedgeHid* instance);

/** Initialize BLE HID interface
 * Like Bad USB pattern - call at app start or when switching to BLE mode
 *
 * @param instance FlipperWedgeHid instance
 */
void flipper_wedge_hid_init_ble(FlipperWedgeHid* instance);

/** Deinitialize BLE HID interface
 * Like Bad USB pattern - call when switching away from BLE mode or at app exit
 *
 * @param instance FlipperWedgeHid instance
 */
void flipper_wedge_hid_deinit_ble(FlipperWedgeHid* instance);

/** Set connection status callback
 *
 * @param instance FlipperWedgeHid instance
 * @param callback Callback function
 * @param context Callback context
 */
void flipper_wedge_hid_set_connection_callback(
    FlipperWedgeHid* instance,
    FlipperWedgeHidConnectionCallback callback,
    void* context);

/** Check if USB HID is connected
 *
 * @param instance FlipperWedgeHid instance
 * @return true if connected
 */
bool flipper_wedge_hid_is_usb_connected(FlipperWedgeHid* instance);

/** Check if BT HID is connected
 *
 * @param instance FlipperWedgeHid instance
 * @return true if connected
 */
bool flipper_wedge_hid_is_bt_connected(FlipperWedgeHid* instance);

/** Check if any HID connection is available
 *
 * @param instance FlipperWedgeHid instance
 * @return true if USB or BT is connected
 */
bool flipper_wedge_hid_is_connected(FlipperWedgeHid* instance);

/** Type a string via HID keyboard
 * Sends to both USB and BT if connected
 *
 * @param instance FlipperWedgeHid instance
 * @param layout Keyboard layout for character mapping (NULL for firmware default)
 * @param str String to type
 */
void flipper_wedge_hid_type_string(FlipperWedgeHid* instance, FlipperWedgeKeyboardLayout* layout, const char* str);

/** Type a single character via HID keyboard
 *
 * @param instance FlipperWedgeHid instance
 * @param layout Keyboard layout for character mapping (NULL for firmware default)
 * @param c Character to type
 */
void flipper_wedge_hid_type_char(FlipperWedgeHid* instance, FlipperWedgeKeyboardLayout* layout, char c);

/** Press and release Enter key
 *
 * @param instance FlipperWedgeHid instance
 */
void flipper_wedge_hid_press_enter(FlipperWedgeHid* instance);

/** Release all keys
 *
 * @param instance FlipperWedgeHid instance
 */
void flipper_wedge_hid_release_all(FlipperWedgeHid* instance);
