/*
This file is part of UDECard App.
A Flipper Zero application to analyse student ID cards from the University of Duisburg-Essen (Intercard)

Copyright (C) 2025-2026 Alexander Hahn

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

#include "main_menu_scene.h"

#include "../udecard_app_i.h"

void udecard_main_menu_callback(void* context, uint32_t index) {
    FURI_LOG_I("UDECard", "Main menu callback");
    App* app = context;

    switch(index) {
    case UDECardMainMenuSceneReadEvent:
        view_dispatcher_send_custom_event(app->view_dispatcher, UDECardMainMenuSceneReadEvent);
        break;
    case UDECardMainMenuSceneLoadEvent:
        view_dispatcher_send_custom_event(app->view_dispatcher, UDECardMainMenuSceneLoadEvent);
        break;
    case UDECardMainMenuSceneAboutEvent:
        view_dispatcher_send_custom_event(app->view_dispatcher, UDECardMainMenuSceneAboutEvent);
        break;
    }
}

void udecard_main_menu_scene_on_enter(void* context) {
    App* app = context;

    submenu_reset(app->submenu);

    submenu_set_header(app->submenu, "UDECard");
    submenu_add_item(
        app->submenu, "Read", UDECardMainMenuSceneItemRead, udecard_main_menu_callback, app);
    submenu_add_item(
        app->submenu, "Load", UDECardMainMenuSceneItemLoad, udecard_main_menu_callback, app);
    submenu_add_item(
        app->submenu, "About", UDECardMainMenuSceneItemAbout, udecard_main_menu_callback, app);

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, UDECardMainMenuScene));

    view_dispatcher_switch_to_view(app->view_dispatcher, UDECardSubmenuView);
}

bool udecard_main_menu_scene_on_event(void* context, SceneManagerEvent event) {
    FURI_LOG_I("UDECard", "Main menu event.");
    App* app = context;

    bool consumed = false;

    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        case UDECardMainMenuSceneReadEvent:
            scene_manager_set_scene_state(
                app->scene_manager, UDECardMainMenuScene, UDECardMainMenuSceneItemRead);
            scene_manager_next_scene(app->scene_manager, UDECardDetectScene);
            consumed = true;
            break;
        case UDECardMainMenuSceneLoadEvent:
            scene_manager_set_scene_state(
                app->scene_manager, UDECardMainMenuScene, UDECardMainMenuSceneItemLoad);
            scene_manager_next_scene(app->scene_manager, UDECardLoadScene);
            consumed = true;
            break;
        case UDECardMainMenuSceneAboutEvent:
            scene_manager_set_scene_state(
                app->scene_manager, UDECardMainMenuScene, UDECardMainMenuSceneItemAbout);
            scene_manager_next_scene(app->scene_manager, UDECardAboutScene);
            consumed = true;
            break;
        }
        break;
    default:
        break;
    }

    return consumed;
}

void udecard_main_menu_scene_on_exit(void* context) {
    App* app = context;

    submenu_reset(app->submenu);
}
