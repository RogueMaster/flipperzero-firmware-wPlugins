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

// Order matches OpMode in fsd_types.h: ListenOnly=0, Active=1, Service=2.
static const char* const op_mode_text[] = {"Listen", "Active", "Service"};
static void op_mode_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, op_mode_text[idx]);
    app->op_mode = (OpMode)idx;
}

static void shield_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_text[idx]);
    app->gtw_shield = (idx == 1);
}

static void ap_first_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_text[idx]);
    app->ap_first = (idx == 1);
}

static void ap_first_edge_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_text[idx]);
    app->ap_first_edge = (idx == 1);
}

static void ap_first_minimal_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_text[idx]);
    app->ap_first_minimal = (idx == 1);
}

static void nag_faithful_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_text[idx]);
    app->nag_epas_faithful = (idx == 1);
}

static void soft_engage_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_text[idx]);
    app->soft_engage = (idx == 1);
}

static void nag_burst_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_text[idx]);
    app->nag_burst = (idx == 1);
}

static void warning_14x_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_text[idx]);
    app->firmware_14x_warning = (idx == 1);
}

static void tlssc_restore_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_text[idx]);
    app->tlssc_restore = (idx == 1);
}

static void tier_override_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_text[idx]);
    app->gtw_tier_override = (idx == 1);
}

static void scroll_press_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_text[idx]);
    app->scroll_press_ap = (idx == 1);
}

static void nav_enable_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_text[idx]);
    app->assist_nav_enable = (idx == 1);
}

static void hands_off_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_text[idx]);
    app->assist_hands_off = (idx == 1);
}

static void dev_mode_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_text[idx]);
    app->assist_dev_mode = (idx == 1);
}

static void lhd_override_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_text[idx]);
    app->assist_lhd_override = (idx == 1);
}

static void lane_graph_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_text[idx]);
    app->assist_show_lane_graph = (idx == 1);
}

static void tlssc_bit38_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_text[idx]);
    app->assist_tlssc_bit38 = (idx == 1);
}

static void telemetry_off_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_text[idx]);
    app->assist_telemetry_off = (idx == 1);
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

static void can_capture_changed(VariableItem* item) {
    TeslaFSDApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_text[idx]);
    app->can_capture = (idx == 1);
}

// Helper macro to reduce boilerplate
#define ADD_TOGGLE(label, callback, field)                           \
    item = variable_item_list_add(list, label, 2, callback, app);    \
    variable_item_set_current_value_index(item, app->field ? 1 : 0); \
    variable_item_set_current_value_text(item, toggle_text[app->field ? 1 : 0]);

void tesla_fsd_scene_settings_on_enter(void* context) {
    TeslaFSDApp* app = context;
    VariableItemList* list = app->var_item_list;
    variable_item_list_reset(list);

    VariableItem* item;

    // ── Operation mode ──
    item = variable_item_list_add(list, "Mode", 3, op_mode_changed, app);
    variable_item_set_current_value_index(item, (uint8_t)app->op_mode);
    variable_item_set_current_value_text(item, op_mode_text[(uint8_t)app->op_mode]);

    // ── Stable features (car-tested) ──
    ADD_TOGGLE("Nag Killer", nag_killer_changed, nag_killer)
    ADD_TOGGLE("Force FSD", force_fsd_changed, force_fsd)
    ADD_TOGGLE("TLSSC Restore", tlssc_restore_changed, tlssc_restore)
    ADD_TOGGLE("AP-First (14.x)", ap_first_changed, ap_first)
    ADD_TOGGLE("Instant Engage (exp.)", ap_first_edge_changed, ap_first_edge)
    ADD_TOGGLE("Minimal Inject (exp.)", ap_first_minimal_changed, ap_first_minimal)
    ADD_TOGGLE("Soft Engage", soft_engage_changed, soft_engage)
    ADD_TOGGLE("Nag Burst (14.x)", nag_burst_changed, nag_burst)
    ADD_TOGGLE("On 14.x?", warning_14x_changed, firmware_14x_warning)
    ADD_TOGGLE("GTW Cfg Replay", shield_changed, gtw_shield)
    ADD_TOGGLE("Suppress Chime", chime_changed, suppress_speed_chime)
    ADD_TOGGLE("Emerg. Vehicle", emerg_changed, emergency_vehicle_detect)
    ADD_TOGGLE("Precondition", precondition_changed, precondition)
    ADD_TOGGLE("CAN Capture", can_capture_changed, can_capture)

    // ── Beta features (report results in GitHub issues) ──
    variable_item_list_add(list, "-- Beta (report!) --", 0, NULL, NULL);
    ADD_TOGGLE("Nag EPAS-faithful", nag_faithful_changed, nag_epas_faithful)
    ADD_TOGGLE("ScrollPress AP", scroll_press_changed, scroll_press_ap)
    ADD_TOGGLE("Nav FSD Route", nav_enable_changed, assist_nav_enable)
    ADD_TOGGLE("TLSSC bit38", tlssc_bit38_changed, assist_tlssc_bit38)
    ADD_TOGGLE("Lane Graph", lane_graph_changed, assist_show_lane_graph)
    ADD_TOGGLE("Tier Override", tier_override_changed, gtw_tier_override)
    ADD_TOGGLE("Dev Mode", dev_mode_changed, assist_dev_mode)
    ADD_TOGGLE("Force LHD", lhd_override_changed, assist_lhd_override)
    ADD_TOGGLE("Hands-Off", hands_off_changed, assist_hands_off)
    ADD_TOGGLE("Telemetry Off", telemetry_off_changed, assist_telemetry_off)

    // ── Hardware ──
    item = variable_item_list_add(list, "MCP Crystal", 3, clock_changed, app);
    variable_item_set_current_value_index(item, app->mcp_clock);
    variable_item_set_current_value_text(item, clock_text[app->mcp_clock]);

    view_dispatcher_switch_to_view(app->view_dispatcher, TeslaFSDViewVarItemList);
}

#undef ADD_TOGGLE

bool tesla_fsd_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void tesla_fsd_scene_settings_on_exit(void* context) {
    TeslaFSDApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
