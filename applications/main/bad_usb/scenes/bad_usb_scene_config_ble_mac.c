#include "../bad_usb_app_i.h"
#include <bt/bt_service/bt.h>

enum ByteInputResult {
    ByteInputResultOk,
};

static void reverse_mac_addr(uint8_t mac_addr[GAP_MAC_ADDR_SIZE]) {
    uint8_t tmp;
    for(size_t i = 0; i < GAP_MAC_ADDR_SIZE / 2; i++) {
        tmp = mac_addr[i];
        mac_addr[i] = mac_addr[GAP_MAC_ADDR_SIZE - 1 - i];
        mac_addr[GAP_MAC_ADDR_SIZE - 1 - i] = tmp;
    }
}

void bad_usb_scene_config_ble_mac_byte_input_callback(void* context) {
    BadUsbApp* bad_usb = context;

    view_dispatcher_send_custom_event(bad_usb->view_dispatcher, ByteInputResultOk);
}

void bad_usb_scene_config_ble_mac_on_enter(void* context) {
    BadUsbApp* bad_usb = context;
    ByteInput* byte_input = bad_usb->byte_input;

    memcpy(bad_usb->ble_mac_buf, bad_usb->script_hid_cfg.ble.mac, sizeof(bad_usb->ble_mac_buf));
    reverse_mac_addr(bad_usb->ble_mac_buf);
    byte_input_set_header_text(byte_input, "Set BLE MAC address");

    byte_input_set_result_callback(
        byte_input,
        bad_usb_scene_config_ble_mac_byte_input_callback,
        NULL,
        bad_usb,
        bad_usb->ble_mac_buf,
        sizeof(bad_usb->ble_mac_buf));

    view_dispatcher_switch_to_view(bad_usb->view_dispatcher, BadUsbAppViewByteInput);
}

bool bad_usb_scene_config_ble_mac_on_event(void* context, SceneManagerEvent event) {
    BadUsbApp* bad_usb = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(event.event == ByteInputResultOk) {
            const BadUsbHidApi* hid = bad_usb_hid_get_interface(bad_usb->interface);
            reverse_mac_addr(bad_usb->ble_mac_buf);
            // Apply to current script config
            memcpy(
                bad_usb->script_hid_cfg.ble.mac,
                bad_usb->ble_mac_buf,
                sizeof(bad_usb->script_hid_cfg.ble.mac));
            hid->adjust_config(&bad_usb->script_hid_cfg);
            // Set in user config to save in settings file
            memcpy(
                bad_usb->user_hid_cfg.ble.mac,
                bad_usb->ble_mac_buf,
                sizeof(bad_usb->user_hid_cfg.ble.mac));

            // CRITICAL: Regenerate NFC tag with new MAC address
            // The tag must be refreshed to reflect the new MAC and update the UID
            if(bad_usb->interface == BadUsbHidInterfaceBle && bad_usb->nfc_pairing_enabled) {
                // Only regenerate if we're currently in BLE mode
                Bt* bt = furi_record_open(RECORD_BT);
                BtStatus bt_status = bt_get_status(bt);
                furi_record_close(RECORD_BT);

                if(bt_status != BtStatusConnected) {
                    // Not connected, restart NFC to refresh tag
                    bad_usb_nfc_pairing_stop(bad_usb);
                    bad_usb_nfc_pairing_start(bad_usb);
                    FURI_LOG_I("BadUsb", "NFC Tag Regenerated with new MAC");
                }
            }
        }
        scene_manager_previous_scene(bad_usb->scene_manager);
    }
    return consumed;
}

void bad_usb_scene_config_ble_mac_on_exit(void* context) {
    BadUsbApp* bad_usb = context;
    ByteInput* byte_input = bad_usb->byte_input;

    byte_input_set_result_callback(byte_input, NULL, NULL, NULL, NULL, 0);
    byte_input_set_header_text(byte_input, "");
}
