/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#include "401LightMsg_main.h"

//#include <assets_icons.h>
static const char* TAG = "401_LightMsg";
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
    app_scene_mainmenu_on_enter,
    app_scene_bmp_editor_on_enter,
    app_scene_splash_on_enter,
    app_scene_config_on_enter,
    app_scene_set_text_on_enter,
    app_scene_acc_on_enter};

// ON EVENT SCENE CALLBACKS
bool (*const app_on_event_handlers[])(void* context, SceneManagerEvent event) = {
    app_scene_mainmenu_on_event,
    app_scene_bmp_editor_on_event,
    app_scene_splash_on_event,
    app_scene_config_on_event,
    app_scene_set_text_on_event,
    app_scene_acc_on_event,
};

// ON EXIT SCENE CALLBACKS
void (*const app_on_exit_handlers[])(void* context) = {
    app_scene_mainmenu_on_exit,
    app_scene_bmp_editor_on_exit,
    app_scene_splash_on_exit,
    app_scene_config_on_exit,
    app_scene_set_text_on_exit,
    app_scene_acc_on_exit,
};

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
    furi_assert(app);
    AppContext* app_ctx = app;
    uint8_t acc_id = 0;
    lis2dh12_device_id_get(&app_ctx->data->lis2dh12, &acc_id);
    if(acc_id == 0) {
        return L401_ERR_HARDWARE;
    }
    return L401_OK;
}

/**
 * Allocates and initializes the app context. Sets up all necessary views,
 * configurations, and event handlers.
 *
 * @return A pointer to the allocated AppContext.
 */
AppContext* app_alloc() {
    // Enables 5V on GPIO Header
    furi_hal_power_enable_otg();
    l401_err res;
    // Initialize application
    AppContext* app_ctx = malloc(sizeof(AppContext));
    Gui* gui = furi_record_open(RECORD_GUI);

    app_ctx->scene_manager = scene_manager_alloc(&app_scene_handlers, app_ctx);
    app_ctx->notifications = furi_record_open(RECORD_NOTIFICATION); // Used for backlight
    app_ctx->view_dispatcher = view_dispatcher_alloc();

    view_dispatcher_attach_to_gui(app_ctx->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    SK6805_init();
    // Initialize MainMenu
    app_ctx->mainmenu = submenu_alloc();
    // scene_manager_set_scene(app_ctx->scene_manager,AppSceneSplash);
    app_ctx->data = malloc(sizeof(AppData));
    // LIS2DH12 Driver configuration
    app_ctx->data->lis2dh12.write_reg = platform_write;
    app_ctx->data->lis2dh12.read_reg = platform_read;

    app_ctx->err = L401_OK;

    if(check_hat(app_ctx) != L401_OK) {
        app_ctx->err = L401_ERR_HARDWARE;
        return app_ctx;
    }
    SK6805_init();

    res = config_alloc(&app_ctx->data->config);
    if(res != L401_OK) {
        app_ctx->err = L401_ERR_INTERNAL;
        return app_ctx;
    }

    // Load configuration.
    res = config_load_json(LIGHTMSGCONF_CONFIG_FILE, app_ctx->data->config);
    if(res != L401_OK) {
        app_ctx->err = L401_ERR_MALFORMED;
        return app_ctx;
    }

    // Get the shader from config
    app_ctx->data->shader = lightmsg_color_value[app_ctx->data->config->color];

    // Populates the main menu
    submenu_add_item(
        app_ctx->mainmenu, "Text", AppMainMenuIndex_Acc_Text, appMainMenu_callback, app_ctx);
    submenu_add_item(
        app_ctx->mainmenu, "Bitmap", AppMainMenuIndex_Acc_Bitmap, appMainMenu_callback, app_ctx);
    submenu_add_item(
        app_ctx->mainmenu,
        "BitmapEditor",
        AppMainMenuIndex_BmpEditor,
        appMainMenu_callback,
        app_ctx);
    submenu_add_item(
        app_ctx->mainmenu, "Configuration", AppMainMenuIndex_Config, appMainMenu_callback, app_ctx);
    submenu_add_item(
        app_ctx->mainmenu,
        "FlashLight",
        AppMainMenuIndex_Flashlight,
        appMainMenu_callback,
        app_ctx);
    submenu_add_item(
        app_ctx->mainmenu, "About", AppMainMenuIndex_About, appMainMenu_callback, app_ctx);
    submenu_add_item(
        app_ctx->mainmenu, "Splash", AppMainMenuIndex_Splash, appMainMenu_callback, app_ctx);

    // Allocate scenes
    app_ctx->sceneSplash = app_splash_alloc(app_ctx);
    app_ctx->sceneConfig = app_config_alloc(app_ctx);
    app_ctx->sceneAcc = app_acc_alloc(app_ctx);
    app_ctx->sceneBmpEditor = app_bmp_editor_alloc(app_ctx);
    app_ctx->sceneSetText = text_input_alloc();

    view_dispatcher_set_event_callback_context(app_ctx->view_dispatcher, app_ctx);

    view_dispatcher_add_view(
        app_ctx->view_dispatcher, AppViewMainMenu, submenu_get_view(app_ctx->mainmenu));
    view_dispatcher_add_view(
        app_ctx->view_dispatcher, AppViewSplash, app_splash_get_view(app_ctx->sceneSplash));
    view_dispatcher_add_view(
        app_ctx->view_dispatcher,
        AppViewBmpEditor,
        app_bitmap_editor_get_view(app_ctx->sceneBmpEditor));
    view_dispatcher_add_view(
        app_ctx->view_dispatcher, AppViewSetText, text_input_get_view(app_ctx->sceneSetText));
    view_dispatcher_add_view(
        app_ctx->view_dispatcher, AppViewConfig, app_config_get_view(app_ctx->sceneConfig));
    view_dispatcher_add_view(
        app_ctx->view_dispatcher, AppViewAcc, app_acc_get_view(app_ctx->sceneAcc));

    // Initialize events
    view_dispatcher_set_navigation_event_callback(
        app_ctx->view_dispatcher, app_navigation_event_callback);
    view_dispatcher_set_custom_event_callback(app_ctx->view_dispatcher, app_custom_event_callback);
    view_set_previous_callback(submenu_get_view(app_ctx->mainmenu), app_Quit_callback);

    scene_manager_next_scene(app_ctx->scene_manager, AppSceneSplash);

    return app_ctx;
}

/**
 * Frees all resources associated with the given app context.
 *
 * @param app_ctx The app context to be freed.
 */
void app_free(AppContext* app_ctx) {
    furi_assert(app_ctx);

    // Free AppViewSplash
    view_dispatcher_remove_view(app_ctx->view_dispatcher, AppViewSplash);
    app_splash_free(app_ctx->sceneSplash);
    // Free AppViewConfig
    view_dispatcher_remove_view(app_ctx->view_dispatcher, AppViewConfig);
    app_config_free(app_ctx->sceneConfig);
    // Free AppViewAcc
    view_dispatcher_remove_view(app_ctx->view_dispatcher, AppViewAcc);
    app_acc_free(app_ctx->sceneAcc);
    // Free BMP Editor
    view_dispatcher_remove_view(app_ctx->view_dispatcher, AppViewBmpEditor);
    app_bmp_editor_free(app_ctx);
    // Free AppViewSetText
    view_dispatcher_remove_view(app_ctx->view_dispatcher, AppViewSetText);
    text_input_free(app_ctx->sceneSetText);
    // Free AppViewMainMenu
    view_dispatcher_remove_view(app_ctx->view_dispatcher, AppViewMainMenu);
    submenu_free(app_ctx->mainmenu);

    view_dispatcher_free(app_ctx->view_dispatcher);
    scene_manager_free(app_ctx->scene_manager);

    furi_record_close(RECORD_NOTIFICATION);
    app_ctx->notifications = NULL;

    furi_record_close(RECORD_GUI);

    free(app_ctx);
}

/**
 * The main entry point for the LightMsg app. Sets up the app, runs the
 * main loop, and then cleans up before exiting.
 *
 * @param p Unused parameter.
 * @return 0 indicating successful execution.
 */
int32_t lightMsg_app(void* p) {
    UNUSED(p);
    AppContext* app_ctx = app_alloc();
    FURI_LOG_I(TAG, "Start Lab401's LighMsg app");
    if(app_ctx->err == L401_OK) {
        with_view_model(
            app_splash_get_view(app_ctx->sceneSplash),
            AppStateCtx * model,
            { model->app_state = AppStateSplash; },
            true);
        view_dispatcher_run(app_ctx->view_dispatcher);
        app_free(app_ctx);
        furi_record_close(RECORD_GUI);
    } else {
        l401_sign_app(app_ctx->err);
    }

    return 0;
}
