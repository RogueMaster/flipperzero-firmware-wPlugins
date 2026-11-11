#include "../hamcram.h"

static const char* const settings_figure_text[] = {"Show", "Skip"};
static const char* const settings_on_off_text[] = {"Off", "On"};

static void settings_figures_changed(VariableItem* item) {
    HamCram* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item) ? 1 : 0;
    app->settings.skip_figures = index != 0;
    variable_item_set_current_value_text(item, settings_figure_text[index]);
}

static void settings_shuffle_changed(VariableItem* item) {
    HamCram* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item) ? 1 : 0;
    app->settings.shuffle = index != 0;
    variable_item_set_current_value_text(item, settings_on_off_text[index]);
}

static void settings_feedback_changed(VariableItem* item) {
    HamCram* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item) ? 1 : 0;
    app->settings.feedback = index != 0;
    variable_item_set_current_value_text(item, settings_on_off_text[index]);
}

void hamcram_scene_settings_on_enter(void* context) {
    HamCram* app = context;
    VariableItemList* list = app->variable_item_list;
    VariableItem* item;
    uint8_t index;

    /* Questions that need a schematic or chart the screen cannot show */
    index = app->settings.skip_figures ? 1 : 0;
    item = variable_item_list_add(list, "Figure Qs", 2, settings_figures_changed, app);
    variable_item_set_current_value_index(item, index);
    variable_item_set_current_value_text(item, settings_figure_text[index]);

    index = app->settings.shuffle ? 1 : 0;
    item = variable_item_list_add(list, "Shuffle answers", 2, settings_shuffle_changed, app);
    variable_item_set_current_value_index(item, index);
    variable_item_set_current_value_text(item, settings_on_off_text[index]);

    /* Green or red LED blink, plus a buzz on a wrong answer */
    index = app->settings.feedback ? 1 : 0;
    item = variable_item_list_add(list, "LED & vibro", 2, settings_feedback_changed, app);
    variable_item_set_current_value_index(item, index);
    variable_item_set_current_value_text(item, settings_on_off_text[index]);

    view_dispatcher_switch_to_view(app->view_dispatcher, HamCramViewVariableItemList);
}

bool hamcram_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void hamcram_scene_settings_on_exit(void* context) {
    HamCram* app = context;
    hamcram_save_settings(app);
    variable_item_list_reset(app->variable_item_list);
}
