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

#include "app/app.h"

#include "scenes.h"

static void dialog_callback(DialogExResult result, void* context) {
    App* app = (App*)context;

    if(result == DialogExResultRight) {
        const char* new_path = furi_string_get_cstr(app->file_path);

        DiskImage* image = disk_image_create(new_path, app->storage);

        if(image != NULL && fdd_insert_disk(app->fdd[app->selected_fdd], image)) {
            furi_string_set(app->config.fdd[app->selected_fdd].image, new_path);
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, SceneFddInfo);
        } else {
            scene_manager_next_scene(app->scene_manager, SceneError);
        }
    } else if(result == DialogExResultLeft) {
        scene_manager_previous_scene(app->scene_manager);
    }
}

void scene_fdd_new_disk_on_enter(void* context) {
    App* app = (App*)context;

    const char* filename = app_build_unique_file_name(app);

    if(strrchr(filename, '/') != NULL) {
        filename = strrchr(filename, '/') + 1;
    }

    FuriString* temp = furi_string_alloc_printf("Create a new file\n%s ?", filename);

    const char* message = furi_string_get_cstr(temp);
    const char* title = "Insert empty disk?";

    dialog_ex_set_header(app->dialog, title, 64, 0, AlignCenter, AlignTop);
    dialog_ex_set_text(app->dialog, message, 64, 24, AlignCenter, AlignTop);
    dialog_ex_set_left_button_text(app->dialog, "No");
    dialog_ex_set_right_button_text(app->dialog, "Yes");

    dialog_ex_set_context(app->dialog, app);
    dialog_ex_set_result_callback(app->dialog, dialog_callback);

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewDialog);

    furi_string_free(temp);
}

bool scene_fdd_new_disk_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;

    UNUSED(context);
    UNUSED(event);

    return consumed;
}

void scene_fdd_new_disk_on_exit(void* context) {
    App* app = (App*)context;

    dialog_ex_reset(app->dialog);
}
