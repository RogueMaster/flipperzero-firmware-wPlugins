#include "ui.h"

#include <infrared_playback_icons.h>

#define LOG_TAG "infrared_playback_remote_select"

#define INFRARED_PATH EXT_PATH("infrared")
#define INFRARED_EXT  ".ir"

static void remote_select_scene_file_browser_selection_callback(void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_check(context);

    UI* ui = context;
    scene_manager_next_scene(ui->scene_manager, RemotePlaybackDisplay);
}

void remote_select_scene_on_enter(void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_check(context);

    UI* ui = context;
    file_browser_start(ui->remote_select_scene->file_browser, ui->remote_select_scene->file_path);

    view_dispatcher_switch_to_view(ui->view_dispatcher, View_RemoteSelectDisplay);
}

bool remote_select_scene_on_event(void* context, SceneManagerEvent event) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_check(context);

    if(event.type == SceneManagerEventTypeCustom) {
        return true;
    }

    return false;
}

void remote_select_scene_on_exit(void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_check(context);

    UI* ui = context;
    file_browser_stop(ui->remote_select_scene->file_browser);
}

RemoteSelectScene* remote_select_scene_alloc(UI* ui) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_check(ui);

    RemoteSelectScene* scene = malloc(sizeof(RemoteSelectScene));

    scene->file_path = furi_string_alloc_set_str(INFRARED_PATH);
    scene->file_browser = file_browser_alloc(scene->file_path);
    file_browser_set_callback(
        scene->file_browser, remote_select_scene_file_browser_selection_callback, ui);

    file_browser_configure(
        scene->file_browser, INFRARED_EXT, INFRARED_PATH, true, true, &I_ir_10px, true);

    return scene;
}

void remote_select_scene_free(RemoteSelectScene* scene) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_check(scene);

    furi_string_free(scene->file_path);
    file_browser_free(scene->file_browser);

    free(scene);
}
