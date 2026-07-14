#include "../cfw_app.h"

enum VarItemListIndex {
    VarItemListIndexScrollType,
    VarItemListIndexMidnightFormat,
    VarItemListIndexGameMode,
};

void cfw_app_scene_interface_general_var_item_list_callback(void* context, uint32_t index) {
    CFWApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void cfw_app_scene_interface_general_scroll_marquee_changed(VariableItem* item) {
    CFWApp* app = variable_item_get_context(item);
    bool value = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, value ? "Marquee" : "Standard");
    cfw_settings.scroll_marquee = value;
    app->save_settings = true;
}

static void cfw_app_scene_interface_general_midnight_format_changed(VariableItem* item) {
    CFWApp* app = variable_item_get_context(item);
    bool value = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, value ? "00:XX" : "12:00");
    cfw_settings.midnight_format_00 = value;
    app->save_settings = true;
}

static void cfw_app_scene_interface_general_game_mode_changed(VariableItem* item) {
    CFWApp* app = variable_item_get_context(item);
    bool value = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, value ? "ON" : "OFF");
    cfw_settings.game_mode = value;
    app->save_settings = true;
}

static void cfw_app_scene_interface_general_popup_overlay_changed(VariableItem* item) {
    CFWApp* app = variable_item_get_context(item);
    bool value = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, value ? "ON" : "OFF");
    cfw_settings.popup_overlay = value;
    app->save_settings = true;
}

void cfw_app_scene_interface_general_on_enter(void* context) {
    CFWApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;
    VariableItem* item;

    item = variable_item_list_add(
        var_item_list,
        "Text Scroll",
        2,
        cfw_app_scene_interface_general_scroll_marquee_changed,
        app);
    variable_item_set_current_value_index(item, cfw_settings.scroll_marquee);
    variable_item_set_current_value_text(
        item, cfw_settings.scroll_marquee ? "Marquee" : "Standard");

    item = variable_item_list_add(
        var_item_list,
        "Clock Midnight Format",
        2,
        cfw_app_scene_interface_general_midnight_format_changed,
        app);
    variable_item_set_current_value_index(item, cfw_settings.midnight_format_00);
    variable_item_set_current_value_text(
        item, cfw_settings.midnight_format_00 ? "00:XX" : "12:XX");

    if(!cfw_settings.game_mode) {
        item = variable_item_list_add(
            var_item_list, "Game Mode", 2, cfw_app_scene_interface_general_game_mode_changed, app);
        variable_item_set_current_value_index(item, cfw_settings.game_mode);
        variable_item_set_current_value_text(item, cfw_settings.game_mode ? "ON" : "OFF");
    }

    item = variable_item_list_add(
        var_item_list,
        "Popup Overlay",
        2,
        cfw_app_scene_interface_general_popup_overlay_changed,
        app);
    variable_item_set_current_value_index(item, cfw_settings.popup_overlay);
    variable_item_set_current_value_text(item, cfw_settings.popup_overlay ? "ON" : "OFF");

    variable_item_list_set_enter_callback(
        var_item_list, cfw_app_scene_interface_general_var_item_list_callback, app);

    variable_item_list_set_selected_item(
        var_item_list,
        scene_manager_get_scene_state(app->scene_manager, CFWAppSceneInterfaceGeneral));

    view_dispatcher_switch_to_view(app->view_dispatcher, CFWAppViewVarItemList);
}

bool cfw_app_scene_interface_general_on_event(void* context, SceneManagerEvent event) {
    CFWApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(
            app->scene_manager, CFWAppSceneInterfaceGeneral, event.event);
        consumed = true;
    }

    return consumed;
}

void cfw_app_scene_interface_general_on_exit(void* context) {
    CFWApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
