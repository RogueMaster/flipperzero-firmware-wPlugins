/*
 * Custom size scene.
 *
 * Lets the user give a width, height and colour to a target whose type code is
 * not in the profile table, so Set Text and Set Image work for it. The IR
 * framing is identical to a table profile; only the screen geometry differs.
 * This does not guess any protocol and only applies to unknown-type targets.
 */

#include "../tagtinker_app.h"
#include "../protocol/tagtinker_proto.h"

enum {
    CustomSizeWidth,
    CustomSizeHeight,
    CustomSizeColor,
    CustomSizeSave,
};

/* index 0 -> MIN, then MIN + i*STEP, up to MAX. */
static uint32_t size_value_count(void) {
    return (TAGTINKER_CUSTOM_SIZE_MAX - TAGTINKER_CUSTOM_SIZE_MIN) / TAGTINKER_CUSTOM_SIZE_STEP +
           1U;
}

static uint16_t size_index_to_px(uint8_t index) {
    return (uint16_t)(TAGTINKER_CUSTOM_SIZE_MIN + (uint32_t)index * TAGTINKER_CUSTOM_SIZE_STEP);
}

static uint8_t size_px_to_index(uint16_t px) {
    if(px < TAGTINKER_CUSTOM_SIZE_MIN) return 0U;
    uint16_t clamped = px > TAGTINKER_CUSTOM_SIZE_MAX ? TAGTINKER_CUSTOM_SIZE_MAX : px;
    return (uint8_t)((clamped - TAGTINKER_CUSTOM_SIZE_MIN) / TAGTINKER_CUSTOM_SIZE_STEP);
}

static void width_changed(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    app->custom_size_w = size_index_to_px(variable_item_get_current_value_index(item));
    char buf[8];
    snprintf(buf, sizeof(buf), "%u", app->custom_size_w);
    variable_item_set_current_value_text(item, buf);
}

static void height_changed(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    app->custom_size_h = size_index_to_px(variable_item_get_current_value_index(item));
    char buf[8];
    snprintf(buf, sizeof(buf), "%u", app->custom_size_h);
    variable_item_set_current_value_text(item, buf);
}

static void color_changed(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    app->custom_size_color = (uint8_t)variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(
        item, tagtinker_profile_color_label((TagTinkerTagColor)app->custom_size_color));
}

static void custom_size_cb(void* ctx, uint32_t index) {
    TagTinkerApp* app = ctx;
    if(index != CustomSizeSave) return;
    if(app->selected_target < 0 || app->selected_target >= app->target_count) return;

    TagTinkerTarget* target = &app->targets[app->selected_target];
    target->custom_width = app->custom_size_w;
    target->custom_height = app->custom_size_h;
    target->custom_color = app->custom_size_color;
    tagtinker_target_refresh_profile(target);
    tagtinker_targets_save(app);

    /* Keep the app's working size in step with the target we just sized. */
    app->esl_width = target->profile.width;
    app->esl_height = target->profile.height;

    scene_manager_previous_scene(app->scene_manager);
}

void tagtinker_scene_custom_size_on_enter(void* ctx) {
    TagTinkerApp* app = ctx;

    uint16_t w = TAGTINKER_CUSTOM_SIZE_MIN;
    uint16_t h = TAGTINKER_CUSTOM_SIZE_MIN;
    uint8_t color = 0U;
    if(app->selected_target >= 0 && app->selected_target < app->target_count) {
        const TagTinkerTarget* t = &app->targets[app->selected_target];
        if(t->custom_width > 0) w = t->custom_width;
        if(t->custom_height > 0) h = t->custom_height;
        color = t->custom_color;
    }
    app->custom_size_w = w;
    app->custom_size_h = h;
    app->custom_size_color = color;

    variable_item_list_reset(app->var_item_list);

    char buf[8];
    VariableItem* item_w = variable_item_list_add(
        app->var_item_list, "Width", (uint8_t)size_value_count(), width_changed, app);
    variable_item_set_current_value_index(item_w, size_px_to_index(w));
    snprintf(buf, sizeof(buf), "%u", w);
    variable_item_set_current_value_text(item_w, buf);

    VariableItem* item_h = variable_item_list_add(
        app->var_item_list, "Height", (uint8_t)size_value_count(), height_changed, app);
    variable_item_set_current_value_index(item_h, size_px_to_index(h));
    snprintf(buf, sizeof(buf), "%u", h);
    variable_item_set_current_value_text(item_h, buf);

    VariableItem* item_c =
        variable_item_list_add(app->var_item_list, "Color", 3, color_changed, app);
    variable_item_set_current_value_index(item_c, color);
    variable_item_set_current_value_text(
        item_c, tagtinker_profile_color_label((TagTinkerTagColor)color));

    variable_item_list_add(app->var_item_list, ">> Save <<", 0, NULL, app);
    variable_item_list_set_enter_callback(app->var_item_list, custom_size_cb, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewVarItemList);
}

bool tagtinker_scene_custom_size_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;
}

void tagtinker_scene_custom_size_on_exit(void* ctx) {
    TagTinkerApp* app = ctx;
    variable_item_list_reset(app->var_item_list);
}
