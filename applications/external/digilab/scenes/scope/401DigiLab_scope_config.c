/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#include "401DigiLab_scope_config.h"
#include "401_config.h"

static const char* TAG = "401_DigiLabScopeConfig";

const DigiLab_ScopeSound digilab_scope_sound_value[] = {
    DigiLab_ScopeSoundOff,
    DigiLab_ScopeSoundAlert,
    Digilab_ScopeSoundOn};
const char* const digilab_scope_sound_text[] = {"OFF", "Alert", "ON"};

const DigiLab_ScopeVibro digilab_scope_vibro_value[] = {
    DigiLab_ScopeVibroOff,
    DigiLab_ScopeVibroAlert};
const char* const digilab_scope_vibro_text[] = {"OFF", "Alert"};

const DigiLab_ScopeLed digilab_scope_led_value[] = {
    DigiLab_ScopeLedOff,
    DigiLab_ScopeLedAlert,
    DigiLab_ScopeLedFollow,
    DigiLab_ScopeLedVariance,
    DigiLab_ScopeLedTrigger};
const char* const digilab_scope_led_text[] = {"OFF", "Alert", "Follow", "Variance", "Trigger"};

const DigiLab_ScopeAlert digilab_scope_alert_value[] = {
    DigiLab_ScopeAlert_lt3V,
    DigiLab_ScopeAlert_gt3V,
    DigiLab_ScopeAlert_lt5V,
    DigiLab_ScopeAlert_gt5V,
    DigiLab_ScopeAlert_osc,
    DigiLab_ScopeAlert_maxV,
    DigiLab_ScopeAlert_0V};
const char* const digilab_scope_alert_text[] =
    {"u<3.3", "u>3.3", "u<5v", "u>5v", "Osc", "~MAX", "~0v"};

/**
 * @brief Callback function to handle orientation menu
 *
 * @param item Selected item
 */
void on_change_scope_sound(VariableItem* item) {
    AppContext* app = variable_item_get_context(item);
    Configuration* digilab_data = (Configuration*)app->data->config;
    uint8_t index = variable_item_get_current_value_index(item);
    digilab_data->ScopeSound = digilab_scope_sound_value[index];
    variable_item_set_current_value_text(item, digilab_scope_sound_text[index]);
}

/**
 * @brief Callback function to handle orientation menu
 *
 * @param item Selected item
 */
void on_change_scope_vibro(VariableItem* item) {
    AppContext* app = variable_item_get_context(item);
    Configuration* digilab_data = (Configuration*)app->data->config;
    uint8_t index = variable_item_get_current_value_index(item);
    digilab_data->ScopeVibro = digilab_scope_vibro_value[index];
    variable_item_set_current_value_text(item, digilab_scope_vibro_text[index]);
}

/**
 * @brief Callback function to handle orientation menu
 *
 * @param item Selected item
 */
void on_change_scope_led(VariableItem* item) {
    AppContext* app = variable_item_get_context(item);
    Configuration* digilab_data = (Configuration*)app->data->config;
    uint8_t index = variable_item_get_current_value_index(item);
    digilab_data->ScopeLed = digilab_scope_led_value[index];
    variable_item_set_current_value_text(item, digilab_scope_led_text[index]);
}

/**
 * @brief Callback function to handle orientation menu
 *
 * @param item Selected item
 */
void on_change_scope_alert(VariableItem* item) {
    AppContext* app = variable_item_get_context(item);
    Configuration* digilab_data = (Configuration*)app->data->config;
    uint8_t index = variable_item_get_current_value_index(item);
    digilab_data->ScopeAlert = digilab_scope_alert_value[index];
    variable_item_set_current_value_text(item, digilab_scope_alert_text[index]);
}

/**
 * @brief Render callback for the app configuration.
 *
 * @param canvas The canvas to be used for rendering.
 * @param ctx The context passed to the callback.
 */
void app_scope_config_render_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 15, 30, AlignLeft, AlignTop, "ScopeConfig");
}

/**
 * @brief Allocate memory and initialize the app configuration.
 *
 * @return Returns a pointer to the allocated AppScopeConfig.
 */

AppScopeConfig* app_scope_config_alloc(void* ctx) {
    furi_assert(ctx);
    AppContext* app = (AppContext*)ctx;
    Configuration* config = app->data->config;
    AppScopeConfig* appScopeConfig = malloc(sizeof(AppScopeConfig));

    appScopeConfig->list = variable_item_list_alloc();
    variable_item_list_reset(appScopeConfig->list);

    VariableItem* item = variable_item_list_add(
        appScopeConfig->list,
        "Sound",
        COUNT_OF(digilab_scope_sound_text),
        on_change_scope_sound,
        app);
    variable_item_set_current_value_index(item,
                                          config->ScopeSound); // 0,10,20,30,...
    variable_item_set_current_value_text(item, digilab_scope_sound_text[config->ScopeSound]);

    item = variable_item_list_add(
        appScopeConfig->list,
        "Vibro",
        COUNT_OF(digilab_scope_vibro_text),
        on_change_scope_vibro,
        app);
    variable_item_set_current_value_index(item,
                                          config->ScopeVibro); // 0,10,20,30,...
    variable_item_set_current_value_text(item, digilab_scope_vibro_text[config->ScopeVibro]);

    item = variable_item_list_add(
        appScopeConfig->list, "LED", COUNT_OF(digilab_scope_led_text), on_change_scope_led, app);
    variable_item_set_current_value_index(item,
                                          config->ScopeLed); // 0,10,20,30,...
    variable_item_set_current_value_text(item, digilab_scope_led_text[config->ScopeLed]);

    if(config->ScopeAlert >= COUNT_OF(digilab_scope_alert_text)) config->ScopeAlert = 0;
    item = variable_item_list_add(
        appScopeConfig->list,
        "Alert",
        COUNT_OF(digilab_scope_alert_text),
        on_change_scope_alert,
        app);
    variable_item_set_current_value_index(item,
                                          config->ScopeAlert); // 0,10,20,30,...
    variable_item_set_current_value_text(item, digilab_scope_alert_text[config->ScopeAlert]);

    return appScopeConfig;
}

/**
 * @brief Callback function to handle input events for the app configuration.
 *
 * @param input_event The input event.
 * @param ctx The context passed to the callback.
 * @return Returns true if the event was handled, otherwise false.
 */
bool app_scope_config_input_callback(InputEvent* input_event, void* ctx) {
    UNUSED(ctx);
    UNUSED(input_event);
    bool handled = false;
    return handled;
}

/**
 * @brief Free the memory occupied by the app configuration.
 *
 * @param appScopeConfig The AppScopeConfig to be freed.
 */
void app_scope_config_free(AppScopeConfig* appScopeConfig) {
    furi_assert(appScopeConfig);
    variable_item_list_free(appScopeConfig->list);
    free(appScopeConfig);
}

/**
 * @brief Get the view associated with the app configuration.
 *
 * @param appScopeConfig The AppScopeConfig for which the view is to be fetched.
 * @return Returns a pointer to the View.
 */
View* app_scope_config_get_view(AppScopeConfig* appScopeConfig) {
    furi_assert(appScopeConfig);
    return variable_item_list_get_view(appScopeConfig->list);
}

/**
 * @brief Callback when the app configuration scene is entered.
 *
 * @param ctx The context passed to the callback.
 */
void app_scene_scope_config_on_enter(void* ctx) {
    AppContext* app = (AppContext*)ctx;
    // AppScopeConfig *appScopeConfig = (AppScopeConfig *)app->sceneConfig;
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewScopeConfig);
}

/**
 * @brief Handle scene manager events for the app configuration scene.
 *
 * @param context The context passed to the callback.
 * @param event The scene manager event.
 * @return Returns true if the event was consumed, otherwise false.
 */
bool app_scene_scope_config_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;
}

/**
 * @brief Callback when the app configuration scene is exited.
 *
 * @param ctx The context passed to the callback.
 */
void app_scene_scope_config_on_exit(void* ctx) {
    AppContext* app = (AppContext*)ctx;
    l401_err res = config_save_json(DIGILABCONF_CONFIG_FILE, app->data->config);

    if(res == L401_OK) {
        FURI_LOG_I(TAG, "Successfully saved configuration to %s.", DIGILABCONF_CONFIG_FILE);
    } else {
        FURI_LOG_E(TAG, "Error while saving to %s: %d", DIGILABCONF_CONFIG_FILE, res);
    }
}
