#include "../seader_i.h"
#include "seader_scene_read_common.h"

enum SubmenuIndex {
    SubmenuIndexSamPresent,
    SubmenuIndexSamMissing,
};

static void seader_scene_start_detect_callback(void* context) {
    Seader* seader = context;
    if(!seader || !seader->start_scene_active) {
        return;
    }
    view_dispatcher_send_custom_event(seader->view_dispatcher, SeaderWorkerEventSamMissing);
}

static void seader_scene_start_begin_detection(Seader* seader) {
    seader_start_popup_set_stage(seader, SeaderStartupStageCheckingSam);
    popup_set_timeout(seader->popup, 2500);
    popup_enable_timeout(seader->popup);
    seader_worker_start(
        seader->worker,
        SeaderWorkerStateCheckSam,
        seader->uart,
        seader_sam_check_worker_callback,
        seader);
}

static void seader_scene_start_finish_board_auto_recover(Seader* seader, bool preserve_read_type) {
    if(!seader) {
        return;
    }

    seader->board_auto_recover_pending = false;
    seader->board_auto_recover_resume_read = false;
    if(!preserve_read_type) {
        seader->board_auto_recover_read_type = SeaderCredentialTypeNone;
    }
}

void seader_scene_start_submenu_callback(void* context, uint32_t index) {
    Seader* seader = context;
    view_dispatcher_send_custom_event(seader->view_dispatcher, index);
}

void seader_scene_start_on_enter(void* context) {
    Seader* seader = context;
    seader_worker_acquire(seader);
    seader->start_scene_active = true;
    seader->board_retry_remaining = 1U;

    popup_set_context(seader->popup, seader);
    popup_set_callback(seader->popup, seader_scene_start_detect_callback);
    seader_start_popup_set_stage(seader, SeaderStartupStageCheckingSam);

    view_dispatcher_switch_to_view(seader->view_dispatcher, SeaderViewPopup);
    view_dispatcher_send_custom_event(seader->view_dispatcher, SeaderCustomEventStartDetect);
}

bool seader_scene_start_on_event(void* context, SceneManagerEvent event) {
    Seader* seader = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SeaderCustomEventStartDetect) {
            if(seader_board_status_requires_power_cycle(seader->board_status) &&
               !seader_board_retry_power_cycle(seader)) {
                view_dispatcher_send_custom_event(
                    seader->view_dispatcher, SeaderWorkerEventSamMissing);
            } else {
                seader_scene_start_begin_detection(seader);
            }
            consumed = true;
        } else if(event.event == SeaderWorkerEventSamPresent) {
            seader->board_status = SeaderBoardStatusReady;
            if(seader->board_auto_recover_pending) {
                const bool resume_read = seader->board_auto_recover_resume_read;
                seader_scene_start_finish_board_auto_recover(seader, resume_read);
                if(resume_read) {
                    scene_manager_next_scene(seader->scene_manager, SeaderSceneRead);
                } else {
                    seader->sam_present_menu_guard_active = true;
                    scene_manager_next_scene(seader->scene_manager, SeaderSceneSamPresent);
                }
            } else {
                seader->sam_present_menu_guard_active = true;
                scene_manager_next_scene(seader->scene_manager, SeaderSceneSamPresent);
            }
            consumed = true;
        } else if(event.event == SeaderWorkerEventSamMissing) {
            if(seader->board_retry_remaining > 0U &&
               seader->board_status == SeaderBoardStatusPowerReadyPendingValidation) {
                seader->board_retry_remaining--;
                seader_worker_release(seader);
                seader_start_popup_set_stage(seader, SeaderStartupStageRetryingBoard);
                if(seader_board_retry_power_cycle(seader)) {
                    seader_scene_start_begin_detection(seader);
                    consumed = true;
                }
            }

            if(consumed) {
                scene_manager_set_scene_state(
                    seader->scene_manager, SeaderSceneStart, event.event);
                return true;
            }

            seader->board_status = seader_board_status_on_sam_missing(seader->board_status);
            seader_scene_start_finish_board_auto_recover(seader, false);
            seader->sam_present = false;
            seader_sam_key_label_format(
                false,
                SeaderSamKeyProbeStatusUnknown,
                NULL,
                0U,
                seader->sam_key_label,
                sizeof(seader->sam_key_label));
            scene_manager_next_scene(seader->scene_manager, SeaderSceneSamMissing);
            consumed = true;
        } else if(event.event == SeaderWorkerEventSamWrong) {
            seader->board_status = SeaderBoardStatusReady;
            seader_scene_start_finish_board_auto_recover(seader, false);
            seader->sam_present = false;
            seader_sam_key_label_format(
                false,
                SeaderSamKeyProbeStatusUnknown,
                NULL,
                0U,
                seader->sam_key_label,
                sizeof(seader->sam_key_label));
            scene_manager_next_scene(seader->scene_manager, SeaderSceneSamWrong);
            consumed = true;
        }

        scene_manager_set_scene_state(seader->scene_manager, SeaderSceneStart, event.event);
    }

    return consumed;
}

void seader_scene_start_on_exit(void* context) {
    Seader* seader = context;
    seader->start_scene_active = false;
    popup_reset(seader->popup);
    seader_worker_release(seader);
}
