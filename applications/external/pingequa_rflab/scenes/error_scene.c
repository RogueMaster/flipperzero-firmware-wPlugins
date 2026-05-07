/**
 * @file error_scene.c
 * @brief 错误屏实现.使用 Flipper 内置 DialogEx 模块,无自定义 View.
 */
#include "error_scene.h"

#include "../core/pq_scene_lifecycle.h"
#include "../pingequa_app.h"

#include <furi.h>

#define SCENE_NAME "error"

static void error_dialog_callback(DialogExResult result, void* ctx) {
    PqApp* app = ctx;
    UNUSED(result); /* 任何按钮都退出. */
    view_dispatcher_send_custom_event(app->view_dispatcher, PqCustomEventErrorExit);
}

void error_scene_on_enter(void* context) {
    PqApp* app = context;
    pq_scene_enter(SCENE_NAME);

    /* DialogEx 是 app-owned,这里只配置内容 + 切换. */
    dialog_ex_reset(app->error_dialog);
    dialog_ex_set_context(app->error_dialog, app);
    dialog_ex_set_result_callback(app->error_dialog, error_dialog_callback);
    dialog_ex_set_header(
        app->error_dialog, "Board Not Detected", 64, 2, AlignCenter, AlignTop);
    dialog_ex_set_text(
        app->error_dialog, app->detect.failure_hint, 64, 20, AlignCenter, AlignTop);
    dialog_ex_set_center_button_text(app->error_dialog, "Exit");

    view_dispatcher_switch_to_view(app->view_dispatcher, PqViewError);
}

bool error_scene_on_event(void* context, SceneManagerEvent event) {
    PqApp* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == PqCustomEventErrorExit) {
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }
    return false;
}

void error_scene_on_exit(void* context) {
    UNUSED(context);
    /* DialogEx 是 app-owned,scene 不释放. */
    pq_scene_exit(SCENE_NAME);
}
