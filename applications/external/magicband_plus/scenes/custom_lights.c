#include "../ble_spam.h"

typedef enum {
    CLSingleColor = 0,
    CLDualColor,
    CLRGB,
    CL5LED,
    CLCustomHex,
} CLItem;

static void cl_cb(void* _ctx, uint32_t index) {
    Ctx* ctx = _ctx;
    int8_t idx = -1;

    switch((CLItem)index) {
    case CLSingleColor: idx = 0; break;
    case CLDualColor:   idx = 1; break;
    case CLRGB:         idx = 2; break;
    case CL5LED:        idx = 3; break;
    case CLCustomHex:   idx = 4; break;
    }

    if(idx < 0) return;
    ble_spam_select_attack(ctx, idx);
    scene_manager_set_scene_state(ctx->scene_manager, SceneConfig, 0);
    scene_manager_next_scene(ctx->scene_manager, SceneConfig);
}

void scene_custom_lights_on_enter(void* _ctx) {
    Ctx* ctx = _ctx;
    submenu_reset(ctx->submenu);
    submenu_set_header(ctx->submenu, "Custom Lights");
    submenu_add_item(ctx->submenu, "Single Color", CLSingleColor, cl_cb, ctx);
    submenu_add_item(ctx->submenu, "Dual Color",   CLDualColor,   cl_cb, ctx);
    submenu_add_item(ctx->submenu, "RGB (6-bit)",  CLRGB,         cl_cb, ctx);
    submenu_add_item(ctx->submenu, "5-LED Colors", CL5LED,        cl_cb, ctx);
    submenu_add_item(ctx->submenu, "Custom Hex",   CLCustomHex,   cl_cb, ctx);
    view_dispatcher_switch_to_view(ctx->view_dispatcher, ViewSubmenu);
}

bool scene_custom_lights_on_event(void* _ctx, SceneManagerEvent event) {
    UNUSED(_ctx);
    UNUSED(event);
    return false;
}

void scene_custom_lights_on_exit(void* _ctx) {
    Ctx* ctx = _ctx;
    submenu_reset(ctx->submenu);
}
