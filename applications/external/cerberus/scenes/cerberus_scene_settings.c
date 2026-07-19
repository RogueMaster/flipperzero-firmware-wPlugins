#include "../cerberus_i.h"

static const char* const scan_mode_text[] = {"Hop 3-band", "Pin 433", "Pin 868", "Pin 915"};
static const char* const sens_text[] = {"Low", "Medium", "High"};
static const char* const onoff_text[] = {"OFF", "ON"};

static void cerberus_settings_changed(VariableItem* item) {
    CerberusSettingCtx* ctx = variable_item_get_context(item);
    Cerberus* app = ctx->app;
    uint8_t idx = variable_item_get_current_value_index(item);

    switch(ctx->field) {
    case CerberusSettingScanMode:
        app->config.scan_mode = idx;
        variable_item_set_current_value_text(item, scan_mode_text[idx]);
        break;
    case CerberusSettingSensitivity:
        app->config.sensitivity = idx;
        variable_item_set_current_value_text(item, sens_text[idx]);
        break;
    case CerberusSettingDetectJam:
        app->config.detect_jam = idx;
        variable_item_set_current_value_text(item, onoff_text[idx]);
        break;
    case CerberusSettingDetectFlood:
        app->config.detect_flood = idx;
        variable_item_set_current_value_text(item, onoff_text[idx]);
        break;
    case CerberusSettingDetectReplay:
        app->config.detect_replay = idx;
        variable_item_set_current_value_text(item, onoff_text[idx]);
        break;
    case CerberusSettingNotifySound:
        app->config.notify_sound = idx;
        variable_item_set_current_value_text(item, onoff_text[idx]);
        break;
    case CerberusSettingNotifyVibro:
        app->config.notify_vibro = idx;
        variable_item_set_current_value_text(item, onoff_text[idx]);
        break;
    case CerberusSettingNotifyLed:
        app->config.notify_led = idx;
        variable_item_set_current_value_text(item, onoff_text[idx]);
        break;
    case CerberusSettingLogCsv:
        app->config.log_csv = idx;
        variable_item_set_current_value_text(item, onoff_text[idx]);
        break;
    case CerberusSettingKeepScreen:
        app->config.keep_screen_on = idx;
        variable_item_set_current_value_text(item, onoff_text[idx]);
        break;
    default:
        break;
    }

    // Keep the running worker (if any) in sync immediately.
    cerberus_apply_config(app);
}

static VariableItem* cerberus_add_choice(
    Cerberus* app,
    uint8_t field,
    const char* label,
    uint8_t count,
    uint8_t value,
    const char* const* texts) {
    VariableItem* item = variable_item_list_add(
        app->var_item_list, label, count, cerberus_settings_changed, &app->setting_ctx[field]);
    if(value >= count) value = 0;
    variable_item_set_current_value_index(item, value);
    variable_item_set_current_value_text(item, texts[value]);
    return item;
}

void cerberus_scene_settings_on_enter(void* context) {
    Cerberus* app = context;
    VariableItemList* list = app->var_item_list;
    variable_item_list_reset(list);

    cerberus_add_choice(
        app, CerberusSettingScanMode, "Scan Mode", 4, app->config.scan_mode, scan_mode_text);
    cerberus_add_choice(
        app, CerberusSettingSensitivity, "Sensitivity", 3, app->config.sensitivity, sens_text);
    cerberus_add_choice(
        app, CerberusSettingDetectJam, "Detect Jam", 2, app->config.detect_jam, onoff_text);
    cerberus_add_choice(
        app, CerberusSettingDetectFlood, "Detect Flood", 2, app->config.detect_flood, onoff_text);
    cerberus_add_choice(
        app,
        CerberusSettingDetectReplay,
        "Detect Replay",
        2,
        app->config.detect_replay,
        onoff_text);
    cerberus_add_choice(
        app, CerberusSettingNotifySound, "Alert Sound", 2, app->config.notify_sound, onoff_text);
    cerberus_add_choice(
        app, CerberusSettingNotifyVibro, "Alert Vibro", 2, app->config.notify_vibro, onoff_text);
    cerberus_add_choice(
        app, CerberusSettingNotifyLed, "Alert LED", 2, app->config.notify_led, onoff_text);
    cerberus_add_choice(
        app, CerberusSettingLogCsv, "Log to SD", 2, app->config.log_csv, onoff_text);
    cerberus_add_choice(
        app, CerberusSettingKeepScreen, "Keep Screen On", 2, app->config.keep_screen_on, onoff_text);

    variable_item_list_set_selected_item(
        list, scene_manager_get_scene_state(app->scene_manager, CerberusSceneSettings));

    view_dispatcher_switch_to_view(app->view_dispatcher, CerberusViewVarItemList);
}

bool cerberus_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void cerberus_scene_settings_on_exit(void* context) {
    Cerberus* app = context;
    scene_manager_set_scene_state(
        app->scene_manager,
        CerberusSceneSettings,
        variable_item_list_get_selected_item_index(app->var_item_list));
    variable_item_list_reset(app->var_item_list);
}
