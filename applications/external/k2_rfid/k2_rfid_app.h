#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/popup.h>
#include <gui/modules/widget.h>
#include <gui/modules/text_input.h>
#include <notification/notification_messages.h>
#include <dialogs/dialogs.h>

#include "crypto/k2_crypto.h"
#include "data/k2_database.h"
#include "nfc/k2_worker.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    K2ViewSubmenu,
    K2ViewVariableItemList,
    K2ViewPopup,
    K2ViewWidget,
    K2ViewTextInput,
} K2View;

typedef enum {
    K2CustomEventCardDetected = 100,
    K2CustomEventSuccess,
    K2CustomEventAuthFailed,
    K2CustomEventFailed,
    K2CustomEventEmulating,
    K2CustomEventStopped,
    K2CustomEventPopupBack,
    K2CustomEventTagInfoSave,
    K2CustomEventTagInfoEmulate,
    K2CustomEventTagInfoEdit,
} K2CustomEvent;

typedef struct K2RfidApp K2RfidApp;

struct K2RfidApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;
    DialogsApp* dialogs;

    K2Worker* worker;

    Submenu* submenu;
    VariableItemList* var_item_list;
    Popup* popup;
    Widget* widget;
    TextInput* text_input;

    /* Emulation */
    NfcListener* listener;
    MfClassicData* emulate_data;

    /* Current spool configuration */
    K2SpoolConfig config;
    size_t printer_idx;
    size_t material_idx;
    size_t color_idx;
    size_t weight_idx;
    char serial_str[8];

    /* Last scanned or loaded spool */
    K2SpoolInfo last_spool;

    /* Text input buffer */
    char text_input_buf[16];
    const char* text_input_header;

    /* File path for saving/loading */
    char file_path[128];
};

void k2_rfid_app_set_default_config(K2RfidApp* app);
void k2_rfid_app_sync_config(K2RfidApp* app);
void k2_rfid_app_randomize_serial(K2RfidApp* app);

#ifdef __cplusplus
}
#endif
