#include "../cfw_app.h"

enum VarItemListIndex {
    VarItemListIndexMenuStyle,
    VarItemListIndexResetMenu,
    VarItemListIndexItem,
    VarItemListIndexAddItem,
    VarItemListIndexMoveItem,
    VarItemListIndexRemoveItem,
};

void cfw_app_scene_interface_mainmenu_var_item_list_callback(void* context, uint32_t index) {
    CFWApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

const char* const menu_style_names[MenuStyleCount] = {
    "List",
    "Wii",
    "DSi",
    "PS4",
    "Vertical",
    "C64",
    "Compact",
    "MNTM",
    "CoverFlow",
};
static void cfw_app_scene_interface_mainmenu_menu_style_changed(VariableItem* item) {
    CFWApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, menu_style_names[index]);
    cfw_settings.menu_style = index;
    app->save_settings = true;
}

static void cfw_app_scene_interface_mainmenu_app_changed(VariableItem* item) {
    CFWApp* app = variable_item_get_context(item);
    app->mainmenu_app_index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(
        item, *CharList_get(app->mainmenu_app_labels, app->mainmenu_app_index));
    size_t count = CharList_size(app->mainmenu_app_labels);
    char label[20];
    snprintf(label, sizeof(label), "Item  %u/%u", 1 + app->mainmenu_app_index, count);
    variable_item_set_item_label(item, label);
}

static void cfw_app_scene_interface_mainmenu_move_app_changed(VariableItem* item) {
    CFWApp* app = variable_item_get_context(item);
    uint8_t idx = app->mainmenu_app_index;
    size_t size = CharList_size(app->mainmenu_app_labels);
    uint8_t dir = variable_item_get_current_value_index(item);
    if(size >= 2) {
        if(dir == 2 && idx != size - 1) {
            // Right
            CharList_swap_at(app->mainmenu_app_labels, idx, idx + 1);
            CharList_swap_at(app->mainmenu_app_exes, idx, idx + 1);
            app->mainmenu_app_index++;
        } else if(dir == 0 && idx != 0) {
            // Left
            CharList_swap_at(app->mainmenu_app_labels, idx, idx - 1);
            CharList_swap_at(app->mainmenu_app_exes, idx, idx - 1);
            app->mainmenu_app_index--;
        }
        view_dispatcher_send_custom_event(app->view_dispatcher, VarItemListIndexMoveItem);
    }
    variable_item_set_current_value_index(item, 1);
}

void cfw_app_scene_interface_mainmenu_on_enter(void* context) {
    CFWApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;
    VariableItem* item;

    item = variable_item_list_add(
        var_item_list,
        "Menu Style",
        MenuStyleCount,
        cfw_app_scene_interface_mainmenu_menu_style_changed,
        app);
    variable_item_set_current_value_text(item, menu_style_names[cfw_settings.menu_style]);
    variable_item_set_current_value_index(item, cfw_settings.menu_style);

    variable_item_list_add(var_item_list, "Reset Menu", 0, NULL, app);

    size_t count = CharList_size(app->mainmenu_app_labels);
    item = variable_item_list_add(
        var_item_list, "Item", count, cfw_app_scene_interface_mainmenu_app_changed, app);
    if(count) {
        app->mainmenu_app_index = CLAMP(app->mainmenu_app_index, count - 1, 0U);
        char label[21];
        snprintf(label, sizeof(label), "Item  %u/%u", 1 + app->mainmenu_app_index, count);
        variable_item_set_item_label(item, label);
        variable_item_set_current_value_text(
            item, *CharList_get(app->mainmenu_app_labels, app->mainmenu_app_index));
    } else {
        app->mainmenu_app_index = 0;
        variable_item_set_current_value_text(item, "None");
    }
    variable_item_set_current_value_index(item, app->mainmenu_app_index);

    variable_item_list_add(var_item_list, "Add Item", 0, NULL, app);

    item = variable_item_list_add(
        var_item_list, "Move Item", 3, cfw_app_scene_interface_mainmenu_move_app_changed, app);
    variable_item_set_current_value_text(item, "");
    variable_item_set_current_value_index(item, 1);
    variable_item_set_locked(item, count < 2, "Can't move\nwith less\nthan 2 apps!");

    variable_item_list_add(var_item_list, "Remove Item", 0, NULL, app);

    variable_item_list_set_enter_callback(
        var_item_list, cfw_app_scene_interface_mainmenu_var_item_list_callback, app);

    variable_item_list_set_selected_item(
        var_item_list,
        scene_manager_get_scene_state(app->scene_manager, CFWAppSceneInterfaceMainmenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, CFWAppViewVarItemList);
}

bool cfw_app_scene_interface_mainmenu_on_event(void* context, SceneManagerEvent event) {
    CFWApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(
            app->scene_manager, CFWAppSceneInterfaceMainmenu, event.event);
        consumed = true;
        switch(event.event) {
        case VarItemListIndexMenuStyle:
            scene_manager_next_scene(app->scene_manager, CFWAppSceneInterfaceMainmenuStyle);
            break;
        case VarItemListIndexResetMenu:
            scene_manager_next_scene(app->scene_manager, CFWAppSceneInterfaceMainmenuReset);
            break;
        case VarItemListIndexRemoveItem:
            if(!CharList_size(app->mainmenu_app_labels)) break;
            if(!CharList_size(app->mainmenu_app_exes)) break;
            free(*CharList_get(app->mainmenu_app_labels, app->mainmenu_app_index));
            free(*CharList_get(app->mainmenu_app_exes, app->mainmenu_app_index));
            CharList_remove_v(
                app->mainmenu_app_labels, app->mainmenu_app_index, app->mainmenu_app_index + 1);
            CharList_remove_v(
                app->mainmenu_app_exes, app->mainmenu_app_index, app->mainmenu_app_index + 1);
            /* fall through */
        case VarItemListIndexMoveItem: {
            app->save_mainmenu_apps = true;
            size_t count = CharList_size(app->mainmenu_app_labels);
            VariableItem* item = variable_item_list_get(app->var_item_list, VarItemListIndexItem);
            if(count) {
                app->mainmenu_app_index = CLAMP(app->mainmenu_app_index, count - 1, 0U);
                char label[21];
                snprintf(label, sizeof(label), "Item  %u/%u", 1 + app->mainmenu_app_index, count);
                variable_item_set_item_label(item, label);
                variable_item_set_current_value_text(
                    item, *CharList_get(app->mainmenu_app_labels, app->mainmenu_app_index));
            } else {
                app->mainmenu_app_index = 0;
                variable_item_set_item_label(item, "Item");
                variable_item_set_current_value_text(item, "None");
            }
            variable_item_set_current_value_index(item, app->mainmenu_app_index);
            variable_item_set_values_count(item, count);
            break;
        }
        case VarItemListIndexAddItem:
            scene_manager_next_scene(app->scene_manager, CFWAppSceneInterfaceMainmenuAdd);
            break;
        default:
            break;
        }
    }

    return consumed;
}

void cfw_app_scene_interface_mainmenu_on_exit(void* context) {
    CFWApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
