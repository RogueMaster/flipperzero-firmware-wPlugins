/* Copyright (C) 2022-2023 Salvatore Sanfilippo -- All Rights Reserved
 * See the LICENSE file for information about the license.
 *
 * File added by Mascot68 to facilitate historical TPMS data.
 * Based on view_info.c
 */

#include "app.h"
#include <gui/view.h>
#include <storage/storage.h>
#include <toolbox/stream/stream.h>
#include <toolbox/stream/file_stream.h>

int curr_page = 1; // Keeps track when scrolling pages
char mytyres[16][12]; // Up to 16 tires read from file
bool fileread = false; // Used to only read file once
bool readfailed = false; // Retrying caused crashes, so using flag to only try once
bool showonlymine = false;
bool show_in_bar = false;

#define LINES_PER_PAGE 6

static void update_favorite_count(ProtoViewApp* app) {
    int i = 0;
    int ityre = 0;
    int temp = 0;
    for(i = 0; i < app->tyre_list_count; i++) {
        if(!app->tyre_list[i].favorite_set && fileread) {
            for(ityre = 0; ityre < 16; ityre++) {
                if(strcmp(mytyres[ityre], app->tyre_list[i].serial) == 0) {
                    app->tyre_list[i].favorite = true;
                    temp++;
                }
                app->tyre_list[i].favorite_set = true;
            }
        }
        app->favorites_count = temp;
        app->dirty = false;
    }
}

static void render_tyre_info(Canvas* const canvas, ProtoViewApp* app) {
    uint8_t x = 0, y = 8, lineheight = 10;
    char buf[64];
    int i;
    int i2;
    int firstrow;

    if(app->dirty) {
        update_favorite_count(app);
    }

    firstrow = (LINES_PER_PAGE * curr_page) - LINES_PER_PAGE;
    i2 = 0;
    for(i = firstrow; i < app->tyre_list_count; i++) {
        canvas_set_font(canvas, FontSecondary);
        if(app->tyre_list[i].favorite) {
            if(show_in_bar) {
                snprintf(
                    buf,
                    sizeof(buf),
                    "*: %s %s: %s %sC",
                    app->tyre_list[i].serial,
                    "Bar",
                    app->tyre_list[i].pressure_bar,
                    app->tyre_list[i].temperature);
            } else {
                snprintf(
                    buf,
                    sizeof(buf),
                    "*: %s %s: %s %sC",
                    app->tyre_list[i].serial,
                    app->tyre_list[i].uom,
                    app->tyre_list[i].pressure,
                    app->tyre_list[i].temperature);
            }
            canvas_draw_str(canvas, x, y, buf);
            y += lineheight;
            i2++;
        } else if(!showonlymine) {
            if(show_in_bar) {
                snprintf(
                    buf,
                    sizeof(buf),
                    "%s: %s %s: %s %sC",
                    app->tyre_list[i].decoder,
                    app->tyre_list[i].serial,
                    "Bar",
                    app->tyre_list[i].pressure_bar,
                    app->tyre_list[i].temperature);
            } else {
                snprintf(
                    buf,
                    sizeof(buf),
                    "%s: %s %s: %s %sC",
                    app->tyre_list[i].decoder,
                    app->tyre_list[i].serial,
                    app->tyre_list[i].uom,
                    app->tyre_list[i].pressure,
                    app->tyre_list[i].temperature);
            }
            canvas_draw_str(canvas, x, y, buf);
            y += lineheight;
            i2++;
        }

        if(i2 == 7) {
            break; // Screen fits 6.5 rows, 7th row will be shown half drawn at the bottom as "there is another screen" indicator
        }
    }
}

void read_file(ProtoViewApp* app) {
    if(fileread) {
        return;
    }
    char buf[220];
    char temp[12] = "";
    size_t size_read;
    uint8_t i;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(!storage_file_open(file, APP_DATA_PATH("tyrelist.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Failed to open file");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        readfailed = true;
        return;
    }
    size_read = storage_file_read(file, buf, 220);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    int i2 = 0;
    int tyreindex = 0;
    for(i = 0; i < size_read; i++) {
        if(tyreindex == 16) {
            break;
        }
        if(buf[i] == ';') {
            temp[i2] = '\0';
            strcpy(mytyres[tyreindex], temp);
            tyreindex++;
            break;
        } else if(buf[i] == ',') {
            temp[strlen(temp)] = '\0';
            i2 = 0;
            strcpy(mytyres[tyreindex], temp);
            tyreindex++;
        } else {
            temp[i2] = buf[i];
            i2++;
        }
    }
    FURI_LOG_I(TAG, "Tyre file read");
    fileread = true;
    app->dirty = true;
}

static void render_info_list_main(Canvas* const canvas, ProtoViewApp* app) {
    canvas_set_font(canvas, FontPrimary);
    uint8_t y = 8, lineheight = 10;
    y += lineheight;
    if(app->tyre_list_count > 0) {
        render_tyre_info(canvas, app);
    }
}

void render_list_view_info(Canvas* const canvas, ProtoViewApp* app) {
    if(app->tyre_list_count == 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 10, 26, "Tyre list will appear here");
        canvas_draw_str(canvas, 18, 36, "Long OK to clear list");
        if(!fileread && !readfailed) {
            canvas_draw_str(canvas, 17, 46, "Short OK to read file");
        } else {
            if(readfailed) {
                canvas_draw_str(canvas, 26, 46, "(File read failed)");
            } else {
                canvas_draw_str(canvas, 34, 46, "(File read OK)");
            }
        }
    } else if(!fileread && showonlymine) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 10, 26, "Showing only favorites");
        canvas_draw_str(canvas, 16, 36, "Long UP to show all");
    } else {
        render_info_list_main(canvas, app);
    }
}

void clear_tyre_list(ProtoViewApp* app) {
    int i;
    if(app->tyre_list_count > 0) {
        for(i = 0; i < app->tyre_list_count; i++) {
            strcpy(app->tyre_list[i].serial, "");
            strcpy(app->tyre_list[i].pressure, "");
            strcpy(app->tyre_list[i].temperature, "");
            strcpy(app->tyre_list[i].uom, "");
            app->tyre_list[i].favorite = false;
            app->tyre_list[i].favorite_set = false;
        }
        app->tyre_list_count = 0;
        curr_page = 1;
    }
}

/* Handle input for the info view. */

void tyre_list_process_input(ProtoViewApp* app, InputEvent input) {
    UNUSED(app);
    if(input.type == InputTypeShort && input.key == InputKeyDown) {
        if(app->tyre_list_count > curr_page * LINES_PER_PAGE) {
            curr_page++;
        }
    } else if(input.type == InputTypeShort && input.key == InputKeyUp) {
        if(curr_page > 1) {
            curr_page--;
        }
    } else if(input.type == InputTypeLong && input.key == InputKeyUp) {
        if(!showonlymine) {
            showonlymine = true;
            curr_page = 1;
            ui_show_alert(app, "Showing only favorites", 1000);
        } else {
            showonlymine = false;
            curr_page = 1;
            ui_show_alert(app, "Showing all", 1000);
        }
    } else if(input.type == InputTypeShort && input.key == InputKeyOk) {
        if(fileread || readfailed) {
            ui_show_alert(app, "Long press to clear list", 1000);
        }
        if(!readfailed && !fileread) {
            read_file(app);
            if(!readfailed) {
                ui_show_alert(app, "File read OK", 1000);
            } else {
                ui_show_alert(app, "File read failed", 1000);
            }
        }
    } else if(input.type == InputTypeLong && input.key == InputKeyOk) {
        if(app->tyre_list_count > 0) {
            clear_tyre_list(app);
            ui_show_alert(app, "Tyre list cleared", 1000);
        }
    } else if(input.type == InputTypeLong && input.key == InputKeyDown) {
        if(!show_in_bar) {
            show_in_bar = true;
        } else {
            show_in_bar = false;
        }
    }
}

/* Called on view exit. */
void info_list_view_exit_info(ProtoViewApp* app) {
    UNUSED(app);
    // InfoViewPrivData* privdata = app->view_privdata;
    // When the user aborts the keyboard input, we are left with the
    // filename buffer allocated.
    // if(privdata->filename) free(privdata->filename);
}
