#pragma once

#include "bad_usb_app.h"
#include "scenes/bad_usb_scene.h"
#include "helpers/ducky_script.h"
#include "helpers/bad_usb_hid.h"
#include "helpers/placeholder.h"

#include <gui/gui.h>
#include <badusb_ph_icons.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <dialogs/dialogs.h>
#include <notification/notification_messages.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/loading.h>
#include <gui/modules/widget.h>
#include <gui/modules/popup.h>
#include "views/bad_usb_view.h"
#include <furi_hal_usb.h>

#include <nfc/nfc.h>
#include <nfc/nfc_listener.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <bt/bt_service/bt.h>

#ifndef HID_MANUF_PRODUCT_NAME_LEN // defined by RM/Unleashed; stock fw uses a bare char[32] field
#define HID_MANUF_PRODUCT_NAME_LEN 32
#endif

#define BAD_USB_APP_BASE_FOLDER        EXT_PATH("badusb")
#define BAD_USB_APP_PATH_LAYOUT_FOLDER BAD_USB_APP_BASE_FOLDER "/assets/layouts"
#define BAD_USB_APP_SCRIPT_EXTENSION   ".txt"
#define BAD_USB_APP_LAYOUT_EXTENSION   ".kl"

#define BAD_USB_PH_CONFIG_NAME_MAX 32
#define BAD_USB_PH_CONFIG_LIST_MAX 16

typedef enum {
    BadUsbAppErrorNoFiles,
} BadUsbAppError;

struct BadUsbApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;
    DialogsApp* dialogs;
    Widget* widget;
    Popup* popup;
    VariableItemList* var_item_list;
    Submenu* submenu;
    TextInput* text_input;
    ByteInput* byte_input;
    Loading* loading;

    // Placeholder feature state
    PlaceholderMap placeholders; // resolved values applied during execution
    FuriString* payload_basename; // basename of selected payload (config namespace)
    size_t ph_input_idx; // current placeholder being entered (ph_input scene)
    char ph_text_buf[PLACEHOLDER_VALUE_MAX]; // scratch buffer for text input
    char ph_config_name_buf[32]; // scratch buffer for config name
    char ph_header_buf[48]; // persistent header text for text input / submenu
    FuriString* ph_config_names[16]; // configs listed in ph_config scene
    size_t ph_config_count; // number of valid entries in ph_config_names

    char ble_name_buf[FURI_HAL_VERSION_DEVICE_NAME_LENGTH];
    uint8_t ble_mac_buf[GAP_MAC_ADDR_SIZE];
    char usb_name_buf[HID_MANUF_PRODUCT_NAME_LEN];
    uint16_t usb_vidpid_buf[2];

    BadUsbAppError error;
    FuriString* file_path;
    FuriString* keyboard_layout;

    BadUsbHidInterface interface;
    BadUsbHidConfig user_hid_cfg;
    BadUsbHidConfig script_hid_cfg;
    bool nfc_pairing_enabled; // Toggle for NFC pairing emulation

    BadUsbScript* bad_usb_script;
    BadUsb* bad_usb_view;

    // NFC Pairing
    Nfc* nfc;
    NfcListener* nfc_listener;
    MfUltralightData* nfc_data;

    Bt* bt;
    bool bt_connected; // cached BLE status; stock fw has no public bt status getter
};

typedef enum {
    BadUsbAppViewWidget,
    BadUsbAppViewPopup,
    BadUsbAppViewWork,
    BadUsbAppViewConfig,
    BadUsbAppViewByteInput,
    BadUsbAppViewTextInput,
    BadUsbAppViewLoading,
    BadUsbAppViewPhConfig, // submenu: pick a placeholder config / manual entry
} BadUsbAppView;

void bad_usb_set_interface(BadUsbApp* app, BadUsbHidInterface interface);

void bad_usb_app_track_bt_status(BadUsbApp* app);

void bad_usb_app_show_loading_popup(BadUsbApp* app, bool show);
