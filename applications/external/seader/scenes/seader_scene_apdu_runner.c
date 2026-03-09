#include "../seader_i.h"
#include <dolphin/dolphin.h>

#define TAG "Seader:Scene:APDURunner"

char seader_scene_apdu_runner_update_text[24];

void seader_apdu_runner_worker_callback(uint32_t event, void* context) {
    Seader* seader = context;
    view_dispatcher_send_custom_event(seader->view_dispatcher, event);
}

void seader_scene_apdu_runner_on_enter(void* context) {
    Seader* seader = context;
    // Setup view
    Popup* popup = seader->popup;
    popup_set_header(popup, "APDU Runner", 68, 30, AlignLeft, AlignTop);

    // TODO: Make icon for interaction with SAM
    popup_set_icon(popup, 0, 3, &I_RFIDDolphinSend_97x61);

    // Start worker
    seader_worker_start(
        seader->worker,
        SeaderWorkerStateAPDURunner,
        seader->uart,
        seader_apdu_runner_worker_callback,
        seader);

    view_dispatcher_switch_to_view(seader->view_dispatcher, SeaderViewPopup);
}

bool seader_scene_apdu_runner_on_event(void* context, SceneManagerEvent event) {
    Seader* seader = context;
    Popup* popup = seader->popup;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            scene_manager_search_and_switch_to_previous_scene(
                seader->scene_manager, SeaderSceneSamPresent);
            consumed = true;
        } else if(event.event == SeaderWorkerEventAPDURunnerUpdate) {
            SeaderAPDURunnerContext apdu_runner_ctx = seader->apdu_runner_ctx;
            snprintf(
                seader_scene_apdu_runner_update_text,
                sizeof(seader_scene_apdu_runner_update_text),
                "APDU Runner\n%d/%d",
                apdu_runner_ctx.current_line + 1,
                apdu_runner_ctx.total_lines);
            popup_set_header(
                popup, seader_scene_apdu_runner_update_text, 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == SeaderWorkerEventAPDURunnerSuccess) {
            notification_message(seader->notifications, &sequence_success);
            popup_set_header(popup, "APDU Runner\nSuccess", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == SeaderWorkerEventAPDURunnerError) {
            notification_message(seader->notifications, &sequence_error);
            popup_set_header(popup, "APDU Runner\nError", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(
            seader->scene_manager, SeaderSceneSamPresent);
        consumed = true;
    }
    return consumed;
}

void seader_scene_apdu_runner_on_exit(void* context) {
    Seader* seader = context;

    // Clear view
    popup_reset(seader->popup);
}
