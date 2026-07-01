#include "kyberwrite_scene.h"

static void color_callback(void* context, uint32_t index) {
    furi_assert(context);
    KyberApp* app    = context;
    app->crystal_index = (uint8_t)index;
    scene_manager_handle_custom_event(app->scene_manager, KyberEventColorSelected);
}

void kyberwrite_scene_color_on_enter(void* context) {
    furi_assert(context);
    KyberApp* app = context;

    submenu_reset(app->color_menu);

    const char* header = (app->mode == KyberModeFullInit)
        ? "Full Init: Pick Crystal"
        : "Quick Change: Pick Crystal";
    submenu_set_header(app->color_menu, header);

    for(uint8_t i = 0; i < KYBER_CRYSTAL_COUNT; i++) {
        submenu_add_item(app->color_menu, KYBER_CRYSTALS[i].name, i, color_callback, app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, KyberViewColorSelect);
}

bool kyberwrite_scene_color_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    KyberApp* app    = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == KyberEventColorSelected) {
        scene_manager_next_scene(app->scene_manager, KyberSceneWriting);
        consumed = true;
    }
    return consumed;
}

void kyberwrite_scene_color_on_exit(void* context) {
    furi_assert(context);
    KyberApp* app = context;
    submenu_reset(app->color_menu);
}
