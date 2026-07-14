/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#include "scenes/spi/401DigiLab_spitool.h"
//static const char* TAG = "401_DigiLabSPITool";

/**
 * Callback function for the main menu. Handles menu item selection and
 * switches to the appropriate scene based on the selected index.
 *
 * @param ctx The application ctx.
 * @param index The index of the selected menu item.
 */
void appSPITool_callback(void* ctx, uint32_t index) {
    AppContext* app = (AppContext*)ctx;
    switch(index) {
    case AppSPIToolIndex_Scanner:
        scene_manager_next_scene(app->scene_manager, AppSceneSPIToolScanner);
        break;
    }
}

/**
 * Handles the on-enter event for the main menu scene. Switches the view to
 * the main menu.
 *
 * @param ctx The application ctx.
 */
void app_scene_spitool_on_enter(void* ctx) {
    AppContext* app = ctx;
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewSPITool);
}

/**
 * Handles events for the main menu scene. Logs specific events and switches
 * to the appropriate scene.
 *
 * @param ctx The application ctx.
 * @param event The scene manager event.
 * @return true if the event was consumed, false otherwise.
 */
bool app_scene_spitool_on_event(void* ctx, SceneManagerEvent event) {
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
void app_scene_spitool_on_exit(void* ctx) {
    UNUSED(ctx);
}

/**
 * Allocates and initializes an instance of AppI2cTool. (Currently returns
 * NULL)
 *
 * @return A pointer to the allocated AppI2cTool or NULL.
 */
Submenu* app_spitool_alloc(void* ctx) {
    AppContext* app = (AppContext*)ctx;
    Submenu* submenu = submenu_alloc();
    submenu_add_item(submenu, "SPI Scanner", AppSPIToolIndex_Scanner, appSPITool_callback, app);
    submenu_add_item(submenu, "SPI Writer", AppSPIToolIndex_Scanner, appSPITool_callback, app);
    submenu_add_item(submenu, "SPI Reader", AppSPIToolIndex_Scanner, appSPITool_callback, app);
    return submenu;
}
