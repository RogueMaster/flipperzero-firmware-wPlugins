#include "../ble_spam.h"

typedef enum {
    StartBrowse,
    StartSingleColor,
    StartCustom,
    StartSettings,
} StartItem;

static void start_cb(void* _ctx, uint32_t index) {
    Ctx* ctx = _ctx;
    switch(index) {
    case StartBrowse:
        // Stay at current preset_index (or reset to 0 if coming fresh)
        scene_manager_next_scene(ctx->scene_manager, SceneMain);
        break;
    case StartSingleColor:
        ctx->preset_index = 0;  // attacks[0] is Single Color
        scene_manager_next_scene(ctx->scene_manager, SceneMain);
        break;
    case StartCustom:
        scene_manager_next_scene(ctx->scene_manager, SceneCustom);
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
    submenu_add_item(ctx->submenu, "Browse Presets",  StartBrowse,      start_cb, ctx);
    submenu_add_item(ctx->submenu, "Single Color",    StartSingleColor, start_cb, ctx);
    submenu_add_item(ctx->submenu, "Custom Command",  StartCustom,      start_cb, ctx);
    submenu_add_item(ctx->submenu, "Settings",        StartSettings,    start_cb, ctx);
    view_dispatcher_switch_to_view(ctx->view_dispatcher, ViewSubmenu);
}

bool scene_start_on_event(void* _ctx, SceneManagerEvent event) {
    UNUSED(_ctx); UNUSED(event);
    return false;
}

void scene_start_on_exit(void* _ctx) {
    Ctx* ctx = _ctx;
    submenu_reset(ctx->submenu);
}
