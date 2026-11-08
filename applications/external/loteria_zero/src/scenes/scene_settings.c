#include "scenes.h"

typedef enum {
    SettingsAutoMode,
    SettingsSkipWelcome,
} SettingsIndex;

static void loteria_settings_auto_change(VariableItem* item) {
    LoteriaApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->persist.auto_mode = index;
    variable_item_set_current_value_text(item, loteria_auto_mode_label(index));
    loteria_app_save(app);
}

static void loteria_settings_welcome_change(VariableItem* item) {
    LoteriaApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->persist.skip_welcome = (index == 1);
    variable_item_set_current_value_text(item, index == 1 ? "Yes" : "No");
    loteria_app_save(app);
}

void loteria_scene_settings_on_enter(void* context) {
    LoteriaApp* app = context;
    VariableItemList* list = app->variable_item_list;
    variable_item_list_reset(list);

    VariableItem* item = variable_item_list_add(
        list, "Auto mode", LoteriaAutoCount, loteria_settings_auto_change, app);
    variable_item_set_current_value_index(item, app->persist.auto_mode);
    variable_item_set_current_value_text(item, loteria_auto_mode_label(app->persist.auto_mode));

    item = variable_item_list_add(list, "Skip welcome", 2, loteria_settings_welcome_change, app);
    uint8_t skip = app->persist.skip_welcome ? 1 : 0;
    variable_item_set_current_value_index(item, skip);
    variable_item_set_current_value_text(item, skip ? "Yes" : "No");

    view_dispatcher_switch_to_view(app->view_dispatcher, LoteriaViewVariableItemList);
}

bool loteria_scene_settings_on_event(void* context, SceneManagerEvent event) {
    LoteriaApp* app = context;
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    UNUSED(SettingsAutoMode);
    UNUSED(SettingsSkipWelcome);
    return false;
}

void loteria_scene_settings_on_exit(void* context) {
    LoteriaApp* app = context;
    variable_item_list_reset(app->variable_item_list);
}
