#include "../glitch_trigger_i.h"

static const char* const on_off[] = {"OFF", "ON"};

static void pin_label(uint8_t idx, char* buf, size_t n) {
    const GlitchPinInfo* p = &glitch_pins[idx % glitch_pins_len];
    snprintf(buf, n, "%s/%u", p->name, p->header);
}

static void set_out_pin(VariableItem* item) {
    GlitchApp* app = variable_item_get_context(item);
    uint8_t v = variable_item_get_current_value_index(item);
    app->params.out_pin = v;
    char b[12];
    pin_label(v, b, sizeof(b));
    variable_item_set_current_value_text(item, b);
}
static void set_in_pin(VariableItem* item) {
    GlitchApp* app = variable_item_get_context(item);
    uint8_t v = variable_item_get_current_value_index(item);
    app->params.in_pin = v;
    char b[12];
    pin_label(v, b, sizeof(b));
    variable_item_set_current_value_text(item, b);
}
static void set_sound(VariableItem* item) {
    GlitchApp* app = variable_item_get_context(item);
    uint8_t v = variable_item_get_current_value_index(item);
    app->sound = v;
    variable_item_set_current_value_text(item, on_off[v]);
}
static void set_vibro(VariableItem* item) {
    GlitchApp* app = variable_item_get_context(item);
    uint8_t v = variable_item_get_current_value_index(item);
    app->vibro = v;
    variable_item_set_current_value_text(item, on_off[v]);
}
static void set_led(VariableItem* item) {
    GlitchApp* app = variable_item_get_context(item);
    uint8_t v = variable_item_get_current_value_index(item);
    app->led = v;
    variable_item_set_current_value_text(item, on_off[v]);
}

void glitch_scene_settings_on_enter(void* context) {
    GlitchApp* app = context;
    VariableItemList* list = app->var_item_list;
    variable_item_list_reset(list);
    VariableItem* item;
    char b[12];

    item = variable_item_list_add(list, "Glitch pin", glitch_pins_len, set_out_pin, app);
    variable_item_set_current_value_index(item, app->params.out_pin);
    pin_label(app->params.out_pin, b, sizeof(b));
    variable_item_set_current_value_text(item, b);

    item = variable_item_list_add(list, "Trig-in pin", glitch_pins_len, set_in_pin, app);
    variable_item_set_current_value_index(item, app->params.in_pin);
    pin_label(app->params.in_pin, b, sizeof(b));
    variable_item_set_current_value_text(item, b);

    item = variable_item_list_add(list, "Sound", 2, set_sound, app);
    variable_item_set_current_value_index(item, app->sound);
    variable_item_set_current_value_text(item, on_off[app->sound]);

    item = variable_item_list_add(list, "Vibro", 2, set_vibro, app);
    variable_item_set_current_value_index(item, app->vibro);
    variable_item_set_current_value_text(item, on_off[app->vibro]);

    item = variable_item_list_add(list, "LED", 2, set_led, app);
    variable_item_set_current_value_index(item, app->led);
    variable_item_set_current_value_text(item, on_off[app->led]);

    view_dispatcher_switch_to_view(app->view_dispatcher, GlitchViewVarList);
}

bool glitch_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void glitch_scene_settings_on_exit(void* context) {
    GlitchApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
