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

void unitemp_scene_about_on_enter(void* context) {
    UnitempApp* app = context;

    widget_add_frame_element(app->widget, 0, 0, 128, 63, 7);
    widget_add_frame_element(app->widget, 0, 0, 128, 64, 7);

    FuriString* temp_str = furi_string_alloc();
    furi_string_printf(temp_str, "\e#\e! Unitemp %s \e!\n", UNITEMP_APP_VER);
    widget_add_text_box_element(
        app->widget, 0, 4, 128, 12, AlignCenter, AlignCenter, furi_string_get_cstr(temp_str), false);

    widget_add_text_scroll_element(
        app->widget,
        4,
        16,
        121,
        44,
        "Universal plugin for viewing the values of temperature\nsensors\n\e#Author: Quenon\ngithub.com/quen0n\n\e#Designer: Svaarich\ngithub.com/Svaarich\n\e#Issues & suggestions\ntiny.one/unitemp\n\e#Special thanks\nxMasterX\nvladin79\ndivinebird\njamisonderek\nkaklik\n...and everyone who helped \nwith development and \ntesting");
    furi_string_free(temp_str);

    view_dispatcher_switch_to_view(app->view_dispatcher, UnitempViewWidget);
}

bool unitemp_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    bool consumed = false;
    return consumed;
}

void unitemp_scene_about_on_exit(void* context) {
    UnitempApp* app = context;
    widget_reset(app->widget);
}
