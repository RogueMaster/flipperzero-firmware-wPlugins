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

    XexFile* xex = xex_file_open(new_path, app->storage);
    if(xex == NULL) {
        scene_manager_next_scene(app->scene_manager, SceneError);
        return;
    }

    if(!xex_loader_start(app->xex_loader, xex, app->sio)) {
        xex_file_close(xex);
        scene_manager_next_scene(app->scene_manager, SceneError);
        return;
    }

    scene_manager_search_and_switch_to_another_scene(app->scene_manager, SceneXexLoader);
}

void scene_xex_select_on_enter(void* context) {
    App* app = (App*)context;

    file_browser_configure(
        app->file_browser, ".xex", XEX_DATA_PATH_PREFIX, true, true, NULL, false);

    file_browser_set_callback(app->file_browser, file_browser_callback, app);

    FuriString* path = furi_string_alloc_set(XEX_DATA_PATH_PREFIX);
    file_browser_start(app->file_browser, path);
    furi_string_free(path);

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewFileBrowser);
}

bool scene_xex_select_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void scene_xex_select_on_exit(void* context) {
    App* app = (App*)context;

    file_browser_stop(app->file_browser);
}
