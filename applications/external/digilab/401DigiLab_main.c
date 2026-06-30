/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#include "401DigiLab_main.h"
#include "drivers/sk6805.h"
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
//  static const char* TAG = "401_DigiLab";
#define CHECK_HAT

/**
 * Handles input events for the app.
 *
 * @param input_event The input event.
 * @param ctx The application context.
 * @return false to indicate the event was not consumed.
 */
bool app_input_callback(InputEvent* input_event, void* ctx) {
    UNUSED(ctx);
    UNUSED(input_event);
    return false;
}

/**
 * Handles custom events for the app.
 *
 * @param context The application context.
 * @param event The custom event identifier.
 * @return true if the event was handled, false otherwise.
 */
bool app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    AppContext* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

/**
 * Handles navigation events for the app. Determines if the app should
 * exit based on back button presses.
 *
 * @param context The application context.
 * @return true if the back event was handled, false otherwise.
 */
bool app_navigation_event_callback(void* context) {
    furi_assert(context);
    AppContext* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

// ON ENTER SCENE CALLBACKS
void (*const app_on_enter_handlers[])(void*) = {
    app_scene_probe_on_enter,
    app_scene_i2ctoolscanner_on_enter,
    app_scene_i2ctoolreader_on_enter,
    app_scene_spitool_on_enter,
    app_scene_spitoolscanner_on_enter,
    app_scene_scope_on_enter,
    app_scene_scope_config_on_enter,
    app_scene_mainmenu_on_enter,
    app_scene_splash_on_enter,
    app_scene_config_on_enter};

// ON EVENT SCENE CALLBACKS
bool (*const app_on_event_handlers[])(void* context, SceneManagerEvent event) = {
    app_scene_probe_on_event,
    app_scene_i2ctoolscanner_on_event,
    app_scene_i2ctoolreader_on_event,
    app_scene_spitool_on_event,
    app_scene_spitoolscanner_on_event,
    app_scene_scope_on_event,
    app_scene_scope_config_on_event,
    app_scene_mainmenu_on_event,
    app_scene_splash_on_event,
    app_scene_config_on_event};

// ON EXIT SCENE CALLBACKS
void (*const app_on_exit_handlers[])(void* context) = {
    app_scene_probe_on_exit,
    app_scene_i2ctoolscanner_on_exit,
    app_scene_i2ctoolreader_on_exit,
    app_scene_spitool_on_exit,
    app_scene_spitoolscanner_on_exit,
    app_scene_scope_on_exit,
    app_scene_scope_config_on_exit,
    app_scene_mainmenu_on_exit,
    app_scene_splash_on_exit,
    app_scene_config_on_exit};

const SceneManagerHandlers app_scene_handlers = {
    .on_enter_handlers = app_on_enter_handlers,
    .on_event_handlers = app_on_event_handlers,
    .on_exit_handlers = app_on_exit_handlers,
    .scene_num = AppSceneNum,
};

/**
 * Handles the quit action for the app.
 *
 * @param context The application context.
 * @return VIEW_NONE indicating no view should be displayed.
 */
uint32_t app_Quit_callback(void* ctx) {
    UNUSED(ctx);
    return VIEW_NONE;
}

/**
 * Navigates to the Main Menu view.
 *
 * @param context The application context.
 * @return AppViewMainMenu indicating the main menu view should be displayed.
 */
uint32_t app_navigateTo_MainMenu_callback(void* ctx) {
    UNUSED(ctx);
    return AppViewMainMenu;
}

/**
 * Navigates to the Splash view.
 *
 * @param context The application context.
 * @return AppViewSplash indicating the splash view should be displayed.
 */
uint32_t app_navigateTo_Splash_callback(void* ctx) {
    UNUSED(ctx);
    return AppViewSplash;
}

l401_err check_hat(AppContext* app) {
    UNUSED(app);
    furi_hal_i2c_acquire(I2C_BUS);
    uint8_t data[4] = {0};
    bool res = false;
    // Read the first 5 bytes from the EEPROM
    res = furi_hal_i2c_read_mem(I2C_BUS, 0x57 << 1, 0x00, data, 4, I2C_MEM_I2C_TIMEOUT);
    furi_hal_i2c_release(I2C_BUS);
    return res ? L401_OK : L401_ERR_HARDWARE;
}

/**
 * Allocates and initializes the app context. Sets up all necessary views,
 * configurations, and event handlers.
 *
 * @return A pointer to the allocated AppContext.
 */
AppContext* app_alloc() {
    bool must_recalibrate = false;
    // Enables 5V on GPIO Header
    if(!furi_hal_power_is_otg_enabled()) furi_hal_power_enable_otg();

    l401_err res;
    // Initialize application
    AppContext* app_ctx = malloc(sizeof(AppContext));
    Gui* gui = furi_record_open(RECORD_GUI);
    SK6805_init();
    app_ctx->scene_manager = scene_manager_alloc(&app_scene_handlers, app_ctx);
    app_ctx->notifications = furi_record_open(RECORD_NOTIFICATION); // Used for backlight
    app_ctx->view_dispatcher = view_dispatcher_alloc();

    view_dispatcher_attach_to_gui(app_ctx->view_dispatcher, gui, ViewDispatcherTypeFullscreen);

    // Initialize MainMenu
    app_ctx->mainmenu = submenu_alloc();
    app_ctx->data = malloc(sizeof(AppData));
    app_ctx->err = L401_OK;

#ifdef CHECK_HAT
    if(check_hat(app_ctx) != L401_OK) {
        app_ctx->err = L401_ERR_HARDWARE;
        return app_ctx;
    }
#endif

    res = config_alloc(&app_ctx->data->config);
    if(res != L401_OK) {
        app_ctx->err = L401_ERR_INTERNAL;
        return app_ctx;
    }
    // Check calibration
    res = config_check_calibration();
    if(res != L401_OK) {
        must_recalibrate = true;
    }
    // Load configuration.
    res = config_load_json(DIGILABCONF_CONFIG_FILE, app_ctx->data->config);
    if(res != L401_OK) {
        app_ctx->err = L401_ERR_MALFORMED;
        return app_ctx;
    }

    view_dispatcher_set_event_callback_context(app_ctx->view_dispatcher, app_ctx);

    submenu_add_item(
        app_ctx->mainmenu, "I2C Probe", AppMainMenuIndex_I2CTool, appMainMenu_callback, app_ctx);
    submenu_add_item(
        app_ctx->mainmenu, "SPI Probe", AppMainMenuIndex_SPITool, appMainMenu_callback, app_ctx);
    submenu_add_item(
        app_ctx->mainmenu, "Probe", AppMainMenuIndex_Probe, appMainMenu_callback, app_ctx);
    submenu_add_item(
        app_ctx->mainmenu, "Scope", AppMainMenuIndex_Osc, appMainMenu_callback, app_ctx);
    submenu_add_item(
        app_ctx->mainmenu, "Calibration", AppMainMenuIndex_Config, appMainMenu_callback, app_ctx);
    submenu_add_item(
        app_ctx->mainmenu, "About", AppMainMenuIndex_About, appMainMenu_callback, app_ctx);
    submenu_add_item(
        app_ctx->mainmenu, "Splash", AppMainMenuIndex_Splash, appMainMenu_callback, app_ctx);

    view_dispatcher_add_view(
        app_ctx->view_dispatcher, AppViewMainMenu, submenu_get_view(app_ctx->mainmenu));

    app_ctx->sceneProbe = app_probe_alloc(app_ctx);
    app_ctx->sceneI2CToolScanner = app_i2ctoolscanner_alloc(app_ctx);
    app_ctx->sceneI2CToolReader = app_i2ctoolreader_alloc(app_ctx);
    app_ctx->spitoolmenu = app_spitool_alloc(app_ctx);
    app_ctx->sceneSPIToolScanner = app_spitoolscanner_alloc(app_ctx);
    app_ctx->sceneScope = app_scope_alloc(app_ctx);
    app_ctx->sceneScopeConfig = app_scope_config_alloc(app_ctx);
    app_ctx->sceneSplash = app_splash_alloc(app_ctx);
    app_ctx->sceneConfig = app_config_alloc(app_ctx);

    view_dispatcher_add_view(
        app_ctx->view_dispatcher, AppViewProbe, app_probe_get_view(app_ctx->sceneProbe));

    view_dispatcher_add_view(
        app_ctx->view_dispatcher,
        AppViewI2CToolScanner,
        app_i2ctoolscanner_get_view(app_ctx->sceneI2CToolScanner));
    view_dispatcher_add_view(
        app_ctx->view_dispatcher,
        AppViewI2CToolReader,
        app_i2ctoolreader_get_view(app_ctx->sceneI2CToolReader));

    view_dispatcher_add_view(
        app_ctx->view_dispatcher, AppViewSPITool, submenu_get_view(app_ctx->spitoolmenu));
    view_dispatcher_add_view(
        app_ctx->view_dispatcher,
        AppViewSPIToolScanner,
        app_spitoolscanner_get_view(app_ctx->sceneSPIToolScanner));

    view_dispatcher_add_view(
        app_ctx->view_dispatcher, AppViewScope, app_scope_get_view(app_ctx->sceneScope));
    view_dispatcher_add_view(
        app_ctx->view_dispatcher,
        AppViewScopeConfig,
        app_scope_config_get_view(app_ctx->sceneScopeConfig));

    view_dispatcher_add_view(
        app_ctx->view_dispatcher, AppViewSplash, app_splash_get_view(app_ctx->sceneSplash));
    view_dispatcher_add_view(
        app_ctx->view_dispatcher, AppViewConfig, app_config_get_view(app_ctx->sceneConfig));

    // Initialize events
    view_dispatcher_set_navigation_event_callback(
        app_ctx->view_dispatcher, app_navigation_event_callback);
    view_dispatcher_set_custom_event_callback(app_ctx->view_dispatcher, app_custom_event_callback);
    view_set_previous_callback(submenu_get_view(app_ctx->mainmenu), app_Quit_callback);

    if(must_recalibrate == true) {
        scene_manager_next_scene(app_ctx->scene_manager, AppSceneConfig);
    } else {
        scene_manager_next_scene(app_ctx->scene_manager, AppSceneSplash);
    }

    return app_ctx;
}

/**
 * Frees all resources associated with the given app context.
 *
 * @param app_ctx The app context to be freed.
 */
void app_free(AppContext* app_ctx) {
    furi_assert(app_ctx);

    // Free AppViewProbe
    view_dispatcher_remove_view(app_ctx->view_dispatcher, AppViewProbe);
    app_probe_free(app_ctx->sceneProbe);
    // Free AppViewI2CToolScanner
    view_dispatcher_remove_view(app_ctx->view_dispatcher, AppViewI2CToolScanner);
    app_i2ctoolscanner_free(app_ctx->sceneI2CToolScanner);
    // Free AppViewI2CToolReader
    view_dispatcher_remove_view(app_ctx->view_dispatcher, AppViewI2CToolReader);
    app_i2ctoolreader_free(app_ctx);
    // Free AppViewSPIToolScanner
    view_dispatcher_remove_view(app_ctx->view_dispatcher, AppViewSPIToolScanner);
    app_spitoolscanner_free(app_ctx->sceneSPIToolScanner);
    // Free AppViewScope
    view_dispatcher_remove_view(app_ctx->view_dispatcher, AppViewScope);
    app_scope_free(app_ctx->sceneScope);
    // Free AppViewScopeConfig
    view_dispatcher_remove_view(app_ctx->view_dispatcher, AppViewScopeConfig);
    app_scope_config_free(app_ctx->sceneScopeConfig);
    // Free AppViewSplash
    view_dispatcher_remove_view(app_ctx->view_dispatcher, AppViewSplash);
    app_splash_free(app_ctx->sceneSplash);
    // Free AppViewConfig
    view_dispatcher_remove_view(app_ctx->view_dispatcher, AppViewConfig);
    app_config_free(app_ctx->sceneConfig);
    // Free AppViewMainMenu
    view_dispatcher_remove_view(app_ctx->view_dispatcher, AppViewMainMenu);
    submenu_free(app_ctx->mainmenu);
    // Free AppViewSPITool
    view_dispatcher_remove_view(app_ctx->view_dispatcher, AppViewSPITool);
    submenu_free(app_ctx->spitoolmenu);

    view_dispatcher_free(app_ctx->view_dispatcher);
    scene_manager_free(app_ctx->scene_manager);

    furi_record_close(RECORD_NOTIFICATION);
    app_ctx->notifications = NULL;

    furi_record_close(RECORD_GUI);

    free(app_ctx->data);
    free(app_ctx);
}

/**
 * The main entry point for the DigiLab app. Sets up the app, runs the
 * main loop, and then cleans up before exiting.
 *
 * @param p Unused parameter.
 * @return 0 indicating successful execution.
 */
int32_t digilab_app(void* p) {
    UNUSED(p);
    AppContext* app_ctx = app_alloc();
    if(app_ctx->err == L401_OK) {
        with_view_model(
            app_splash_get_view(app_ctx->sceneSplash),
            AppStateCtx * model,
            {
                model->app_state = AppStateSplash;
                model->screen = 0;
            },
            true);
        view_dispatcher_run(app_ctx->view_dispatcher);
        app_free(app_ctx);
        furi_record_close(RECORD_GUI);
    } else {
        l401_sign_app(app_ctx->err);
    }
    return 0;
}
