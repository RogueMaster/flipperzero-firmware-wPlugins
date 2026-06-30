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
#include "view_no_sensors.h"
#include "../unitemp.h"

#include <gui/elements.h>

#include "unitemp_icons.h"

struct NoSensors {
    View* view;
    void* context;
};

static void no_sensors_draw_callback(Canvas* canvas, void* model) {
    UNUSED(model);
    canvas_draw_icon(canvas, 7, 17, &I_sherlok_53x45);
    //Drawing a frame
    canvas_draw_rframe(canvas, 0, 0, 128, 63, 7);
    canvas_draw_rframe(canvas, 0, 0, 128, 64, 7);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 63, 10, AlignCenter, AlignCenter, "No sensors yet");
    canvas_set_font(canvas, FontSecondary);
    const uint8_t x = 65, y = 32;
    canvas_draw_rframe(canvas, x - 4, y - 10, 54, 33, 3);
    canvas_draw_rframe(canvas, x - 4, y - 10, 54, 34, 3);
    canvas_draw_str(canvas, x, y, "Press OK");
    canvas_draw_icon(canvas, x + 38, y - 8, &I_Ok_btn_9x9);
    canvas_draw_str(canvas, x, y + 9, "to add a");
    canvas_draw_str(canvas, x, y + 18, "new sensor");
}

static bool no_sensors_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    NoSensors* no_sensors = context;
    UnitempApp* app = no_sensors->context;
    bool consumed = false;

    if(event->key == InputKeyOk && event->type == InputTypeShort) {
        scene_manager_next_scene(app->scene_manager, UnitempSceneMenu);
        consumed = true;
    }

    return consumed;
}

NoSensors* no_sensors_alloc(void* context) {
    UnitempApp* app = context;
    NoSensors* no_sensors = malloc(sizeof(NoSensors));

    no_sensors->view = view_alloc();
    no_sensors->context = app;

    view_set_context(no_sensors->view, no_sensors);
    view_set_draw_callback(no_sensors->view, no_sensors_draw_callback);
    view_set_input_callback(no_sensors->view, no_sensors_input_callback);

    return no_sensors;
}

void no_sensors_free(NoSensors* no_sensors) {
    furi_assert(no_sensors);
    view_free(no_sensors->view);
    free(no_sensors);
}

View* no_sensors_get_view(NoSensors* no_sensors) {
    furi_assert(no_sensors);
    return no_sensors->view;
}
