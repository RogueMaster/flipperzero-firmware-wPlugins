#pragma once

#include "keys_dict.h"
#include <furi.h>

#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_scanner.h>
#include <nfc/nfc_poller.h>
#include <nfc/nfc_listener.h>

#include <notification/notification_messages.h>
#include <dialogs/dialogs.h>

#include <datetime/datetime.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/menu.h>
#include <gui/modules/popup.h>
#include <gui/modules/widget.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/text_input.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/number_input.h>
#include <gui/modules/date_time_input.h>
#include <gui/modules/variable_item_list.h>
#include <assets_icons.h>

#include "scenes/saflip_scene.h"
#include "util/saflok/saflok.h"

#define TAG "Saflip"

typedef enum {
    SaflipViewMenu,
    SaflipViewPopup,
    SaflipViewWidget,
    SaflipViewDialog,
    SaflipViewSubmenu,
    SaflipViewTextBox,
    SaflipViewTextInput,
    SaflipViewByteInput,
    SaflipViewNumberInput,
    SaflipViewDateTimeInput,
    SaflipViewVariableItemList,
} SaflipViews;

typedef enum {
    SaflipDetectCardModeRead,
    SaflipDetectCardModeWrite,
} SaflipDetectCardMode;

typedef enum {
    SaflipSaveModeSaflip,
    SaflipSaveModeNFC,
} SaflipSaveMode;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    Menu* menu;
    Popup* popup;
    Widget* widget;
    Submenu* submenu;
    DialogEx* dialog;
    TextBox* text_box;
    TextInput* text_input;
    ByteInput* byte_input;
    NumberInput* number_input;
    DateTimeInput* date_time_input;
    VariableItemList* variable_item_list;

    BasicAccessData* data;
    DateTime* date_time;
    uint8_t* uid;
    size_t uid_len;
    char text_store[100];
    uint8_t num_store;

    // Store up to 72 log entries (the max that can fit on a MFC1k)
    // TODO: Is there a better way to store this that won't use unneccesary memory?
    LogEntry log[72];
    size_t log_entries;

    // Store up to 64 variable keys
    // TODO: Is there a better way to store this that won't use unneccesary memory?
    VariableKey keys[64];
    size_t variable_keys;
    VariableKeysOptionalFunction variable_keys_optional_function;

    NotificationApp* notifications;
    DialogsApp* dialogs_app;
    Storage* storage;

    Nfc* nfc;
    NfcDevice* nfc_device;
    NfcScanner* nfc_scanner;
    NfcPoller* nfc_poller;
    NfcListener* nfc_listener;

    KeysDict* nfc_keys_dictionary;
} SaflipApp;

void saflip_reset_data(SaflipApp* app);
