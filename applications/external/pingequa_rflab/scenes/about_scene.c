/**
 * @file about_scene.c
 * @brief About Scene 实现 —— 三页静态信息屏,Up/Down 翻页.
 *
 * on_enter 复位到首页(page=0),设 input_callback 处理 Up/Down.无 worker、
 * 无 timer、无芯片句柄.Back 键:input_callback 不消费 → ViewDispatcher
 * navigation 接管 → 弹回 main_menu.
 */
#include "about_scene.h"

#include "../core/pq_scene_lifecycle.h"
#include "../pingequa_app.h"
#include "../views/about_view.h"

#include <furi.h>
#include <input/input.h>

#define SCENE_NAME "about"

/* Input 回调 — 跑在 GUI 线程.Up/Down clamp 到 [0, ABOUT_PAGE_COUNT-1],
 * 不 wrap(到边界后按键无反应,行为更可预期).Back 返 false 交给 navigation. */
static bool about_input_callback(InputEvent* event, void* ctx) {
    PqApp* app = ctx;
    if(event->type != InputTypeShort) return false;

    uint8_t p = about_view_get_page(app->about_view);

    if(event->key == InputKeyDown) {
        if(p + 1 < ABOUT_PAGE_COUNT) about_view_set_page(app->about_view, p + 1);
        return true;
    }
    if(event->key == InputKeyUp) {
        if(p > 0) about_view_set_page(app->about_view, p - 1);
        return true;
    }
    /* Back / OK / Left / Right 不消费. */
    return false;
}

void about_scene_on_enter(void* context) {
    PqApp* app = context;
    pq_scene_enter(SCENE_NAME);

    /* 复位到首页 —— 每次进 About 都从品牌+QR 页开始. */
    about_view_set_page(app->about_view, 0);

    View* v = about_view_get_view(app->about_view);
    view_set_context(v, app);
    view_set_input_callback(v, about_input_callback);

    view_dispatcher_switch_to_view(app->view_dispatcher, PqViewAbout);
}

bool about_scene_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    /* 无自定义事件;Back 由 navigation callback 处理. */
    return false;
}

void about_scene_on_exit(void* context) {
    UNUSED(context);
    pq_scene_exit(SCENE_NAME);
}
