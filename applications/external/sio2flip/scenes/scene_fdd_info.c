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

static void fdd_screen_callback(void* context, FddScreenMenuKey key) {
    App* app = (App*)context;

    switch(key) {
    case FddScreenMenuKey_Config:
        scene_manager_next_scene(app->scene_manager, SceneFddSettings);
        break;

    case FddScreenMenuKey_Prev:
        app->selected_fdd = (app->selected_fdd + FDD_EMULATOR_COUNT - 1) % FDD_EMULATOR_COUNT;
        scene_manager_search_and_switch_to_another_scene(app->scene_manager, SceneFddInfo);
        break;

    case FddScreenMenuKey_Next:
        app->selected_fdd = (app->selected_fdd + 1) % FDD_EMULATOR_COUNT;
        scene_manager_search_and_switch_to_another_scene(app->scene_manager, SceneFddInfo);
        break;
    }
}

void scene_fdd_info_on_enter(void* context) {
    App* app = (App*)context;

    fdd_screen_set_menu_callback(app->fdd_screen, fdd_screen_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewFddScreen);

    fdd_screen_update_state(app->fdd_screen, app->fdd[app->selected_fdd]);
}

bool scene_fdd_info_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void scene_fdd_info_on_exit(void* context) {
    App* app = (App*)context;

    UNUSED(app);
}
