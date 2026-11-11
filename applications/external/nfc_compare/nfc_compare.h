#pragma once

#include <furi.h>
#include <furi_hal.h>

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/widget.h>

#include <notification/notification_messages.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>

#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/nfc_device.h>
#include <nfc/protocols/nfc_protocol.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>

#include "scenes/nfc_compare_scene.h"

/* UID buffers are bounded to the largest ISO14443-3A UID (10 bytes). */
#define NFC_COMPARE_UID_MAX_LEN (10U)
#define NFC_COMPARE_NFC_FOLDER  EXT_PATH("nfc")
#define NFC_COMPARE_NFC_EXT     ".nfc"

typedef enum {
    NfcCompareViewWidget,
} NfcCompareView;

typedef enum {
    NfcCompareCustomEventCardDetected = 100,
} NfcCompareCustomEvent;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;
    DialogsApp* dialogs;
    Widget* widget;

    FuriString* file_path;

    NfcDevice* saved_device;
    Nfc* nfc;
    NfcPoller* poller;

    uint8_t saved_uid[NFC_COMPARE_UID_MAX_LEN];
    size_t saved_uid_len;

    uint8_t read_uid[NFC_COMPARE_UID_MAX_LEN];
    size_t read_uid_len;

    bool match;
} NfcCompareApp;
