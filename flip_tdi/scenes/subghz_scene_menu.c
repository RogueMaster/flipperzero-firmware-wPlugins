#include "../flip_tdi_app_i.h"

void flip_tdi_scene_menu_submenu_callback(void* context, uint32_t index) {
    furi_assert(context);
    FlipTDIApp* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void flip_tdi_scene_menu_on_enter(void* context) {
    furi_assert(context);

    FlipTDIApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_add_item(
        submenu, "Wiring UART", SubmenuIndexWiringUart, flip_tdi_scene_menu_submenu_callback, app);
    submenu_add_item(
        submenu, "Wiring SPI", SubmenuIndexWiringSpi, flip_tdi_scene_menu_submenu_callback, app);
    submenu_add_item(
        submenu, "Wiring GPIO", SubmenuIndexWiringGpio, flip_tdi_scene_menu_submenu_callback, app);
    submenu_add_item(
        submenu, "About", SubmenuIndexAbout, flip_tdi_scene_menu_submenu_callback, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, FlipTDISceneMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipTDIViewSubmenu);
}

bool flip_tdi_scene_menu_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);

    FlipTDIApp* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexAbout) {
            scene_manager_next_scene(app->scene_manager, FlipTDISceneAbout);
            consumed = true;
        } else if(event.event == SubmenuIndexWiringUart) {
            scene_manager_next_scene(app->scene_manager, FlipTDISceneWiringUart);
            consumed = true;
        } else if(event.event == SubmenuIndexWiringSpi) {
            scene_manager_next_scene(app->scene_manager, FlipTDISceneWiringSpi);
            consumed = true;
        } else if(event.event == SubmenuIndexWiringGpio) {
            scene_manager_next_scene(app->scene_manager, FlipTDISceneWiringGpio);
            consumed = true;
        }
        scene_manager_set_scene_state(app->scene_manager, FlipTDIViewSubmenu, event.event);
    }

    return consumed;
}

void flip_tdi_scene_menu_on_exit(void* context) {
    furi_assert(context);

    FlipTDIApp* app = context;
    submenu_reset(app->submenu);
}
