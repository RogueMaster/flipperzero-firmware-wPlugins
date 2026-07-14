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

#include "datalog_screen.h"
#include "utils.h"

#include <gui/elements.h>

struct DatalogScreen {
    View* view;
    DatalogScreenButtonCallback button_callback;
    void* callback_context;
};

typedef struct {
    bool running;
    char file_name[64];
    uint32_t record_count;
    uint32_t duration;
    uint32_t file_size;

} DatalogModel;

static void datalog_screen_draw_callback(Canvas* canvas, void* _model) {
    DatalogModel* model = (DatalogModel*)_model;
    furi_check(model);

    FuriString* text = furi_string_alloc();

    // Top bar
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 13);

    // Left and right borders
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, 0, 0, 0, 64);
    canvas_draw_line(canvas, 127, 0, 127, 64);

    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);

    canvas_draw_str(canvas, 2, 10, model->running ? "Recording..." : "Recording stopped");

    if(model->record_count > 0) {
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignBottom, model->file_name);

        if(model->duration < 60) {
            furi_string_printf(text, "%lds", model->duration);
        } else if(model->duration < 3600) {
            furi_string_printf(text, "%02ld:%02ld", model->duration / 60, model->duration % 60);
        } else {
            furi_string_printf(
                text,
                "%02ld:%02ld:%02ld",
                model->duration / 3600,
                (model->duration / 60) % 60,
                model->duration % 60);
        }

        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas, 40, 44, AlignCenter, AlignBottom, furi_string_get_cstr(text));

        if(model->file_size < 1024 * 1024) {
            furi_string_printf(text, "%ldKB", (model->file_size + 512) / 1024);
        } else {
            furi_string_printf(
                text,
                "%.1fMB",
                (double)(model->file_size + (1024 + 1024) / 20) / (double)(1024.0 * 1024.0));
        }

        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas, 88, 44, AlignCenter, AlignBottom, furi_string_get_cstr(text));
    } else {
        if(!model->running) {
            canvas_set_color(canvas, ColorBlack);
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(
                canvas, 64, 36, AlignCenter, AlignBottom, "Press OK to start recording");
        } else {
            canvas_set_color(canvas, ColorBlack);
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignBottom, "No records yet");
        }
    }
    // Buttons
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    const char* button_text = "Stop";

    if(!model->running) {
        if(model->record_count == 0) {
            button_text = "Start";
        } else {
            button_text = "Start new";
        }
    } else {
        button_text = "Stop";
    }

    elements_button_center(canvas, button_text);

    furi_string_free(text);
}

static void invoke_button_callback(DatalogScreen* screen, DatalogScreenButton button) {
    if(screen->button_callback) {
        screen->button_callback(screen->callback_context, button);
    }
}

bool datalog_screen_input_callback(InputEvent* event, void* context) {
    DatalogScreen* screen = (DatalogScreen*)context;
    furi_check(screen != NULL);

    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        invoke_button_callback(screen, DatalogScreenButton_StartStop);
        return true;
    }

    return false;
}

DatalogScreen* datalog_screen_alloc(void) {
    DatalogScreen* screen = (DatalogScreen*)malloc(sizeof(DatalogScreen));

    screen->view = view_alloc();
    view_set_context(screen->view, screen);
    view_allocate_model(screen->view, ViewModelTypeLocking, sizeof(DatalogModel));
    view_set_draw_callback(screen->view, datalog_screen_draw_callback);
    view_set_input_callback(screen->view, datalog_screen_input_callback);

    return screen;
}

View* datalog_screen_get_view(DatalogScreen* screen) {
    furi_check(screen != NULL);

    return screen->view;
}

void datalog_screen_free(DatalogScreen* screen) {
    furi_check(screen != NULL);

    view_free_model(screen->view);
    view_free(screen->view);
    free(screen);
}

void datalog_screen_update(DatalogScreen* screen, Datalog* log) {
    with_view_model(
        screen->view,
        DatalogModel * model,
        {
            if(log != NULL) {
                model->running = true;

                const char* filename = furi_string_get_cstr(datalog_get_file_name(log));
                if(strrchr(filename, '/') != NULL) {
                    filename = strrchr(filename, '/') + 1;
                }
                strncpy(model->file_name, filename, sizeof(model->file_name) - 1);
                model->file_name[sizeof(model->file_name) - 1] = '\0';

                model->record_count = datalog_get_record_count(log);
                model->duration = datalog_get_duration(log);
                model->file_size = datalog_get_file_size(log);
            } else {
                model->running = false;
            }
        },
        true);
}

void datalog_screen_set_button_callback(
    DatalogScreen* screen,
    DatalogScreenButtonCallback callback,
    void* context) {
    furi_check(screen != NULL);

    screen->button_callback = callback;
    screen->callback_context = context;
}
