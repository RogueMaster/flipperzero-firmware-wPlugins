#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/widget.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/popup.h>
#include <gui/modules/loading.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>
#include <lib/nfc/nfc.h>
#include <lib/nfc/nfc_device.h>
#include <lib/nfc/nfc_listener.h>
#include <lib/flipper_format/flipper_format.h>
#include <notification/notification_messages.h>

#include "qrcode.h"

#define TAG "NfcQrApp"

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;
    DialogsApp* dialogs;
    Storage* storage;

    // NFC
    Nfc* nfc;
    NfcDevice* nfc_device;
    NfcListener* nfc_listener;
    FuriString* nfc_file_path;

    // QR Code
    QRCode* qrcode;
    FuriString* qr_file_path;
    FuriString* qr_message;
    bool qr_loading;

    // Views
    Submenu* submenu;
    View* emulate_view;
    Widget* widget;
    Popup* popup;
    Loading* loading;
    
    // Debug
    uint32_t current_view;
} NfcQrApp;

typedef enum {
    NfcQrAppViewSubmenu,
    NfcQrAppViewEmulate,
    NfcQrAppViewCredits,
} NfcQrAppView;

typedef enum {
    NfcQrCustomEventMenuSelectQr,
    NfcQrCustomEventMenuSelectNfc,
    NfcQrCustomEventMenuStart,
    NfcQrCustomEventMenuCredits,
    NfcQrCustomEventEmulateBack,
    NfcQrCustomEventLoadFile,
    NfcQrCustomEventAllocNfc,
    NfcQrCustomEventStartNfc,
} NfcQrCustomEvent;

typedef struct {
    QRCode* qrcode;
} NfcQrAppModel;
