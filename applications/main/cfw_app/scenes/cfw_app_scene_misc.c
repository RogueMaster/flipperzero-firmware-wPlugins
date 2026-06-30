#include "../cfw_app.h"

enum VarItemListIndex {
    VarItemListIndexScreen,
    VarItemListIndexDolphin,
    VarItemListIndexSpoof,
    VarItemListIndexVgm,
    VarItemListIndexShowCFWIntro,
};

void cfw_app_scene_misc_var_item_list_callback(void* context, uint32_t index) {
    CFWApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void cfw_app_scene_misc_on_enter(void* context) {
    CFWApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;
    VariableItem* item;

    item = variable_item_list_add(var_item_list, "Screen", 0, NULL, app);
    variable_item_set_current_value_text(item, ">");

    item = variable_item_list_add(var_item_list, "Dolphin", 0, NULL, app);
    variable_item_set_current_value_text(item, ">");

    item = variable_item_list_add(var_item_list, "Spoofing Options", 0, NULL, app);
    variable_item_set_current_value_text(item, ">");

    item = variable_item_list_add(var_item_list, "VGM Options", 0, NULL, app);
    variable_item_set_current_value_text(item, ">");

    variable_item_list_add(var_item_list, "Show CFW Intro", 0, NULL, app);

    variable_item_list_set_enter_callback(
        var_item_list, cfw_app_scene_misc_var_item_list_callback, app);

    variable_item_list_set_selected_item(
        var_item_list, scene_manager_get_scene_state(app->scene_manager, CFWAppSceneMisc));

    view_dispatcher_switch_to_view(app->view_dispatcher, CFWAppViewVarItemList);
}

bool cfw_app_scene_misc_on_event(void* context, SceneManagerEvent event) {
    CFWApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, CFWAppSceneMisc, event.event);
        consumed = true;
        switch(event.event) {
        case VarItemListIndexScreen:
            scene_manager_set_scene_state(app->scene_manager, CFWAppSceneMiscScreen, 0);
            scene_manager_next_scene(app->scene_manager, CFWAppSceneMiscScreen);
            break;
        case VarItemListIndexDolphin:
            scene_manager_set_scene_state(app->scene_manager, CFWAppSceneMiscDolphin, 0);
            scene_manager_next_scene(app->scene_manager, CFWAppSceneMiscDolphin);
            break;
        case VarItemListIndexSpoof:
            scene_manager_set_scene_state(app->scene_manager, CFWAppSceneMiscSpoof, 0);
            scene_manager_next_scene(app->scene_manager, CFWAppSceneMiscSpoof);
            break;
        case VarItemListIndexVgm:
            scene_manager_set_scene_state(app->scene_manager, CFWAppSceneMiscVgm, 0);
            scene_manager_next_scene(app->scene_manager, CFWAppSceneMiscVgm);
            break;
        case VarItemListIndexShowCFWIntro: {
            for(int i = 0; i < 10; i++) {
                if(storage_common_copy(
                       app->storage, EXT_PATH("dolphin/firstboot.bin"), SLIDESHOW_FS_PATH)) {
                    app->show_slideshow = true;
                    cfw_app_apply(app);
                    break;
                }
            }
            break;
        }
        default:
            break;
        }
    }

    return consumed;
}

void cfw_app_scene_misc_on_exit(void* context) {
    CFWApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
