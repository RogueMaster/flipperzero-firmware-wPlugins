#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <dialogs/dialogs.h>
#include <notification/notification_messages.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/scene_manager.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/button_menu.h>
#include <gui/modules/number_input.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include "scenes/flipper_wedge_scene.h"
#include "views/flipper_wedge_startscreen.h"
#include "helpers/flipper_wedge_storage.h"
#include "helpers/flipper_wedge_hid.h"
#include "helpers/flipper_wedge_keyboard_layout.h"
#include "helpers/flipper_wedge_hid_worker.h"
#include "helpers/flipper_wedge_nfc.h"
#include "helpers/flipper_wedge_rfid.h"
#include "helpers/flipper_wedge_format.h"
#include "helpers/flipper_wedge_log.h"
#include "flipper_wedge_icons.h"

#define TAG "FlipperWedge"

#define FLIPPER_WEDGE_VERSION "1.1"
#define FLIPPER_WEDGE_TEXT_STORE_SIZE 128
#define FLIPPER_WEDGE_TEXT_STORE_COUNT 3
#define FLIPPER_WEDGE_DELIMITER_MAX_LEN 8
#define FLIPPER_WEDGE_OUTPUT_MAX_LEN 1200  // Increased to support large NDEF text (1024) + UIDs + delimiters

// Scan modes
typedef enum {
    FlipperWedgeModeNfc,           // NFC only (UID)
    FlipperWedgeModeRfid,          // RFID only (UID)
    FlipperWedgeModeNdef,          // NDEF only (text records)
    FlipperWedgeModeNfcThenRfid,   // NFC -> RFID combo
    FlipperWedgeModeRfidThenNfc,   // RFID -> NFC combo
    FlipperWedgeModeCount,
} FlipperWedgeMode;

// Mode startup behavior
typedef enum {
    FlipperWedgeModeStartupRemember,       // Remember last used mode
    FlipperWedgeModeStartupDefaultNfc,     // Always start with NFC mode
    FlipperWedgeModeStartupDefaultRfid,    // Always start with RFID mode
    FlipperWedgeModeStartupDefaultNdef,    // Always start with NDEF mode
    FlipperWedgeModeStartupDefaultNfcRfid, // Always start with NFC+RFID mode
    FlipperWedgeModeStartupDefaultRfidNfc, // Always start with RFID+NFC mode
    FlipperWedgeModeStartupCount,
} FlipperWedgeModeStartup;

// Output mode
typedef enum {
    FlipperWedgeOutputUsb,      // USB HID only
    FlipperWedgeOutputBle,      // Bluetooth LE HID only
    FlipperWedgeOutputCount,
} FlipperWedgeOutput;

// Scan state machine
typedef enum {
    FlipperWedgeScanStateIdle,           // Not scanning
    FlipperWedgeScanStateScanning,       // Actively scanning for first tag
    FlipperWedgeScanStateWaitingSecond,  // In combo mode, waiting for second tag
    FlipperWedgeScanStateDisplaying,     // Showing result before "Sent"
    FlipperWedgeScanStateCooldown,       // Brief pause after output
} FlipperWedgeScanState;

// Vibration levels
typedef enum {
    FlipperWedgeVibrationOff,      // No vibration
    FlipperWedgeVibrationLow,      // 30ms
    FlipperWedgeVibrationMedium,   // 60ms
    FlipperWedgeVibrationHigh,     // 100ms
    FlipperWedgeVibrationCount,
} FlipperWedgeVibration;

// NDEF text maximum length limits
typedef enum {
    FlipperWedgeNdefMaxLen250,      // 250 characters (recommended for fast typing)
    FlipperWedgeNdefMaxLen500,      // 500 characters (covers most use cases)
    FlipperWedgeNdefMaxLen1000,     // 1000 characters (maximum, may take several seconds to type)
    FlipperWedgeNdefMaxLenCount,
} FlipperWedgeNdefMaxLen;

typedef struct {
    Gui* gui;
    NotificationApp* notification;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    SceneManager* scene_manager;
    VariableItemList* variable_item_list;
    FlipperWedgeStartscreen* flipper_wedge_startscreen;
    DialogsApp* dialogs;
    FuriString* file_path;
    ButtonMenu* button_menu;
    NumberInput* number_input;
    int32_t current_number;
    TextInput* text_input;
    char text_store[FLIPPER_WEDGE_TEXT_STORE_COUNT][FLIPPER_WEDGE_TEXT_STORE_SIZE + 1];

    // HID module (managed by worker thread)
    FlipperWedgeHidWorker* hid_worker;
    FlipperWedgeOutput output_mode;
    bool usb_debug_mode;  // Deprecated: kept for backward compatibility reading only

    // Keyboard layout for HID output
    FlipperWedgeKeyboardLayout* keyboard_layout;

    // NFC module
    FlipperWedgeNfc* nfc;

    // RFID module
    FlipperWedgeRfid* rfid;

    // Scan mode and state
    FlipperWedgeMode mode;
    FlipperWedgeModeStartup mode_startup_behavior;
    FlipperWedgeScanState scan_state;

    // Scanned data
    uint8_t nfc_uid[FLIPPER_WEDGE_NFC_UID_MAX_LEN];
    uint8_t nfc_uid_len;
    char ndef_text[FLIPPER_WEDGE_NDEF_MAX_LEN];
    FlipperWedgeNfcError nfc_error;
    uint8_t rfid_uid[FLIPPER_WEDGE_RFID_UID_MAX_LEN];
    uint8_t rfid_uid_len;

    // Settings
    char delimiter[FLIPPER_WEDGE_DELIMITER_MAX_LEN];
    bool append_enter;
    FlipperWedgeVibration vibration_level;
    FlipperWedgeNdefMaxLen ndef_max_len;  // Maximum NDEF text length to type
    bool log_to_sd;        // Log scanned UIDs to SD card
    bool restart_pending;  // True if output mode changed and restart is required

    // Output mode switching (async to avoid UI thread blocking on bt_profile_start)
    bool output_switch_pending;
    FlipperWedgeOutput output_switch_target;

    // Timers
    FuriTimer* timeout_timer;
    FuriTimer* display_timer;

    // Output buffer
    char output_buffer[FLIPPER_WEDGE_OUTPUT_MAX_LEN];
} FlipperWedge;

typedef enum {
    FlipperWedgeViewIdStartscreen,
    FlipperWedgeViewIdMenu,
    FlipperWedgeViewIdTextInput,
    FlipperWedgeViewIdNumberInput,
    FlipperWedgeViewIdSettings,
    FlipperWedgeViewIdBtPair,
    FlipperWedgeViewIdOutputRestart,  // Deprecated: no longer used (dynamic switching works)
} FlipperWedgeViewId;

/** Switch output mode dynamically (USB <-> BLE)
 * Stops workers, deinits current HID, switches mode, inits new HID, restarts workers
 * Like Bad USB's dynamic switching pattern
 *
 * @param app FlipperWedge instance
 * @param new_mode New output mode to switch to
 */
void flipper_wedge_switch_output_mode(FlipperWedge* app, FlipperWedgeOutput new_mode);

/** Get HID instance from worker
 * Helper macro to access HID interface managed by worker thread
 */
#define flipper_wedge_get_hid(app) flipper_wedge_hid_worker_get_hid((app)->hid_worker)
