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

#include <gui/elements.h>

#include "app/app_common.h"

#include "xex_screen.h"

struct XexScreen {
    View* view;
    XexScreenMenuCallback menu_callback;
    void* callback_context;
};

typedef struct {
    char file_name[64];
    size_t file_size;
    size_t file_offset;
} XexModel;

static void xex_screen_draw_callback(Canvas* canvas, void* _model) {
    XexModel* model = (XexModel*)_model;
    furi_check(model);

    FuriString* text = furi_string_alloc();

    // Title background
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 13);

    // Title text
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str(canvas, 2, 10, "XEX File Loader");

    // File name
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 3, 22, model->file_name);

    // File size and progress in percent
    int pct = (model->file_offset * 100) / model->file_size;
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    furi_string_printf(text, "%d%% of %dKB", pct, model->file_size / 1024);
    canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignBottom, furi_string_get_cstr(text));

    // Progress bar
    int progress = (model->file_offset * 120) / model->file_size;
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, 4, 30, 120, 6);
    canvas_draw_box(canvas, 4, 30, progress, 6);

    // Left and right borders
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, 0, 0, 0, 64);
    canvas_draw_line(canvas, 127, 0, 127, 64);

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    elements_button_left(canvas, "Cancel");

    furi_string_free(text);
}

bool xex_screen_input_callback(InputEvent* event, void* context) {
    XexScreen* screen = (XexScreen*)context;
    furi_check(screen != NULL);

    UNUSED(event);
    UNUSED(context);

    if(event->type == InputTypeShort && event->key == InputKeyLeft) {
        if(screen->menu_callback) {
            screen->menu_callback(screen->callback_context, XexScreenMenuKey_Cancel);
        }

        return true;
    }

    return false;
}

XexScreen* xex_screen_alloc(void) {
    XexScreen* screen = (XexScreen*)malloc(sizeof(XexScreen));

    memset(screen, 0, sizeof(XexScreen));

    screen->view = view_alloc();
    view_set_context(screen->view, screen);
    view_allocate_model(screen->view, ViewModelTypeLocking, sizeof(XexModel));
    view_set_draw_callback(screen->view, xex_screen_draw_callback);
    view_set_input_callback(screen->view, xex_screen_input_callback);

    return screen;
}

View* xex_screen_get_view(XexScreen* screen) {
    furi_check(screen != NULL);

    return screen->view;
}

void xex_screen_free(XexScreen* screen) {
    if(screen != NULL) {
        view_free_model(screen->view);
        view_free(screen->view);
        free(screen);
    }
}

void xex_screen_update_state(XexScreen* screen, XexLoader* loader) {
    furi_check(screen != NULL);

    with_view_model(
        screen->view,
        XexModel * model,
        {
            XexFile* xex = xex_loader_file(loader);

            model->file_size = xex ? xex_file_size(xex) : 0;
            model->file_offset = xex ? xex_loader_last_offset(loader) : 0;

            const char* filename = xex_file_name(xex);
            if(strrchr(filename, '/') != NULL) {
                filename = strrchr(filename, '/') + 1;
            }
            strncpy(model->file_name, filename, sizeof(model->file_name) - 1);
            model->file_name[sizeof(model->file_name) - 1] = '\0';
        },
        true);
}

void xex_screen_set_menu_callback(XexScreen* screen, XexScreenMenuCallback callback, void* context) {
    furi_check(screen != NULL);

    screen->menu_callback = callback;
    screen->callback_context = context;
}
