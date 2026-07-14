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

#include <assets_icons.h>
#include "unitemp_icons.h"

void unitemp_scene_help_on_enter(void* context) {
    UnitempApp* app = context;

    widget_add_icon_element(app->widget, 3, 7, &I_repo_qr_50x50);
    widget_add_icon_element(app->widget, 80, 18, &I_WarningDolphinFlip_45x42);

    widget_add_string_multiline_element(
        app->widget, 55, 5, AlignLeft, AlignTop, FontSecondary, "You can find help\nthere");

    widget_add_frame_element(app->widget, 0, 0, 128, 63, 7);
    widget_add_frame_element(app->widget, 0, 0, 128, 64, 7);

    view_dispatcher_switch_to_view(app->view_dispatcher, UnitempViewWidget);
}

bool unitemp_scene_help_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    bool consumed = false;
    return consumed;
}

void unitemp_scene_help_on_exit(void* context) {
    UnitempApp* app = context;
    widget_reset(app->widget);
}
