/*
    Unitemp - Universal temperature reader
    Copyright (C) 2022-2026  Victor Nikitchuk (https://github.com/quen0n)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "../unitemp.h"
#include "scenes/unitemp_scene.h"

enum SubmenuIndex {
    SubmenuIndexAddNewSensor,
    SubmenuIndexSettings,
    SubmenuIndexHelp,
    SubmenuIndexAbout,
};

void unitemp_scene_menu_on_enter(void* context) {
    UnitempApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_add_item(
        submenu, "Add a new sensor", SubmenuIndexAddNewSensor, unitemp_submenu_callback, app);
    submenu_add_item(submenu, "Settings", SubmenuIndexSettings, unitemp_submenu_callback, app);
    submenu_add_item(submenu, "Help", SubmenuIndexHelp, unitemp_submenu_callback, app);
    submenu_add_item(submenu, "About", SubmenuIndexAbout, unitemp_submenu_callback, app);

    submenu_set_selected_item(app->submenu, 0);

    view_dispatcher_switch_to_view(app->view_dispatcher, UnitempViewSubmenu);
}

bool unitemp_scene_menu_on_event(void* context, SceneManagerEvent event) {
    UnitempApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, UnitempSceneMenu, event.event);
        consumed = true;
        if(event.event == SubmenuIndexAddNewSensor) {
            scene_manager_next_scene(app->scene_manager, UnitempSceneSensorsList);
        } else if(event.event == SubmenuIndexSettings) {
            scene_manager_next_scene(app->scene_manager, UnitempSceneSettings);
        } else if(event.event == SubmenuIndexHelp) {
            scene_manager_next_scene(app->scene_manager, UnitempSceneHelp);
        } else if(event.event == SubmenuIndexAbout) {
            scene_manager_next_scene(app->scene_manager, UnitempSceneAbout);
        }
    }

    return consumed;
}

void unitemp_scene_menu_on_exit(void* context) {
    UnitempApp* app = context;
    submenu_reset(app->submenu);
    submenu_set_selected_item(app->submenu, 0);
}
