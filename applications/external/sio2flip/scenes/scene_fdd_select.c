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

#include <dialogs/dialogs.h>

#include "app/app.h"

#include "scenes.h"

static void file_browser_callback(void* context) {
    App* app = (App*)context;

    const char* new_path = furi_string_get_cstr(app->file_path);

    for(size_t i = 0; i < FDD_EMULATOR_COUNT; i++) {
        DiskImage* image = fdd_get_disk(app->fdd[i]);
        if(image != NULL) {
            const FuriString* image_path = disk_image_path(image);
            if(furi_string_equal_str(image_path, new_path)) {
                set_last_error("Image not loaded", "%s \n already loaded in D%d", new_path, i + 1);
                scene_manager_next_scene(app->scene_manager, SceneError);
                return;
            }
        }
    }

    DiskImage* image = disk_image_open(new_path, app->storage);

    if(image != NULL && fdd_insert_disk(app->fdd[app->selected_fdd], image)) {
        furi_string_set(app->config.fdd[app->selected_fdd].image, new_path);
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, SceneFddInfo);
    } else {
        scene_manager_next_scene(app->scene_manager, SceneError);
    }
}

void scene_fdd_select_on_enter(void* context) {
    App* app = (App*)context;

    file_browser_configure(
        app->file_browser, ".atr", ATR_DATA_PATH_PREFIX, true, true, NULL, false);

    file_browser_set_callback(app->file_browser, file_browser_callback, app);

    FuriString* path = furi_string_alloc_set(ATR_DATA_PATH_PREFIX);
    file_browser_start(app->file_browser, path);
    furi_string_free(path);

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewFileBrowser);
}

bool scene_fdd_select_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void scene_fdd_select_on_exit(void* context) {
    App* app = (App*)context;

    file_browser_stop(app->file_browser);
}
