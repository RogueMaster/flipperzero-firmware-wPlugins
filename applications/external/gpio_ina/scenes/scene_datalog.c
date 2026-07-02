/* 
 * This file is part of the INA Meter application for Flipper Zero (https://github.com/cepetr/flipper-tina).
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

#include "app.h"
#include "scenes/scenes.h"

static void scene_datalog_button_callback(void* context, DatalogScreenButton button) {
    App* app = (App*)context;

    if(button == DatalogScreenButton_StartStop) {
        if(app->datalog != NULL) {
            scene_manager_next_scene(app->scene_manager, SceneDialogStop);
        } else {
            FuriString* file_name = datalog_build_unique_file_name();
            app->datalog = datalog_open(app->storage, furi_string_get_cstr(file_name));
            furi_string_free(file_name);
            datalog_screen_update(app->datalog_screen, app->datalog);
        }
    }
}

void scene_datalog_on_enter(void* context) {
    App* app = (App*)context;

    datalog_screen_set_button_callback(app->datalog_screen, scene_datalog_button_callback, app);
    datalog_screen_update(app->datalog_screen, app->datalog);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewDatalog);
}

bool scene_datalog_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;

    App* app = context;

    if(event.type == SceneManagerEventTypeTick) {
        if(app->datalog != NULL) {
            // app->sensor->get_state(app->sensor, &state);
            //datalog_view_update(app->datalog_view, &state);
        }
    }

    return consumed;
}

void scene_datalog_on_exit(void* context) {
    App* app = (App*)context;

    UNUSED(app);
}
