#include "../pocketlab_i.h"

typedef enum {
    SettingsIndexSound,
    SettingsIndexReset,
} SettingsIndex;

static void pocketlab_scene_settings_sound_changed(VariableItem* item) {
    PocketLab* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);

    app->state.sound = index;
    variable_item_set_current_value_text(
        item, pocketlab_text(index ? PocketLabTextOn : PocketLabTextOff));

    if(index) {
        pocketlab_sound_play(app->notifications, true, PocketLabSoundClick);
    }
}

static void pocketlab_scene_settings_enter_callback(void* context, uint32_t index) {
    PocketLab* app = context;
    if(index == SettingsIndexReset) {
        scene_manager_next_scene(app->scene_manager, PocketLabSceneResetConfirm);
    }
}

void pocketlab_scene_settings_on_enter(void* context) {
    PocketLab* app = context;
    VariableItemList* list = app->variable_item_list;

    variable_item_list_reset(list);

    VariableItem* sound_item = variable_item_list_add(
        list,
        pocketlab_text(PocketLabTextSettingsSound),
        2,
        pocketlab_scene_settings_sound_changed,
        app);
    variable_item_set_current_value_index(sound_item, app->state.sound ? 1 : 0);
    variable_item_set_current_value_text(
        sound_item, pocketlab_text(app->state.sound ? PocketLabTextOn : PocketLabTextOff));

    VariableItem* reset_item =
        variable_item_list_add(list, pocketlab_text(PocketLabTextSettingsReset), 1, NULL, app);
    variable_item_set_current_value_text(reset_item, "");

    variable_item_list_set_enter_callback(list, pocketlab_scene_settings_enter_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, PocketLabViewSettings);
}

bool pocketlab_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void pocketlab_scene_settings_on_exit(void* context) {
    PocketLab* app = context;
    variable_item_list_reset(app->variable_item_list);
    pocketlab_storage_save(&app->state);
}
