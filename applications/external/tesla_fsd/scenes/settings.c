#include "../tesla_fsd_app.h"
#include "../scenes_config/app_scene_functions.h"

static const char* const toggle_text[] = {"OFF", "ON"};

static void force_fsd_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_text[idx]);
    app->force_fsd = (idx == 1);
}

static void chime_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_text[idx]);
    app->suppress_speed_chime = (idx == 1);
}

static void emerg_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_text[idx]);
    app->emergency_vehicle_detect = (idx == 1);
}

static void nag_killer_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_text[idx]);
    app->nag_killer = (idx == 1);
}

static const char* const op_mode_text[] = {"Active", "Listen", "Service"};
static void op_mode_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, op_mode_text[idx]);
    app->op_mode = (OpMode)idx;
}

static const char* const clock_text[] = {"16 MHz", "8 MHz", "12 MHz"};
static void clock_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, clock_text[idx]);
    app->mcp_clock = idx;
}

static void precondition_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_text[idx]);
    app->precondition = (idx == 1);
}

void tesla_fsd_scene_settings_on_enter(void* context) {
    TeslaFSDApp* app = context;
    VariableItemList* list = app->var_item_list;
    variable_item_list_reset(list);

    VariableItem* item;

    item = variable_item_list_add(list, "Force FSD", 2, force_fsd_changed, app);
    variable_item_set_current_value_index(item, app->force_fsd ? 1 : 0);
    variable_item_set_current_value_text(item, toggle_text[app->force_fsd ? 1 : 0]);

    item = variable_item_list_add(list, "Suppress Chime", 2, chime_changed, app);
    variable_item_set_current_value_index(item, app->suppress_speed_chime ? 1 : 0);
    variable_item_set_current_value_text(item, toggle_text[app->suppress_speed_chime ? 1 : 0]);

    item = variable_item_list_add(list, "Emerg. Vehicle", 2, emerg_changed, app);
    variable_item_set_current_value_index(item, app->emergency_vehicle_detect ? 1 : 0);
    variable_item_set_current_value_text(item, toggle_text[app->emergency_vehicle_detect ? 1 : 0]);

    item = variable_item_list_add(list, "Nag Killer", 2, nag_killer_changed, app);
    variable_item_set_current_value_index(item, app->nag_killer ? 1 : 0);
    variable_item_set_current_value_text(item, toggle_text[app->nag_killer ? 1 : 0]);

    item = variable_item_list_add(list, "Mode", 3, op_mode_changed, app);
    variable_item_set_current_value_index(item, (uint8_t)app->op_mode);
    variable_item_set_current_value_text(item, op_mode_text[(uint8_t)app->op_mode]);

    item = variable_item_list_add(list, "Precondition", 2, precondition_changed, app);
    variable_item_set_current_value_index(item, app->precondition ? 1 : 0);
    variable_item_set_current_value_text(item, toggle_text[app->precondition ? 1 : 0]);

    item = variable_item_list_add(list, "MCP Crystal", 3, clock_changed, app);
    variable_item_set_current_value_index(item, app->mcp_clock);
    variable_item_set_current_value_text(item, clock_text[app->mcp_clock]);

    view_dispatcher_switch_to_view(app->view_dispatcher, TeslaFSDViewVarItemList);
}

bool tesla_fsd_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void tesla_fsd_scene_settings_on_exit(void* context) {
    TeslaFSDApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
