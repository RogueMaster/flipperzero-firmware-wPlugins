/**
 * @file main_menu_scene.c
 * @brief 主菜单实现 — Submenu 模块(规范 §4.3 强制清单).
 *
 * 标准模式(参考 applications/main/subghz/scenes/subghz_scene_start.c):
 *   submenu_reset → submenu_add_item × N → switch_to_view.
 *   选择回调:view_dispatcher_send_custom_event(index) → on_event 接到
 *   SceneManagerEventTypeCustom 后 next_scene.
 */
#include "main_menu_scene.h"

#include "../core/pq_scene_lifecycle.h"
#include "../pingequa_app.h"

#include <furi.h>
#include <gui/modules/submenu.h>

#define TAG       "PqMainMenu"
#define SCENE_NAME "main_menu"

/* Submenu item index = custom event id;数值与 enum 对齐. */
typedef enum {
    PqMainMenuItemScanner = 0,
    PqMainMenuItemJammer,
} PqMainMenuItem;

static void main_menu_select_callback(void* context, uint32_t index) {
    PqApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void main_menu_scene_on_enter(void* context) {
    PqApp* app = context;
    pq_scene_enter(SCENE_NAME);

    Submenu* sm = app->main_menu_submenu;
    submenu_reset(sm);
    submenu_set_header(sm, "PINGEQUA RF Lab");
    submenu_add_item(sm, "Channel Scanner", PqMainMenuItemScanner,
                     main_menu_select_callback, app);
    submenu_add_item(sm, "NRF24 Jammer", PqMainMenuItemJammer,
                     main_menu_select_callback, app);

    /* 保留 Submenu 上次选中位置(scene_manager 切回时用户指针仍在原位). */
    submenu_set_selected_item(
        sm, scene_manager_get_scene_state(app->scene_manager, PqSceneMainMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, PqViewMainMenu);
}

bool main_menu_scene_on_event(void* context, SceneManagerEvent event) {
    PqApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        /* 记住用户选择,以便回到 main_menu 时光标停留. */
        scene_manager_set_scene_state(app->scene_manager, PqSceneMainMenu, event.event);

        switch(event.event) {
        case PqMainMenuItemScanner:
            scene_manager_next_scene(app->scene_manager, PqSceneScanner);
            return true;
        case PqMainMenuItemJammer:
            scene_manager_next_scene(app->scene_manager, PqSceneJammer);
            return true;
        default:
            return false;
        }
    }

    if(event.type == SceneManagerEventTypeBack) {
        /* main_menu 是用户感知的栈底 — Back 直接退出 app.
         *
         * 为什么不交给 SceneManager.previous_scene:
         *   scene_manager_search_and_switch_to_another_scene(splash → main_menu)
         *   实际栈是 [splash, main_menu](SDK 行为:只清首个之后的 scene,
         *   不清首个).如果让 previous_scene 弹,会回到 splash 重跑探测.
         *   显式 view_dispatcher_stop 绕开此回退路径,行为与栈状态解耦. */
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }

    return false;
}

void main_menu_scene_on_exit(void* context) {
    PqApp* app = context;
    submenu_reset(app->main_menu_submenu);
    pq_scene_exit(SCENE_NAME);
}
