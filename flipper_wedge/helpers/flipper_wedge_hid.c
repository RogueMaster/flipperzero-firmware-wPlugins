#include "flipper_wedge_hid.h"
#include "flipper_wedge_keyboard_layout.h"
#include "flipper_wedge_debug.h"
#include <storage/storage.h>

#define TAG "FlipperWedgeHid"

#define HID_TYPE_DELAY_MS 2

// MAC address XOR to make Flipper appear as different device in HID mode
#define HID_BT_MAC_XOR 0xF1D0  // "FliD" in hex - unique identifier

struct FlipperWedgeHid {
    // USB HID
    FuriHalUsbInterface* usb_mode_prev;  // Save previous USB mode for restoration
    bool usb_initialized;

    // Bluetooth HID
    Bt* bt;
    FuriHalBleProfileBase* ble_hid_profile;
    bool bt_initialized;
    bool bt_connected;

    // Callback
    FlipperWedgeHidConnectionCallback connection_callback;
    void* connection_callback_context;
};

static void flipper_wedge_hid_bt_status_callback(BtStatus status, void* context) {
    furi_assert(context);
    FlipperWedgeHid* instance = context;

    bool connected = (status == BtStatusConnected);
    bool prev_connected = instance->bt_connected;
    instance->bt_connected = connected;

    FURI_LOG_I(TAG, "BT status: %d (prev=%d, new=%d)", status, prev_connected, connected);

    if(instance->connection_callback) {
        bool usb_connected = flipper_wedge_hid_is_usb_connected(instance);
        instance->connection_callback(usb_connected, connected, instance->connection_callback_context);
    }
}

FlipperWedgeHid* flipper_wedge_hid_alloc(void) {
    FlipperWedgeHid* instance = malloc(sizeof(FlipperWedgeHid));

    instance->usb_mode_prev = NULL;
    instance->usb_initialized = false;
    instance->bt = NULL;
    instance->ble_hid_profile = NULL;
    instance->bt_initialized = false;
    instance->bt_connected = false;
    instance->connection_callback = NULL;
    instance->connection_callback_context = NULL;

    return instance;
}

void flipper_wedge_hid_free(FlipperWedgeHid* instance) {
    furi_assert(instance);

    // Clean up any initialized interfaces
    if(instance->usb_initialized) {
        flipper_wedge_hid_deinit_usb(instance);
    }
    if(instance->bt_initialized) {
        flipper_wedge_hid_deinit_ble(instance);
    }

    free(instance);
}

void flipper_wedge_hid_init_usb(FlipperWedgeHid* instance) {
    furi_assert(instance);

    if(instance->usb_initialized) {
        FURI_LOG_W(TAG, "USB HID already initialized");
        return;
    }

    FURI_LOG_I(TAG, "Initializing USB HID");
    flipper_wedge_debug_log(TAG, "Init USB HID");

    // Save current USB mode for restoration (like Bad USB)
    instance->usb_mode_prev = furi_hal_usb_get_config();
    furi_hal_usb_unlock();
    furi_check(furi_hal_usb_set_config(&usb_hid, NULL) == true);
    instance->usb_initialized = true;

    FURI_LOG_I(TAG, "USB HID initialized");

    // Notify connection callback
    if(instance->connection_callback) {
        bool usb_connected = flipper_wedge_hid_is_usb_connected(instance);
        bool bt_connected = flipper_wedge_hid_is_bt_connected(instance);
        instance->connection_callback(usb_connected, bt_connected, instance->connection_callback_context);
    }
}

void flipper_wedge_hid_deinit_usb(FlipperWedgeHid* instance) {
    furi_assert(instance);

    if(!instance->usb_initialized) {
        FURI_LOG_W(TAG, "USB HID not initialized");
        return;
    }

    FURI_LOG_I(TAG, "Deinitializing USB HID");
    flipper_wedge_debug_log(TAG, "Deinit USB HID");

    // Restore previous USB mode (like Bad USB)
    if(instance->usb_mode_prev) {
        furi_hal_usb_set_config(instance->usb_mode_prev, NULL);
    }
    instance->usb_initialized = false;

    FURI_LOG_I(TAG, "USB HID deinitialized");

    // Notify connection callback
    if(instance->connection_callback) {
        bool bt_connected = flipper_wedge_hid_is_bt_connected(instance);
        instance->connection_callback(false, bt_connected, instance->connection_callback_context);
    }
}

void flipper_wedge_hid_init_ble(FlipperWedgeHid* instance) {
    furi_assert(instance);

    if(instance->bt_initialized) {
        FURI_LOG_W(TAG, "BLE HID already initialized");
        return;
    }

    FURI_LOG_I(TAG, "Initializing BLE HID");
    flipper_wedge_debug_log(TAG, "Init BLE HID - opening BT record");

    instance->bt = furi_record_open(RECORD_BT);
    flipper_wedge_debug_log(TAG, "BT record opened");

    // Disconnect from any existing connection before profile switch
    flipper_wedge_debug_log(TAG, "Disconnecting BT...");
    bt_disconnect(instance->bt);
    flipper_wedge_debug_log(TAG, "BT disconnected, waiting 200ms for NVM sync");
    // Wait 200ms for 2nd core to update NVM storage (CRITICAL!)
    furi_delay_ms(200);
    flipper_wedge_debug_log(TAG, "NVM sync complete");

    // Set up key storage path
    flipper_wedge_debug_log(TAG, "Setting up BT key storage");
    bt_keys_storage_set_storage_path(instance->bt, APP_DATA_PATH(FLIPPER_WEDGE_BT_KEYS_STORAGE_NAME));
    flipper_wedge_debug_log(TAG, "BT key storage configured");

    // Start BLE HID profile with "HID" prefix (max 8 chars)
    // MAC XOR makes Flipper appear as different device, preventing host from
    // using cached pairing credentials from the default Flipper profile
    flipper_wedge_debug_log(TAG, "Starting BLE HID profile...");
    BleProfileHidParams hid_params = {
        .device_name_prefix = "HID",  // Must be <8 chars per firmware limitation
        .mac_xor = HID_BT_MAC_XOR,  // XOR MAC to appear as different device
    };
    instance->ble_hid_profile = bt_profile_start(instance->bt, ble_profile_hid, &hid_params);
    flipper_wedge_debug_log(TAG, "bt_profile_start returned: %p", (void*)instance->ble_hid_profile);

    if(!instance->ble_hid_profile) {
        FURI_LOG_E(TAG, "FATAL: bt_profile_start returned NULL!");
        flipper_wedge_debug_log(TAG, "ERROR: bt_profile_start failed!");
        furi_record_close(RECORD_BT);
        instance->bt = NULL;
        return;  // Fail gracefully instead of crashing
    }

    // Start advertising
    flipper_wedge_debug_log(TAG, "Starting BT advertising");
    furi_hal_bt_start_advertising();

    // Register connection status callback
    flipper_wedge_debug_log(TAG, "Registering BT status callback");
    bt_set_status_changed_callback(instance->bt, flipper_wedge_hid_bt_status_callback, instance);

    instance->bt_initialized = true;

    FURI_LOG_I(TAG, "BLE HID initialized and advertising");
    flipper_wedge_debug_log(TAG, "BLE HID init complete!");
}

void flipper_wedge_hid_deinit_ble(FlipperWedgeHid* instance) {
    furi_assert(instance);

    if(!instance->bt_initialized || !instance->bt) {
        FURI_LOG_W(TAG, "BLE HID not initialized");
        return;
    }

    FURI_LOG_I(TAG, "Deinitializing BLE HID");
    flipper_wedge_debug_log(TAG, "Deinit BLE HID");

    bt_set_status_changed_callback(instance->bt, NULL, NULL);
    bt_disconnect(instance->bt);
    furi_delay_ms(200);  // CRITICAL delay for NVM sync
    bt_keys_storage_set_default_path(instance->bt);

    FURI_LOG_I(TAG, "Restoring default BT profile");
    furi_check(bt_profile_restore_default(instance->bt));

    // Wait for 2nd core to restart and apply default profile
    furi_delay_ms(200);

    // Explicitly restart advertising with default profile
    furi_hal_bt_start_advertising();

    furi_record_close(RECORD_BT);
    instance->bt = NULL;
    instance->ble_hid_profile = NULL;
    instance->bt_initialized = false;
    instance->bt_connected = false;

    FURI_LOG_I(TAG, "BLE HID deinitialized and default profile restored");

    // Notify connection callback
    if(instance->connection_callback) {
        bool usb_connected = flipper_wedge_hid_is_usb_connected(instance);
        instance->connection_callback(usb_connected, false, instance->connection_callback_context);
    }
}

void flipper_wedge_hid_set_connection_callback(
    FlipperWedgeHid* instance,
    FlipperWedgeHidConnectionCallback callback,
    void* context) {
    furi_assert(instance);
    instance->connection_callback = callback;
    instance->connection_callback_context = context;
}

bool flipper_wedge_hid_is_usb_connected(FlipperWedgeHid* instance) {
    furi_assert(instance);
    if(!instance->usb_initialized) return false;
    return furi_hal_hid_is_connected();
}

bool flipper_wedge_hid_is_bt_connected(FlipperWedgeHid* instance) {
    furi_assert(instance);
    if(!instance->bt_initialized) return false;
    return instance->bt_connected;
}

bool flipper_wedge_hid_is_connected(FlipperWedgeHid* instance) {
    return flipper_wedge_hid_is_usb_connected(instance) || flipper_wedge_hid_is_bt_connected(instance);
}

void flipper_wedge_hid_type_char(FlipperWedgeHid* instance, FlipperWedgeKeyboardLayout* layout, char c) {
    furi_assert(instance);

    uint16_t keycode;
    if(layout) {
        keycode = flipper_wedge_keyboard_layout_get_keycode(layout, c);
    } else {
        keycode = HID_ASCII_TO_KEY(c);
    }
    if(keycode == HID_KEYBOARD_NONE) return;

    // Send to USB HID if initialized
    if(instance->usb_initialized && flipper_wedge_hid_is_usb_connected(instance)) {
        furi_hal_hid_kb_press(keycode);
        furi_hal_hid_kb_release(keycode);
    }

    // Send to BT HID if initialized
    if(instance->bt_initialized && flipper_wedge_hid_is_bt_connected(instance) && instance->ble_hid_profile) {
        ble_profile_hid_kb_press(instance->ble_hid_profile, keycode);
        ble_profile_hid_kb_release(instance->ble_hid_profile, keycode);
    }

    furi_delay_ms(HID_TYPE_DELAY_MS);
}

void flipper_wedge_hid_type_string(FlipperWedgeHid* instance, FlipperWedgeKeyboardLayout* layout, const char* str) {
    furi_assert(instance);
    furi_assert(str);

    while(*str) {
        flipper_wedge_hid_type_char(instance, layout, *str);
        str++;
    }
}

void flipper_wedge_hid_press_enter(FlipperWedgeHid* instance) {
    furi_assert(instance);

    uint16_t keycode = HID_KEYBOARD_RETURN;

    // Send to USB HID if initialized
    if(instance->usb_initialized && flipper_wedge_hid_is_usb_connected(instance)) {
        furi_hal_hid_kb_press(keycode);
        furi_hal_hid_kb_release(keycode);
    }

    // Send to BT HID if initialized
    if(instance->bt_initialized && flipper_wedge_hid_is_bt_connected(instance) && instance->ble_hid_profile) {
        ble_profile_hid_kb_press(instance->ble_hid_profile, keycode);
        ble_profile_hid_kb_release(instance->ble_hid_profile, keycode);
    }
}

void flipper_wedge_hid_release_all(FlipperWedgeHid* instance) {
    furi_assert(instance);

    // Release all keys on USB HID if initialized
    if(instance->usb_initialized && flipper_wedge_hid_is_usb_connected(instance)) {
        furi_hal_hid_kb_release_all();
    }

    // Release all keys on BT HID if initialized
    if(instance->bt_initialized && flipper_wedge_hid_is_bt_connected(instance) && instance->ble_hid_profile) {
        ble_profile_hid_kb_release_all(instance->ble_hid_profile);
    }
}
