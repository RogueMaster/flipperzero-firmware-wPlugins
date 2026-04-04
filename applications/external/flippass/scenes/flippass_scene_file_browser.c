/**
 * @file flippass_scene_file_browser.c
 * @brief Implementation of the file browser scene.
 *
 * This file contains the implementation of the scene handlers for the
 * file browser, including on_enter, on_event, and on_exit.
 */
#include "flippass_scene_file_browser.h"
#include "../flippass.h"
#include "flippass_icons.h"
#include "flippass_scene.h"
#include <gui/modules/file_browser.h>
#include <storage/storage.h>

#define KBDX_EXTENSION   "kdbx" /**<  File extension for KeePass databases. */
#define KBDX_ICON        &I_Storage /**< Icon for KeePass database files. */
#define KBDX_SEARCH_PATH EXT_PATH("apps_data/flippass") /**< Path to the application data directory. */

/**
 * @brief Callback for the file browser.
 *
 * This function is called when a file is selected in the file browser. It
 * saves the selected file path to the application's settings and transitions
 * to the password entry scene..
 *
 * @param context The application context.
 */
static void flippass_file_browser_callback(void* context) {
    App* app = context;
    flippass_reset_database(app);
    flippass_clear_text_buffer(app);
    flippass_clear_master_password(app);
    flippass_save_settings(app);
    scene_manager_next_scene(app->scene_manager, FlipPassScene_PasswordEntry);
}

/**
 * @brief Handler for the on_enter event of the file browser scene.
 * @param context The application context.
 */
void flippass_scene_file_browser_on_enter(void* context) {
    App* app = context;
    file_browser_configure(
        app->file_browser, KBDX_EXTENSION, KBDX_SEARCH_PATH, true, true, KBDX_ICON, true);
    file_browser_set_callback(app->file_browser, flippass_file_browser_callback, app);
    file_browser_start(app->file_browser, app->file_path);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewFileBrowser);
}

/**
 * @brief Handler for the on_event event of the file browser scene.
 * @param context The application context.
 * @param event The event that was triggered.
 * @return True if the event was handled, false otherwise.
 */
bool flippass_scene_file_browser_on_event(void* context, SceneManagerEvent event) {
    App* app      = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        if(!scene_manager_previous_scene(app->scene_manager)) {
            flippass_request_exit(app);
        }
        consumed = true;
    }
    return consumed;
}

/**
 * @brief Handler for the on_exit event of the file browser scene.
 * @param context The application context.
 */
void flippass_scene_file_browser_on_exit(void* context) {
    App* app = context;
    file_browser_stop(app->file_browser);
}
