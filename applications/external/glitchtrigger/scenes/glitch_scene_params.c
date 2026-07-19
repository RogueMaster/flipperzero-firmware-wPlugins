#include "../glitch_trigger_i.h"

/* Parameter editor. Every field the user edits rides a "nice number" ladder or
 * a small enum, so one knob covers the whole range and the value text always
 * reads in friendly units. */

static void item_delay(VariableItem* item) {
    GlitchApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->params.delay_us = glitch_ladder_delay_us[i];
    char b[16];
    glitch_fmt_us(app->params.delay_us, b, sizeof(b));
    variable_item_set_current_value_text(item, b);
}
static void item_width(VariableItem* item) {
    GlitchApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->params.width_ns = glitch_ladder_width_ns[i];
    char b[16];
    glitch_fmt_ns(app->params.width_ns, b, sizeof(b));
    variable_item_set_current_value_text(item, b);
}
static void item_pulses(VariableItem* item) {
    GlitchApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->params.pulses = glitch_ladder_pulses[i];
    char b[8];
    snprintf(b, sizeof(b), "%u", app->params.pulses);
    variable_item_set_current_value_text(item, b);
}
static void item_gap(VariableItem* item) {
    GlitchApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->params.gap_us = glitch_ladder_gap_us[i];
    char b[16];
    glitch_fmt_us(app->params.gap_us, b, sizeof(b));
    variable_item_set_current_value_text(item, b);
}
static void item_polarity(VariableItem* item) {
    GlitchApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->params.polarity = (GlitchPolarity)i;
    variable_item_set_current_value_text(item, glitch_polarity_label(app->params.polarity));
}
static void item_trigger(VariableItem* item) {
    GlitchApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->params.trigger = (GlitchTriggerMode)i;
    variable_item_set_current_value_text(item, glitch_trigger_label(app->params.trigger));
}
static void item_edge(VariableItem* item) {
    GlitchApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->params.ext_edge = (GlitchEdge)i;
    variable_item_set_current_value_text(item, glitch_edge_label(app->params.ext_edge));
}
static void item_repeat(VariableItem* item) {
    GlitchApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->params.repeat_ms = glitch_ladder_repeat_ms[i];
    char b[16];
    glitch_fmt_ms(app->params.repeat_ms, b, sizeof(b));
    variable_item_set_current_value_text(item, b);
}
static void item_sweep_from(VariableItem* item) {
    GlitchApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->params.sweep_from_ns = glitch_ladder_width_ns[i];
    char b[16];
    glitch_fmt_ns(app->params.sweep_from_ns, b, sizeof(b));
    variable_item_set_current_value_text(item, b);
}
static void item_sweep_to(VariableItem* item) {
    GlitchApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->params.sweep_to_ns = glitch_ladder_width_ns[i];
    char b[16];
    glitch_fmt_ns(app->params.sweep_to_ns, b, sizeof(b));
    variable_item_set_current_value_text(item, b);
}
static void item_sweep_step(VariableItem* item) {
    GlitchApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->params.sweep_step_ns = glitch_ladder_width_ns[i];
    char b[16];
    glitch_fmt_ns(app->params.sweep_step_ns, b, sizeof(b));
    variable_item_set_current_value_text(item, b);
}

void glitch_scene_params_on_enter(void* context) {
    GlitchApp* app = context;
    VariableItemList* list = app->var_item_list;
    variable_item_list_reset(list);
    GlitchParams* p = &app->params;
    VariableItem* item;
    char b[16];
    size_t idx;

    idx = glitch_ladder_nearest_u32(glitch_ladder_delay_us, glitch_ladder_delay_len, p->delay_us);
    item = variable_item_list_add(list, "Delay", glitch_ladder_delay_len, item_delay, app);
    variable_item_set_current_value_index(item, idx);
    glitch_fmt_us(glitch_ladder_delay_us[idx], b, sizeof(b));
    variable_item_set_current_value_text(item, b);

    idx = glitch_ladder_nearest_u32(glitch_ladder_width_ns, glitch_ladder_width_len, p->width_ns);
    item = variable_item_list_add(list, "Width", glitch_ladder_width_len, item_width, app);
    variable_item_set_current_value_index(item, idx);
    glitch_fmt_ns(glitch_ladder_width_ns[idx], b, sizeof(b));
    variable_item_set_current_value_text(item, b);

    idx = glitch_ladder_nearest_u16(glitch_ladder_pulses, glitch_ladder_pulses_len, p->pulses);
    item = variable_item_list_add(list, "Pulses", glitch_ladder_pulses_len, item_pulses, app);
    variable_item_set_current_value_index(item, idx);
    snprintf(b, sizeof(b), "%u", glitch_ladder_pulses[idx]);
    variable_item_set_current_value_text(item, b);

    idx = glitch_ladder_nearest_u32(glitch_ladder_gap_us, glitch_ladder_gap_len, p->gap_us);
    item = variable_item_list_add(list, "Gap", glitch_ladder_gap_len, item_gap, app);
    variable_item_set_current_value_index(item, idx);
    glitch_fmt_us(glitch_ladder_gap_us[idx], b, sizeof(b));
    variable_item_set_current_value_text(item, b);

    item = variable_item_list_add(list, "Polarity", GlitchPolarityCount, item_polarity, app);
    variable_item_set_current_value_index(item, p->polarity);
    variable_item_set_current_value_text(item, glitch_polarity_label(p->polarity));

    item = variable_item_list_add(list, "Trigger", GlitchTriggerCount, item_trigger, app);
    variable_item_set_current_value_index(item, p->trigger);
    variable_item_set_current_value_text(item, glitch_trigger_label(p->trigger));

    item = variable_item_list_add(list, "Ext Edge", GlitchEdgeCount, item_edge, app);
    variable_item_set_current_value_index(item, p->ext_edge);
    variable_item_set_current_value_text(item, glitch_edge_label(p->ext_edge));

    idx =
        glitch_ladder_nearest_u32(glitch_ladder_repeat_ms, glitch_ladder_repeat_len, p->repeat_ms);
    item = variable_item_list_add(list, "Repeat", glitch_ladder_repeat_len, item_repeat, app);
    variable_item_set_current_value_index(item, idx);
    glitch_fmt_ms(glitch_ladder_repeat_ms[idx], b, sizeof(b));
    variable_item_set_current_value_text(item, b);

    idx = glitch_ladder_nearest_u32(
        glitch_ladder_width_ns, glitch_ladder_width_len, p->sweep_from_ns);
    item = variable_item_list_add(list, "Sweep from", glitch_ladder_width_len, item_sweep_from, app);
    variable_item_set_current_value_index(item, idx);
    glitch_fmt_ns(glitch_ladder_width_ns[idx], b, sizeof(b));
    variable_item_set_current_value_text(item, b);

    idx =
        glitch_ladder_nearest_u32(glitch_ladder_width_ns, glitch_ladder_width_len, p->sweep_to_ns);
    item = variable_item_list_add(list, "Sweep to", glitch_ladder_width_len, item_sweep_to, app);
    variable_item_set_current_value_index(item, idx);
    glitch_fmt_ns(glitch_ladder_width_ns[idx], b, sizeof(b));
    variable_item_set_current_value_text(item, b);

    idx = glitch_ladder_nearest_u32(
        glitch_ladder_width_ns, glitch_ladder_width_len, p->sweep_step_ns);
    item = variable_item_list_add(list, "Sweep step", glitch_ladder_width_len, item_sweep_step, app);
    variable_item_set_current_value_index(item, idx);
    glitch_fmt_ns(glitch_ladder_width_ns[idx], b, sizeof(b));
    variable_item_set_current_value_text(item, b);

    view_dispatcher_switch_to_view(app->view_dispatcher, GlitchViewVarList);
}

bool glitch_scene_params_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void glitch_scene_params_on_exit(void* context) {
    GlitchApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
