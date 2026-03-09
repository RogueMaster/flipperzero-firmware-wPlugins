#include "seader_scene_read_common.h"

#include "../seader_i.h"
#include "../trace_log.h"

void seader_sam_check_worker_callback(uint32_t event, void* context) {
    Seader* seader = context;
    view_dispatcher_send_custom_event(seader->view_dispatcher, event);
}

void seader_scene_read_prepare(Seader* seader) {
    furi_assert(seader);
    FURI_LOG_D("SceneRead", "Prepare session sam=%d", seader->samCommand);
    seader_trace(
        "SceneRead",
        "prepare sam=%d state=%d intent=%d",
        seader->samCommand,
        seader->sam_state,
        seader->sam_intent);
    if(seader->sam_state == SeaderSamStateIdle) {
        seader->samCommand = SamCommand_PR_NOTHING;
    }
    memset(seader->read_error, 0, sizeof(seader->read_error));
    seader_worker_reset_poller_session(seader->worker);
}

void seader_scene_read_cleanup(Seader* seader) {
    furi_assert(seader);
    FURI_LOG_D("SceneRead", "Cleanup session sam=%d", seader->samCommand);
    seader_trace(
        "SceneRead",
        "cleanup sam=%d state=%d intent=%d",
        seader->samCommand,
        seader->sam_state,
        seader->sam_intent);
    seader_worker_cancel_poller_session(seader->worker);

    if(seader_sam_has_active_card(seader)) {
        seader_send_no_card_detected(seader);
    }

    popup_reset(seader->popup);
    seader_worker_reset_poller_session(seader->worker);
    if(seader->sam_state == SeaderSamStateIdle) {
        seader->samCommand = SamCommand_PR_NOTHING;
    }
    seader_blink_stop(seader);
}
