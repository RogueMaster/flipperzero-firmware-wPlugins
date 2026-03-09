#include "../seader_i.h"
#include "seader_scene_read_common.h"
#include <dolphin/dolphin.h>

void seader_scene_read_on_enter(void* context) {
    Seader* seader = context;
    dolphin_deed(DolphinDeedNfcRead);

    // Setup view
    Popup* popup = seader->popup;
    popup_set_header(popup, "Detecting\nHF card...", 68, 30, AlignLeft, AlignTop);
    popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);

    // Start worker
    view_dispatcher_switch_to_view(seader->view_dispatcher, SeaderViewPopup);

    seader_scene_read_prepare(seader);
    seader_credential_clear(seader->credential);
    if(seader->selected_read_type == SeaderCredentialTypeNone) {
        seader->detected_card_type_count = 0;
        memset(seader->detected_card_types, 0, sizeof(seader->detected_card_types));
    }
    seader_worker_start(
        seader->worker,
        SeaderWorkerStateReading,
        seader->uart,
        seader_sam_check_worker_callback,
        seader);

    seader_blink_start(seader);
}

bool seader_scene_read_on_event(void* context, SceneManagerEvent event) {
    Seader* seader = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SeaderCustomEventWorkerExit) {
            scene_manager_next_scene(seader->scene_manager, SeaderSceneReadCardSuccess);
            consumed = true;
        } else if(event.event == SeaderCustomEventPollerDetect) {
            Popup* popup = seader->popup;
            popup_set_header(popup, "DON'T\nMOVE", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == SeaderWorkerEventSuccess) {
            scene_manager_next_scene(seader->scene_manager, SeaderSceneReadCardSuccess);
            consumed = true;
        } else if(event.event == SeaderWorkerEventSelectCardType) {
            scene_manager_next_scene(seader->scene_manager, SeaderSceneReadCardType);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        seader->selected_read_type = SeaderCredentialTypeNone;
        seader->detected_card_type_count = 0;
        memset(seader->detected_card_types, 0, sizeof(seader->detected_card_types));
        scene_manager_search_and_switch_to_previous_scene(
            seader->scene_manager, SeaderSceneSamPresent);
        consumed = true;
    }

    return consumed;
}

void seader_scene_read_on_exit(void* context) {
    Seader* seader = context;
    seader_worker_stop(seader->worker);
    seader_scene_read_cleanup(seader);
}
