#include "../cfw_app.h"

enum VarItemListIndex {
    VarItemListIndexGraphics,
    VarItemListIndexMainmenu,
    VarItemListIndexLockscreen,
    VarItemListIndexStatusbar,
    VarItemListIndexFileBrowser,
    VarItemListIndexPassport,
    VarItemListIndexGeneral,
};

void cfw_app_scene_interface_var_item_list_callback(void* context, uint32_t index) {
    CFWApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void cfw_app_scene_interface_on_enter(void* context) {
    CFWApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;
    VariableItem* item;

    item = variable_item_list_add(var_item_list, "Graphics", 0, NULL, app);
    variable_item_set_current_value_text(item, ">");

    item = variable_item_list_add(var_item_list, "Mainmenu", 0, NULL, app);
    variable_item_set_current_value_text(item, ">");

    item = variable_item_list_add(var_item_list, "Lockscreen", 0, NULL, app);
    variable_item_set_current_value_text(item, ">");

    item = variable_item_list_add(var_item_list, "Statusbar", 0, NULL, app);
    variable_item_set_current_value_text(item, ">");

    item = variable_item_list_add(var_item_list, "File Browser", 0, NULL, app);
    variable_item_set_current_value_text(item, ">");

    item = variable_item_list_add(var_item_list, "Passport", 0, NULL, app);
    variable_item_set_current_value_text(item, ">");

    item = variable_item_list_add(var_item_list, "General", 0, NULL, app);
    variable_item_set_current_value_text(item, ">");

    variable_item_list_set_enter_callback(
        var_item_list, cfw_app_scene_interface_var_item_list_callback, app);

    variable_item_list_set_selected_item(
        var_item_list, scene_manager_get_scene_state(app->scene_manager, CFWAppSceneInterface));

    view_dispatcher_switch_to_view(app->view_dispatcher, CFWAppViewVarItemList);
}

bool cfw_app_scene_interface_on_event(void* context, SceneManagerEvent event) {
    CFWApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, CFWAppSceneInterface, event.event);
        consumed = true;
        switch(event.event) {
        case VarItemListIndexGraphics:
            scene_manager_set_scene_state(app->scene_manager, CFWAppSceneInterfaceGraphics, 0);
            scene_manager_next_scene(app->scene_manager, CFWAppSceneInterfaceGraphics);
            break;
        case VarItemListIndexMainmenu:
            scene_manager_set_scene_state(app->scene_manager, CFWAppSceneInterfaceMainmenu, 0);
            scene_manager_next_scene(app->scene_manager, CFWAppSceneInterfaceMainmenu);
            break;
        case VarItemListIndexLockscreen:
            scene_manager_set_scene_state(app->scene_manager, CFWAppSceneInterfaceLockscreen, 0);
            scene_manager_next_scene(app->scene_manager, CFWAppSceneInterfaceLockscreen);
            break;
        case VarItemListIndexStatusbar:
            scene_manager_set_scene_state(app->scene_manager, CFWAppSceneInterfaceStatusbar, 0);
            scene_manager_next_scene(app->scene_manager, CFWAppSceneInterfaceStatusbar);
            break;
        case VarItemListIndexFileBrowser:
            scene_manager_set_scene_state(app->scene_manager, CFWAppSceneInterfaceFilebrowser, 0);
            scene_manager_next_scene(app->scene_manager, CFWAppSceneInterfaceFilebrowser);
            break;
        case VarItemListIndexPassport:
            scene_manager_set_scene_state(app->scene_manager, CFWAppSceneInterfacePassport, 0);
            scene_manager_next_scene(app->scene_manager, CFWAppSceneInterfacePassport);
            break;
        case VarItemListIndexGeneral:
            scene_manager_set_scene_state(app->scene_manager, CFWAppSceneInterfaceGeneral, 0);
            scene_manager_next_scene(app->scene_manager, CFWAppSceneInterfaceGeneral);
            break;
        default:
            break;
        }
    }

    return consumed;
}

void cfw_app_scene_interface_on_exit(void* context) {
    CFWApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
