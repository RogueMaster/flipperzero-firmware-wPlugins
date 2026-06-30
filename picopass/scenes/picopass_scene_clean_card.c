#include "../picopass_i.h"
#include "../picopass_keys.h"

typedef enum {
    CleanCardKeyAttemptHid,
    CleanCardKeyAttemptKi,
    CleanCardKeyAttemptDone,
} CleanCardKeyAttempt;

NfcCommand picopass_scene_clean_poller_callback(PicopassPollerEvent event, void* context) {
    Picopass* picopass = context;
    NfcCommand command = NfcCommandContinue;

    if(event.type == PicopassPollerEventTypeRequestMode) {
        event.data->req_mode.mode = PicopassPollerModeClean;
    } else if(event.type == PicopassPollerEventTypeRequestKey) {
        uint32_t key_attempt =
            scene_manager_get_scene_state(picopass->scene_manager, PicopassSceneCleanCard);
        if(key_attempt == CleanCardKeyAttemptHid) {
            memcpy(event.data->req_key.key, picopass_iclass_key, PICOPASS_BLOCK_LEN);
            event.data->req_key.is_elite_key = false;
            event.data->req_key.is_key_provided = true;
        } else if(key_attempt == CleanCardKeyAttemptKi) {
            memcpy(event.data->req_key.key, picopass_ki_debit_key, PICOPASS_BLOCK_LEN);
            event.data->req_key.is_elite_key = false;
            event.data->req_key.is_key_provided = true;
        } else {
            event.data->req_key.is_key_provided = false;
        }
    } else if(event.type == PicopassPollerEventTypeSuccess) {
        view_dispatcher_send_custom_event(
            picopass->view_dispatcher, PicopassCustomEventPollerSuccess);
    } else if(event.type == PicopassPollerEventTypeAuthFail) {
        uint32_t key_attempt =
            scene_manager_get_scene_state(picopass->scene_manager, PicopassSceneCleanCard);
        if(key_attempt == CleanCardKeyAttemptHid) {
            // HID key failed, try KI key next
            scene_manager_set_scene_state(
                picopass->scene_manager, PicopassSceneCleanCard, CleanCardKeyAttemptKi);
        } else {
            // Both keys failed
            scene_manager_set_scene_state(
                picopass->scene_manager, PicopassSceneCleanCard, CleanCardKeyAttemptDone);
            view_dispatcher_send_custom_event(
                picopass->view_dispatcher, PicopassCustomEventPollerFail);
        }
    } else if(event.type == PicopassPollerEventTypeFail) {
        view_dispatcher_send_custom_event(
            picopass->view_dispatcher, PicopassCustomEventPollerFail);
    }

    return command;
}

void picopass_scene_clean_card_on_enter(void* context) {
    Picopass* picopass = context;

    // Setup view
    Popup* popup = picopass->popup;
    popup_set_header(popup, "Cleaning\nMKF tag", 68, 30, AlignLeft, AlignTop);
    popup_set_icon(popup, 0, 3, &I_RFIDDolphinSend_97x61);
    scene_manager_set_scene_state(
        picopass->scene_manager, PicopassSceneCleanCard, CleanCardKeyAttemptHid);

    // Start worker
    view_dispatcher_switch_to_view(picopass->view_dispatcher, PicopassViewPopup);

    picopass->poller = picopass_poller_alloc(picopass->nfc);
    picopass_poller_start(picopass->poller, picopass_scene_clean_poller_callback, picopass);

    picopass_blink_start(picopass);
}

bool picopass_scene_clean_card_on_event(void* context, SceneManagerEvent event) {
    Picopass* picopass = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == PicopassCustomEventPollerFail) {
            scene_manager_next_scene(picopass->scene_manager, PicopassSceneCleanCardFailure);
            consumed = true;
        } else if(event.event == PicopassCustomEventPollerSuccess) {
            scene_manager_next_scene(picopass->scene_manager, PicopassSceneCleanCardSuccess);
            consumed = true;
        }
    }
    return consumed;
}

void picopass_scene_clean_card_on_exit(void* context) {
    Picopass* picopass = context;

    // Stop worker
    picopass_poller_stop(picopass->poller);
    picopass_poller_free(picopass->poller);
    // Clear view
    popup_reset(picopass->popup);

    picopass_blink_stop(picopass);
}
