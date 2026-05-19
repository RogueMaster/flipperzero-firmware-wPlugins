#include "../ble_spam.h"

typedef enum {
    StartBrowse = 0,
    StartSingleColor,
    StartCustomLights,
    StartSettings,
} StartItem;

static void start_cb(void* _ctx, uint32_t index) {
    Ctx* ctx = _ctx;
    switch(index) {
    case StartBrowse:
        ble_spam_select_attack(ctx, 5);
        scene_manager_next_scene(ctx->scene_manager, SceneMain);
        break;
    case StartSingleColor:
        ble_spam_select_attack(ctx, 0);
        scene_manager_next_scene(ctx->scene_manager, SceneConfig);
        break;
    case StartCustomLights:
        scene_manager_next_scene(ctx->scene_manager, SceneCustomLights);
        break;
    case StartSettings:
        scene_manager_next_scene(ctx->scene_manager, SceneSettings);
        break;
    }
}

void scene_start_on_enter(void* _ctx) {
    Ctx* ctx = _ctx;
    submenu_reset(ctx->submenu);
    submenu_set_header(ctx->submenu, "MB+ Transmitter");
    submenu_add_item(ctx->submenu, "Browse Presets", StartBrowse,       start_cb, ctx);
    submenu_add_item(ctx->submenu, "Single Color",   StartSingleColor,  start_cb, ctx);
    submenu_add_item(ctx->submenu, "Custom Lights",  StartCustomLights, start_cb, ctx);
    submenu_add_item(ctx->submenu, "Settings",       StartSettings,     start_cb, ctx);
    view_dispatcher_switch_to_view(ctx->view_dispatcher, ViewSubmenu);
}

bool scene_start_on_event(void* _ctx, SceneManagerEvent event) {
    UNUSED(_ctx);
    UNUSED(event);
    return false;
}

void scene_start_on_exit(void* _ctx) {
    Ctx* ctx = _ctx;
    submenu_reset(ctx->submenu);
}
