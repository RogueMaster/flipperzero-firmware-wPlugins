#include "../hotspot_arcade_i.h"
#include "../helpers/ha_storage.h"

static const char* const on_off[] = {"OFF", "ON"};

// Content languages the host can stream. "" = English (the packs/<game>/ root); the
// others stream packs/<game>/<code>/ with a per-game fallback to English. ASCII labels
// only (the Flipper's 1-bit font). Adding a language is one row here plus its packs.
static const struct {
    const char* code;
    const char* label;
} ha_langs[] = {
    {"", "English"},
    {"pt-br", "Portugues BR"},
};
#define HA_LANG_COUNT (sizeof(ha_langs) / sizeof(ha_langs[0]))

static void ha_settings_lang_cb(VariableItem* item) {
    HotspotArcadeApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    strlcpy(app->lang, ha_langs[i].code, sizeof(app->lang));
    variable_item_set_current_value_text(item, ha_langs[i].label);
    ha_storage_save_config(app);
}

static void ha_settings_sound_cb(VariableItem* item) {
    HotspotArcadeApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->sound_on = (i != 0);
    variable_item_set_current_value_text(item, on_off[i]);
    ha_storage_save_config(app);
}

static void ha_settings_vibro_cb(VariableItem* item) {
    HotspotArcadeApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->vibro_on = (i != 0);
    variable_item_set_current_value_text(item, on_off[i]);
    ha_storage_save_config(app);
}

void hotspot_arcade_scene_settings_on_enter(void* context) {
    HotspotArcadeApp* app = context;
    variable_item_list_reset(app->var_item_list);
    VariableItem* item;

    item = variable_item_list_add(app->var_item_list, "Sound", 2, ha_settings_sound_cb, app);
    variable_item_set_current_value_index(item, app->sound_on ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[app->sound_on ? 1 : 0]);

    item = variable_item_list_add(app->var_item_list, "Vibration", 2, ha_settings_vibro_cb, app);
    variable_item_set_current_value_index(item, app->vibro_on ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[app->vibro_on ? 1 : 0]);

    uint8_t li = 0;
    for(uint8_t i = 0; i < HA_LANG_COUNT; i++)
        if(strcmp(app->lang, ha_langs[i].code) == 0) {
            li = i;
            break;
        }
    item = variable_item_list_add(
        app->var_item_list, "Language", HA_LANG_COUNT, ha_settings_lang_cb, app);
    variable_item_set_current_value_index(item, li);
    variable_item_set_current_value_text(item, ha_langs[li].label);

    view_dispatcher_switch_to_view(app->view_dispatcher, HaViewVarItemList);
}

bool hotspot_arcade_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void hotspot_arcade_scene_settings_on_exit(void* context) {
    HotspotArcadeApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
