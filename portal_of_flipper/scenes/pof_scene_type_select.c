#include "../portal_of_flipper_i.h"
#include "../pof_token.h"

#define TAG "PoFSceneTypeSelect"

enum SubmenuIndex {
    SubmenuIndexSwapHid,
    SubmenuIndexSwapXbox360
};

void pof_scene_type_select_submenu_callback(void* context, uint32_t index) {
    PoFApp* pof = context;
    view_dispatcher_send_custom_event(pof->view_dispatcher, index);
}

void pof_scene_type_select_on_enter(void* context) {
    PoFApp* pof = context;
    Submenu* submenu = pof->submenu;
    submenu_reset(pof->submenu);
    submenu_add_item(
    submenu,
    "Emulate Xbox 360",
    SubmenuIndexSwapXbox360,
    pof_scene_type_select_submenu_callback,
    pof);
    submenu_add_item(
    submenu,
    "Emulate HID",
    SubmenuIndexSwapHid,
    pof_scene_type_select_submenu_callback,
    pof);
    view_dispatcher_switch_to_view(pof->view_dispatcher, PoFViewSubmenu);
}

bool pof_scene_type_select_on_event(void* context, SceneManagerEvent event) {
    PoFApp* pof = context;
    if(event.type == SceneManagerEventTypeCustom) {
        if (event.event == SubmenuIndexSwapHid) {
            virtual_portal_set_type(pof->virtual_portal, PoFHid);
            pof_start(pof);
            scene_manager_next_scene(pof->scene_manager, PoFSceneMain);
            return true;
        } else if (event.event == SubmenuIndexSwapXbox360) {
            virtual_portal_set_type(pof->virtual_portal, PoFXbox360);
            pof_start(pof);
            scene_manager_next_scene(pof->scene_manager, PoFSceneMain);
            return true;
        } 
    }
    return false;
}

void pof_scene_type_select_on_exit(void* context) {
    PoFApp* pof = context;
    submenu_reset(pof->submenu);
}
