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

#include "detect_scene.h"

#include "udecard_app_i.h"

bool udecard_detect_scene_is_event_from_mfc(NfcScannerEvent event) {
    if(event.type == NfcScannerEventTypeDetected) {
        for(unsigned int i = 0; i < event.data.protocol_num; i++) {
            if(event.data.protocols[i] == NfcProtocolMfClassic) {
                return true;
            }
        }
    }
    return false;
}

void udecard_detect_scene_nfc_scanner_callback(NfcScannerEvent event, void* context) {
    App* app = context;

    switch(event.type) {
    case NfcScannerEventTypeDetected:
        if(udecard_detect_scene_is_event_from_mfc(event)) { // mfclassic?
            view_dispatcher_send_custom_event(
                app->view_dispatcher, UDECardDetectSceneDetectedEvent);
        } else {
            view_dispatcher_send_custom_event(app->view_dispatcher, UDECardDetectSceneErrorEvent);
        }
        break;
    default:
        break;
    }
}

void udecard_detect_scene_on_enter(void* context) {
    App* app = context;

    popup_reset(app->popup);
    popup_set_header(app->popup, "Reading", 97, 15, AlignCenter, AlignTop);
    popup_set_text(
        app->popup, "Put UDECard\nnext to\nFlipper's back", 94, 27, AlignCenter, AlignTop);
    popup_set_icon(app->popup, 0, 8, &I_ApplyFlipperToUDE_60x50);
    view_dispatcher_switch_to_view(app->view_dispatcher, UDECardPopupView);

    if(!udecard_gather_keys(app->sector_keys)) {
        udecard_app_error_dialog(app, "Gathering keys failed.");
        view_dispatcher_send_custom_event(app->view_dispatcher, UDECardDetectSceneFatalErrorEvent);
    }

    // start nfc scanner
    app->nfc_scanner = nfc_scanner_alloc(app->nfc);
    nfc_scanner_start(app->nfc_scanner, udecard_detect_scene_nfc_scanner_callback, app);

    udecard_app_blink_start(app);
}

bool udecard_detect_scene_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        case UDECardDetectSceneDetectedEvent:
            scene_manager_next_scene(app->scene_manager, UDECardReadScene);
            consumed = true;
            break;
        case UDECardDetectSceneErrorEvent:
            nfc_scanner_stop(app->nfc_scanner);
            nfc_scanner_start(
                app->nfc_scanner, udecard_detect_scene_nfc_scanner_callback, context);
            break;
        case UDECardDetectSceneFatalErrorEvent:
            scene_manager_previous_scene(app->scene_manager);
        }
        break;
    default:
        break;
    }

    return consumed;
}

void udecard_detect_scene_on_exit(void* context) {
    App* app = context;
    nfc_scanner_stop(app->nfc_scanner);
    nfc_scanner_free(app->nfc_scanner);

    popup_reset(app->popup);
    udecard_app_blink_stop(app);
}
