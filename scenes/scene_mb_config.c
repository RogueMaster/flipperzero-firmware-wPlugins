#include "../disney_toolbox_app.h"

static uint32_t mb_broadcast_item_index;

static const char* const mb_bool_names[] = {"Off", "On"};
static const char* const mb_scaler_names[] = {"0", "1"};
static const char* const mb_fade_out_names[] = {"None", "1s", "2s", "3s"};

static char mb_time_value_str[4];
static char mb_rgb_red_str[4];
static char mb_rgb_green_str[4];
static char mb_rgb_blue_str[4];

static void mb_config_color_changed(VariableItem* item) {
    DisneyToolboxApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->code_params.color = (MagicBandColor)index;
    variable_item_set_current_value_text(item, magicband_color_names[index]);
}

static void mb_config_color_inner_changed(VariableItem* item) {
    DisneyToolboxApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->code_params.color_inner = (MagicBandColor)index;
    variable_item_set_current_value_text(item, magicband_color_names[index]);
}

static void mb_config_color_outer_changed(VariableItem* item) {
    DisneyToolboxApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->code_params.color_outer = (MagicBandColor)index;
    variable_item_set_current_value_text(item, magicband_color_names[index]);
}

static void mb_config_color_center_changed(VariableItem* item) {
    DisneyToolboxApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->code_params.color_center = (MagicBandColor)index;
    variable_item_set_current_value_text(item, magicband_color_names[index]);
}

static void mb_config_color_top_right_changed(VariableItem* item) {
    DisneyToolboxApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->code_params.color_top_right = (MagicBandColor)index;
    variable_item_set_current_value_text(item, magicband_color_names[index]);
}

static void mb_config_color_bottom_right_changed(VariableItem* item) {
    DisneyToolboxApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->code_params.color_bottom_right = (MagicBandColor)index;
    variable_item_set_current_value_text(item, magicband_color_names[index]);
}

static void mb_config_color_bottom_left_changed(VariableItem* item) {
    DisneyToolboxApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->code_params.color_bottom_left = (MagicBandColor)index;
    variable_item_set_current_value_text(item, magicband_color_names[index]);
}

static void mb_config_color_top_left_changed(VariableItem* item) {
    DisneyToolboxApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->code_params.color_top_left = (MagicBandColor)index;
    variable_item_set_current_value_text(item, magicband_color_names[index]);
}

static void mb_config_mask_changed(VariableItem* item) {
    DisneyToolboxApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->code_params.mask = (MagicBandMask)index;
    variable_item_set_current_value_text(item, magicband_mask_names[index]);
}

static void mb_config_vibration_changed(VariableItem* item) {
    DisneyToolboxApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->code_params.vibration = (MagicBandVibration)index;
    variable_item_set_current_value_text(item, magicband_vibration_names[index]);
}

static void mb_config_always_on_changed(VariableItem* item) {
    DisneyToolboxApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->code_params.always_on = (index == 1);
    variable_item_set_current_value_text(item, mb_bool_names[index]);
}

static void mb_config_timing_scaler_changed(VariableItem* item) {
    DisneyToolboxApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->code_params.timing_scaler = index;
    variable_item_set_current_value_text(item, mb_scaler_names[index]);
}

static void mb_config_fade_out_changed(VariableItem* item) {
    DisneyToolboxApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->code_params.fade_out = index;
    variable_item_set_current_value_text(item, mb_fade_out_names[index]);
}

static void mb_config_time_value_changed(VariableItem* item) {
    DisneyToolboxApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->code_params.time_value = index;
    snprintf(mb_time_value_str, sizeof(mb_time_value_str), "%u", index);
    variable_item_set_current_value_text(item, mb_time_value_str);
}

static void mb_config_rgb_red_changed(VariableItem* item) {
    DisneyToolboxApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->code_params.rgb_red = index;
    snprintf(mb_rgb_red_str, sizeof(mb_rgb_red_str), "%u", index);
    variable_item_set_current_value_text(item, mb_rgb_red_str);
}

static void mb_config_rgb_green_changed(VariableItem* item) {
    DisneyToolboxApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->code_params.rgb_green = index;
    snprintf(mb_rgb_green_str, sizeof(mb_rgb_green_str), "%u", index);
    variable_item_set_current_value_text(item, mb_rgb_green_str);
}

static void mb_config_rgb_blue_changed(VariableItem* item) {
    DisneyToolboxApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->code_params.rgb_blue = index;
    snprintf(mb_rgb_blue_str, sizeof(mb_rgb_blue_str), "%u", index);
    variable_item_set_current_value_text(item, mb_rgb_blue_str);
}

static void mb_config_enter_callback(void* context, uint32_t index) {
    DisneyToolboxApp* app = context;
    UNUSED(index);
    scene_manager_set_scene_state(app->scene_manager, DisneyToolboxAppSceneMbConfig, index);
    scene_manager_next_scene(app->scene_manager, DisneyToolboxAppSceneMbBroadcast);
}

void disney_toolbox_app_scene_mb_config_on_enter(void* context) {
    DisneyToolboxApp* app = context;
    VariableItemList* list = app->var_item_list;
    VariableItem* item;
    uint32_t item_count = 0;

    if (app->selected_code_type == MagicBandCodeTypeE908) {
        item = variable_item_list_add(list, "Red", 64, mb_config_rgb_red_changed, app);
        variable_item_set_current_value_index(item, app->code_params.rgb_red);
        snprintf(mb_rgb_red_str, sizeof(mb_rgb_red_str), "%u", app->code_params.rgb_red);
        variable_item_set_current_value_text(item, mb_rgb_red_str);
        item_count++;

        item = variable_item_list_add(list, "Green", 64, mb_config_rgb_green_changed, app);
        variable_item_set_current_value_index(item, app->code_params.rgb_green);
        snprintf(mb_rgb_green_str, sizeof(mb_rgb_green_str), "%u", app->code_params.rgb_green);
        variable_item_set_current_value_text(item, mb_rgb_green_str);
        item_count++;

        item = variable_item_list_add(list, "Blue", 64, mb_config_rgb_blue_changed, app);
        variable_item_set_current_value_index(item, app->code_params.rgb_blue);
        snprintf(mb_rgb_blue_str, sizeof(mb_rgb_blue_str), "%u", app->code_params.rgb_blue);
        variable_item_set_current_value_text(item, mb_rgb_blue_str);
        item_count++;
    } else if (app->selected_code_type == MagicBandCodeTypeE909) {
        item = variable_item_list_add(list, "Center", MagicBandColorCount,
                                      mb_config_color_center_changed, app);
        variable_item_set_current_value_index(item, app->code_params.color_center);
        variable_item_set_current_value_text(item,
                                             magicband_color_names[app->code_params.color_center]);
        item_count++;

        item = variable_item_list_add(list, "Top Right", MagicBandColorCount,
                                      mb_config_color_top_right_changed, app);
        variable_item_set_current_value_index(item, app->code_params.color_top_right);
        variable_item_set_current_value_text(
            item, magicband_color_names[app->code_params.color_top_right]);
        item_count++;

        item = variable_item_list_add(list, "Bottom Right", MagicBandColorCount,
                                      mb_config_color_bottom_right_changed, app);
        variable_item_set_current_value_index(item, app->code_params.color_bottom_right);
        variable_item_set_current_value_text(
            item, magicband_color_names[app->code_params.color_bottom_right]);
        item_count++;

        item = variable_item_list_add(list, "Bottom Left", MagicBandColorCount,
                                      mb_config_color_bottom_left_changed, app);
        variable_item_set_current_value_index(item, app->code_params.color_bottom_left);
        variable_item_set_current_value_text(
            item, magicband_color_names[app->code_params.color_bottom_left]);
        item_count++;

        item = variable_item_list_add(list, "Top Left", MagicBandColorCount,
                                      mb_config_color_top_left_changed, app);
        variable_item_set_current_value_index(item, app->code_params.color_top_left);
        variable_item_set_current_value_text(
            item, magicband_color_names[app->code_params.color_top_left]);
        item_count++;
    } else if (app->selected_code_type == MagicBandCodeTypeE906) {
        item = variable_item_list_add(list, "Inner Color", MagicBandColorCount,
                                      mb_config_color_inner_changed, app);
        variable_item_set_current_value_index(item, app->code_params.color_inner);
        variable_item_set_current_value_text(item,
                                             magicband_color_names[app->code_params.color_inner]);
        item_count++;

        item = variable_item_list_add(list, "Outer Color", MagicBandColorCount,
                                      mb_config_color_outer_changed, app);
        variable_item_set_current_value_index(item, app->code_params.color_outer);
        variable_item_set_current_value_text(item,
                                             magicband_color_names[app->code_params.color_outer]);
        item_count++;
    } else {
        item = variable_item_list_add(list, "Color", MagicBandColorCount, mb_config_color_changed,
                                      app);
        variable_item_set_current_value_index(item, app->code_params.color);
        variable_item_set_current_value_text(item, magicband_color_names[app->code_params.color]);
        item_count++;

        item = variable_item_list_add(list, "Light Mask", MagicBandMaskCount,
                                      mb_config_mask_changed, app);
        variable_item_set_current_value_index(item, app->code_params.mask);
        variable_item_set_current_value_text(item, magicband_mask_names[app->code_params.mask]);
        item_count++;
    }

    item = variable_item_list_add(list, "Vibration", MagicBandVibCount, mb_config_vibration_changed,
                                  app);
    variable_item_set_current_value_index(item, app->code_params.vibration);
    variable_item_set_current_value_text(item,
                                         magicband_vibration_names[app->code_params.vibration]);
    item_count++;

    item = variable_item_list_add(list, "Always On", 2, mb_config_always_on_changed, app);
    variable_item_set_current_value_index(item, app->code_params.always_on ? 1 : 0);
    variable_item_set_current_value_text(item, mb_bool_names[app->code_params.always_on ? 1 : 0]);
    item_count++;

    item = variable_item_list_add(list, "Time Scaler", 2, mb_config_timing_scaler_changed, app);
    variable_item_set_current_value_index(item, app->code_params.timing_scaler);
    variable_item_set_current_value_text(item, mb_scaler_names[app->code_params.timing_scaler]);
    item_count++;

    item = variable_item_list_add(list, "Time Value", 16, mb_config_time_value_changed, app);
    variable_item_set_current_value_index(item, app->code_params.time_value);
    snprintf(mb_time_value_str, sizeof(mb_time_value_str), "%u", app->code_params.time_value);
    variable_item_set_current_value_text(item, mb_time_value_str);
    item_count++;

    item = variable_item_list_add(list, "Fade Out", 4, mb_config_fade_out_changed, app);
    variable_item_set_current_value_index(item, app->code_params.fade_out);
    variable_item_set_current_value_text(item, mb_fade_out_names[app->code_params.fade_out]);
    item_count++;

    variable_item_list_add(list, "Start Broadcast", 0, NULL, app);
    mb_broadcast_item_index = item_count;

    uint32_t selected =
        scene_manager_get_scene_state(app->scene_manager, DisneyToolboxAppSceneMbConfig);
    variable_item_list_set_selected_item(list, selected);

    variable_item_list_set_enter_callback(list, mb_config_enter_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, DisneyToolboxAppViewConfig);
}

bool disney_toolbox_app_scene_mb_config_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void disney_toolbox_app_scene_mb_config_on_exit(void* context) {
    DisneyToolboxApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
