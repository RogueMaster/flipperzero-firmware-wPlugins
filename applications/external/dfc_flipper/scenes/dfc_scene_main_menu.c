#include "../dfc_i.h"

#define TAG "SceneMainMenu"

enum SubmenuIndex {
    SubmenuIndexSaved,
    SubmenuIndexBlankCard,
    SubmenuIndexAbout,
};

void dfc_scene_main_menu_submenu_callback(void* context, uint32_t index) {
    Dfc* dfc = context;
    view_dispatcher_send_custom_event(dfc->view_dispatcher, index);
}

void dfc_scene_main_menu_on_enter(void* context) {
    Dfc* dfc = context;
    Submenu* submenu = dfc->submenu;
    submenu_reset(submenu);

    submenu_add_item(
        submenu, "Saved", SubmenuIndexSaved, dfc_scene_main_menu_submenu_callback, dfc);
    submenu_add_item(
        submenu, "Blank Card", SubmenuIndexBlankCard, dfc_scene_main_menu_submenu_callback, dfc);
    submenu_add_item(
        submenu, "About", SubmenuIndexAbout, dfc_scene_main_menu_submenu_callback, dfc);

    submenu_set_selected_item(
        dfc->submenu, scene_manager_get_scene_state(dfc->scene_manager, DfcSceneMainMenu));

    view_dispatcher_switch_to_view(dfc->view_dispatcher, DfcViewMenu);
}

bool dfc_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    Dfc* dfc = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexSaved) {
            scene_manager_set_scene_state(dfc->scene_manager, DfcSceneMainMenu, SubmenuIndexSaved);
            scene_manager_next_scene(dfc->scene_manager, DfcSceneFileSelect);
            consumed = true;
        } else if(event.event == SubmenuIndexBlankCard) {
            scene_manager_set_scene_state(
                dfc->scene_manager, DfcSceneMainMenu, SubmenuIndexBlankCard);
            dfc_credential_init_blank(dfc->credential);
            dfc->emulate_from_blank = true;
            scene_manager_next_scene(dfc->scene_manager, DfcSceneEmulate);
            consumed = true;
        } else if(event.event == SubmenuIndexAbout) {
            scene_manager_set_scene_state(dfc->scene_manager, DfcSceneMainMenu, SubmenuIndexAbout);
            scene_manager_next_scene(dfc->scene_manager, DfcSceneAbout);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        while(scene_manager_previous_scene(dfc->scene_manager))
            ;
    }

    return consumed;
}

void dfc_scene_main_menu_on_exit(void* context) {
    Dfc* dfc = context;

    submenu_reset(dfc->submenu);
}
