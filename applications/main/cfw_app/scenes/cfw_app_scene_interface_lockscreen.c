#include "../cfw_app.h"

enum VarItemListIndex {
    VarItemListIndexLockOnBoot,
    VarItemListIndexFormatOn10BadPins,
    VarItemListIndexPinUnlockFromApp,
    VarItemListIndexShowTime,
    VarItemListIndexShowSeconds,
    VarItemListIndexShowDate,
    VarItemListIndexShowStatusbar,
    VarItemListIndexUnlockPrompt,
};

void cfw_app_scene_interface_lockscreen_var_item_list_callback(void* context, uint32_t index) {
    CFWApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void cfw_app_scene_interface_lockscreen_lock_on_boot_changed(VariableItem* item) {
    CFWApp* app = variable_item_get_context(item);
    bool value = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, value ? "ON" : "OFF");
    cfw_settings.lock_on_boot = value;
    app->save_settings = true;
}

static void cfw_app_scene_interface_lockscreen_bad_pins_format_changed(VariableItem* item) {
    CFWApp* app = variable_item_get_context(item);
    bool value = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, value ? "ON" : "OFF");
    cfw_settings.bad_pins_format = value;
    app->save_settings = true;
}

static void cfw_app_scene_interface_lockscreen_allow_locked_rpc_usb_changed(VariableItem* item) {
    CFWApp* app = variable_item_get_context(item);
    bool value = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, value ? "ON" : "OFF");
    cfw_settings.allow_locked_rpc_usb = value;
    app->save_settings = true;
}

static void cfw_app_scene_interface_lockscreen_allow_locked_rpc_ble_changed(VariableItem* item) {
    CFWApp* app = variable_item_get_context(item);
    bool value = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, value ? "ON" : "OFF");
    cfw_settings.allow_locked_rpc_ble = value;
    app->save_settings = true;
}

static void cfw_app_scene_interface_lockscreen_lockscreen_poweroff_changed(VariableItem* item) {
    CFWApp* app = variable_item_get_context(item);
    bool value = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, value ? "ON" : "OFF");
    cfw_settings.lockscreen_poweroff = value;
    app->save_settings = true;
}

static void cfw_app_scene_interface_lockscreen_lockscreen_time_changed(VariableItem* item) {
    CFWApp* app = variable_item_get_context(item);
    bool value = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, value ? "ON" : "OFF");
    cfw_settings.lockscreen_time = value;
    app->save_settings = true;
}

static void cfw_app_scene_interface_lockscreen_lockscreen_seconds_changed(VariableItem* item) {
    CFWApp* app = variable_item_get_context(item);
    bool value = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, value ? "ON" : "OFF");
    cfw_settings.lockscreen_seconds = value;
    app->save_settings = true;
}

static void cfw_app_scene_interface_lockscreen_lockscreen_date_changed(VariableItem* item) {
    CFWApp* app = variable_item_get_context(item);
    bool value = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, value ? "ON" : "OFF");
    cfw_settings.lockscreen_date = value;
    app->save_settings = true;
}

static void cfw_app_scene_interface_lockscreen_lockscreen_statusbar_changed(VariableItem* item) {
    CFWApp* app = variable_item_get_context(item);
    bool value = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, value ? "ON" : "OFF");
    cfw_settings.lockscreen_statusbar = value;
    app->save_settings = true;
}

static void cfw_app_scene_interface_lockscreen_lockscreen_prompt_changed(VariableItem* item) {
    CFWApp* app = variable_item_get_context(item);
    bool value = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, value ? "ON" : "OFF");
    cfw_settings.lockscreen_prompt = value;
    app->save_settings = true;
}

static void cfw_app_scene_interface_lockscreen_lockscreen_transparent_changed(VariableItem* item) {
    CFWApp* app = variable_item_get_context(item);
    bool value = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, value ? "ON" : "OFF");
    cfw_settings.lockscreen_transparent = value;
    app->save_settings = true;
}

static void
    cfw_app_scene_interface_lockscreen_lockscreen_skip_animation_changed(VariableItem* item) {
    CFWApp* app = variable_item_get_context(item);
    bool value = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, value ? "ON" : "OFF");
    cfw_settings.lockscreen_skip_animation = value;
    app->save_settings = true;
}

void cfw_app_scene_interface_lockscreen_on_enter(void* context) {
    CFWApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;
    VariableItem* item;

    item = variable_item_list_add(
        var_item_list,
        "Lock on Boot if PIN set",
        2,
        cfw_app_scene_interface_lockscreen_lock_on_boot_changed,
        app);
    variable_item_set_current_value_index(item, cfw_settings.lock_on_boot);
    variable_item_set_current_value_text(item, cfw_settings.lock_on_boot ? "ON" : "OFF");

    item = variable_item_list_add(
        var_item_list,
        "Format on 10 Bad PINs",
        2,
        cfw_app_scene_interface_lockscreen_bad_pins_format_changed,
        app);
    variable_item_set_current_value_index(item, cfw_settings.bad_pins_format);
    variable_item_set_current_value_text(item, cfw_settings.bad_pins_format ? "ON" : "OFF");

    item = variable_item_list_add(
        var_item_list,
        "Allow USB RPC While Locked",
        2,
        cfw_app_scene_interface_lockscreen_allow_locked_rpc_usb_changed,
        app);
    variable_item_set_current_value_index(item, cfw_settings.allow_locked_rpc_usb);
    variable_item_set_current_value_text(item, cfw_settings.allow_locked_rpc_usb ? "ON" : "OFF");

    item = variable_item_list_add(
        var_item_list,
        "Allow BLE RPC While Locked",
        2,
        cfw_app_scene_interface_lockscreen_allow_locked_rpc_ble_changed,
        app);
    variable_item_set_current_value_index(item, cfw_settings.allow_locked_rpc_ble);
    variable_item_set_current_value_text(item, cfw_settings.allow_locked_rpc_ble ? "ON" : "OFF");

    item = variable_item_list_add(
        var_item_list,
        "Allow Poweroff",
        2,
        cfw_app_scene_interface_lockscreen_lockscreen_poweroff_changed,
        app);
    variable_item_set_current_value_index(item, cfw_settings.lockscreen_poweroff);
    variable_item_set_current_value_text(item, cfw_settings.lockscreen_poweroff ? "ON" : "OFF");

    item = variable_item_list_add(
        var_item_list,
        "Show Time",
        2,
        cfw_app_scene_interface_lockscreen_lockscreen_time_changed,
        app);
    variable_item_set_current_value_index(item, cfw_settings.lockscreen_time);
    variable_item_set_current_value_text(item, cfw_settings.lockscreen_time ? "ON" : "OFF");

    item = variable_item_list_add(
        var_item_list,
        "Show Seconds",
        2,
        cfw_app_scene_interface_lockscreen_lockscreen_seconds_changed,
        app);
    variable_item_set_current_value_index(item, cfw_settings.lockscreen_seconds);
    variable_item_set_current_value_text(item, cfw_settings.lockscreen_seconds ? "ON" : "OFF");

    item = variable_item_list_add(
        var_item_list,
        "Show Date",
        2,
        cfw_app_scene_interface_lockscreen_lockscreen_date_changed,
        app);
    variable_item_set_current_value_index(item, cfw_settings.lockscreen_date);
    variable_item_set_current_value_text(item, cfw_settings.lockscreen_date ? "ON" : "OFF");

    item = variable_item_list_add(
        var_item_list,
        "Show Statusbar",
        2,
        cfw_app_scene_interface_lockscreen_lockscreen_statusbar_changed,
        app);
    variable_item_set_current_value_index(item, cfw_settings.lockscreen_statusbar);
    variable_item_set_current_value_text(item, cfw_settings.lockscreen_statusbar ? "ON" : "OFF");

    item = variable_item_list_add(
        var_item_list,
        "Unlock Prompt",
        2,
        cfw_app_scene_interface_lockscreen_lockscreen_prompt_changed,
        app);
    variable_item_set_current_value_index(item, cfw_settings.lockscreen_prompt);
    variable_item_set_current_value_text(item, cfw_settings.lockscreen_prompt ? "ON" : "OFF");

    item = variable_item_list_add(
        var_item_list,
        "Transparent (see animation)",
        2,
        cfw_app_scene_interface_lockscreen_lockscreen_transparent_changed,
        app);
    variable_item_set_current_value_index(item, cfw_settings.lockscreen_transparent);
    variable_item_set_current_value_text(item, cfw_settings.lockscreen_transparent ? "ON" : "OFF");

    item = variable_item_list_add(
        var_item_list,
        "Skip Sliding Animation",
        2,
        cfw_app_scene_interface_lockscreen_lockscreen_skip_animation_changed,
        app);
    variable_item_set_current_value_index(item, cfw_settings.lockscreen_skip_animation);
    variable_item_set_current_value_text(
        item, cfw_settings.lockscreen_skip_animation ? "ON" : "OFF");

    variable_item_list_set_enter_callback(
        var_item_list, cfw_app_scene_interface_lockscreen_var_item_list_callback, app);

    variable_item_list_set_selected_item(
        var_item_list,
        scene_manager_get_scene_state(app->scene_manager, CFWAppSceneInterfaceLockscreen));

    view_dispatcher_switch_to_view(app->view_dispatcher, CFWAppViewVarItemList);
}

bool cfw_app_scene_interface_lockscreen_on_event(void* context, SceneManagerEvent event) {
    CFWApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(
            app->scene_manager, CFWAppSceneInterfaceLockscreen, event.event);
        consumed = true;
        switch(event.event) {
        default:
            break;
        }
    }

    return consumed;
}

void cfw_app_scene_interface_lockscreen_on_exit(void* context) {
    CFWApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
