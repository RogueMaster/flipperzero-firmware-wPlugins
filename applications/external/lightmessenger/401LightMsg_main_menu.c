/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#include "401LightMsg_main_menu.h"
//static const char* TAG = "401_LightMsgMainMenu";

/**
 * Callback function for the main menu. Handles menu item selection and
 * switches to the appropriate scene based on the selected index.
 *
 * @param ctx The application ctx.
 * @param index The index of the selected menu item.
 */
void appMainMenu_callback(void* ctx, uint32_t index) {
    AppContext* app = (AppContext*)ctx;
    switch(index) {
    case AppMainMenuIndex_Splash:
        with_view_model(
            app_splash_get_view(app->sceneSplash),
            AppStateCtx * model,
            { model->app_state = AppStateSplash; },
            true);
        scene_manager_next_scene(app->scene_manager, AppSceneSplash);
        break;
    case AppMainMenuIndex_Flashlight:
        with_view_model(
            app_splash_get_view(app->sceneSplash),
            AppStateCtx * model,
            { model->app_state = AppStateFlashlight; },
            true);
        scene_manager_next_scene(app->scene_manager, AppSceneSplash);
        break;
    case AppMainMenuIndex_About:
        with_view_model(
            app_splash_get_view(app->sceneSplash),
            AppStateCtx * model,
            { model->app_state = AppStateAbout; },
            true);
        scene_manager_next_scene(app->scene_manager, AppSceneSplash);
        break;
    case AppMainMenuIndex_Config:
        scene_manager_next_scene(app->scene_manager, AppSceneConfig);
        break;
    case AppMainMenuIndex_SetText:
        scene_manager_next_scene(app->scene_manager, AppSceneSetText);
        break;
    case AppMainMenuIndex_BmpEditor:
        scene_manager_next_scene(app->scene_manager, AppSceneBmpEditor);
        break;
    case AppMainMenuIndex_Acc_Text:
        app->sceneAcc->displayMode = APPACC_DISPLAYMODE_TEXT;
        scene_manager_next_scene(app->scene_manager, AppSceneSetText);
        break;
    case AppMainMenuIndex_Acc_Bitmap:
        app->sceneAcc->displayMode = APPACC_DISPLAYMODE_BITMAP;
        scene_manager_next_scene(app->scene_manager, AppSceneAcc);
        break;
    default:
        break;
    }
}

/**
 * Handles the on-enter event for the main menu scene. Switches the view to
 * the main menu.
 *
 * @param ctx The application ctx.
 */
void app_scene_mainmenu_on_enter(void* ctx) {
    AppContext* app = ctx;
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewMainMenu);
}

/**
 * Handles events for the main menu scene. Logs specific events and switches
 * to the appropriate scene.
 *
 * @param ctx The application ctx.
 * @param event The scene manager event.
 * @return true if the event was consumed, false otherwise.
 */
bool app_scene_mainmenu_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    bool consumed = false;
    return consumed;
}

/**
 * Handles the on-exit event for the main menu scene.
 *
 * @param ctx The application ctx.
 */
void app_scene_mainmenu_on_exit(void* ctx) {
    UNUSED(ctx);
}

/**
 * Allocates and initializes an instance of AppMainMenu. (Currently returns
 * NULL)
 *
 * @return A pointer to the allocated AppMainMenu or NULL.
 */
AppMainMenu* app_mainmenu_alloc() {
    return NULL;
}
