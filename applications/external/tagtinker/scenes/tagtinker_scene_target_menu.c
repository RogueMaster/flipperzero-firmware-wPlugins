/*
 * Saved tag menu - saved targets + add new
 */

#include "../tagtinker_app.h"

enum {
    TargetMenuScanNfc = 99,
    TargetMenuAddNew = 100,
};

static void target_menu_cb(void* ctx, uint32_t index) {
    TagTinkerApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void tagtinker_scene_target_menu_on_enter(void* ctx) {
    TagTinkerApp* app = ctx;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Targeted Payloads");

    /* Add new target */
    submenu_add_item(app->submenu, "+ Scan NFC", TargetMenuScanNfc, target_menu_cb, app);
    submenu_add_item(app->submenu, "+ Type Barcode", TargetMenuAddNew, target_menu_cb, app);

    /* List saved targets */
    for(uint8_t i = 0; i < app->target_count; i++) {
        /* Use index as event id (0..15) */
        submenu_add_item(
            app->submenu,
            app->targets[i].name[0] ? app->targets[i].name : app->targets[i].barcode,
            i,
            target_menu_cb,
            app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewSubmenu);
}

bool tagtinker_scene_target_menu_on_event(void* ctx, SceneManagerEvent event) {
    TagTinkerApp* app = ctx;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == TargetMenuScanNfc) {
        scene_manager_next_scene(app->scene_manager, TagTinkerSceneNfcScan);
        return true;
    }

    if(event.event == TargetMenuAddNew) {
        /* Go to barcode input, then come back */
        scene_manager_set_scene_state(
            app->scene_manager, TagTinkerSceneBarcodeInput, TagTinkerSceneTargetActions);
        scene_manager_next_scene(app->scene_manager, TagTinkerSceneBarcodeInput);
        return true;
    }

    /* Selected a saved target */
    if(event.event < app->target_count) {
        tagtinker_select_target(app, (uint8_t)event.event);
        scene_manager_next_scene(app->scene_manager, TagTinkerSceneTargetActions);
        return true;
    }

    return false;
}

void tagtinker_scene_target_menu_on_exit(void* ctx) {
    TagTinkerApp* app = ctx;
    submenu_reset(app->submenu);
}
