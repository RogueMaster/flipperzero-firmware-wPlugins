#include "kyberwrite_scene.h"

typedef enum {
    MenuIndexFullInit,
    MenuIndexQuickChange,
    MenuIndexRead,
} MenuIndex;

static void menu_callback(void* context, uint32_t index) {
    furi_assert(context);
    KyberApp* app = context;
    switch(index) {
    case MenuIndexFullInit:
        scene_manager_handle_custom_event(app->scene_manager, KyberEventMenuFullInit);
        break;
    case MenuIndexQuickChange:
        scene_manager_handle_custom_event(app->scene_manager, KyberEventMenuQuickChange);
        break;
    case MenuIndexRead:
        scene_manager_handle_custom_event(app->scene_manager, KyberEventMenuRead);
        break;
    }
}

void kyberwrite_scene_menu_on_enter(void* context) {
    furi_assert(context);
    KyberApp* app = context;

    submenu_reset(app->menu);
    submenu_set_header(app->menu, "Kyber Crystal Tool");
    submenu_add_item(app->menu, "Read Crystal",           MenuIndexRead,        menu_callback, app);
    submenu_add_item(app->menu, "Quick Change (color)",   MenuIndexQuickChange, menu_callback, app);
    submenu_add_item(app->menu, "Full Init (new chip)",   MenuIndexFullInit,    menu_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, KyberViewMenu);
}


bool kyberwrite_scene_menu_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    KyberApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == KyberEventMenuFullInit) {
            app->mode = KyberModeFullInit;
            scene_manager_next_scene(app->scene_manager, KyberSceneColorSelect);
            consumed = true;
        } else if(event.event == KyberEventMenuQuickChange) {
            app->mode = KyberModeQuickChange;
            scene_manager_next_scene(app->scene_manager, KyberSceneColorSelect);
            consumed = true;
        } else if(event.event == KyberEventMenuRead) {
            scene_manager_next_scene(app->scene_manager, KyberSceneRead);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void kyberwrite_scene_menu_on_exit(void* context) {
    furi_assert(context);
    KyberApp* app = context;
    submenu_reset(app->menu);
}
