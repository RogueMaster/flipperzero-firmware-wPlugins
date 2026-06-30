#include "../seos_i.h"
#include "seos_scene.h"
#include <dolphin/dolphin.h>

#define TAG "SeosSceneWrite"

void seos_scene_write_on_enter(void* context) {
    Seos* seos = context;
    dolphin_deed(DolphinDeedNfcRead);

    // Setup view
    Popup* popup = seos->popup;
    popup_set_header(popup, "Writing", 68, 30, AlignLeft, AlignTop);
    popup_set_icon(popup, 0, 3, &I_RFIDDolphinSend_97x61);

    seos->credential->write = true;
    seos->poller = nfc_poller_alloc(seos->nfc, NfcProtocolIso14443_4a);
    nfc_poller_start(seos->poller, seos_worker_poller_callback, seos);

    seos_blink_start(seos);

    view_dispatcher_switch_to_view(seos->view_dispatcher, SeosViewPopup);
}

bool seos_scene_write_on_event(void* context, SceneManagerEvent event) {
    Seos* seos = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SeosCustomEventPollerSuccess) {
            scene_manager_next_scene(seos->scene_manager, SeosSceneWriteSuccess);
            consumed = true;
        } else if(event.event == SeosCustomEventPollerError) {
            scene_manager_next_scene(seos->scene_manager, SeosSceneWriteError);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(seos->scene_manager, SeosSceneSavedMenu);
        consumed = true;
    }

    return consumed;
}

void seos_scene_write_on_exit(void* context) {
    Seos* seos = context;

    if(seos->poller) {
        nfc_poller_stop(seos->poller);
        nfc_poller_free(seos->poller);
        seos->poller = NULL;
    }

    // Clear view
    popup_reset(seos->popup);

    seos_blink_stop(seos);
}
