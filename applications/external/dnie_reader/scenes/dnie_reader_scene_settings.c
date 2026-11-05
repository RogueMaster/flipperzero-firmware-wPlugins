#include "../dnie_reader.h"
#include "../dnie_sounds.h"
#include "dnie_reader_scene.h"

// Ajustes: volumen del himno (se oye una fanfarria de prueba al cambiarlo)

static const char* const VOLUME_TEXT[] = {"Off", "25%", "50%", "75%", "100%"};

static void volume_changed(VariableItem* item) {
    uint8_t i = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, VOLUME_TEXT[i]);
    dnie_sound_volume_set(i);
    dnie_sound_preview();
}

void dnie_scene_settings_on_enter(void* context) {
    DnieApp* app = context;
    VariableItemList* list = app->settings;
    variable_item_list_reset(list);
    VariableItem* item = variable_item_list_add(list, "Anthem volume", 5, volume_changed, app);
    uint8_t v = dnie_sound_volume();
    variable_item_set_current_value_index(item, v);
    variable_item_set_current_value_text(item, VOLUME_TEXT[v]);
    view_dispatcher_switch_to_view(app->view_dispatcher, DnieViewSettings);
}

bool dnie_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void dnie_scene_settings_on_exit(void* context) {
    DnieApp* app = context;
    dnie_sound_stop();
    variable_item_list_reset(app->settings);
}
