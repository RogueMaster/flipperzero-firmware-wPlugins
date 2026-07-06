#include "bad_usb_app_i.h"
#include <furi.h>
#include <furi_hal.h>
#include <string.h>
#include <storage/storage.h>
#include <lib/toolbox/path.h>
#include <flipper_format/flipper_format.h>
#include <furi_hal_bt.h>
#include <furi_hal_version.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>

#define TAG "BadUsb"

#define BAD_USB_SETTINGS_PATH           BAD_USB_APP_BASE_FOLDER "/.badusb.settings"
#define BAD_USB_SETTINGS_FILE_TYPE      "Flipper BadUSB Settings File"
#define BAD_USB_SETTINGS_VERSION        1
#define BAD_USB_SETTINGS_DEFAULT_LAYOUT BAD_USB_APP_PATH_LAYOUT_FOLDER "/en-US.kl"

static bool bad_usb_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    BadUsbApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool bad_usb_app_back_event_callback(void* context) {
    furi_assert(context);
    BadUsbApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void bad_usb_app_bt_status_changed_callback(BtStatus status, void* context) {
    furi_assert(context);
    BadUsbApp* app = context;
    app->bt_connected = (status == BtStatusConnected);
}

void bad_usb_app_track_bt_status(BadUsbApp* app) {
    // Stock firmware exposes no public BT status getter, so cache it from the status-changed
    // callback. The BLE HID worker registers its own callback on the shared RECORD_BT singleton
    // and clears it on stop, so we re-assert ours on Work-scene exit.
    bt_set_status_changed_callback(app->bt, bad_usb_app_bt_status_changed_callback, app);
}

void bad_usb_nfc_pairing_stop(BadUsbApp* app) {
    if(app->nfc_listener) {
        nfc_listener_stop(app->nfc_listener);
        nfc_listener_free(app->nfc_listener);
        app->nfc_listener = NULL;
    }
    if(app->nfc_data) {
        mf_ultralight_free(app->nfc_data);
        app->nfc_data = NULL;
    }
    // Update GUI
    bad_usb_view_set_nfc_status(app->bad_usb_view, false);
}

// Correct helper implementation
static void
    bad_usb_nfc_generate_pairing_data(MfUltralightData* data, const uint8_t* mac_override) {
    // Prepare a flat buffer then copy to pages
    uint8_t ndef_buf[64] = {0};
    uint8_t i = 0;
    Iso14443_3aData* iso_ptr = NULL;

    // --- 1. MAC Address Determination First ---
    uint8_t mac_buf[6] = {0};

    // Priority: 1. Custom Override (from Bad USB Settings), 2. Default
    bool use_override = false;
    if(mac_override) {
        for(int m = 0; m < 6; m++) {
            if(mac_override[m] != 0) {
                use_override = true;
                break;
            }
        }
    }

    if(use_override) {
        // Settings store MAC in Little Endian (due to reversal in scene config).
        // Reverse to Big Endian for consistent processing
        for(int m = 0; m < 6; m++) {
            mac_buf[m] = mac_override[5 - m];
        }
        FURI_LOG_I(TAG, "NFC Pairing: Using Custom MAC from Settings");
    } else {
        // Fallback to default MAC derived from device
        const uint8_t default_mac[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
        memcpy(mac_buf, default_mac, 6);
        FURI_LOG_I(TAG, "NFC Pairing: Using Default MAC");
    }

    FURI_LOG_I(
        TAG,
        "NFC MAC: %02X:%02X:%02X:%02X:%02X:%02X",
        mac_buf[0],
        mac_buf[1],
        mac_buf[2],
        mac_buf[3],
        mac_buf[4],
        mac_buf[5]);

    // --- 2. Initialize Usage Structures ---
    // Save pointer to allocated ISO struct BEFORE clearing parent struct
    iso_ptr = data->iso14443_3a_data;

    // Clear parent struct
    memset(data, 0, sizeof(MfUltralightData));

    // Restore pointer and clear ISO struct
    data->iso14443_3a_data = iso_ptr;
    memset(data->iso14443_3a_data, 0, sizeof(Iso14443_3aData));

    // --- 3. Dynamic UID Generation ---
    // We bind the UID to the MAC address to ensure phone cache invalidation.
    // Fixed prefix: 0x04 (NXP), 0x11, 0x22
    // Variable suffix: MAC[2..5]

    uint8_t uid[7];
    uid[0] = 0x04; // NXP
    uid[1] = 0x11; // Random prefix
    uid[2] = 0x22; // Random prefix
    uid[3] = mac_buf[2];
    uid[4] = mac_buf[3];
    uid[5] = mac_buf[4];
    uid[6] = mac_buf[5];

    // Calculate Checksums
    // BCC0 = 0x88 ^ UID0 ^ UID1 ^ UID2
    uint8_t bcc0 = 0x88 ^ uid[0] ^ uid[1] ^ uid[2];
    // BCC1 = UID3 ^ UID4 ^ UID5 ^ UID6
    uint8_t bcc1 = uid[3] ^ uid[4] ^ uid[5] ^ uid[6];

    // --- 4. Populate Data ---

    // Set ISO14443-3A params for Anti-Collision
    data->type = MfUltralightTypeNTAG215; // Match version 0x11 size
    data->pages_total = 135; // NTAG215 Size
    data->pages_read = 135; // Simulate full read

    // Populate ISO14443-3A Data
    data->iso14443_3a_data->uid_len = 7;
    memcpy(data->iso14443_3a_data->uid, uid, 7);
    data->iso14443_3a_data->atqa[0] = 0x00;
    data->iso14443_3a_data->atqa[1] = 0x44;
    data->iso14443_3a_data->sak = 0x00;

    // Populate Version (from working file: 00 04 04 02 01 00 11 03)
    data->version.header = 0x00;
    data->version.vendor_id = 0x04;
    data->version.prod_type = 0x04;
    data->version.prod_subtype = 0x02;
    data->version.prod_ver_major = 0x01;
    data->version.prod_ver_minor = 0x00;
    data->version.storage_size = 0x11;
    data->version.protocol_type = 0x03;

    // Zero signature
    memset(data->signature.data, 0, sizeof(data->signature.data));

    // Populate Pages
    // Page 0: UID0, UID1, UID2, BCC0
    data->page[0].data[0] = uid[0];
    data->page[0].data[1] = uid[1];
    data->page[0].data[2] = uid[2];
    data->page[0].data[3] = bcc0;

    // Page 1: UID3, UID4, UID5, UID6
    data->page[1].data[0] = uid[3];
    data->page[1].data[1] = uid[4];
    data->page[1].data[2] = uid[5];
    data->page[1].data[3] = uid[6];

    // Page 2: BCC1, Internal, Lock0, Lock1
    data->page[2].data[0] = bcc1;
    data->page[2].data[1] = 0x48;
    data->page[2].data[2] = 0x00;
    data->page[2].data[3] = 0x00;

    // Page 3: OTP
    data->page[3].data[0] = 0xE1;
    data->page[3].data[1] = 0x10;
    data->page[3].data[2] = 0x06;
    data->page[3].data[3] = 0x00; // CC capability container

    // NDEF Message starting at Page 4
    // 03 2B: NDEF Message (03), Length (43 bytes = 0x2B)
    ndef_buf[i++] = 0x03;
    ndef_buf[i++] = 0x2B;
    // D2: MB=1, ME=1, CF=0, SR=1, IL=0, TNF=2 (MIME)
    ndef_buf[i++] = 0xD2;
    // 20: Type Length (32 bytes)
    ndef_buf[i++] = 0x20;

    // 08: Payload Length (8 bytes)
    ndef_buf[i++] = 0x08;

    // Type: "application/vnd.bluetooth.ep.oob"
    const char* type_str = "application/vnd.bluetooth.ep.oob";
    memcpy(&ndef_buf[i], type_str, 32);
    i += 32;

    // Payload
    // 08 00 (OOB Data Len + Unused)
    ndef_buf[i++] = 0x08;
    ndef_buf[i++] = 0x00;

    // Copy MAC (Reverse Order for Little Endian NFC payload)
    // NFC OOB expects LSB first.
    for(int j = 5; j >= 0; j--) {
        ndef_buf[i++] = mac_buf[j];
    }

    // FE: Terminator
    ndef_buf[i++] = 0xFE;

    // Copy to pages
    uint8_t* raw_pages = (uint8_t*)&data->page[4];
    memcpy(raw_pages, ndef_buf, i);
}

void bad_usb_nfc_pairing_start(BadUsbApp* app) {
    bad_usb_nfc_pairing_stop(app);

    app->nfc_data = mf_ultralight_alloc();
    // Pass the user-configurable MAC from settings (or NULL if cleared/default)
    bad_usb_nfc_generate_pairing_data(app->nfc_data, app->user_hid_cfg.ble.mac);

    app->nfc_listener = nfc_listener_alloc(app->nfc, NfcProtocolMfUltralight, app->nfc_data);
    nfc_listener_start(app->nfc_listener, NULL, NULL);

    // Visual indication check
    FURI_LOG_I(TAG, "Starting NFC Pairing Emulation");
    notification_message(app->notifications, &sequence_set_blue_255);

    // Update GUI
    bad_usb_view_set_nfc_status(app->bad_usb_view, true);
}

static void bad_usb_app_tick_event_callback(void* context) {
    furi_assert(context);
    BadUsbApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);

    if(app->interface == BadUsbHidInterfaceBle) {
        bool connected = app->bt_connected;
        // FURI_LOG_I(TAG, "Tick: Connected=%d, Listener=%p", connected, app->nfc_listener);
        if(connected && app->nfc_listener) {
            FURI_LOG_I(TAG, "Stopping NFC (Connected)");
            bad_usb_nfc_pairing_stop(app);
            notification_message(app->notifications, &sequence_reset_blue);
        } else if(!connected && !app->nfc_listener) {
            FURI_LOG_I(TAG, "Starting NFC (Disconnected)");
            if(app->nfc_pairing_enabled) {
                bad_usb_nfc_pairing_start(app);
            }
        }
    }
}

static void bad_usb_load_settings(BadUsbApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* fff = flipper_format_file_alloc(storage);
    bool loaded = false;

    BadUsbHidConfig* hid_cfg = &app->user_hid_cfg;
    FuriString* temp_str = furi_string_alloc();
    uint32_t temp_uint = 0;

    if(flipper_format_file_open_existing(fff, BAD_USB_SETTINGS_PATH)) {
        do {
            if(!flipper_format_read_header(fff, temp_str, &temp_uint)) break;
            if((strcmp(furi_string_get_cstr(temp_str), BAD_USB_SETTINGS_FILE_TYPE) != 0) ||
               (temp_uint != BAD_USB_SETTINGS_VERSION))
                break;

            if(flipper_format_read_string(fff, "layout", temp_str)) {
                furi_string_set(app->keyboard_layout, temp_str);
                FileInfo layout_file_info;
                FS_Error file_check_err = storage_common_stat(
                    storage, furi_string_get_cstr(app->keyboard_layout), &layout_file_info);
                if((file_check_err != FSE_OK) || (layout_file_info.size != 256)) {
                    furi_string_set(app->keyboard_layout, BAD_USB_SETTINGS_DEFAULT_LAYOUT);
                }
            } else {
                furi_string_set(app->keyboard_layout, BAD_USB_SETTINGS_DEFAULT_LAYOUT);
                flipper_format_rewind(fff);
            }

            if(!flipper_format_read_uint32(fff, "interface", &temp_uint, 1) ||
               temp_uint >= BadUsbHidInterfaceMAX) {
                temp_uint = BadUsbHidInterfaceUsb;
                flipper_format_rewind(fff);
            }
            app->interface = temp_uint;

            if(!flipper_format_read_bool(fff, "ble_bonding", &hid_cfg->ble.bonding, 1)) {
                hid_cfg->ble.bonding = true;
                flipper_format_rewind(fff);
            }

            if(!flipper_format_read_uint32(fff, "ble_pairing", &temp_uint, 1) ||
               temp_uint >
                   GapPairingPinCodeVerifyYesNo) { // last valid; GapPairingCount is RM-only
                temp_uint = GapPairingPinCodeVerifyYesNo;
                flipper_format_rewind(fff);
            }
            hid_cfg->ble.pairing = temp_uint;

            if(flipper_format_read_string(fff, "ble_name", temp_str)) {
                strlcpy(
                    hid_cfg->ble.name, furi_string_get_cstr(temp_str), sizeof(hid_cfg->ble.name));
            } else {
                hid_cfg->ble.name[0] = '\0';
                flipper_format_rewind(fff);
            }

            if(!flipper_format_read_hex(
                   fff, "ble_mac", hid_cfg->ble.mac, sizeof(hid_cfg->ble.mac))) {
                memset(hid_cfg->ble.mac, 0, sizeof(hid_cfg->ble.mac));
                flipper_format_rewind(fff);
            }

            if(flipper_format_read_string(fff, "usb_manuf", temp_str)) {
                strlcpy(
                    hid_cfg->usb.manuf,
                    furi_string_get_cstr(temp_str),
                    sizeof(hid_cfg->usb.manuf));
            } else {
                hid_cfg->usb.manuf[0] = '\0';
                flipper_format_rewind(fff);
            }

            if(flipper_format_read_string(fff, "usb_product", temp_str)) {
                strlcpy(
                    hid_cfg->usb.product,
                    furi_string_get_cstr(temp_str),
                    sizeof(hid_cfg->usb.product));
            } else {
                hid_cfg->usb.product[0] = '\0';
                flipper_format_rewind(fff);
            }

            if(!flipper_format_read_uint32(fff, "usb_vid", &hid_cfg->usb.vid, 1)) {
                hid_cfg->usb.vid = 0;
                flipper_format_rewind(fff);
            }

            if(!flipper_format_read_uint32(fff, "usb_pid", &hid_cfg->usb.pid, 1)) {
                hid_cfg->usb.pid = 0;
                flipper_format_rewind(fff);
            }

            // NFC Pairing toggle (default: enabled)
            if(!flipper_format_read_bool(fff, "nfc_pairing", &app->nfc_pairing_enabled, 1)) {
                app->nfc_pairing_enabled = true; // Default to enabled
                flipper_format_rewind(fff);
            }

            loaded = true;
        } while(0);
    }

    furi_string_free(temp_str);

    flipper_format_free(fff);
    furi_record_close(RECORD_STORAGE);

    if(!loaded) {
        furi_string_set(app->keyboard_layout, BAD_USB_SETTINGS_DEFAULT_LAYOUT);
        app->interface = BadUsbHidInterfaceUsb;
        hid_cfg->ble.name[0] = '\0';
        memset(hid_cfg->ble.mac, 0, sizeof(hid_cfg->ble.mac));
        hid_cfg->ble.bonding = true;
        hid_cfg->ble.pairing = GapPairingPinCodeVerifyYesNo;
        hid_cfg->usb.vid = 0;
        hid_cfg->usb.pid = 0;
        hid_cfg->usb.manuf[0] = '\0';
        hid_cfg->usb.product[0] = '\0';
    }
}

static void bad_usb_save_settings(BadUsbApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* fff = flipper_format_file_alloc(storage);
    BadUsbHidConfig* hid_cfg = &app->user_hid_cfg;
    uint32_t temp_uint = 0;

    if(flipper_format_file_open_always(fff, BAD_USB_SETTINGS_PATH)) {
        do {
            if(!flipper_format_write_header_cstr(
                   fff, BAD_USB_SETTINGS_FILE_TYPE, BAD_USB_SETTINGS_VERSION))
                break;
            if(!flipper_format_write_string(fff, "layout", app->keyboard_layout)) break;
            temp_uint = app->interface;
            if(!flipper_format_write_uint32(fff, "interface", &temp_uint, 1)) break;
            if(!flipper_format_write_bool(fff, "ble_bonding", &hid_cfg->ble.bonding, 1)) break;
            temp_uint = hid_cfg->ble.pairing;
            if(!flipper_format_write_uint32(fff, "ble_pairing", &temp_uint, 1)) break;
            if(!flipper_format_write_string_cstr(fff, "ble_name", hid_cfg->ble.name)) break;
            if(!flipper_format_write_hex(
                   fff, "ble_mac", (uint8_t*)&hid_cfg->ble.mac, sizeof(hid_cfg->ble.mac)))
                break;
            if(!flipper_format_write_string_cstr(fff, "usb_manuf", hid_cfg->usb.manuf)) break;
            if(!flipper_format_write_string_cstr(fff, "usb_product", hid_cfg->usb.product)) break;
            if(!flipper_format_write_uint32(fff, "usb_vid", &hid_cfg->usb.vid, 1)) break;
            if(!flipper_format_write_uint32(fff, "usb_pid", &hid_cfg->usb.pid, 1)) break;
            if(!flipper_format_write_bool(fff, "nfc_pairing", &app->nfc_pairing_enabled, 1)) break;
        } while(0);
    }

    flipper_format_free(fff);
    furi_record_close(RECORD_STORAGE);
}

void bad_usb_set_interface(BadUsbApp* app, BadUsbHidInterface interface) {
    if(app->interface != interface) {
        if(interface == BadUsbHidInterfaceUsb) {
            if(app->interface == BadUsbHidInterfaceBle && app->nfc_pairing_enabled) {
                bad_usb_nfc_pairing_stop(app);
            }
            notification_message(app->notifications, &sequence_reset_blue);
        } else if(interface == BadUsbHidInterfaceBle) {
            FURI_LOG_I(TAG, "Set Interface: BLE");
            if(!app->bt_connected) {
                if(app->nfc_pairing_enabled) {
                    bad_usb_nfc_pairing_start(app);
                }
            }
        }
    }
    app->interface = interface;
    bad_usb_view_set_interface(app->bad_usb_view, interface);
}

void bad_usb_app_show_loading_popup(BadUsbApp* app, bool show) {
    if(show) {
        // Raise timer priority so that animations can play
        furi_timer_set_thread_priority(FuriTimerThreadPriorityElevated);
        view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbAppViewLoading);
    } else {
        // Restore default timer priority
        furi_timer_set_thread_priority(FuriTimerThreadPriorityNormal);
    }
}

BadUsbApp* bad_usb_app_alloc(char* arg) {
    BadUsbApp* app = malloc(sizeof(BadUsbApp));
    FURI_LOG_I(TAG, "BadUsb App Alloc");

    app->bad_usb_script = NULL;

    app->file_path = furi_string_alloc();
    app->keyboard_layout = furi_string_alloc();
    placeholder_map_init(&app->placeholders);
    app->payload_basename = furi_string_alloc();
    app->ph_config_count = 0;
    if(arg && strlen(arg)) {
        furi_string_set(app->file_path, arg);
    }

    bad_usb_load_settings(app);

    // NFC Alloc
    app->nfc = nfc_alloc();
    app->bt = furi_record_open(RECORD_BT);
    bad_usb_app_track_bt_status(app);

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    app->view_dispatcher = view_dispatcher_alloc();
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    view_dispatcher_enable_queue(
        app->view_dispatcher); // required on RM; deprecated no-op on stock
#pragma GCC diagnostic pop
    app->scene_manager = scene_manager_alloc(&bad_usb_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, bad_usb_app_tick_event_callback, 250);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, bad_usb_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, bad_usb_app_back_event_callback);

    // Custom Widget
    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BadUsbAppViewWidget, widget_get_view(app->widget));

    // Popup
    app->popup = popup_alloc();
    view_dispatcher_add_view(app->view_dispatcher, BadUsbAppViewPopup, popup_get_view(app->popup));

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        BadUsbAppViewConfig,
        variable_item_list_get_view(app->var_item_list));

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BadUsbAppViewPhConfig, submenu_get_view(app->submenu));

    app->bad_usb_view = bad_usb_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BadUsbAppViewWork, bad_usb_view_get_view(app->bad_usb_view));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BadUsbAppViewTextInput, text_input_get_view(app->text_input));

    app->byte_input = byte_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BadUsbAppViewByteInput, byte_input_get_view(app->byte_input));

    app->loading = loading_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BadUsbAppViewLoading, loading_get_view(app->loading));

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    if(!furi_string_empty(app->file_path)) {
        scene_manager_set_scene_state(app->scene_manager, BadUsbSceneWork, true);
        scene_manager_next_scene(app->scene_manager, BadUsbSceneWork);
    } else {
        furi_string_set(app->file_path, BAD_USB_APP_BASE_FOLDER);
        scene_manager_next_scene(app->scene_manager, BadUsbSceneFileSelect);
    }

    if(app->interface == BadUsbHidInterfaceBle) {
        if(!app->bt_connected) {
            if(app->nfc_pairing_enabled) {
                bad_usb_nfc_pairing_start(app);
            }
        }
    }
    return app;
}

void bad_usb_app_free(BadUsbApp* app) {
    furi_assert(app);

    if(app->bad_usb_script) {
        bad_usb_script_close(app->bad_usb_script);
        app->bad_usb_script = NULL;
    }

    bad_usb_nfc_pairing_stop(app);
    notification_message(app->notifications, &sequence_reset_blue);
    nfc_free(app->nfc);
    bt_set_status_changed_callback(app->bt, NULL, NULL);
    furi_record_close(RECORD_BT);
    app->bt = NULL;

    // Views
    view_dispatcher_remove_view(app->view_dispatcher, BadUsbAppViewWork);
    bad_usb_view_free(app->bad_usb_view);

    // Custom Widget
    view_dispatcher_remove_view(app->view_dispatcher, BadUsbAppViewWidget);
    widget_free(app->widget);

    // Popup
    view_dispatcher_remove_view(app->view_dispatcher, BadUsbAppViewPopup);
    popup_free(app->popup);

    // Config menu
    view_dispatcher_remove_view(app->view_dispatcher, BadUsbAppViewConfig);
    variable_item_list_free(app->var_item_list);

    // Placeholder config submenu
    view_dispatcher_remove_view(app->view_dispatcher, BadUsbAppViewPhConfig);
    submenu_free(app->submenu);

    // Text Input
    view_dispatcher_remove_view(app->view_dispatcher, BadUsbAppViewTextInput);
    text_input_free(app->text_input);

    // Byte Input
    view_dispatcher_remove_view(app->view_dispatcher, BadUsbAppViewByteInput);
    byte_input_free(app->byte_input);

    // Loading
    view_dispatcher_remove_view(app->view_dispatcher, BadUsbAppViewLoading);
    loading_free(app->loading);

    // View dispatcher
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    // Close records
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_DIALOGS);

    bad_usb_save_settings(app);

    furi_string_free(app->file_path);
    furi_string_free(app->keyboard_layout);
    furi_string_free(app->payload_basename);
    placeholder_map_reset(&app->placeholders);
    for(size_t i = 0; i < app->ph_config_count; i++) {
        furi_string_free(app->ph_config_names[i]);
    }

    free(app);
}

int32_t bad_usb_app(void* p) {
    BadUsbApp* bad_usb_app = bad_usb_app_alloc((char*)p);

    view_dispatcher_run(bad_usb_app->view_dispatcher);

    bad_usb_app_free(bad_usb_app);
    return 0;
}
