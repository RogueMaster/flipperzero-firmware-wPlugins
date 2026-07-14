#pragma once

#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <notification/notification.h>
#include <nfc_listener/nfc_sniffer.h>
#include <storage/storage.h>

// Defines the data for use within each scene
typedef struct {
    TextBox* log_display;
    FuriString* log_text;

    NfcLogger* nfc_sniffer;
} LogScene;

typedef struct {
    Submenu* protocol_menu;
} ProtocolSelectScene;

typedef struct {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;
    Storage* storage;
    Gui* gui;

    // The scenes that will be displayed by the manager
    LogScene* log_scene;
    ProtocolSelectScene* protocol_select_scene;

    // Log File
    File* log_file;

    // NFC Data
    Nfc* nfc;
    NfcTech nfc_protocol;
} UI;

UI* ui_alloc();
void ui_start(UI* ui);
void ui_free(UI* ui);
