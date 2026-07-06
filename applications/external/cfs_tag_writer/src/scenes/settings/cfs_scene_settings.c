#include "src/core/cfs_app_i.h"

typedef enum {
    SettingsItemBrand,
    SettingsItemSerial,
    SettingsItemColors,
    SettingsItemWeights,
    SettingsItemSelfTest,
} SettingsItem;

static void cfs_settings_brand_changed(VariableItem* item) {
    CfsApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.default_brand = (CfsBrand)idx;
    variable_item_set_current_value_text(item, cfs_brand_to_name((CfsBrand)idx));
    cfs_settings_save(app->storage, &app->settings);
}

static void cfs_settings_serial_changed(VariableItem* item) {
    CfsApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.serial_mode = (CfsSerialMode)idx;
    variable_item_set_current_value_text(item, cfs_serial_mode_label((CfsSerialMode)idx));
    cfs_settings_save(app->storage, &app->settings);
}

static void cfs_settings_colors_changed(VariableItem* item) {
    CfsApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.color_palette = (CfsColorPalette)idx;
    variable_item_set_current_value_text(item, cfs_palette_label((CfsColorPalette)idx));
    cfs_settings_save(app->storage, &app->settings);
}

static void cfs_settings_enter_callback(void* context, uint32_t index) {
    CfsApp* app = context;
    if(index == SettingsItemWeights) {
        scene_manager_next_scene(app->scene_manager, CfsSceneSettingsWeight);
    } else if(index == SettingsItemSelfTest) {
        scene_manager_next_scene(app->scene_manager, CfsSceneDebugSelftest);
    }
}

void cfs_scene_settings_on_enter(void* context) {
    CfsApp* app = context;
    VariableItemList* list = app->variable_item_list;
    variable_item_list_reset(list);

    VariableItem* item;
    // Only writable brands (Generic/Creality) — eSUN is read/catalog-only.
    item = variable_item_list_add(
        list, "Brand", CFS_BRAND_WRITABLE_COUNT, cfs_settings_brand_changed, app);
    variable_item_set_current_value_index(item, app->settings.default_brand);
    variable_item_set_current_value_text(item, cfs_brand_to_name(app->settings.default_brand));

    item =
        variable_item_list_add(list, "Serial", CfsSerialModeCount, cfs_settings_serial_changed, app);
    variable_item_set_current_value_index(item, app->settings.serial_mode);
    variable_item_set_current_value_text(item, cfs_serial_mode_label(app->settings.serial_mode));

    item = variable_item_list_add(list, "Colors", CfsPaletteCount, cfs_settings_colors_changed, app);
    variable_item_set_current_value_index(item, app->settings.color_palette);
    variable_item_set_current_value_text(item, cfs_palette_label(app->settings.color_palette));

    variable_item_list_add(list, "Weights", 0, NULL, app);
    variable_item_list_add(list, "Self-Test", 0, NULL, app);

    variable_item_list_set_enter_callback(list, cfs_settings_enter_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewVariableItemList);
}

bool cfs_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void cfs_scene_settings_on_exit(void* context) {
    CfsApp* app = context;
    variable_item_list_reset(app->variable_item_list);
}
