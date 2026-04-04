#include "../uk_mbirth_sonicare.h"
#include <uk_mbirth_sonicare_icons.h>

#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_scanner.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <dolphin/dolphin.h>

void nfc_scene_detect_scan_callback(NfcScannerEvent event, void* context) {
    furi_assert(context);
    
    Sonicare* app = context;
    
    if (event.type == NfcScannerEventTypeDetected) {
        //nfc_detected_protocols_set(app->detected_protocols, event.data.protocols, event.data.protocol_num);
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcCustomEventWorkerExit);
    }
}

NfcCommand nfc_scene_poller_callback(NfcGenericEvent event, void* context) {
    Sonicare* app = context;

    const MfUltralightPollerEvent* ev = event.event_data;
    
    if (event.protocol == NfcProtocolMfUltralight && ev->type == MfUltralightPollerEventTypeReadSuccess) {
        FURI_LOG_I("sonicare_scene_read", "NFC Poller reports Read Success");
        nfc_device_set_data(app->nfc_device, NfcProtocolMfUltralight, nfc_poller_get_data(app->poller));
        FURI_LOG_D("sonicare_scene_read", "Pulling Mifare Ultralight data from poller");
        const MfUltralightData* ul_data = nfc_device_get_data(app->nfc_device, NfcProtocolMfUltralight);
        
        mf_ultralight_copy(app->nfc_data, ul_data);

        FURI_LOG_I("sonicare_scene_read", "Dataset has %i of %i pages read from Mifare Ultralight", ul_data->pages_read, ul_data->pages_total);
        
        if (ul_data->pages_read == 43) {
            // only stop when we have all data
            view_dispatcher_send_custom_event(app->view_dispatcher, NfcCustomEventWorkerExit);
            return NfcCommandStop;
        }
    }

    return NfcCommandContinue;
}

void sonicare_scene_read_on_enter(void* context) {
    Sonicare* app = context;
    Popup* popup = app->popup;

    popup_reset(popup);
    // image is 39px wide, screen is 128px --> 89px left, centre point X: 39 + (89/2) = 83
    popup_set_header(popup, "Reading", 83, 8, AlignCenter, AlignTop);
    popup_set_text(popup, "Hold brush stem\nnext to\nFlipper's back", 83, 27, AlignCenter, AlignTop);
    popup_set_icon(app->popup, 0, 0, &I_sonicare_read);
    view_dispatcher_switch_to_view(app->view_dispatcher, SonicareViewPopup);
    
    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfUltralight);
    nfc_poller_start(app->poller, nfc_scene_poller_callback, app);
}

bool sonicare_scene_read_on_event(void* context, SceneManagerEvent event) {
    Sonicare* app = context;
    bool consumed = false;

    if (event.type == SceneManagerEventTypeCustom) {
        if (event.event == NfcCustomEventWorkerExit) {
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
    popup_reset(app->popup);
}
