#include "../mfp_app.h"
#include "../mfp_listener.h"
#include "../mfp_storage.h"
#include "../mfp_emulate_view.h"
#include "mfp_scene_config.h"

#include <gui/scene_manager.h>
#include <notification/notification_messages.h>
#include <furi.h>
#include <string.h>

/* Scene has two states:
 *  - Running: live counters and sector activity grid on custom view
 *  - Summary: post-session screen with final counts and save status
 * First back = switch to summary (or exit if nothing happened).
 * Second back = exit scene and jump back to the parent. */

typedef enum {
    EmulateStateRunning,
    EmulateStateSummary,
} EmulateState;

/* When exiting Emulate we skip both the EmulateSetup submenu and the
 * Actions menu that were pushed right before us, landing directly on
 * ReadAllResult (used for both fresh dumps and loaded saved files). */
static void emulate_navigate_back_to_parent(MfpApp* app) {
    if(!scene_manager_search_and_switch_to_previous_scene(
           app->scene_manager, MfpSceneReadAllResult)) {
        scene_manager_previous_scene(app->scene_manager);
    }
}

static void emulate_activity_cb(void* ctx) {
    MfpApp* app = ctx;
    notification_message(app->notifications, &sequence_blink_blue_10);

    MfpListener* emu = (MfpListener*)app->emulator;
    if(!emu) return;
    mfp_emulate_view_record(
        app->emulate_view,
        emu->auths_count,
        emu->reads_count,
        emu->writes_count,
        emu->last_op_type,
        emu->last_op_sector,
        emu->last_op_block);
}

void mfp_scene_emulate_on_enter(void* ctx) {
    MfpApp* app = ctx;

    app->modified_saved = false;
    app->modified_save_path[0] = '\0';

    scene_manager_set_scene_state(app->scene_manager, MfpSceneEmulate, EmulateStateRunning);

    /* Allocate emulator */
    MfpListener* emu = mfp_listener_alloc(app->nfc);
    mfp_listener_set_from_app(emu, app);
    app->emulator = emu;
    emu->on_activity = emulate_activity_cb;
    emu->activity_ctx = app;
    mfp_listener_start(emu);

    /* Seed the view with card info and which sectors have loaded keys. */
    bool loaded[MFP_SECTORS_4K] = {0};
    uint8_t total = mfp_sector_count(app->version.size);
    for(uint8_t s = 0; s < total && s < MFP_SECTORS_4K; s++) {
        loaded[s] = (app->sector_results[s].status == MfpSectorOk);
    }
    mfp_emulate_view_reset(
        app->emulate_view,
        app->version.size,
        total,
        app->version.uid,
        app->version.uid_len,
        loaded,
        app->allow_overwrite);

    view_dispatcher_switch_to_view(app->view_dispatcher, MfpViewEmulate);
    notification_message(app->notifications, &sequence_blink_start_cyan);
}

bool mfp_scene_emulate_on_event(void* ctx, SceneManagerEvent event) {
    MfpApp* app = ctx;

    if(event.type == SceneManagerEventTypeBack) {
        EmulateState state = (EmulateState)scene_manager_get_scene_state(
            app->scene_manager, MfpSceneEmulate);

        if(state == EmulateStateRunning) {
            notification_message(app->notifications, &sequence_blink_stop);

            MfpListener* emu = (MfpListener*)app->emulator;
            uint32_t writes = 0;
            uint32_t reads = 0;
            uint32_t auths = 0;
            if(emu) {
                writes = emu->writes_count;
                reads = emu->reads_count;
                auths = emu->auths_count;
                mfp_listener_stop(emu);

                if(writes > 0 && app->allow_overwrite) {
                    memcpy(app->blocks, emu->blocks, sizeof(app->blocks));
                    if(mfp_storage_save_modifications(
                           app, app->modified_save_path, sizeof(app->modified_save_path))) {
                        app->modified_saved = true;
                    }
                }

                mfp_listener_free(emu);
                app->emulator = NULL;
            }

            if(writes == 0 && reads == 0 && auths == 0) {
                /* Nothing happened — exit immediately, skipping the
                 * EmulateSetup + Actions scenes. */
                emulate_navigate_back_to_parent(app);
                return true;
            }

            /* Show session summary */
            notification_message(app->notifications, &sequence_success);
            scene_manager_set_scene_state(
                app->scene_manager, MfpSceneEmulate, EmulateStateSummary);

            const char* fname = "";
            if(app->modified_saved) {
                fname = strrchr(app->modified_save_path, '/');
                fname = fname ? fname + 1 : app->modified_save_path;
            }
            mfp_emulate_view_show_summary(app->emulate_view, app->modified_saved, fname);
            return true; /* consume back — stay in scene showing summary */
        }
        /* Summary state: back jumps past setup/actions to parent. */
        emulate_navigate_back_to_parent(app);
        return true;
    }

    return false;
}

void mfp_scene_emulate_on_exit(void* ctx) {
    MfpApp* app = ctx;
    notification_message(app->notifications, &sequence_blink_stop);

    /* Forced exit (not via back button): save only if allowed */
    if(app->emulator) {
        MfpListener* emu = (MfpListener*)app->emulator;
        mfp_listener_stop(emu);
        if(emu->writes_count > 0 && app->allow_overwrite) {
            memcpy(app->blocks, emu->blocks, sizeof(app->blocks));
            if(mfp_storage_save_modifications(
                   app, app->modified_save_path, sizeof(app->modified_save_path))) {
                app->modified_saved = true;
            }
        }
        mfp_listener_free(emu);
        app->emulator = NULL;
    }
}
