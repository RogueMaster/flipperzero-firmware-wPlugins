#include "../seos_i.h"
#include <dolphin/dolphin.h>

#define TAG "SeosSceneBleReader"

void seos_scene_ble_peripheral_on_enter(void* context) {
    Seos* seos = context;
    dolphin_deed(DolphinDeedNfcRead);

    // Setup view
    Popup* popup = seos->popup;
    popup_set_header(popup, "Starting", 68, 30, AlignLeft, AlignTop);
    if(seos->flow_mode == FLOW_READER) {
        popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);
    } else if(seos->flow_mode == FLOW_CRED) {
        popup_set_icon(popup, 0, 3, &I_RFIDDolphinSend_97x61);
    }

    if(seos->has_external_ble) {
        seos->seos_characteristic = seos_characteristic_alloc(seos);
        seos_characteristic_start(seos->seos_characteristic, seos->flow_mode);
    } else {
        seos->native_peripheral = seos_native_peripheral_alloc(seos);
        seos_native_peripheral_start(seos->native_peripheral, seos->flow_mode);
    }

    seos_blink_start(seos);

    view_dispatcher_switch_to_view(seos->view_dispatcher, SeosViewPopup);
}

bool seos_scene_ble_peripheral_on_event(void* context, SceneManagerEvent event) {
    Seos* seos = context;
    Popup* popup = seos->popup;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SeosCustomEventPollerSuccess) {
            notification_message(seos->notifications, &sequence_success);
            scene_manager_next_scene(seos->scene_manager, SeosSceneReadSuccess);
            consumed = true;
        } else if(event.event == SeosCustomEventPollerError) {
            scene_manager_next_scene(seos->scene_manager, SeosSceneReadError);
            consumed = true;
        } else if(event.event == SeosCustomEventHCIInit) {
            popup_set_header(popup, "Init", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == SeosCustomEventAdvertising) {
            popup_set_header(popup, "Advertising", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == SeosCustomEventConnected) {
            popup_set_header(popup, "Connected", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == SeosCustomEventAuthenticated) {
            popup_set_header(popup, "Auth'd", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == SeosCustomEventSIORequested) {
            popup_set_header(popup, "SIO\nRequested", 68, 30, AlignLeft, AlignTop);
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

void seos_scene_ble_peripheral_on_exit(void* context) {
    Seos* seos = context;

    if(seos->seos_characteristic) {
        seos_characteristic_stop(seos->seos_characteristic);
        seos_characteristic_free(seos->seos_characteristic);
        seos->seos_characteristic = NULL;
    }
    if(seos->native_peripheral) {
        seos_native_peripheral_stop(seos->native_peripheral);
        seos_native_peripheral_free(seos->native_peripheral);
        seos->native_peripheral = NULL;
    }

    // Clear view
    popup_reset(seos->popup);

    seos_blink_stop(seos);
}
