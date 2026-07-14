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
#include "views/xex_screen.h"

#include "scenes.h"

static void xex_screen_callback(void* context, XexScreenMenuKey key) {
    App* app = (App*)context;

    switch(key) {
    case XexScreenMenuKey_Cancel:
        scene_manager_previous_scene(app->scene_manager);
        break;
    }
}

void scene_xex_loader_on_enter(void* context) {
    App* app = (App*)context;

    xex_screen_set_menu_callback(app->xex_screen, xex_screen_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewXexScreen);

    xex_screen_update_state(app->xex_screen, app->xex_loader);
}

bool scene_xex_loader_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void scene_xex_loader_on_exit(void* context) {
    App* app = (App*)context;

    UNUSED(app);
}
