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

static void error_dialog_callback(DialogExResult result, void* context) {
    App* app = (App*)context;

    UNUSED(result);

    scene_manager_previous_scene(app->scene_manager);
}

void scene_error_on_enter(void* context) {
    App* app = (App*)context;

    const char* error_title = furi_string_get_cstr(get_last_error_title());
    const char* error_message = furi_string_get_cstr(get_last_error_message());

    error_title = error_title ? error_title : "Error";
    error_message = error_message ? error_message : "Unknown error";

    dialog_ex_set_header(app->dialog, error_title, 64, 0, AlignCenter, AlignTop);
    dialog_ex_set_text(app->dialog, error_message, 64, 24, AlignCenter, AlignTop);
    dialog_ex_set_center_button_text(app->dialog, "OK");

    dialog_ex_set_context(app->dialog, app);
    dialog_ex_set_result_callback(app->dialog, error_dialog_callback);

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewDialog);
}

bool scene_error_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;

    UNUSED(context);
    UNUSED(event);

    return consumed;
}

void scene_error_on_exit(void* context) {
    App* app = (App*)context;

    dialog_ex_reset(app->dialog);
}
