/*
 * Extras scene — BETA toggles for CAN features beyond FSD.
 *
 * Each toggle here maps to a CAN frame handler that is wired into the
 * worker loop in fsd_running.c.  These features are marked BETA because
 * they have not yet been verified on a real vehicle — the CAN IDs and
 * bit positions come from public sources (opendbc, mikegapinski,
 * tuncasoftbildik) but Model 3/Y may remap some of them to the
 * Ethernet bus or different CAN IDs.
 *
 * All extras default to OFF and only TX when Mode = Service.
 * Adding a new extra is intentionally cheap: one bool in TeslaFSDApp,
 * one toggle in this file, one handler in fsd_handler.c, one dispatch
 * line in fsd_running.c.
 */

#include "../tesla_fsd_app.h"
#include "../scenes_config/app_scene_functions.h"

static const char* const toggle_text[] = {"OFF", "ON"};

static void hazard_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_text[idx]);
    app->extra_hazard_lights = (idx == 1);
}

static void rear_heat_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_text[idx]);
    app->extra_rear_window_heat = (idx == 1);
}

static void wipers_off_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_text[idx]);
    app->extra_auto_wipers_off = (idx == 1);
}

static void fold_mirrors_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_text[idx]);
    app->extra_fold_mirrors = (idx == 1);
}

static void rear_fog_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_text[idx]);
    app->extra_rear_fog = (idx == 1);
}

static const char* const steering_text[] = {"--", "Comfort", "Standard", "Sport"};
static void steering_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, steering_text[idx]);
    app->extra_steering_mode = idx;
}

static void strobe_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_text[idx]);
    app->extra_highbeam_strobe = (idx == 1);
}

static void turn_left_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_text[idx]);
    app->extra_turn_left = (idx == 1);
}

static void turn_right_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_text[idx]);
    app->extra_turn_right = (idx == 1);
}

void tesla_fsd_scene_extras_on_enter(void* context) {
    TeslaFSDApp* app = context;
    VariableItemList* list = app->var_item_list;
    variable_item_list_reset(list);

    VariableItem* item;

    item = variable_item_list_add(list, "Hazard Lights", 2, hazard_changed, app);
    variable_item_set_current_value_index(item, app->extra_hazard_lights ? 1 : 0);
    variable_item_set_current_value_text(item, toggle_text[app->extra_hazard_lights ? 1 : 0]);

    item = variable_item_list_add(list, "Rear Window Heat", 2, rear_heat_changed, app);
    variable_item_set_current_value_index(item, app->extra_rear_window_heat ? 1 : 0);
    variable_item_set_current_value_text(item, toggle_text[app->extra_rear_window_heat ? 1 : 0]);

    item = variable_item_list_add(list, "Auto Wipers Off", 2, wipers_off_changed, app);
    variable_item_set_current_value_index(item, app->extra_auto_wipers_off ? 1 : 0);
    variable_item_set_current_value_text(item, toggle_text[app->extra_auto_wipers_off ? 1 : 0]);

    item = variable_item_list_add(list, "Fold Mirrors", 2, fold_mirrors_changed, app);
    variable_item_set_current_value_index(item, app->extra_fold_mirrors ? 1 : 0);
    variable_item_set_current_value_text(item, toggle_text[app->extra_fold_mirrors ? 1 : 0]);

    item = variable_item_list_add(list, "Rear Fog Light", 2, rear_fog_changed, app);
    variable_item_set_current_value_index(item, app->extra_rear_fog ? 1 : 0);
    variable_item_set_current_value_text(item, toggle_text[app->extra_rear_fog ? 1 : 0]);

    // Steering tune — requires Chassis CAN tap (not OBD-II Party CAN)
    item = variable_item_list_add(list, "Steering [ChassisCAN]", 4, steering_changed, app);
    variable_item_set_current_value_index(item, app->extra_steering_mode);
    variable_item_set_current_value_text(item, steering_text[app->extra_steering_mode]);

    // High beam strobe — Party CAN (SCCM_leftStalk 0x249)
    item = variable_item_list_add(list, "High Beam Strobe", 2, strobe_changed, app);
    variable_item_set_current_value_index(item, app->extra_highbeam_strobe ? 1 : 0);
    variable_item_set_current_value_text(item, toggle_text[app->extra_highbeam_strobe ? 1 : 0]);

    // Turn signal injection — Party CAN
    item = variable_item_list_add(list, "Turn Left", 2, turn_left_changed, app);
    variable_item_set_current_value_index(item, app->extra_turn_left ? 1 : 0);
    variable_item_set_current_value_text(item, toggle_text[app->extra_turn_left ? 1 : 0]);

    item = variable_item_list_add(list, "Turn Right", 2, turn_right_changed, app);
    variable_item_set_current_value_index(item, app->extra_turn_right ? 1 : 0);
    variable_item_set_current_value_text(item, toggle_text[app->extra_turn_right ? 1 : 0]);

    view_dispatcher_switch_to_view(app->view_dispatcher, TeslaFSDViewVarItemList);
}

bool tesla_fsd_scene_extras_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void tesla_fsd_scene_extras_on_exit(void* context) {
    TeslaFSDApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
