#include "../mfp_app.h"
#include "../mfp_keys.h"
#include "../mfp_storage.h"
#include "../mfp_result_view.h"
#include "mfp_scene_config.h"

#include <gui/scene_manager.h>
#include <furi.h>

typedef enum {
    ReadAllResultEventActions = 0,
} ReadAllResultCustomEvent;

static void read_all_result_actions_cb(void* ctx) {
    MfpApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, ReadAllResultEventActions);
}

void mfp_scene_read_all_result_on_enter(void* ctx) {
    MfpApp* app = ctx;

    uint8_t total = app->scan_total_sectors
                        ? app->scan_total_sectors
                        : mfp_sector_count(app->version.size);

    /* Pack per-sector key flags into the view's compact byte format. */
    uint8_t states[MFP_SECTORS_4K] = {0};
    for(uint8_t s = 0; s < total && s < MFP_SECTORS_4K; s++) {
        const MfpSectorResult* r = &app->sector_results[s];
        if(r->status == MfpSectorOk) {
            if(r->key_a_found && r->key_b_found) {
                states[s] = MFP_RESULT_SECTOR_KEY_AB;
            } else if(r->key_a_found) {
                states[s] = MFP_RESULT_SECTOR_KEY_A;
            } else if(r->key_b_found) {
                states[s] = MFP_RESULT_SECTOR_KEY_B;
            } else {
                states[s] = MFP_RESULT_SECTOR_FAIL;
            }
        } else {
            states[s] = MFP_RESULT_SECTOR_FAIL;
        }
    }

    mfp_result_view_update(
        app->result_view,
        app->version.size,
        app->version.sl,
        app->version.uid,
        app->version.uid_len,
        total,
        states);
    mfp_result_view_set_actions_callback(
        app->result_view, read_all_result_actions_cb, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, MfpViewResult);
}

bool mfp_scene_read_all_result_on_event(void* ctx, SceneManagerEvent event) {
    MfpApp* app = ctx;
    if(event.type == SceneManagerEventTypeCustom &&
       event.event == ReadAllResultEventActions) {
        scene_manager_next_scene(app->scene_manager, MfpSceneActions);
        return true;
    }
    if(event.type == SceneManagerEventTypeBack) {
        /* Loaded from Saved → go back to the Saved file list.
         * Fresh dump → skip past the scan pipeline to Start. */
        if(app->loaded_from_file) {
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, MfpSceneSaved);
        } else {
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, MfpSceneStart);
        }
        return true;
    }
    return false;
}

void mfp_scene_read_all_result_on_exit(void* ctx) {
    MfpApp* app = ctx;
    mfp_result_view_set_actions_callback(app->result_view, NULL, NULL);
}
