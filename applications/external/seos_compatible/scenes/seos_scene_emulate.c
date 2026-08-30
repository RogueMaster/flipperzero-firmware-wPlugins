#include "../seos_i.h"
#include <dolphin/dolphin.h>
#include <seos_icons.h>

#define TAG "SeosSceneEmulate"

void seos_scene_emulate_on_enter(void* context) {
    Seos* seos = context;
    dolphin_deed(DolphinDeedNfcEmulate);

    // Setup view
    Popup* popup = seos->popup;
    popup_set_header(popup, "Emulating", 68, 30, AlignLeft, AlignTop);
    popup_set_icon(popup, 0, 3, &I_RFIDDolphinSend_97x61);

    nfc_device_load(seos->nfc_device, APP_ASSETS_PATH("seos.nfc"));

    const Iso14443_4aData* data = nfc_device_get_data(seos->nfc_device, NfcProtocolIso14443_4a);
    seos->listener = nfc_listener_alloc(seos->nfc, NfcProtocolIso14443_4a, data);
    nfc_listener_start(seos->listener, seos_worker_listener_callback, seos);

    seos_blink_start(seos);

    view_dispatcher_switch_to_view(seos->view_dispatcher, SeosViewPopup);
}

bool seos_scene_emulate_on_event(void* context, SceneManagerEvent event) {
    Seos* seos = context;
    Popup* popup = seos->popup;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SeosCustomEventEmulate) {
            popup_set_header(popup, "Emulating", 68, 30, AlignLeft, AlignTop);
        } else if(event.event == SeosCustomEventAIDSelected) {
            popup_set_header(popup, "AID\nSelected", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == SeosCustomEventADFMatched) {
            popup_set_header(popup, "ADF\nMatched", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == SeosCustomEventAuthenticated) {
            popup_set_header(popup, "Auth'd", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == SeosCustomEventSIORequested) {
            popup_set_header(popup, "SIO\nRequested", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == SeosCustomEventSIOWritten) {
            /* A reader stored a credential on us. Put it back where this one
             * came from, so the change survives leaving the screen. */
            bool saved = seos_credential_save_to_load_path(seos->credential);
            popup_set_header(
                popup,
                saved ? "SIO\nWritten" : "SIO\nWritten\n(unsaved)",
                68,
                30,
                AlignLeft,
                AlignTop);
            notification_message(seos->notifications, &sequence_success);
            consumed = true;
        }
    }

    return consumed;
}

void seos_scene_emulate_on_exit(void* context) {
    Seos* seos = context;

    if(seos->listener) {
        nfc_listener_stop(seos->listener);
        nfc_listener_free(seos->listener);
        seos->listener = NULL;
    }

    // Clear view
    popup_reset(seos->popup);

    seos_blink_stop(seos);
}
