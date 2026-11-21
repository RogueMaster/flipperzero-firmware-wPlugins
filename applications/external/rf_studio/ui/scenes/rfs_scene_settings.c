#include "../../rf_studio_i.h"

enum {
    SettingRadio,
    SettingScope,
    SettingTrigger,
    SettingPreset,
    SettingSort,
    SettingLogging,
    SettingRefresh,
    SettingBackup,
};

#define TRIGGER_STEPS (((RFS_RSSI_MAX) - (RFS_RSSI_MIN)) / RFS_RSSI_STEP + 1)

static void rfs_setting_radio_changed(VariableItem* item) {
    RfStudioApp* app = variable_item_get_context(item);
    app->settings.radio_mode = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, rfs_radio_mode_name(app->settings.radio_mode));
}

static void rfs_setting_scope_changed(VariableItem* item) {
    RfStudioApp* app = variable_item_get_context(item);
    app->settings.discover_scope = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(
        item, rfs_discover_scope_name(app->settings.discover_scope));
}

static void rfs_setting_trigger_text(VariableItem* item, int32_t value) {
    char text[16];
    snprintf(text, sizeof(text), "%ld dBm", (long)value);
    variable_item_set_current_value_text(item, text);
}

static void rfs_setting_trigger_changed(VariableItem* item) {
    RfStudioApp* app = variable_item_get_context(item);
    app->settings.rssi_threshold =
        RFS_RSSI_MIN + (int32_t)variable_item_get_current_value_index(item) * RFS_RSSI_STEP;
    rfs_setting_trigger_text(item, app->settings.rssi_threshold);
}

static void rfs_setting_preset_changed(VariableItem* item) {
    RfStudioApp* app = variable_item_get_context(item);
    const char* name =
        rfs_radio_preset_name(app->radio, variable_item_get_current_value_index(item));
    strlcpy(app->settings.default_preset, name, sizeof(app->settings.default_preset));
    variable_item_set_current_value_text(item, name);
}

static void rfs_setting_sort_changed(VariableItem* item) {
    RfStudioApp* app = variable_item_get_context(item);
    app->settings.sort = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, rfs_sort_name(app->settings.sort));
}

static void rfs_setting_logging_changed(VariableItem* item) {
    RfStudioApp* app = variable_item_get_context(item);
    app->settings.logging = variable_item_get_current_value_index(item) == 1;
    variable_item_set_current_value_text(item, app->settings.logging ? "On" : "Off");
}

static void rfs_setting_enter_callback(void* context, uint32_t index) {
    RfStudioApp* app = context;
    if(index == SettingRefresh || index == SettingBackup) {
        view_dispatcher_send_custom_event(app->view_dispatcher, RfsEventMenuBase + index);
    }
}

void rfs_scene_settings_on_enter(void* context) {
    RfStudioApp* app = context;
    VariableItemList* list = app->var_list;
    variable_item_list_reset(list);
    VariableItem* item;

    item =
        variable_item_list_add(list, "Radio", RfsRadioModeCount, rfs_setting_radio_changed, app);
    variable_item_set_current_value_index(item, app->settings.radio_mode);
    variable_item_set_current_value_text(item, rfs_radio_mode_name(app->settings.radio_mode));

    item = variable_item_list_add(
        list, "Auto Detect", RfsDiscoverCount, rfs_setting_scope_changed, app);
    variable_item_set_current_value_index(item, app->settings.discover_scope);
    variable_item_set_current_value_text(
        item, rfs_discover_scope_name(app->settings.discover_scope));

    item = variable_item_list_add(
        list, "Trigger Level", TRIGGER_STEPS, rfs_setting_trigger_changed, app);
    variable_item_set_current_value_index(
        item, (app->settings.rssi_threshold - RFS_RSSI_MIN) / RFS_RSSI_STEP);
    rfs_setting_trigger_text(item, app->settings.rssi_threshold);

    size_t presets = rfs_radio_preset_count(app->radio);
    item = variable_item_list_add(
        list, "Modulation", presets > 0 ? presets : 1, rfs_setting_preset_changed, app);
    uint8_t preset_index = 0;
    for(size_t i = 0; i < presets; i++) {
        if(strcmp(rfs_radio_preset_name(app->radio, i), app->settings.default_preset) == 0) {
            preset_index = i;
        }
    }
    variable_item_set_current_value_index(item, preset_index);
    variable_item_set_current_value_text(item, app->settings.default_preset);

    item =
        variable_item_list_add(list, "Sort Remotes", RfsSortCount, rfs_setting_sort_changed, app);
    variable_item_set_current_value_index(item, app->settings.sort);
    variable_item_set_current_value_text(item, rfs_sort_name(app->settings.sort));

    item = variable_item_list_add(list, "Error Log File", 2, rfs_setting_logging_changed, app);
    variable_item_set_current_value_index(item, app->settings.logging ? 1 : 0);
    variable_item_set_current_value_text(item, app->settings.logging ? "On" : "Off");

    variable_item_list_add(list, "Refresh Radios", 0, NULL, app);
    variable_item_list_add(list, "Back Up Library", 0, NULL, app);

    variable_item_list_set_enter_callback(list, rfs_setting_enter_callback, app);
    variable_item_list_set_selected_item(
        list, scene_manager_get_scene_state(app->scene_manager, RfsSceneSettings));
    view_dispatcher_switch_to_view(app->view_dispatcher, RfsViewVarList);
}

bool rfs_scene_settings_on_event(void* context, SceneManagerEvent event) {
    RfStudioApp* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event < RfsEventMenuBase) return false;
    uint32_t index = event.event - RfsEventMenuBase;
    scene_manager_set_scene_state(app->scene_manager, RfsSceneSettings, index);
    FuriString* text = furi_string_alloc();

    if(index == SettingRefresh) {
        /* RF-013: rediscover capabilities on request. */
        if(rfs_tx_is_running(app->tx) || rfs_radio_is_busy(app->radio)) {
            rfs_app_message(app, "Radio Busy", "Try again in a moment.", RfsSceneNum);
        } else {
            rfs_radio_refresh(app->radio, app->settings.radio_mode != RfsRadioModeInternal);
            const RfsCapabilities* caps = rfs_radio_caps(app->radio);
            furi_string_printf(
                text,
                "Internal: %s\nExternal: %s\n%u frequencies",
                caps->device_connected[RfsDeviceInternal] ? "ready" : "unavailable",
                caps->device_connected[RfsDeviceExternal] ? "connected" : "not detected",
                (unsigned)rfs_radio_freq_count(app->radio, RfsDiscoverAll));
            rfs_app_message(app, "Radios Refreshed", furi_string_get_cstr(text), RfsSceneNum);
        }
    } else if(index == SettingBackup) {
        FuriString* path = furi_string_alloc();
        if(app->sd_ready && rfs_storage_backup(app->storage, path)) {
            furi_string_printf(
                text,
                "Saved to rf_studio/\nbackups/%s",
                strrchr(furi_string_get_cstr(path), '/') + 1);
            rfs_app_message(app, "Backed Up", furi_string_get_cstr(text), RfsSceneNum);
        } else {
            rfs_diag_error(app->diag, "Backup failed");
            rfs_app_message(app, "Backup Failed", "Could not copy the library.", RfsSceneNum);
        }
        furi_string_free(path);
    }
    furi_string_free(text);
    return true;
}

void rfs_scene_settings_on_exit(void* context) {
    RfStudioApp* app = context;
    scene_manager_set_scene_state(
        app->scene_manager,
        RfsSceneSettings,
        variable_item_list_get_selected_item_index(app->var_list));
    if(app->sd_ready && !rfs_settings_save(app->storage, &app->settings)) {
        rfs_diag_error(app->diag, "Settings not saved");
    }
    rfs_diag_set_logging(app->diag, app->settings.logging && app->sd_ready);
    variable_item_list_reset(app->var_list);
}
