#include "../seader_i.h"
enum SubmenuIndex {
    SubmenuIndexRead,
    SubmenuIndexSaved,
    SubmenuIndexAPDURunner,
    SubmenuIndexReadConfigCard,
    SubmenuIndexSamInfo,
};

static uint8_t fwChecks = 3;

void seader_scene_sam_present_submenu_callback(void* context, uint32_t index);

static void seader_scene_sam_present_rebuild_menu(Seader* seader, uint32_t selected_item) {
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
    submenu_add_item(
        submenu,
        seader->sam_key_label,
        SubmenuIndexSamInfo,
        seader_scene_sam_present_submenu_callback,
        seader);

    if(seader->sam_version[0] != 0 && seader->sam_version[1] != 0) {
        fwChecks = 0;
    }

    submenu_set_selected_item(submenu, selected_item);
    view_dispatcher_switch_to_view(seader->view_dispatcher, SeaderViewMenu);
}

void seader_scene_sam_present_submenu_callback(void* context, uint32_t index) {
    Seader* seader = context;
    view_dispatcher_send_custom_event(seader->view_dispatcher, index);
}

void seader_scene_sam_present_on_update(void* context) {
    Seader* seader = context;
    seader_scene_sam_present_rebuild_menu(
        seader, scene_manager_get_scene_state(seader->scene_manager, SeaderSceneSamPresent));
}

void seader_scene_sam_present_on_enter(void* context) {
    seader_scene_sam_present_on_update(context);
}

bool seader_scene_sam_present_on_event(void* context, SceneManagerEvent event) {
    Seader* seader = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(seader->sam_present_menu_guard_active &&
           (event.event == SubmenuIndexRead || event.event == SubmenuIndexSaved ||
            event.event == SubmenuIndexAPDURunner || event.event == SubmenuIndexReadConfigCard ||
            event.event == SubmenuIndexSamInfo)) {
            seader->sam_present_menu_guard_active = false;
            consumed = true;
        } else if(event.event == SubmenuIndexRead) {
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
        } else if(event.event == SeaderWorkerEventHfTeardownComplete) {
            consumed = seader_hf_finish_teardown_action(seader);
        } else if(event.event == SeaderCustomEventSamStatusUpdated) {
            seader_scene_sam_present_rebuild_menu(
                seader, submenu_get_selected_item(seader->submenu));
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = seader_hf_request_teardown(seader, SeaderHfTeardownActionStopApp);
    } else if(event.type == SceneManagerEventTypeTick) {
        if(seader->sam_present_menu_guard_active) {
            seader->sam_present_menu_guard_active = false;
        }
        if(fwChecks > 0 && seader->sam_version[0] != 0 && seader->sam_version[1] != 0) {
            fwChecks--;
            seader_scene_sam_present_rebuild_menu(
                seader, submenu_get_selected_item(seader->submenu));
        }
    }

    return consumed;
}

void seader_scene_sam_present_on_exit(void* context) {
    Seader* seader = context;
    submenu_reset(seader->submenu);
}
