#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/popup.h>
#include <gui/modules/file_browser.h>
#include <gui/modules/text_input.h>
#include <gui/modules/text_box.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>
#include <toolbox/path.h>
#include <notification/notification_messages.h>
#include <datetime/datetime.h>
#include <toolbox/stream/file_stream.h>
//#include "assets_icons.h"
#include <gui/icon.h>
#include "nfc_dict_manager_icons.h"

#define TAG                 "NFC_Dict_Manager"
#define DICT_FOLDER_PATH    "/ext/nfc/dictionaries"
#define BACKUP_FOLDER_PATH  "/ext/nfc/dictionaries/backup"
#define SYSTEM_DICT_PATH    "/ext/nfc/assets/mf_classic_dict.nfc"
#define USER_DICT_PATH      "/ext/nfc/assets/mf_classic_dict_user.nfc"
#define DICT_EXTENSION      ".nfc"
#define MAX_PATH_LENGTH     256
#define MAX_FILENAME_LENGTH 128
#define MAX_KEY_LENGTH      12

typedef enum {
    NfcDictManagerViewSubmenu,
    NfcDictManagerViewFileBrowser,
    NfcDictManagerViewSelectType,
    NfcDictManagerViewMergeSelectA,
    NfcDictManagerViewMergeSelectB,
    NfcDictManagerViewMergeMenu,
    NfcDictManagerViewPopup,
    NfcDictManagerViewAbout,
    NfcDictManagerViewTextBox,
} NfcDictManagerView;

typedef enum {
    NfcDictManagerSceneStart,
    NfcDictManagerSceneBackup,
    NfcDictManagerSceneSelect,
    NfcDictManagerSceneSelectType,
    NfcDictManagerSceneMerge,
    NfcDictManagerSceneMergeSelectA,
    NfcDictManagerSceneMergeSelectB,
    NfcDictManagerSceneOptimize,
    NfcDictManagerSceneAbout,
    NfcDictManagerSceneCount,
} NfcDictManagerScene;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    Submenu* submenu;
    FileBrowser* file_browser;
    DialogEx* dialog_ex;
    Popup* popup;
    TextBox* text_box;
    DialogsApp* dialogs;
    NotificationApp* notification;

    FuriString* selected_dict_a;
    FuriString* selected_dict_b;
    FuriString* current_dict;
    FuriString* file_browser_result;
    FuriString* text_box_content;

    Storage* storage;
    FuriTimer* backup_timer;
    FuriTimer* success_timer;
    FuriTimer* merge_timer;
    FuriTimer* optimize_timer;
    bool backup_success;
    uint8_t backup_stage;
    uint8_t merge_stage;
    uint8_t optimize_stage;
} NfcDictManager;

void nfc_dict_manager_scene_start_on_enter(void* context);
bool nfc_dict_manager_scene_start_on_event(void* context, SceneManagerEvent event);
void nfc_dict_manager_scene_start_on_exit(void* context);

void nfc_dict_manager_scene_backup_on_enter(void* context);
bool nfc_dict_manager_scene_backup_on_event(void* context, SceneManagerEvent event);
void nfc_dict_manager_scene_backup_on_exit(void* context);

void nfc_dict_manager_scene_select_on_enter(void* context);
bool nfc_dict_manager_scene_select_on_event(void* context, SceneManagerEvent event);
void nfc_dict_manager_scene_select_on_exit(void* context);

void nfc_dict_manager_scene_select_type_on_enter(void* context);
bool nfc_dict_manager_scene_select_type_on_event(void* context, SceneManagerEvent event);
void nfc_dict_manager_scene_select_type_on_exit(void* context);

void nfc_dict_manager_scene_merge_on_enter(void* context);
bool nfc_dict_manager_scene_merge_on_event(void* context, SceneManagerEvent event);
void nfc_dict_manager_scene_merge_on_exit(void* context);

void nfc_dict_manager_scene_merge_select_a_on_enter(void* context);
bool nfc_dict_manager_scene_merge_select_a_on_event(void* context, SceneManagerEvent event);
void nfc_dict_manager_scene_merge_select_a_on_exit(void* context);

void nfc_dict_manager_scene_merge_select_b_on_enter(void* context);
bool nfc_dict_manager_scene_merge_select_b_on_event(void* context, SceneManagerEvent event);
void nfc_dict_manager_scene_merge_select_b_on_exit(void* context);

void nfc_dict_manager_scene_optimize_on_enter(void* context);
bool nfc_dict_manager_scene_optimize_on_event(void* context, SceneManagerEvent event);
void nfc_dict_manager_scene_optimize_on_exit(void* context);

void nfc_dict_manager_scene_about_on_enter(void* context);
bool nfc_dict_manager_scene_about_on_event(void* context, SceneManagerEvent event);
void nfc_dict_manager_scene_about_on_exit(void* context);

int32_t nfc_dict_manager_app(void* p);
