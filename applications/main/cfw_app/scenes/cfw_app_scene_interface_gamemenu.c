#include "../cfw_app.h"

extern const char* const menu_style_names[MenuStyleCount];

enum {
    GameMenuIndexStyle,
    GameMenuIndexStartPoint,
    GameMenuIndexReset,
    GameMenuIndexItem,
    GameMenuIndexAdd,
    GameMenuIndexMove,
    GameMenuIndexRemove,
};

static void gamemenu_enter_callback(void* context, uint32_t index) {
    CFWApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static size_t gamemenu_step(size_t index, size_t count, uint8_t direction) {
    if(!count) return 0;
    if(direction == 0) return index ? index - 1 : count - 1;
    if(direction == 2) return (index + 1) % count;
    return index;
}

static void gamemenu_refresh(CFWApp* app) {
    size_t count = CharList_size(app->gamemenu_app_labels);
    if(app->gamemenu_app_index >= count) app->gamemenu_app_index = count ? count - 1 : 0;
    if(cfw_settings.game_start_point >= count && cfw_settings.game_start_point) {
        cfw_settings.game_start_point = 0;
        app->save_settings = true;
    }

    VariableItem* item = variable_item_list_get(app->var_item_list, GameMenuIndexItem);
    char label[32];
    if(count) {
        snprintf(label, sizeof(label), "Item %zu/%zu", app->gamemenu_app_index + 1, count);
        variable_item_set_item_label(item, label);
        variable_item_set_current_value_text(
            item, *CharList_get(app->gamemenu_app_labels, app->gamemenu_app_index));
    } else {
        variable_item_set_item_label(item, "Item");
        variable_item_set_current_value_text(item, "None");
    }
    variable_item_set_values_count(item, count ? 3 : 0);
    variable_item_set_current_value_index(item, 1);

    item = variable_item_list_get(app->var_item_list, GameMenuIndexStartPoint);
    variable_item_set_values_count(item, count ? 3 : 0);
    variable_item_set_current_value_index(item, 1);
    variable_item_set_current_value_text(
        item,
        count ? *CharList_get(app->gamemenu_app_labels, cfw_settings.game_start_point) : "None");
    variable_item_set_locked(item, !count, "Add a game first.");

    item = variable_item_list_get(app->var_item_list, GameMenuIndexMove);
    variable_item_set_locked(item, count < 2, "Add at least\n2 games to move.");
    item = variable_item_list_get(app->var_item_list, GameMenuIndexRemove);
    variable_item_set_locked(item, !count, "Add a game first.");
}

static void gamemenu_style_changed(VariableItem* item) {
    CFWApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= MenuStyleCount) return;
    cfw_settings.game_menu_style = index;
    variable_item_set_current_value_text(item, menu_style_names[index]);
    app->save_settings = true;
}

static void gamemenu_start_changed(VariableItem* item) {
    CFWApp* app = variable_item_get_context(item);
    size_t count = CharList_size(app->gamemenu_app_labels);
    if(count) {
        cfw_settings.game_start_point = gamemenu_step(
            cfw_settings.game_start_point, count, variable_item_get_current_value_index(item));
        variable_item_set_current_value_text(
            item, *CharList_get(app->gamemenu_app_labels, cfw_settings.game_start_point));
        app->save_settings = true;
    }
    variable_item_set_current_value_index(item, 1);
}

static void gamemenu_item_changed(VariableItem* item) {
    CFWApp* app = variable_item_get_context(item);
    size_t count = CharList_size(app->gamemenu_app_labels);
    if(count) {
        app->gamemenu_app_index = gamemenu_step(
            app->gamemenu_app_index, count, variable_item_get_current_value_index(item));
        char label[32];
        snprintf(label, sizeof(label), "Item %zu/%zu", app->gamemenu_app_index + 1, count);
        variable_item_set_item_label(item, label);
        variable_item_set_current_value_text(
            item, *CharList_get(app->gamemenu_app_labels, app->gamemenu_app_index));
    }
    variable_item_set_current_value_index(item, 1);
}

static void gamemenu_move_changed(VariableItem* item) {
    CFWApp* app = variable_item_get_context(item);
    size_t count = CharList_size(app->gamemenu_app_labels);
    size_t index = app->gamemenu_app_index;
    uint8_t direction = variable_item_get_current_value_index(item);
    if(count > 1 && index < count) {
        size_t target = index;
        if(direction == 0 && index) target--;
        if(direction == 2 && index + 1 < count) target++;
        if(target != index) {
            CharList_swap_at(app->gamemenu_app_labels, index, target);
            CharList_swap_at(app->gamemenu_app_exes, index, target);
            if(cfw_settings.game_start_point == index) {
                cfw_settings.game_start_point = target;
                app->save_settings = true;
            } else if(cfw_settings.game_start_point == target) {
                cfw_settings.game_start_point = index;
                app->save_settings = true;
            }
            app->gamemenu_app_index = target;
            app->save_gamemenu_apps = true;
            app->gamemenu_source = GameMenuSourceCustom;
            view_dispatcher_send_custom_event(app->view_dispatcher, GameMenuIndexMove);
        }
    }
    variable_item_set_current_value_index(item, 1);
}

void cfw_app_scene_interface_gamemenu_on_enter(void* context) {
    CFWApp* app = context;
    if(!app->gamemenu_apps_loaded) cfw_app_load_gamemenu_apps(app);
    if((uint32_t)cfw_settings.game_menu_style >= MenuStyleCount) {
        cfw_settings.game_menu_style = MenuStyleList;
        app->save_settings = true;
    }
    VariableItemList* list = app->var_item_list;
    variable_item_list_set_header(list, "Game Menu");
    VariableItem* item =
        variable_item_list_add(list, "Menu Style", MenuStyleCount, gamemenu_style_changed, app);
    variable_item_set_current_value_index(item, cfw_settings.game_menu_style);
    variable_item_set_current_value_text(item, menu_style_names[cfw_settings.game_menu_style]);
    variable_item_list_add(list, "Start Point", 3, gamemenu_start_changed, app);
    variable_item_list_add(list, "Reset Menu", 0, NULL, app);
    /* Three direction values keep the editor usable beyond the widget's uint8_t count limit.
     * OK opens the full selector; Left/Right browse individual entries. */
    variable_item_list_add(list, "Item", 3, gamemenu_item_changed, app);
    variable_item_list_add(list, "Add App", 0, NULL, app);
    item = variable_item_list_add(list, "Move App", 3, gamemenu_move_changed, app);
    variable_item_set_current_value_index(item, 1);
    variable_item_set_current_value_text(item, "");
    variable_item_list_add(list, "Remove App", 0, NULL, app);
    gamemenu_refresh(app);
    variable_item_list_set_enter_callback(list, gamemenu_enter_callback, app);
    variable_item_list_set_selected_item(
        list, scene_manager_get_scene_state(app->scene_manager, CFWAppSceneInterfaceGamemenu));
    view_dispatcher_switch_to_view(app->view_dispatcher, CFWAppViewVarItemList);
}

bool cfw_app_scene_interface_gamemenu_on_event(void* context, SceneManagerEvent event) {
    CFWApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    scene_manager_set_scene_state(app->scene_manager, CFWAppSceneInterfaceGamemenu, event.event);
    switch(event.event) {
    case GameMenuIndexStyle:
        scene_manager_next_scene(app->scene_manager, CFWAppSceneInterfaceGamemenuStyle);
        break;
    case GameMenuIndexStartPoint:
    case GameMenuIndexItem:
        if(CharList_size(app->gamemenu_app_labels)) {
            scene_manager_set_scene_state(
                app->scene_manager,
                CFWAppSceneInterfaceGamemenuSelect,
                event.event == GameMenuIndexStartPoint);
            scene_manager_next_scene(app->scene_manager, CFWAppSceneInterfaceGamemenuSelect);
        }
        break;
    case GameMenuIndexReset:
        scene_manager_next_scene(app->scene_manager, CFWAppSceneInterfaceGamemenuReset);
        break;
    case GameMenuIndexAdd:
        scene_manager_next_scene(app->scene_manager, CFWAppSceneInterfaceGamemenuAdd);
        break;
    case GameMenuIndexRemove: {
        size_t index = app->gamemenu_app_index;
        size_t count = CharList_size(app->gamemenu_app_labels);
        if(index >= count) break;
        free(*CharList_get(app->gamemenu_app_labels, index));
        free(*CharList_get(app->gamemenu_app_exes, index));
        CharList_remove_v(app->gamemenu_app_labels, index, index + 1);
        CharList_remove_v(app->gamemenu_app_exes, index, index + 1);
        if(cfw_settings.game_start_point > index) {
            cfw_settings.game_start_point--;
            app->save_settings = true;
        }
        app->save_gamemenu_apps = true;
        app->gamemenu_source = GameMenuSourceCustom;
        gamemenu_refresh(app);
        break;
    }
    case GameMenuIndexMove:
        gamemenu_refresh(app);
        break;
    default:
        break;
    }
    return true;
}

void cfw_app_scene_interface_gamemenu_on_exit(void* context) {
    CFWApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
