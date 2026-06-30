#include "../seader_i.h"
#include "seader_scene_read_common.h"
#include <dolphin/dolphin.h>

void seader_read_config_card_worker_callback(uint32_t event, void* context) {
    UNUSED(event);
    Seader* seader = context;
    view_dispatcher_send_custom_event(seader->view_dispatcher, SeaderCustomEventWorkerExit);
}

void seader_scene_read_config_card_on_enter(void* context) {
    Seader* seader = context;
    seader_worker_acquire(seader);

    // Setup view
    Popup* popup = seader->popup;
    popup_set_header(popup, "Detecting\nConfig\ncard", 68, 30, AlignLeft, AlignTop);
    popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);

    // Start worker
    view_dispatcher_switch_to_view(seader->view_dispatcher, SeaderViewPopup);

    seader_scene_read_prepare(seader);
    seader_credential_clear(seader->credential);
    seader->credential->type = SeaderCredentialTypeConfig;

    seader_worker_start(
        seader->worker,
        SeaderWorkerStateReading,
        seader->uart,
        seader_sam_check_worker_callback,
        seader);

    seader_blink_start(seader);
}

bool seader_scene_read_config_card_on_event(void* context, SceneManagerEvent event) {
    Seader* seader = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SeaderCustomEventWorkerExit || event.event == SeaderWorkerEventSuccess) {
            scene_manager_next_scene(seader->scene_manager, SeaderSceneReadConfigCardSuccess);
            consumed = true;
        } else if(event.event == SeaderWorkerEventFail) {
            scene_manager_search_and_switch_to_previous_scene(
                seader->scene_manager, SeaderSceneSamPresent);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(
            seader->scene_manager, SeaderSceneSamPresent);
        consumed = true;
    }

    return consumed;
}

void seader_scene_read_config_card_on_exit(void* context) {
    Seader* seader = context;
    if(seader->worker) {
        seader_worker_stop(seader->worker);
    }
    seader_scene_read_cleanup(seader);
    seader_worker_release(seader);
}
