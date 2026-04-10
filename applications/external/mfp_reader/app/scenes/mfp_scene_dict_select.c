#include "../mfp_app.h"
#include "mfp_scene_config.h"

#include <gui/scene_manager.h>
#include <storage/storage.h>

#define DICT_EXTENSION ".dic"

typedef enum {
    DictSelectEventSelected = 0,
} DictSelectCustomEvent;

static void dict_select_cb(void* ctx) {
    MfpApp* app = ctx;
    /* file_browser_result holds the selected file path */
    furi_string_set(app->dict_path, app->file_browser_result);
    view_dispatcher_send_custom_event(app->view_dispatcher, DictSelectEventSelected);
}

void mfp_scene_dict_select_on_enter(void* ctx) {
    MfpApp* app = ctx;

    /* Start browser from the app data folder */
    furi_string_set_str(app->file_browser_result, MFP_APP_FOLDER);

    file_browser_configure(
        app->file_browser,
        DICT_EXTENSION,
        MFP_APP_FOLDER,
        false, /* skip_assets */
        true,  /* hide_dot_files */
        NULL,  /* file_icon */
        false); /* hide_ext */

    file_browser_set_callback(app->file_browser, dict_select_cb, app);
    file_browser_start(app->file_browser, app->file_browser_result);

    view_dispatcher_switch_to_view(app->view_dispatcher, MfpViewFileBrowser);
}

bool mfp_scene_dict_select_on_event(void* ctx, SceneManagerEvent event) {
    MfpApp* app = ctx;
    if(event.type == SceneManagerEventTypeCustom &&
       event.event == DictSelectEventSelected) {
        scene_manager_next_scene(app->scene_manager, MfpSceneReadAll);
        return true;
    }
    return false;
}

void mfp_scene_dict_select_on_exit(void* ctx) {
    MfpApp* app = ctx;
    file_browser_stop(app->file_browser);
}
