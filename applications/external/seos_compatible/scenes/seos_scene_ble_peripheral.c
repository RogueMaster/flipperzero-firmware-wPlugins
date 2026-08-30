#include "../seos_i.h"
#include "../seos_ble.h"
#include <dolphin/dolphin.h>
#include <seos_icons.h>

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

    if(seos_ble_acquire_role(seos, SeosBleRolePeripheral)) {
        seos_ble_start(seos, seos->flow_mode);
    } else {
        popup_set_header(popup, "No BLE\nsupport", 68, 30, AlignLeft, AlignTop);
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

    seos_ble_release(seos);

    // Clear view
    popup_reset(seos->popup);

    seos_blink_stop(seos);
}
