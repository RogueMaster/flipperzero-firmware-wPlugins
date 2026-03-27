#include "../uk_mbirth_sonicare.h"
#include "core/core_defines.h"
#include "gui/scene_manager.h"
#include "gui/view_dispatcher.h"
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_scanner.h>
#include <nfc/protocols/nfc_protocol.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <dolphin/dolphin.h>
//#include <nfc/helpers/protocol_support/nfc_protocol_support_common.h>

void nfc_scene_detect_scan_callback(NfcScannerEvent event, void* context) {
    furi_assert(context);
    
    Sonicare* app = context;
    
    if (event.type == NfcScannerEventTypeDetected) {
        //nfc_detected_protocols_set(app->detected_protocols, event.data.protocols, event.data.protocol_num);
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcCustomEventWorkerExit);
    }
}

NfcCommand nfc_scene_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(event.protocol == NfcProtocolIso14443_3a);

    Sonicare* app = context;
    const Iso14443_3aPollerEvent* ev = event.event_data;
    
    if (ev->type == Iso14443_3aPollerEventTypeReady) {
        nfc_device_set_data(app->nfc_device, NfcProtocolIso14443_3a, nfc_poller_get_data(app->poller));
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcCustomEventWorkerExit);
        return NfcCommandStop;
    }

    return NfcCommandContinue;
}

void sonicare_scene_read_on_enter(void* context) {
    Sonicare* app = context;
    Popup* popup = app->popup;

    popup_reset(popup);
    popup_set_header(popup, "Reading", 97, 15, AlignCenter, AlignTop);
    popup_set_text(popup, "Hold brush stem next\nto Flipper's back", 94, 27, AlignCenter, AlignTop);
    //popup_set_icon(app->popup, 0, 8, &I_NFC_manual_60x50);
    view_dispatcher_switch_to_view(app->view_dispatcher, SonicareViewPopup);
    
    //nfc_detected_protocols_reset(app->detected_protocols);
    app->nfc = nfc_alloc();
    app->nfc_device = nfc_device_alloc();
    // we probably don't need to "scan" but can instead directly "poll" for NTAG213 (ISO 14443-3a) data
    //app->scanner = nfc_scanner_alloc(app->nfc);
    //nfc_scanner_start(app->scanner, nfc_scene_detect_scan_callback, app);
    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolIso14443_3a);
    nfc_poller_start(app->poller, nfc_scene_poller_callback, app);
}

bool sonicare_scene_read_on_event(void* context, SceneManagerEvent event) {
    Sonicare* app = context;
    bool consumed = false;

    if (event.type == SceneManagerEventTypeCustom) {
        if (event.event == NfcCustomEventWorkerExit) {
            //if (nfc_detected_protocols_get_num(app->detected_protocols) > 1) {
            if (true) {
                //notification_message(app->notifications, &sequence_single_vibro);
                notification_message(app->notifications, &sequence_success);
                scene_manager_next_scene(app->scene_manager, SonicareSceneReadComplete);
            } else {
                notification_message(app->notifications, &sequence_error);
                scene_manager_next_scene(app->scene_manager, SonicareSceneReadComplete);
            }
            consumed = true;
        }
    }

    return consumed;
}

void sonicare_scene_read_on_exit(void* context) {
    Sonicare* app = context;
    //notification_message(app->notifications, &sequence_blink_stop);

    nfc_poller_stop(app->poller);
    nfc_poller_free(app->poller);
    //nfc_scanner_stop(app->scanner);
    //nfc_scanner_free(app->scanner);
    nfc_device_free(app->nfc_device);
    nfc_free(app->nfc);
    popup_reset(app->popup);
}
