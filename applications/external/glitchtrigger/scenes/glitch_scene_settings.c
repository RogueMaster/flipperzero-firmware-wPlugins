#include "../glitch_trigger_i.h"

static const char* const on_off[] = {"OFF", "ON"};
static uint32_t s_success_index; // list index of the "Success str" row

static void settings_enter_cb(void* context, uint32_t index) {
    GlitchApp* app = context;
    if(index == s_success_index) {
        scene_manager_next_scene(app->scene_manager, GlitchSceneSuccessStr);
    }
}

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
static void set_fb_pin(VariableItem* item) {
    GlitchApp* app = variable_item_get_context(item);
    uint8_t v = variable_item_get_current_value_index(item);
    app->params.fb_pin = v;
    char b[12];
    pin_label(v, b, sizeof(b));
    variable_item_set_current_value_text(item, b);
}
static const char* const level_lbl[] = {"LOW", "HIGH"};
static void set_fb_level(VariableItem* item) {
    GlitchApp* app = variable_item_get_context(item);
    uint8_t v = variable_item_get_current_value_index(item);
    app->params.fb_active_high = v;
    variable_item_set_current_value_text(item, level_lbl[v]);
}
static void set_fb_source(VariableItem* item) {
    GlitchApp* app = variable_item_get_context(item);
    uint8_t v = variable_item_get_current_value_index(item);
    app->params.fb_source = (GlitchFeedbackSource)v;
    variable_item_set_current_value_text(item, glitch_fb_source_label(app->params.fb_source));
}
static void set_uart_baud(VariableItem* item) {
    GlitchApp* app = variable_item_get_context(item);
    uint8_t v = variable_item_get_current_value_index(item);
    app->params.uart_baud = glitch_ladder_baud[v];
    char b[12];
    snprintf(b, sizeof(b), "%lu", (unsigned long)app->params.uart_baud);
    variable_item_set_current_value_text(item, b);
}
static void set_auto_hit(VariableItem* item) {
    GlitchApp* app = variable_item_get_context(item);
    uint8_t v = variable_item_get_current_value_index(item);
    app->params.auto_hit = v;
    variable_item_set_current_value_text(item, on_off[v]);
}
static void set_log_hits(VariableItem* item) {
    GlitchApp* app = variable_item_get_context(item);
    uint8_t v = variable_item_get_current_value_index(item);
    app->params.log_hits = v;
    variable_item_set_current_value_text(item, on_off[v]);
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

    item = variable_item_list_add(list, "Feedback pin", glitch_pins_len, set_fb_pin, app);
    variable_item_set_current_value_index(item, app->params.fb_pin);
    pin_label(app->params.fb_pin, b, sizeof(b));
    variable_item_set_current_value_text(item, b);

    item = variable_item_list_add(list, "Success lvl", 2, set_fb_level, app);
    variable_item_set_current_value_index(item, app->params.fb_active_high);
    variable_item_set_current_value_text(item, level_lbl[app->params.fb_active_high]);

    item = variable_item_list_add(list, "FB source", GlitchFbCount, set_fb_source, app);
    variable_item_set_current_value_index(item, app->params.fb_source);
    variable_item_set_current_value_text(item, glitch_fb_source_label(app->params.fb_source));

    size_t bidx = glitch_ladder_nearest_u32(
        glitch_ladder_baud, glitch_ladder_baud_len, app->params.uart_baud);
    item = variable_item_list_add(list, "UART baud", glitch_ladder_baud_len, set_uart_baud, app);
    variable_item_set_current_value_index(item, bidx);
    snprintf(b, sizeof(b), "%lu", (unsigned long)glitch_ladder_baud[bidx]);
    variable_item_set_current_value_text(item, b);

    /* Success str: press OK to edit (enter callback -> text-input scene).
     * Index 6 = the six rows above: Glitch pin, Trig-in, Feedback pin,
     * Success lvl, FB source, UART baud. */
    s_success_index = 6;
    item = variable_item_list_add(list, "Success str", 1, NULL, app);
    variable_item_set_current_value_text(
        item, app->params.success_str[0] ? app->params.success_str : "(set)");

    item = variable_item_list_add(list, "Auto-hit", 2, set_auto_hit, app);
    variable_item_set_current_value_index(item, app->params.auto_hit);
    variable_item_set_current_value_text(item, on_off[app->params.auto_hit]);

    item = variable_item_list_add(list, "Log hits", 2, set_log_hits, app);
    variable_item_set_current_value_index(item, app->params.log_hits);
    variable_item_set_current_value_text(item, on_off[app->params.log_hits]);

    item = variable_item_list_add(list, "Sound", 2, set_sound, app);
    variable_item_set_current_value_index(item, app->sound);
    variable_item_set_current_value_text(item, on_off[app->sound]);

    item = variable_item_list_add(list, "Vibro", 2, set_vibro, app);
    variable_item_set_current_value_index(item, app->vibro);
    variable_item_set_current_value_text(item, on_off[app->vibro]);

    item = variable_item_list_add(list, "LED", 2, set_led, app);
    variable_item_set_current_value_index(item, app->led);
    variable_item_set_current_value_text(item, on_off[app->led]);

    variable_item_list_set_enter_callback(list, settings_enter_cb, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, GlitchViewVarList);
}

bool glitch_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void glitch_scene_settings_on_exit(void* context) {
    GlitchApp* app = context;
    /* the enter callback lives on the shared module - clear it so it can't fire
     * on the Params scene, which reuses the same variable_item_list */
    variable_item_list_set_enter_callback(app->var_item_list, NULL, app);
    variable_item_list_reset(app->var_item_list);
}
