/*
 * This file is part of the 8-bit ATAR SIO Emulator for Flipper Zero
 * (https://github.com/cepetr/sio2flip).
 * Copyright (c) 2025
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <furi.h>
#include <furi_hal.h>

#include <notification/notification_messages.h>

#include "scenes/scenes.h"

#include "app.h"

static bool app_custom_event_callback(void* context, uint32_t event) {
    furi_check(context != NULL);
    App* app = (App*)context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool app_back_event_callback(void* context) {
    furi_check(context != NULL);
    App* app = (App*)context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static bool is_charging(App* app) {
    PowerInfo power_info;
    power_get_info(app->power, &power_info);
    return power_info.is_charging;
}

static void
    fdd_activity_callback(void* context, SIODevice device, FddActivity activity, uint16_t sector) {
    furi_check(context != NULL);
    App* app = (App*)context;

    // When charging, do not blink the LED, as it is somewhat confusing and
    // significantly slows down the emulator speed.

    if(app->config.led_blinking && !is_charging(app)) {
        switch(activity) {
        case FddEmuActivity_Read:
            notification_message(app->notifications, &sequence_blink_green_10);
            break;
        case FddEmuActivity_Write:
            notification_message(app->notifications, &sequence_blink_red_10);
            break;
        default:
            notification_message(app->notifications, &sequence_blink_blue_10);
            break;
        }
    }

    if(app->selected_fdd == device - SIO_DEVICE_DISK1) {
        fdd_screen_update_activity(app->fdd_screen, activity, sector);
    }
}

static void xex_activity_callback(void* context) {
    furi_check(context != NULL);
    App* app = (App*)context;

    if(app->config.led_blinking && !is_charging(app)) {
        notification_message(app->notifications, &sequence_blink_green_10);
    }

    xex_screen_update_state(app->xex_screen, app->xex_loader);
}

static App* app_alloc() {
    FURI_LOG_T(TAG, "Starting application...");

    // Allocate the application state
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));

    // Open the GUI
    app->gui = furi_record_open(RECORD_GUI);
    // Open storage
    app->storage = furi_record_open(RECORD_STORAGE);

    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->power = furi_record_open(RECORD_POWER);

    // Create the data directories
    storage_common_mkdir(app->storage, ATR_DATA_PATH_PREFIX);
    storage_common_mkdir(app->storage, XEX_DATA_PATH_PREFIX);

    // Initialize the application configuration
    app_config_init(&app->config);
    app_config_load(&app->config, app->storage);

    // Initialize scene manager
    app->scene_manager = scene_manager_alloc(&scene_handlers, app);

    // Initialize view dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, app_back_event_callback);

    // Attach view dispatcher to the GUI
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Allocate view
    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        AppViewVariableList,
        variable_item_list_get_view(app->var_item_list));

    app->number_input = number_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, AppViewNumberInput, number_input_get_view(app->number_input));

    app->fdd_screen = fdd_screen_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, AppViewFddScreen, fdd_screen_get_view(app->fdd_screen));

    app->xex_screen = xex_screen_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, AppViewXexScreen, xex_screen_get_view(app->xex_screen));

    app->popup = popup_alloc();
    view_dispatcher_add_view(app->view_dispatcher, AppViewPopup, popup_get_view(app->popup));

    app->dialog = dialog_ex_alloc();
    view_dispatcher_add_view(app->view_dispatcher, AppViewDialog, dialog_ex_get_view(app->dialog));

    app->file_path = furi_string_alloc();

    app->file_browser = file_browser_alloc(app->file_path);
    view_dispatcher_add_view(
        app->view_dispatcher, AppViewFileBrowser, file_browser_get_view(app->file_browser));

    app->sio = sio_driver_alloc();
    furi_check(app->sio != NULL);

    app->selected_fdd = 0;

    for(size_t i = 0; i < FDD_EMULATOR_COUNT; i++) {
        app->fdd[i] = fdd_alloc(SIO_DEVICE_DISK1 + i, &app->config);
        furi_check(app->fdd[i] != NULL);

        fdd_set_activity_callback(app->fdd[i], fdd_activity_callback, app);

        const char* path = furi_string_get_cstr(app->config.fdd[i].image);

        if(path[0] != '\0') {
            DiskImage* image = disk_image_open(path, app->storage);
            if(image != NULL && !fdd_insert_disk(app->fdd[i], image)) {
                disk_image_close(image);
            }
        }
    }

    app->xex_loader = xex_loader_alloc(&app->config);
    furi_check(app->xex_loader != NULL);

    xex_loader_set_activity_callback(app->xex_loader, xex_activity_callback, app);

    app->atari850 = atari850_alloc(&app->config);
    furi_check(app->atari850 != NULL);

    atari850_load_rom(app->atari850, app->storage);

    FURI_LOG_T(TAG, "Application started");

    return app;
}

static void app_free(App* app) {
    FURI_LOG_T(TAG, "Stopping application...");

    xex_loader_free(app->xex_loader);

    if(app->sio) {
        for(size_t i = 0; i < FDD_EMULATOR_COUNT; i++) {
            fdd_free(app->fdd[i]);
        }
    }

    atari850_free(app->atari850);

    usb_vcp_free(app->usb_vcp);

    sio_driver_free(app->sio);

    app_config_save(&app->config, app->storage);

    // Free views
    view_dispatcher_remove_view(app->view_dispatcher, AppViewFileBrowser);
    file_browser_free(app->file_browser);
    furi_string_free(app->file_path);

    view_dispatcher_remove_view(app->view_dispatcher, AppViewVariableList);
    variable_item_list_free(app->var_item_list);

    view_dispatcher_remove_view(app->view_dispatcher, AppViewNumberInput);
    number_input_free(app->number_input);

    view_dispatcher_remove_view(app->view_dispatcher, AppViewPopup);
    popup_free(app->popup);

    view_dispatcher_remove_view(app->view_dispatcher, AppViewDialog);
    dialog_ex_free(app->dialog);

    view_dispatcher_remove_view(app->view_dispatcher, AppViewFddScreen);
    fdd_screen_free(app->fdd_screen);

    view_dispatcher_remove_view(app->view_dispatcher, AppViewXexScreen);
    xex_screen_free(app->xex_screen);

    // Free view dispatcher and scene manager
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_POWER);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);

    app_config_free(&app->config);

    free(app);

    FURI_LOG_T(TAG, "Application stopped");
}

static void app_run(App* app) {
    // Switch to the fdd emulator screen
    scene_manager_next_scene(app->scene_manager, SceneMainMenu);
    scene_manager_next_scene(app->scene_manager, SceneSplashScreen);

    FURI_LOG_D(TAG, "Running view dispatcher...");
    view_dispatcher_run(app->view_dispatcher);
}

// Application entrypoint called by the Furi runtime
int32_t app_startup(void* p) {
    UNUSED(p);

    App* app = app_alloc();
    app_run(app);
    app_free(app);

    return 0;
}

void app_start_fdd_emulation(App* app) {
    for(size_t i = 0; i < FDD_EMULATOR_COUNT; i++) {
        fdd_start(app->fdd[i], app->sio);
    }

    if(app->config.atari850) {
        atari850_start(app->atari850, app->sio);
        app->usb_vcp = usb_vcp_alloc(app->sio);
        furi_check(app->usb_vcp != NULL);
    }
}

void app_stop_emulation(App* app) {
    for(size_t i = 0; i < FDD_EMULATOR_COUNT; i++) {
        fdd_stop(app->fdd[i]);
    }
    xex_loader_stop(app->xex_loader);

    usb_vcp_free(app->usb_vcp);
    app->usb_vcp = NULL;

    atari850_stop(app->atari850);
}

const char* app_build_unique_file_name(App* app) {
    DateTime time;
    furi_hal_rtc_get_datetime(&time);

    furi_string_printf(
        app->file_path,
        ATR_DATA_PATH_PREFIX "/%04d%02d%02d_%02d%02d%02d.atr",
        time.year,
        time.month,
        time.day,
        time.hour,
        time.minute,
        time.second);

    return furi_string_get_cstr(app->file_path);
}
