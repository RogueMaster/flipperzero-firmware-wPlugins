/*
This file is part of UDECard App.
A Flipper Zero application to analyse student ID cards from the University of Duisburg-Essen (Intercard)

Copyright (C) 2025-2026 Alexander Hahn

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include "udecard_app.h"

#include <furi.h>

// gui
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/popup.h>
#include <gui/modules/widget.h>

#include <notification/notification_messages.h>

#include <dialogs/dialogs.h>

// nfc
#include <nfc/nfc_scanner.h>
#include <nfc/nfc_poller.h>
#include <nfc/nfc_device.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <nfc/protocols/mf_classic/mf_classic_poller.h>
#include <toolbox/keys_dict.h>

// scenes
#include "scenes/main_menu_scene.h"
#include "scenes/detect_scene.h"
#include "scenes/read_scene.h"
#include "scenes/load_scene.h"
#include "scenes/results_scene.h"
#include "scenes/about_scene.h"

// helpers
#include "helpers/udecard.h"
#include "helpers/target_manager.h"
#include "helpers/util.h"

// icons
#include <udecard_icons.h>

#include <dolphin/dolphin.h>

typedef struct App {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    TextBox* textbox;
    Popup* popup;
    Widget* widget;

    DialogsApp* dialogs_app;
    DialogMessage* dialog_message;
    NotificationApp* notifications;

    Nfc* nfc;
    NfcScanner* nfc_scanner; // allocated and free'd on demand
    NfcPoller* nfc_poller; // allocated and free'd on demand
    NfcDevice* nfc_device; // allocated and free'd on demand

    ReadTargetManager* target_manager; // allocated and free'd on demand

    uint8_t sector_keys[16][UDECARD_KEY_SIZE];

    FuriString* file_path;

    UDECard* udecard;
} App;

typedef enum {
    UDECardSubmenuView,
    UDECardTextBoxView,
    UDECardPopupView,
    UDECardWidgetView,
} UDECardView;

typedef enum {
    UDECardMainMenuScene,
    UDECardDetectScene,
    UDECardReadScene,
    UDECardLoadScene,
    UDECardResultsScene,
    UDECardAboutScene,
    UDECardSceneCount
} UDECardScene;

void udecard_app_blink_start(App* app);
void udecard_app_blink_stop(App* app);
void udecard_app_error_dialog(App* app, const char* message);
