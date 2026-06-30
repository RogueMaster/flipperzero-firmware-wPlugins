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

static void scene_gauge_button_callback(void* context, CurrentGaugeButton button) {
    App* app = (App*)context;

    if(button == CurrentGaugeButton_Menu) {
        scene_manager_next_scene(app->scene_manager, SceneSettings);
    } else if(button == CurrentGaugeButton_DataLog) {
        scene_manager_next_scene(app->scene_manager, SceneDatalog);
    } else if(button == CurrentGaugeButton_ShuntSwitch) {
        scene_manager_next_scene(app->scene_manager, SceneShuntSwitch);
    }
}

void scene_gauge_on_enter(void* context) {
    App* app = (App*)context;

    current_gauge_set_button_callback(app->current_gauge, scene_gauge_button_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewCurrentGauge);
}

bool scene_gauge_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;

    App* app = context;

    if(event.type == SceneManagerEventTypeTick) {
        if(app->sensor != NULL) {
            SensorState state;
            app->sensor->get_state(app->sensor, &state);
            current_gauge_update(app->current_gauge, &state);
        }
    }

    return consumed;
}

void scene_gauge_on_exit(void* context) {
    App* app = (App*)context;

    UNUSED(app);
}
