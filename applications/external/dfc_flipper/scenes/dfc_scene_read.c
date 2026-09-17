#include "../dfc_i.h"
#include <dolphin/dolphin.h>

#define TAG "DfcSceneRead"

void dfc_scene_read_on_enter(void* context) {
    Dfc* dfc = context;
    dolphin_deed(DolphinDeedNfcRead);

    // Setup view
    Popup* popup = dfc->popup;
    popup_set_header(popup, "Reading", 68, 30, AlignLeft, AlignTop);
    popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);

    dfc->poller = nfc_poller_alloc(dfc->nfc, NfcProtocolIso14443_4a);
    nfc_poller_start(dfc->poller, dfc_worker_poller_callback, dfc);

    dfc_blink_start(dfc);

    view_dispatcher_switch_to_view(dfc->view_dispatcher, DfcViewPopup);
}

bool dfc_scene_read_on_event(void* context, SceneManagerEvent event) {
    Dfc* dfc = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DfcCustomEventPollerSuccess) {
            scene_manager_next_scene(dfc->scene_manager, DfcSceneReadSuccess);
            consumed = true;
        } else if(event.event == DfcCustomEventPollerError) {
            scene_manager_next_scene(dfc->scene_manager, DfcSceneReadError);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(dfc->scene_manager, DfcSceneMainMenu);
        consumed = true;
    }

    return consumed;
}

void dfc_scene_read_on_exit(void* context) {
    Dfc* dfc = context;

    if(dfc->poller) {
        nfc_poller_stop(dfc->poller);
        nfc_poller_free(dfc->poller);
        dfc->poller = NULL;
    }

    // Clear view
    popup_reset(dfc->popup);

    dfc_blink_stop(dfc);
}
