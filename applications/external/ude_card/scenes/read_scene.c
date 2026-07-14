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

#include "read_scene.h"

#include "udecard_app_i.h"

NfcCommand udecard_read_scene_nfc_poller_callback(NfcGenericEvent event, void* context) {
    App* app = context;

    NfcCommand command = NfcCommandContinue;

    MfClassicPollerEvent* mfc_event = event.event_data;

    ReadTarget* target = NULL;

    switch(mfc_event->type) {
    case MfClassicPollerEventTypeCardDetected:
        view_dispatcher_send_custom_event(app->view_dispatcher, UDECardReadSceneCardDetectedEvent);
        command = NfcCommandContinue;
        break;

    case MfClassicPollerEventTypeCardLost:
        view_dispatcher_send_custom_event(app->view_dispatcher, UDECardReadSceneCardLostEvent);
        command = NfcCommandStop;
        break;

    case MfClassicPollerEventTypeRequestMode:
        // poller wants mode, let's give him one
        mfc_event->data->poller_mode.mode = MfClassicPollerModeRead;
        break;

    case MfClassicPollerEventTypeRequestReadSector:
        // poller requests data to read sector
        target = read_target_manager_get_next(app->target_manager);
        if(target == NULL) {
            // no more targets, stop poller
            mfc_event->data->read_sector_request_data.key_provided = false;
            view_dispatcher_send_custom_event(app->view_dispatcher, UDECardReadSceneDoneEvent);
            break;
        }
        // configure read
        mfc_event->data->read_sector_request_data.sector_num = target->sector_num;
        mfc_event->data->read_sector_request_data.key = target->keyA;
        mfc_event->data->read_sector_request_data.key_type = MfClassicKeyTypeA;
        mfc_event->data->read_sector_request_data.key_provided = true;
        break;

    case MfClassicPollerEventTypeSuccess:
        // save read data
        command = NfcCommandStop;
        nfc_device_set_data(
            app->nfc_device, NfcProtocolMfClassic, nfc_poller_get_data(app->nfc_poller));
        break;

    case MfClassicPollerEventTypeFail:
        view_dispatcher_send_custom_event(app->view_dispatcher, UDECardReadSceneErrorEvent);
        break;
    default:
        break;
    }

    return command;
}

void udecard_read_scene_on_enter(void* context) {
    App* app = context;

    popup_reset(app->popup);
    popup_set_header(app->popup, "Don't move", 97, 15, AlignCenter, AlignTop);
    popup_set_icon(app->popup, 0, 8, &I_loading_24x24);
    view_dispatcher_switch_to_view(app->view_dispatcher, UDECardPopupView);

    // set read targets
    app->target_manager = read_target_manager_alloc();
    app->target_manager->head = read_target_alloc(0, app->sector_keys[0]);
    for(int i = 1; i < 10; i++) {
        if(i == 4 || i == 7) continue; // skip 4 and 7
        read_target_append(app->target_manager->head, read_target_alloc(i, app->sector_keys[i]));
    }

    // create nfc device for data saving
    app->nfc_device = nfc_device_alloc();

    // start nfc poller
    app->nfc_poller = nfc_poller_alloc(app->nfc, NfcProtocolMfClassic);
    nfc_poller_start(app->nfc_poller, udecard_read_scene_nfc_poller_callback, app);

    udecard_app_blink_start(app);
}

bool udecard_read_scene_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    bool consumed = false;

    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        case UDECardReadSceneCardDetectedEvent:
            consumed = true;
            break;
        case UDECardReadSceneCardLostEvent:
        case UDECardReadSceneErrorEvent:
            udecard_app_blink_stop(app);
            udecard_app_error_dialog(app, "Card lost / Error occured.");
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
            break;
        case UDECardReadSceneDoneEvent:
            udecard_app_blink_stop(app);
            dolphin_deed(DolphinDeedNfcReadSuccess);

            // parse card data
            UDECardLoadingResult loading_result =
                udecard_load_from_nfc_device(app->udecard, app->nfc_device);
            if(loading_result != UDECardLoadingResultSuccess) {
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, UDECardReadSceneLoadFailedEvent);
                return true;
            }
            UDECardParsingResult parsing_result =
                udecard_parse(app->udecard, app->udecard->carddata);
            if(parsing_result &
               UDECardParsingResultErrorMagic) { // other parsing errors are not fatal
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, UDECardReadSceneLoadFailedEvent);
                return true;
            } else {
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, UDECardReadSceneLoadSuccessEvent);
            }
            consumed = true;
            break;

        case UDECardReadSceneLoadFailedEvent:
            if(app->udecard->loading_result != UDECardLoadingResultSuccess) {
                udecard_app_error_dialog(
                    app, udecard_loading_error_string(app->udecard->loading_result));
            } else { // parsing error
                udecard_app_error_dialog(app, "Not a UDECard.");
            }
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
            break;

        case UDECardReadSceneLoadSuccessEvent:
            notification_message(app->notifications, &sequence_single_vibro);
            scene_manager_next_scene(app->scene_manager, UDECardResultsScene);
            consumed = true;
            break;

        default:
            break;
        }
        break;
    default:
        break;
    }

    return consumed;
}

void udecard_read_scene_on_exit(void* context) {
    App* app = context;

    nfc_poller_stop(app->nfc_poller);
    nfc_poller_free(app->nfc_poller);
    nfc_device_free(app->nfc_device);

    read_target_manager_free(app->target_manager);

    popup_reset(app->popup);
    udecard_app_blink_stop(app);
}
