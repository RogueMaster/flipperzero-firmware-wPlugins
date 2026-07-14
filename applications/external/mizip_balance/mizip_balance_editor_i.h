#pragma once

#include <furi.h>

#include <dialogs/dialogs.h>

#include <gui/gui.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/number_input.h>
#include <gui/modules/text_box.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <dolphin/dolphin.h>
#include <notification/notification_messages.h>

#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_scanner.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <lib/nfc/protocols/mf_classic/mf_classic_poller.h>
#include <toolbox/keys_dict.h>

#include <mizip_balance_editor_icons.h>
#include "scenes/mizip_balance_editor_scene.h"

#include "mizip_balance_editor.h"
#include "adapted_from_ofw/mizip.h"

#define NFC_APP_FOLDER           EXT_PATH("nfc")
#define NFC_APP_EXTENSION        ".nfc"
#define NFC_APP_SHADOW_EXTENSION ".shd"
#define MIZIP_SECTOR_COUNT       5
#define MIZIP_KEY_LENGTH         6
#define MIZIP_KEYA_0_BYTES       {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5}
#define MIZIP_KEYB_0_BYTES       {0xB4, 0xC1, 0x32, 0x43, 0x9E, 0xEF}
#define MIZIP_BALANCE_MIN_VALUE  0
//Theorical max value is 65535 but it makes the checksum be 00, which prevent some machine to recognize the tag correctly
#define MIZIP_BALANCE_MAX_VALUE  65534

// Enumeration of the view indexes.
typedef enum {
    MiZipBalanceEditorViewIdMainMenu,
    MiZipBalanceEditorViewIdScanner,
    MiZipBalanceEditorViewIdReader,
    MiZipBalanceEditorViewIdFileSelect,
    MiZipBalanceEditorViewIdNumberInput,
    MiZipBalanceEditorViewIdShowBalance,
    MiZipBalanceEditorViewIdWriteSuccess,
    MiZipBalanceEditorViewIdAbout,
} MiZipBalanceEditorViewId;

// Enumeration of submenu items.
typedef enum {
    SubmenuIndexDirectToTag,
    SubmenuIndexEditMiZipFile,
    SubmenuIndexAbout,
} SubmenuIndex;

// Custom view events
enum MiZipBalanceEditorCustomEvent {
    MiZipBalanceEditorCustomEventCardDetected,
    MiZipBalanceEditorCustomEventCardLost,
    MiZipBalanceEditorCustomEventMfClassicCard,
    MiZipBalanceEditorCustomEventPollerDone,
    MiZipBalanceEditorCustomEventWrongCard,
    MiZipBalanceEditorCustomEventViewExit,
};

// Data source
typedef enum {
    DataSourceFile,
    DataSourceNfc,
} DataSource;

// Main application structure.
struct MiZipBalanceEditorApp {
    //GUI components
    Gui* gui;
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;

    //Scenes
    Submenu* submenu;
    Popup* popup;
    DialogsApp* dialogs;
    DialogEx* dialog_ex;
    NumberInput* number_input;
    TextBox* text_box;

    SubmenuIndex last_selected_submenu_index;

    DataSource currentDataSource;

    //NFC
    Nfc* nfc;
    NfcScanner* scanner;
    NfcPoller* poller;
    NfcDevice* nfc_device;
    bool is_scan_active;

    //File
    Storage* storage;
    FuriString* filePath;
    FuriString* shadowFilePath;
    bool is_shadow_file_exists;
    bool is_file_loaded;

    //Mifare Classic data
    uint8_t current_sector;
    uint8_t calculated_keyA[MIZIP_SECTOR_COUNT][MIZIP_KEY_LENGTH];
    uint8_t calculated_keyB[MIZIP_SECTOR_COUNT][MIZIP_KEY_LENGTH];
    MfClassicData* mf_classic_data;
    bool is_valid_mizip_data;
    bool is_number_input_active;

    //MiZip data
    uint8_t uid[4];
    uint8_t credit_pointer;
    uint8_t previous_credit_pointer;
    uint16_t current_balance;
    uint16_t previous_balance;
    uint16_t new_balance;
};

bool mizip_balance_editor_write_new_balance(void* context);
bool mizip_balance_editor_write_new_balance_to_file(void* context);
bool mizip_balance_editor_write_new_balance_to_tag(void* context);
