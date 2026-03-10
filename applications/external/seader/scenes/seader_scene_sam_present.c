#include "../seader_i.h"
enum SubmenuIndex {
    SubmenuIndexSamInfo,
    SubmenuIndexRead,
    SubmenuIndexSaved,
    SubmenuIndexAPDURunner,
    SubmenuIndexReadConfigCard,
};

static uint8_t fwChecks = 3;

void seader_scene_sam_present_submenu_callback(void* context, uint32_t index) {
    Seader* seader = context;
    view_dispatcher_send_custom_event(seader->view_dispatcher, index);
}

void seader_scene_sam_present_on_update(void* context) {
    Seader* seader = context;
    SeaderWorker* seader_worker = seader->worker;

    Submenu* submenu = seader->submenu;
    submenu_reset(submenu);

    submenu_add_item(
        submenu, "Read HF", SubmenuIndexRead, seader_scene_sam_present_submenu_callback, seader);
    submenu_add_item(
        submenu, "Saved", SubmenuIndexSaved, seader_scene_sam_present_submenu_callback, seader);

    if(seader->is_debug_enabled) {
        submenu_add_item(
            submenu,
            "Read Config Card",
            SubmenuIndexReadConfigCard,
            seader_scene_sam_present_submenu_callback,
            seader);
    }

    if(apdu_log_check_presence(SEADER_APDU_RUNNER_FILE_NAME)) {
        submenu_add_item(
            submenu,
            "Run APDUs",
            SubmenuIndexAPDURunner,
            seader_scene_sam_present_submenu_callback,
            seader);
    }
    if(seader_worker->sam_version[0] != 0 && seader_worker->sam_version[1] != 0) {
        submenu_add_item(
            submenu,
            seader->sam_key_label,
            SubmenuIndexSamInfo,
            seader_scene_sam_present_submenu_callback,
            seader);
        fwChecks = 0;
    } else {
        submenu_add_item(
            submenu,
            seader->sam_key_label,
            SubmenuIndexSamInfo,
            seader_scene_sam_present_submenu_callback,
            seader);
    }

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(seader->scene_manager, SeaderSceneSamPresent));

    view_dispatcher_switch_to_view(seader->view_dispatcher, SeaderViewMenu);
}

void seader_scene_sam_present_on_enter(void* context) {
    seader_scene_sam_present_on_update(context);
}

bool seader_scene_sam_present_on_event(void* context, SceneManagerEvent event) {
    Seader* seader = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexRead) {
            scene_manager_set_scene_state(
                seader->scene_manager, SeaderSceneSamPresent, event.event);
            scene_manager_next_scene(seader->scene_manager, SeaderSceneRead);
            consumed = true;
        } else if(event.event == SubmenuIndexReadConfigCard) {
            scene_manager_set_scene_state(
                seader->scene_manager, SeaderSceneSamPresent, SubmenuIndexReadConfigCard);
            scene_manager_next_scene(seader->scene_manager, SeaderSceneReadConfigCard);
            consumed = true;
        } else if(event.event == SubmenuIndexSamInfo) {
            scene_manager_set_scene_state(
                seader->scene_manager, SeaderSceneSamPresent, event.event);
            scene_manager_next_scene(seader->scene_manager, SeaderSceneSamInfo);
            consumed = true;
        } else if(event.event == SubmenuIndexSaved) {
            scene_manager_set_scene_state(
                seader->scene_manager, SeaderSceneSamPresent, event.event);
            scene_manager_next_scene(seader->scene_manager, SeaderSceneFileSelect);
            consumed = true;
        } else if(event.event == SeaderWorkerEventSamMissing) {
            scene_manager_next_scene(seader->scene_manager, SeaderSceneSamMissing);
            consumed = true;
        } else if(event.event == SubmenuIndexAPDURunner) {
            scene_manager_set_scene_state(
                seader->scene_manager, SeaderSceneSamPresent, event.event);
            scene_manager_next_scene(seader->scene_manager, SeaderSceneAPDURunner);
            consumed = true;
        } else if(event.event == SeaderCustomEventSamStatusUpdated) {
            seader_scene_sam_present_on_update(context);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_stop(seader->scene_manager);
        view_dispatcher_stop(seader->view_dispatcher);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeTick) {
        SeaderWorker* seader_worker = seader->worker;
        if(fwChecks > 0 && seader_worker->sam_version[0] != 0 &&
           seader_worker->sam_version[1] != 0) {
            fwChecks--;
            seader_scene_sam_present_on_update(context);
        }
    }

    return consumed;
}

void seader_scene_sam_present_on_exit(void* context) {
    Seader* seader = context;
    submenu_reset(seader->submenu);
}
