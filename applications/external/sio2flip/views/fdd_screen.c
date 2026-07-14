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

#include "fdd_screen.h"

struct FddScreen {
    View* view;
    FddScreenMenuCallback menu_callback;
    void* callback_context;
};

typedef struct {
    SIODevice device;
    char image_name[64];
    size_t disk_size;
    size_t sector_size;
    bool write_protect;
    uint16_t sector;
} FddModel;

static void fdd_screen_draw_callback(Canvas* canvas, void* _model) {
    FddModel* model = (FddModel*)_model;
    furi_check(model);
    int x, y;

    FuriString* text = furi_string_alloc();

    // Top bar
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 13);

    // Top bar - FDD device, FDD type
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontKeyboard);
    furi_string_printf(text, "D%c:", model->device);
    canvas_draw_str(canvas, 2, 10, furi_string_get_cstr(text));
    if(model->write_protect) {
        canvas_draw_str(canvas, 26, 10, "WR PROTECT");
    }
    canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignBottom, "FDD");

    // FDD device
    x = 8;
    y = 27;
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, x, y, 16, 20, 3);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontBigNumbers);
    furi_string_printf(text, "%c", model->device);
    canvas_draw_str(canvas, x + 3, y + 17, furi_string_get_cstr(text));

    // Image name
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    if(model->image_name[0] == '\0') {
        canvas_draw_str(canvas, 3, 22, "No disk inserted");
    } else {
        canvas_draw_str(canvas, 3, 22, model->image_name);
    }

    // Disk capacity & sector length
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    if(model->disk_size >= 5 * 1024 * 1024) {
        furi_string_printf(text, "%.2gMB", (double)model->disk_size / (1024 * 1024));
    } else {
        furi_string_printf(text, "%dKB", model->disk_size / 1024);
    }
    canvas_draw_str(canvas, 28, 36, furi_string_get_cstr(text));
    furi_string_printf(text, "%dB", model->sector_size);
    canvas_draw_str(canvas, 28, 46, furi_string_get_cstr(text));

    // FDD sector index
    if(model->sector == 0) {
        furi_string_printf(text, "----");
    } else if(model->sector < 10000) {
        furi_string_printf(text, "%04u", model->sector);
    } else {
        furi_string_printf(text, "%05u", model->sector);
    }

    canvas_set_color(canvas, ColorBlack);
    if(furi_string_size(text) <= 4) { // 4 and less digits
        x = 66;
        y = 27;
        canvas_draw_rframe(canvas, x, y, 52, 20, 3);
    } else { // 5 digits
        x = 60;
        y = 27;
        canvas_draw_rframe(canvas, x, y, 64, 20, 3);
    }
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str(canvas, x + 3, y + 17, furi_string_get_cstr(text));

    // Left and right borders
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, 0, 0, 0, 64);
    canvas_draw_line(canvas, 127, 0, 127, 64);

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    elements_button_left(canvas, "Prev");

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    elements_button_center(canvas, "Config");

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    elements_button_right(canvas, "Next");

    furi_string_free(text);
}

bool fdd_screen_input_callback(InputEvent* event, void* context) {
    FddScreen* screen = (FddScreen*)context;
    furi_check(screen != NULL);

    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(screen->menu_callback) {
            screen->menu_callback(screen->callback_context, FddScreenMenuKey_Config);
        }
        return true;
    } else if(event->type == InputTypeShort && event->key == InputKeyLeft) {
        if(screen->menu_callback) {
            screen->menu_callback(screen->callback_context, FddScreenMenuKey_Prev);
        }
        return true;
    } else if(event->type == InputTypeShort && event->key == InputKeyRight) {
        if(screen->menu_callback) {
            screen->menu_callback(screen->callback_context, FddScreenMenuKey_Next);
        }
        return true;
    }

    return false;
}

FddScreen* fdd_screen_alloc(void) {
    FddScreen* screen = (FddScreen*)malloc(sizeof(FddScreen));

    memset(screen, 0, sizeof(FddScreen));

    screen->view = view_alloc();
    view_set_context(screen->view, screen);
    view_allocate_model(screen->view, ViewModelTypeLocking, sizeof(FddModel));
    view_set_draw_callback(screen->view, fdd_screen_draw_callback);
    view_set_input_callback(screen->view, fdd_screen_input_callback);

    return screen;
}

View* fdd_screen_get_view(FddScreen* screen) {
    furi_check(screen != NULL);

    return screen->view;
}

void fdd_screen_free(FddScreen* screen) {
    furi_check(screen != NULL);

    view_free_model(screen->view);
    view_free(screen->view);
    free(screen);
}

void fdd_screen_update_state(FddScreen* screen, FddEmulator* emu) {
    furi_check(screen != NULL);

    with_view_model(
        screen->view,
        FddModel * model,
        {
            model->device = fdd_get_device(emu);

            DiskImage* image = fdd_get_disk(emu);
            if(image != NULL) {
                model->disk_size = disk_image_size(image);
                model->sector_size = disk_image_sector_size(image);
                model->write_protect = disk_image_get_write_protect(image);
                model->sector = fdd_get_last_sector(emu);

                const char* filename = furi_string_get_cstr(disk_image_path(image));
                if(strrchr(filename, '/') != NULL) {
                    filename = strrchr(filename, '/') + 1;
                }
                strncpy(model->image_name, filename, sizeof(model->image_name) - 1);
                model->image_name[sizeof(model->image_name) - 1] = '\0';
            } else {
                model->disk_size = 0;
                model->sector_size = 0;
                model->sector = 0;
                model->write_protect = false;
                model->image_name[0] = '\0';
            }
        },
        true);
}

void fdd_screen_update_activity(FddScreen* screen, FddActivity activity, uint16_t sector) {
    furi_check(screen != NULL);

    UNUSED(activity);

    with_view_model(screen->view, FddModel * model, { model->sector = sector; }, true);
}

void fdd_screen_set_menu_callback(FddScreen* screen, FddScreenMenuCallback callback, void* context) {
    furi_check(screen != NULL);

    screen->menu_callback = callback;
    screen->callback_context = context;
}
