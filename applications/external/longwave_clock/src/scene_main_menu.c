#include "flipper.h"
#include "protocols.h"

#include "app_state.h"
#include "scenes.h"
#include "scene_main_menu.h"

/* main menu scene */
static LWCMainMenuEvent protocol_event[] = {LWCMainMenuSelectDCF77, LWCMainMenuSelectMSF};

/** main menu callback - sends custom events to the scene manager based on the selection */
void lwc_menu_callback(void* context, uint32_t index) {
    App* app = context;
    switch(index) {
    case LWCMainMenuDCF77:
        scene_manager_handle_custom_event(app->scene_manager, LWCMainMenuSelectDCF77);
        break;
    case LWCMainMenuMSF:
        scene_manager_handle_custom_event(app->scene_manager, LWCMainMenuSelectMSF);
        break;
    case LWCMainMenuInfo:
        scene_manager_handle_custom_event(app->scene_manager, LWCMainMenuSelectInfo);
        break;
    case LWCMainMenuAbout:
        scene_manager_handle_custom_event(app->scene_manager, LWCMainMenuSelectAbout);
        break;
    }
}

/** main menu scene - resets the submenu, and gives it content, callbacks and selection enums */
void lwc_main_menu_scene_on_enter(void* context) {
    App* app = context;

    if(submenu_get_selected_item(app->main_menu) == 0) {
        for(uint8_t i = 0; i < __lwc_number_of_protocols; i++) {
            submenu_add_item(
                app->main_menu,
                get_protocol_name((LWCType)(i)),
                protocol_event[i],
                lwc_menu_callback,
                app);
        }
        submenu_add_item(
            app->main_menu, "General infos", LWCMainMenuSelectInfo, lwc_menu_callback, app);
        submenu_add_item(app->main_menu, "About", LWCMainMenuSelectAbout, lwc_menu_callback, app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, LWCMainMenuView);
}

/** main menu event handler - switches scene based on the event */
bool lwc_main_menu_scene_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;
    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        case LWCMainMenuSelectDCF77:
            app_init_lwc(app, DCF77);
            scene_manager_next_scene(app->scene_manager, LWCSubMenuScene);
            consumed = true;
            break;
        case LWCMainMenuSelectMSF:
            app_init_lwc(app, MSF);
            scene_manager_next_scene(app->scene_manager, LWCSubMenuScene);
            consumed = true;
            break;
        case LWCMainMenuSelectInfo:
            scene_manager_next_scene(app->scene_manager, LWCInfoScene);
            consumed = true;
            break;
        case LWCMainMenuSelectAbout:
            scene_manager_next_scene(app->scene_manager, LWCAboutScene);
            consumed = true;
            break;
        }
        break;
    default:
        consumed = false;
        break;
    }
    return consumed;
}

void lwc_main_menu_scene_on_exit(void* context) {
    App* app = context;
    if(submenu_get_selected_item(app->main_menu) == 0) {
        submenu_reset(app->main_menu);
    }
}
