#include "../ir_maker.h"

typedef enum {
    IrMakerStartItemNew,
    IrMakerStartItemOpen,
} IrMakerStartItem;

static void ir_maker_scene_start_submenu_callback(void* context, uint32_t index) {
    furi_assert(context);
    IrMakerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void ir_maker_scene_start_on_enter(void* context) {
    furi_assert(context);
    IrMakerApp* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "IR Remote Maker");
    submenu_add_item(
        app->submenu,
        "New Remote",
        IrMakerStartItemNew,
        ir_maker_scene_start_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Edit Existing .ir",
        IrMakerStartItemOpen,
        ir_maker_scene_start_submenu_callback,
        app);

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, IrMakerSceneStart));
    view_dispatcher_switch_to_view(app->view_dispatcher, IrMakerViewSubmenu);
}

bool ir_maker_scene_start_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    IrMakerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, IrMakerSceneStart, event.event);
        if(event.event == IrMakerStartItemNew) {
            ir_maker_draft_reset(app->draft);
            app->def_protocol = InfraredProtocolNEC;
            app->def_address = 0;
            scene_manager_next_scene(app->scene_manager, IrMakerSceneRemoteName);
            consumed = true;
        } else if(event.event == IrMakerStartItemOpen) {
            scene_manager_next_scene(app->scene_manager, IrMakerSceneOpen);
            consumed = true;
        }
    }

    return consumed;
}

void ir_maker_scene_start_on_exit(void* context) {
    furi_assert(context);
    IrMakerApp* app = context;
    submenu_reset(app->submenu);
}
