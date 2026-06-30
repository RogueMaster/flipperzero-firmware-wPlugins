#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_nfc.h>
#include <furi_hal_usb.h>
#include <furi_hal_usb_hid.h>
#include <furi_hal_bt.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <gui/modules/file_browser.h>
#include <gui/modules/byte_input.h>
#include <gui/view.h>
#include <input/input.h>
#include <storage/storage.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller_sync.h>
#if defined(__has_include)
    #if __has_include(<nfc_login_icons.h>)
        // Prefer app-specific icon pack when available (extapps / OFW)
        #include <nfc_login_icons.h>
    #elif __has_include(<assets_icons.h>)
        // Fallback to firmware-wide assets (e.g. Momentum)
        #include <assets_icons.h>
    #else
        #error "Icon assets header not found. Need nfc_login_icons.h (extapps) or assets_icons.h (Momentum)."
    #endif
#else
    // Conservative fallback: expect extapp-generated header
    #include <nfc_login_icons.h>
#endif
#include <string.h>
#include <stdlib.h>

// Centralized BLE HID API availability check
#undef HAS_BLE_HID_API
#ifdef __has_include
    #if __has_include(<extra_profiles/hid_profile.h>) && __has_include(<bt/bt_service/bt.h>)
        #define HAS_BLE_HID_API 1
    #else
        #define HAS_BLE_HID_API 0
    #endif
#else
    #define HAS_BLE_HID_API 0
#endif

#if !defined(HAS_MOMENTUM_SUPPORT)
    #ifdef __has_include
        #if __has_include(<momentum/momentum.h>) || __has_include(<firmware/momentum.h>)
            #define HAS_MOMENTUM_SUPPORT
        #elif defined(FIRMWARE_MOMENTUM) || defined(MOMENTUM_FIRMWARE) || defined(__MOMENTUM__) || defined(MOMENTUM) || defined(HAS_MOMENTUM) || defined(MOMENTUM_FW)
            #define HAS_MOMENTUM_SUPPORT
        #endif
    #elif defined(FIRMWARE_MOMENTUM) || defined(MOMENTUM_FIRMWARE) || defined(__MOMENTUM__) || defined(MOMENTUM) || defined(HAS_MOMENTUM) || defined(MOMENTUM_FW)
        #define HAS_MOMENTUM_SUPPORT
    #endif
#endif

#define MAX_PASSCODE_SEQUENCE_LEN 64

#define TAG "nfc_login"
#define APP_DATA_DIR "/ext/apps_data/nfc_login"
#define NFC_CARDS_FILE_ENC APP_DATA_DIR "/cards.enc"
#define NFC_SETTINGS_FILE APP_DATA_DIR "/settings.txt"
#define BADUSB_LAYOUTS_DIR "/ext/badusb/assets/layouts"
#define MAX_CARDS 50
#define MAX_UID_LEN 10
#define MAX_PASSWORD_LEN 64
#define MAX_LAYOUT_PATH 256
#define SETTINGS_MENU_ITEMS 7
#define SETTINGS_VISIBLE_ITEMS 3
#define SETTINGS_HELP_Y_POS 54
#define CREDITS_PAGES 2
#define MAX_LAYOUTS 30
#define CARD_LIST_VISIBLE_ITEMS 4
#define KEY_MOD_LEFT_SHIFT 0x02
#define NFC_COOLDOWN_DELAY_MS 50
#define NFC_ENROLL_SCAN_DELAY_MS 150
#define NFC_SCAN_DELAY_MS 500
#define KEY_PRESS_DELAY_MS 10
#define KEY_RELEASE_DELAY_MS 10
#define ENTER_PRESS_DELAY_MS 50
#define ENTER_RELEASE_DELAY_MS 50
#define HID_SETTLE_DELAY_MS 100
#define HID_INIT_DELAY_MS 25
#define CRYPTO_SETTLE_DELAY_MS 150
#define BLE_DISCONNECT_DELAY_MS 300
#define BLE_ADVERTISE_DELAY_MS 150
#define BLE_DEINIT_DELAY_MS 200
#define BLE_CONNECTION_RETRY_DELAY_MS 100
#define PASSCODE_DELAY_MS 50
#define SCENE_DELAY_MS 100
#define STORAGE_READ_DELAY_MS 100
#define HID_POST_CONNECT_DELAY_MS 1000
#define HID_POST_TYPE_DELAY_MS 1000
#define ERROR_NOTIFICATION_DELAY_MS 300
#define HID_CONNECT_TIMEOUT_MS 5000
#define HID_CONNECT_RETRY_MS 100

typedef enum {
    EnrollmentStateNone,
    EnrollmentStateName,
    EnrollmentStatePassword,
} EnrollmentState;

typedef struct {
    uint8_t uid[MAX_UID_LEN];
    size_t uid_len;
    char password[MAX_PASSWORD_LEN];
    char name[32];
} NfcCard;

typedef enum { EditStateNone = 0, EditStateName = 1, EditStatePassword = 2, EditStateUid = 3 } EditState;

typedef enum { HidModeUsb = 0, HidModeBle = 1 } HidMode;

typedef enum {
    ViewSubmenu,
    ViewTextInput,
    ViewWidget,
    ViewFileBrowser,
    ViewByteInput,
    ViewPasscodeCanvas,
} ViewId;

typedef enum {
    SubmenuAddCard,
    SubmenuListCards,
    SubmenuStartScan,
    SubmenuSettings,
} SubmenuIndex;

typedef enum {
    EventAddCardStart = 1,
    EventStartScan = 2,
    EventPromptPassword = 3,
    EventEditUidDone = 4,
    EventManualUidEntry = 5,
} AppEvent;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    TextInput* text_input;
    Widget* widget;
    FileBrowser* file_browser;
    FuriString* fb_output_path;
    ByteInput* byte_input;
    View* passcode_canvas_view;
    NotificationApp* notification;

    NfcCard cards[MAX_CARDS];
    size_t card_count;
    size_t selected_card;
    bool has_active_selection;
    size_t active_card_index;
    uint8_t edit_menu_index;
    size_t edit_card_index;
    EditState edit_state;
    char edit_uid_text[MAX_UID_LEN * 2 + 1];
    uint8_t edit_uid_bytes[MAX_UID_LEN];
    uint8_t edit_uid_len;

    bool scanning;
    FuriThread* scan_thread;
    FuriHalUsbInterface* previous_usb_config;

    EnrollmentState enrollment_state;
    NfcCard enrollment_card;

    uint32_t current_view;
    uint8_t widget_state;

    bool enrollment_scanning;
    FuriThread* enroll_scan_thread;

    bool append_enter;
    uint16_t input_delay_ms;
    uint8_t settings_menu_index;
    uint8_t settings_scroll_offset;
    uint8_t card_list_scroll_offset;
    char keyboard_layout[64];
    bool layout_loaded;
    bool selecting_keyboard_layout;
    uint16_t layout[128];
    uint8_t credits_page;
    bool just_entered_edit_mode;
    
    // Passcode prompt state
    bool passcode_prompt_active;
    char passcode_sequence[MAX_PASSCODE_SEQUENCE_LEN];
    size_t passcode_sequence_len;
    bool passcode_needed;
    bool passcode_disabled; // Disable passcode for NFC Login app only
    uint8_t passcode_failed_attempts; // Track failed passcode attempts
    
    HidMode hid_mode; // USB or BLE HID mode
} App;

void app_switch_to_view(App* app, uint32_t view_id);