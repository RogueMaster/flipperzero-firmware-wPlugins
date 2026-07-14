#pragma once

#include "nfc_eink_app.h"
#include <furi.h>
#include <furi_hal.h>

#include <dolphin/dolphin.h>
#include <dolphin/helpers/dolphin_deed.h>
#include <notification/notification_messages.h>

#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/empty_screen.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/popup.h>
#include <gui/modules/loading.h>
#include <gui/modules/text_input.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/text_box.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>

#include <dialogs/dialogs.h>
#include <storage/storage.h>

#include <nfc/nfc_listener.h>
#include <nfc/nfc_poller.h>
#include <toolbox/saved_struct.h>

#include "nfc_eink_icons.h"

#include "nfc_eink_screen/nfc_eink_screen.h"
#include "scenes/scenes.h"

#include "views/eink_progress.h"
#include "views/image_scroll.h"

#define NFC_EINK_NAME_SIZE       (50)
#define NFC_EINK_APP_EXTENSION   ".eink"
#define NFC_EINK_APP_FOLDER_NAME "nfc/nfc_eink"

#define NFC_EINK_APP_FOLDER          EXT_PATH(NFC_EINK_APP_FOLDER_NAME)
#define NFC_EINK_APP_TEXT_STORE_SIZE (100)

typedef enum {
    NfcEinkAppCustomEventReserved = 100,

    NfcEinkAppCustomEventTextInputDone,
    NfcEinkAppCustomEventTimerExpired,
    NfcEinkAppCustomEventBlockProcessed,
    NfcEinkAppCustomEventUpdating,
    NfcEinkAppCustomEventProcessFinish,
    NfcEinkAppCustomEventTargetDetected,
    NfcEinkAppCustomEventError,
    NfcEinkAppCustomEventExit,
} NfcEinkAppCustomEvents;

typedef enum {
    NfcEinkViewMenu,
    NfcEinkViewDialogEx,
    NfcEinkViewPopup,
    NfcEinkViewLoading,
    NfcEinkViewTextInput,
    NfcEinkViewByteInput,
    NfcEinkViewTextBox,
    NfcEinkViewWidget,
    NfcEinkViewVarItemList,
    NfcEinkViewProgress,
    NfcEinkViewImageScroll,
} NfcEinkView;

typedef enum {
    NfcEinkWriteModeStrict,
    NfcEinkWriteModeResolution,
    NfcEinkWriteModeManufacturer,
    NfcEinkWriteModeFree
} NfcEinkWriteMode;

typedef enum {
    NfcEinkLoadResultFailed,
    NfcEinkLoadResultSuccess,
    NfcEinkLoadResultCanceled,
} NfcEinkLoadResult;

typedef struct {
    NfcEinkWriteMode write_mode;
    bool invert_image;
} NfcEinkSettings;

struct NfcEinkApp {
    Gui* gui;
    DialogsApp* dialogs;
    NotificationApp* notifications;

    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    // Common Views
    Submenu* submenu;
    DialogEx* dialog_ex;
    Popup* popup;
    Loading* loading;
    TextInput* text_input;
    ByteInput* byte_input;
    TextBox* text_box;
    Widget* widget;
    VariableItemList* var_item_list;
    EinkProgress* eink_progress;
    ImageScroll* image_scroll;

    Nfc* nfc;

    NfcListener* listener;
    NfcPoller* poller;
    NfcEinkScreen* screen;
    NfcEinkScreenError last_error;
    const NfcEinkScreenInfo* info_temp;
    EinkScreenInfoArray_t arr;
    NfcEinkSettings settings;

    bool screen_loaded;
    char text_store[NFC_EINK_APP_TEXT_STORE_SIZE];
    FuriString* file_path;
    FuriString* file_name;
};

NfcEinkLoadResult nfc_eink_load_from_file_select(NfcEinkApp* instance);
void nfc_eink_blink_emulate_start(NfcEinkApp* app);
void nfc_eink_blink_write_start(NfcEinkApp* app);
void nfc_eink_blink_stop(NfcEinkApp* app);
void nfc_eink_save_settings(NfcEinkApp* instance);
