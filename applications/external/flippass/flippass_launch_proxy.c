#include "flippass.h"
#include "flippass_launch.h"
#include "plugins/flippass_launch_plugin.h"
#include "scenes/flippass_progress_view.h"
#include "scenes/flippass_scene_editor.h"
#include "scenes/flippass_scene_status.h"
#include "scenes/flippass_scene_db_entries.h"
#include "kdbx/memzero.h"
#include <stdlib.h>
#include <string.h>

void flippass_launch_cancel(App* app) {
    if(app->launch_request != NULL) {
        memzero(app->launch_request, app->launch_request->allocation_bytes);
        free(app->launch_request);
        app->launch_request = NULL;
    }
}

static const FlipPassLaunchPluginV2* flippass_launch_get(App* app) {
    const FlipperAppPluginDescriptor* descriptor = flippass_module_ensure(
        app,
        FlipPassModuleSlotLaunch,
        NULL,
        FLIPPASS_LAUNCH_PLUGIN_APP_ID,
        FLIPPASS_LAUNCH_PLUGIN_API_VERSION,
        NULL);
    const FlipPassLaunchPluginV2* api = descriptor ? descriptor->entry_point : NULL;
    if(api == NULL || api->api_version != FLIPPASS_LAUNCH_PLUGIN_API_VERSION ||
       api->start == NULL || api->after_open == NULL) {
        flippass_module_unload(app, FlipPassModuleSlotLaunch);
        return NULL;
    }
    flippass_module_pin(app, FlipPassModuleSlotLaunch);
    return api;
}

static void flippass_launch_release(App* app) {
    flippass_module_unpin(app, FlipPassModuleSlotLaunch);
    flippass_module_unload(app, FlipPassModuleSlotLaunch);
}

bool flippass_launch_start(App* app, const char* arguments) {
    if(arguments == NULL || arguments[0] == '\0' || app->rpc_mode ||
       strncmp(arguments, "RPC ", 4U) == 0)
        return false;
    const FlipPassLaunchPluginV2* api = flippass_launch_get(app);
    if(api != NULL) {
        const bool handled = api->start(app, arguments);
        flippass_launch_release(app);
        return handled;
    } else {
        scene_manager_next_scene(app->scene_manager, FlipPassScene_FileBrowser);
        flippass_scene_status_show(
            app, "Launch Failed", "Launch module unavailable.", FlipPassScene_FileBrowser);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
    }
    return true;
}

void flippass_launch_tick(App* app) {
    if(app->launch_request == NULL || flippass_progress_guard_active(app->progress_view)) return;
    const uint32_t scene = scene_manager_get_current_scene(app->scene_manager);
    if(scene == FlipPassScene_VaultFallback) return;
    if(scene != FlipPassScene_DbEntries || !app->database_loaded || app->idle_lock_active) {
        flippass_launch_cancel(app);
        return;
    }
    const FlipPassLaunchPluginV2* api = flippass_launch_get(app);
    FlipPassLaunchOutcome outcome = FlipPassLaunchBrowse;
    if(api != NULL) {
        outcome = api->after_open(app);
        flippass_launch_release(app);
    } else {
        flippass_scene_status_show(
            app, "Launch Failed", "Launch module unavailable.", FlipPassScene_DbEntries);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
    }
    flippass_launch_cancel(app);
    if(outcome != FlipPassLaunchExit && app->database_loaded) {
        /* Type/lock do not use the editor. Preserve their output/keyboard RAM;
         * existing editor scene proxies can load it if editing is requested. */
        if(outcome == FlipPassLaunchBrowse) flippass_editor_begin_database_session(app);
        if(outcome == FlipPassLaunchLock) {
            flippass_lock_session(app);
        } else if(outcome == FlipPassLaunchTypeMenu) {
            flippass_scene_db_entries_on_event(
                app,
                (SceneManagerEvent){
                    .type = SceneManagerEventTypeCustom,
                    .event = FlipPassSceneDbEntriesEventOpenActionMenu});
        }
    }
    flippass_update_last_interaction(app);
}
