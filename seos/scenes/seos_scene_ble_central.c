#include "../seos_i.h"
#include "../seos_central.h"
#include "../seos_common.h"
#include <dolphin/dolphin.h>

#define TAG "SeosSceneBleCentral"

void seos_scene_ble_central_on_enter(void* context) {
    Seos* seos = context;
    dolphin_deed(DolphinDeedNfcRead);

    // Setup view
    Popup* popup = seos->popup;
    popup_set_header(popup, "Starting...", 68, 20, AlignLeft, AlignTop);
    // popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);

    seos->seos_central = seos_central_alloc(seos);
    seos_central_start(seos->seos_central, seos->flow_mode);

    seos_blink_start(seos);

    view_dispatcher_switch_to_view(seos->view_dispatcher, SeosViewPopup);
}

bool seos_scene_ble_central_on_event(void* context, SceneManagerEvent event) {
    Seos* seos = context;
    Popup* popup = seos->popup;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SeosCustomEventPollerSuccess) {
            notification_message(seos->notifications, &sequence_success);
            scene_manager_next_scene(seos->scene_manager, SeosSceneReadSuccess);
            consumed = true;
        } else if(event.event == SeosCustomEventPollerError) {
            notification_message(seos->notifications, &sequence_error);
            scene_manager_next_scene(seos->scene_manager, SeosSceneReadError);
            consumed = true;
        } else if(event.event == SeosCustomEventHCIInit) {
            popup_set_header(popup, "Init", 68, 20, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == SeosCustomEventScan) {
            popup_set_header(popup, "Scanning...", 68, 20, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == SeosCustomEventFound) {
            popup_set_header(popup, "Device\nfound", 68, 20, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == SeosCustomEventConnected) {
            popup_set_header(popup, "Connected", 68, 20, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == SeosCustomEventAuthenticated) {
            popup_set_header(popup, "Auth'd", 68, 20, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == SeosCustomEventSIORequested) {
            popup_set_header(popup, "SIO\nRequested", 68, 20, AlignLeft, AlignTop);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        if(seos->credential->sio_len > 0) {
            scene_manager_search_and_switch_to_previous_scene(
                seos->scene_manager, SeosSceneSavedMenu);
        } else {
            scene_manager_previous_scene(seos->scene_manager);
        }
        consumed = true;
    }

    return consumed;
}

void seos_scene_ble_central_on_exit(void* context) {
    Seos* seos = context;

    if(seos->seos_central) {
        FURI_LOG_D(TAG, "Cleanup");
        seos_central_stop(seos->seos_central);
        seos_central_free(seos->seos_central);
        seos->seos_central = NULL;
    }

    // Clear view
    popup_reset(seos->popup);

    seos_blink_stop(seos);
}
