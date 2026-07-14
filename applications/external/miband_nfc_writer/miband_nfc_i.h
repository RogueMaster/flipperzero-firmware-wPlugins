/**
 * @file miband_nfc_i.h
 * @brief Internal header - Updated with new features
 */

#pragma once

#include <furi.h>
#include <dialogs/dialogs.h>
#include <gui/gui.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/text_box.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_scanner.h>
#include <nfc/nfc_poller.h>
#include <nfc/nfc_listener.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <lib/nfc/protocols/mf_classic/mf_classic_poller.h>
#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>
#include <nfc/protocols/mf_classic/mf_classic_listener.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller_sync.h>
#include <storage/storage.h>

#include <furi_hal_rtc.h>
#include "miband_nfc.h"
#include "miband_nfc_icons.h"
#include "miband_nfc_scene.h"
#include "progress_tracker.h"
#include "miband_logger.h"

#define NFC_APP_FOLDER    EXT_PATH("nfc")
#define NFC_APP_EXTENSION ".nfc"

typedef enum {
    MiBandNfcViewIdMainMenu, // 0
    MiBandNfcViewIdScanner, // 1
    MiBandNfcViewIdMagicEmulator, // 2
    MiBandNfcViewIdWriter, // 3
    MiBandNfcViewIdFileSelect, // 4
    MiBandNfcViewIdAbout, // 5
    MiBandNfcViewIdUidReport, // 6
    MiBandNfcViewIdDialog, // 7
} MiBandNfcViewId;

/**
 * @brief Menu item indices
 */
typedef enum {
    SubmenuIndexQuickUidCheck = 0,
    SubmenuIndexEmulateNfcMagic,
    SubmenuIndexWriteOriginalData,
    SubmenuIndexSaveMagic,
    SubmenuIndexVerify,
    SubmenuIndexSettings,
    SubmenuIndexAbout,
} SubmenuIndex;

typedef enum {
    SettingsIndexAutoBackup = 0,
    SettingsIndexVerifyAfterWrite,
    SettingsIndexShowProgress,
    SettingsIndexEnableLogging,
    SettingsIndexExportLogs,
    SettingsIndexClearLogs,
    SettingsIndexBack,
} SettingsIndex;

enum MiBandNfcCustomEvent {
    MiBandNfcCustomEventCardDetected,
    MiBandNfcCustomEventCardLost,
    MiBandNfcCustomEventMfClassicCard,
    MiBandNfcCustomEventPollerDone,
    MiBandNfcCustomEventWrongCard,
    MiBandNfcCustomEventViewExit,
    MiBandNfcCustomEventPollerFailed,
    MiBandNfcCustomEventVerifyExit,
    MiBandNfcCustomEventVerifyViewDetails,
};

typedef enum {
    OperationTypeEmulateMagic,
    OperationTypeWriteOriginal,
    OperationTypeSaveMagic,
    OperationTypeVerify,
} OperationType;

struct MiBandNfcApp {
    // GUI components
    Gui* gui;
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;

    // Views
    Submenu* submenu;
    Popup* popup;
    TextBox* text_box; // Shared by About and UID Check
    TextBox* text_box_report;
    DialogEx* dialog_ex;

    // File handling
    DialogsApp* dialogs;
    Storage* storage;
    FuriString* file_path;

    MiBandLogger* logger;

    // Settings
    bool auto_backup_enabled;
    bool verify_after_write;
    bool show_detailed_progress;
    bool enable_logging;

    // NFC components (allocated on demand)
    Nfc* nfc;
    NfcScanner* scanner;
    NfcPoller* poller;
    NfcListener* listener;
    NfcDevice* nfc_device;

    // NFC data
    MfClassicData* target_data; // Data read from physical card
    MfClassicData* mf_classic_data; // Data loaded from file

    // State
    bool is_valid_nfc_data;
    bool is_scan_active;
    SubmenuIndex last_selected_submenu_index;
    OperationType current_operation;

    FuriString* temp_text_buffer;

    bool is_emulating;
    void* uid_check_context; // Puntatore a UidCheckContext (allocato dinamicamente)
    void* emulation_stats; // Puntatore a EmulationStats
};

bool miband_settings_save(MiBandNfcApp* app);
bool miband_settings_load(MiBandNfcApp* app);
