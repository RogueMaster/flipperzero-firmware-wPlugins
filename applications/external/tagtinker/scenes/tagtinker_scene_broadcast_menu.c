/*
 * Broadcast menu - page select / diagnostic page
 */

#include "../tagtinker_app.h"

static void broadcast_menu_cb(void* ctx, uint32_t index) {
    TagTinkerApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void tagtinker_scene_broadcast_menu_on_enter(void* ctx) {
    TagTinkerApp* app = ctx;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Broadcast Payloads");

    submenu_add_item(app->submenu, "Change Page", TagTinkerBroadcastFlipPage, broadcast_menu_cb, app);
    submenu_add_item(app->submenu, "Diagnostic Page", TagTinkerBroadcastDebugScreen, broadcast_menu_cb, app);

    submenu_set_selected_item(
        app->submenu,
        scene_manager_get_scene_state(app->scene_manager, TagTinkerSceneBroadcastMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewSubmenu);
}

bool tagtinker_scene_broadcast_menu_on_event(void* ctx, SceneManagerEvent event) {
    TagTinkerApp* app = ctx;
    if(event.type != SceneManagerEventTypeCustom) return false;

    scene_manager_set_scene_state(app->scene_manager, TagTinkerSceneBroadcastMenu, event.event);

    switch(event.event) {
    case TagTinkerBroadcastFlipPage:
        app->broadcast_type = TagTinkerBroadcastFlipPage;
        scene_manager_next_scene(app->scene_manager, TagTinkerSceneBroadcast);
        return true;
    case TagTinkerBroadcastDebugScreen:
        app->broadcast_type = TagTinkerBroadcastDebugScreen;
        scene_manager_next_scene(app->scene_manager, TagTinkerSceneBroadcast);
        return true;
    }
    return false;
}

void tagtinker_scene_broadcast_menu_on_exit(void* ctx) {
    TagTinkerApp* app = ctx;
    submenu_reset(app->submenu);
}
