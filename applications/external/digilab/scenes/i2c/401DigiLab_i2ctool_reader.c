/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#include "scenes/i2c/401DigiLab_i2ctool_reader.h"
static const char* TAG = "401_DigiLabI2CToolReader";

#include <401_gui.h>
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <furi_hal_rtc.h>
#include <gui/elements.h>
#include <gui/gui.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <storage/storage.h>
#include <toolbox/path.h>
////#include <u8g2_glue.h>

#include <401_err.h>
#include "cJSON/cJSON.h"

#define CELL_WIDTH       4
#define LINE_HEIGHT      6
#define HEXDUMP_OFFSET_W 1
#define HEXDUMP_OFFSET_H 13

static uint8_t input_read_offset = 0;

/**
 * @brief Transform a xy coordiante on the grid into a xtyt coordiante on the screen
 *
 * @param x Position x on the grid
 * @param y Position y on the grid
 * @param xt pointer to the transormed x position
 * @param yt pointer to the transormed y position
 */
static void hex_dump_grid_transform(uint8_t x, uint8_t y, uint8_t* xt, uint8_t* yt) {
    uint8_t space = 18;
    if(x < 18) {
        space = (x / 2) * 2;
    }
    *xt = HEXDUMP_OFFSET_W + x * CELL_WIDTH + space;
    *yt = HEXDUMP_OFFSET_H + y * LINE_HEIGHT;
}

/**
 * @brief Draws a single character based on it's coordinate
 *
 * @param canvas Canvas to be rendered to
 * @param cell_x Position x on the grid
 * @param cell_y Position y on the grid
 * @param c byte to be shown
 */
void hex_dump_grid_draw(Canvas* canvas, int cell_x, int cell_y, char c) {
    char str[2] = {c, '\0'}; // Convertir le caractère en chaîne de 1 caractère
    uint8_t x = 0;
    uint8_t y = 0;
    hex_dump_grid_transform(cell_x, cell_y, &x, &y);
    // If the octet is on Offset side.
    if(cell_x <= 1) {
        canvas_set_color(canvas, ColorWhite);
    }
    // If the octet is on the Data side.
    else {
        canvas_set_color(canvas, ColorBlack);
    }
    canvas_draw_str_aligned(canvas, x, y, AlignLeft, AlignTop, str);
}

/**
 * @brief Draws a selected octet at offset "offset"
 *
 * @param canvas Canvas to be rendered to
 * @param offset Offset of the byte to be drawn as selected
 */
void hex_dump_grid_draw_selected(Canvas* canvas, uint8_t offset) {
    canvas_set_color(canvas, ColorXOR);
    uint8_t xb = ((offset % 8) + 1) * 2;
    uint8_t yb = offset / 8;
    uint8_t xa = (offset % 8) + 18;
    uint8_t ya = yb;
    uint8_t xt = 0;
    uint8_t yt = 0;

    hex_dump_grid_transform(xb, yb, &xt, &yt);
    canvas_draw_box(canvas, xt - 1, yt - 1, (CELL_WIDTH * 2) + 1, LINE_HEIGHT + 1);
    hex_dump_grid_transform(xa, ya, &xt, &yt);
    canvas_draw_box(canvas, xt - 1, yt - 1, (CELL_WIDTH) + 1, LINE_HEIGHT + 1);
}

/**
 * @brief Draws the hex editor
 *
 * @param canvas Canvas to be rendered to
 * @param ctx I2CTool reader model
 */
void app_i2ctoolreader_render_callback(Canvas* canvas, void* ctx) {
    furi_assert(canvas);
    furi_assert(ctx);

    i2CToolReaderModel* model = (i2CToolReaderModel*)ctx;

    canvas_clear(canvas);

    size_t data_len = model->data_len;
    uint8_t* data = model->data;

    canvas_set_custom_u8g2_font(canvas, CUSTOM_FONT_TINY_REGULAR);
    uint8_t bytes_per_line = 8; // Number of bytes to display per row
    size_t offset = 0; // Current data offset
    int line = 0; // Current row index

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rbox(
        canvas,
        0,
        HEXDUMP_OFFSET_H,
        CELL_WIDTH * 2 + 2,
        LINE_HEIGHT * ((data_len / bytes_per_line) + 1) + 2,
        2);

    // Loop over data in increments of bytes_per_line
    while(offset < data_len) {
        int cell_x = 0;

        // Draw offset marker (two hex digits)
        char offset_str[3];
        snprintf(offset_str, sizeof(offset_str), "%02X", offset);
        hex_dump_grid_draw(canvas, cell_x++, line, offset_str[0]);
        hex_dump_grid_draw(canvas, cell_x++, line, offset_str[1]);

        // Draw HEX part: each byte as two hex digits
        for(size_t i = 0; i < bytes_per_line; i++) {
            if(offset + i < data_len) {
                char byte_str[3];
                snprintf(byte_str, sizeof(byte_str), "%02X", data[offset + i]);
                hex_dump_grid_draw(canvas, cell_x++, line, byte_str[0]);
                hex_dump_grid_draw(canvas, cell_x++, line, byte_str[1]);
            } else {
                // Fill empty cells when data exhausted
                cell_x += 2;
            }
        }

        // Draw ASCII part: printable characters or '.' for non-printable
        for(size_t i = 0; i < bytes_per_line; i++) {
            if(offset + i < data_len) {
                unsigned char byte = data[offset + i];
                char ascii_char = isprint(byte) ? byte : '.';
                hex_dump_grid_draw(canvas, cell_x++, line, ascii_char);
            } else {
                // Skip cell when no data
                cell_x++;
            }
        }

        // Advance to next line
        offset += bytes_per_line;
        line++;
    }

    // Highlight the currently selected byte
    hex_dump_grid_draw_selected(canvas, model->data_offset);

    // Draw title and buttons
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "Hex editor");
    canvas_set_font(canvas, FontSecondary);
    elements_button_up(canvas, "+");
    elements_button_down(canvas, "-");
    elements_button_center(canvas, "Write");
    elements_button_left(canvas, "Offset-");
    elements_button_right(canvas, "Offset+");
}

/**
 * @brief Write on i2C Mems in page_size blocks
 *
 * @param addr I2C address
 * @param offset Offset address
 * @param data Data pointer
 * @param length Total length of data
 * @param page_size Size of pages
 * @return Returns true/false.
 */
bool i2cmem_write(uint8_t addr, uint16_t offset, uint8_t* data, uint8_t length, uint8_t page_size) {
    furi_hal_i2c_acquire(I2C_BUS);
    bool res = true;

    FURI_LOG_I(
        TAG, "Writing %d bytes starting at 0x%04X with %d-byte pages", length, offset, page_size);

    while(length > 0) {
        uint16_t page_start = (offset / page_size) * page_size; // Start of the current page
        uint8_t page_offset = offset % page_size; // Offset within the current page
        uint8_t write_length = (page_size - page_offset < length) ? (page_size - page_offset) :
                                                                    length;

        FURI_LOG_I(
            TAG,
            "Writing %d bytes at 0x%04X (Page start: 0x%04X, Page offset: %d)",
            write_length,
            offset,
            page_start,
            page_offset);

        if(!furi_hal_i2c_write_mem(I2C_BUS, addr, offset, data, write_length, 100)) {
            FURI_LOG_E(TAG, "Write failed at 0x%04X", offset);
            res = false;
        }

        data += write_length; // Advance data pointer
        offset += write_length; // Advance offset
        length -= write_length; // Reduce remaining bytes
    }

    furi_hal_i2c_release(I2C_BUS);
    return res;
}

/**
 * @brief Triggers the "Sucess" popup.
 *
 * @param ctx App's CTX
 */
void app_i2ctoolreader_write_success_popup_callback(void* ctx) {
    AppContext* app = (AppContext*)ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, I2CReaderEventExitPopup);
}

/**
 * @brief Callback function to handle input events for the app i2ctoolreader.
 *
 * @param input_event The input event.
 * @param ctx The context passed to the callback.
 * @return Returns true if the event was handled, otherwise false.
 */
bool app_i2ctoolreader_input_callback(InputEvent* input_event, void* ctx) {
    AppContext* app = (AppContext*)ctx;
    AppI2CToolReader* appI2CToolReader = app->sceneI2CToolReader;
    i2CToolReaderModel* model = view_get_model(appI2CToolReader->view);
    UNUSED(ctx);
    bool consumed = false;
    uint8_t increment = 1;
    if((input_event->type == InputTypeShort) || (input_event->type == InputTypeRepeat)) {
        switch(input_event->key) {
        case InputKeyUp:
            if(input_event->type == InputTypeRepeat) {
                increment = 0x10;
            }
            if(model->data != NULL) {
                model->data[model->data_offset] += increment;
                view_commit_model(appI2CToolReader->view, true);
            }
            break;
        case InputKeyDown:
            if(input_event->type == InputTypeRepeat) {
                increment = 0x10;
            }
            if(model->data != NULL) {
                model->data[model->data_offset] -= increment;
                view_commit_model(appI2CToolReader->view, true);
            }
            break;

        case InputKeyLeft: // Modify dataOffset
            if(input_event->type == InputTypeRepeat) {
                increment = 4;
            }
            if(model->data_offset == 0) {
                model->data_offset = model->data_len - 1;
            } else {
                model->data_offset -= increment;
            }
            view_commit_model(appI2CToolReader->view, true);
            break;

        case InputKeyRight: // Modify dataOffset
            if(input_event->type == InputTypeRepeat) {
                increment = 4;
            }
            if(model->data_offset < model->data_len - 1) {
                model->data_offset += increment;
                if(model->data_offset >= model->data_len - 1) {
                    model->data_offset = model->data_len - 1;
                }
            } else {
                model->data_offset = 0;
            }
            view_commit_model(appI2CToolReader->view, true);
            break;
        case InputKeyOk:
            bool res = false;

            res = i2cmem_write(
                model->device->addr << 1, model->read_offset, model->data, model->data_len, 7);
            if(res == false) {
                FURI_LOG_E(TAG, "Could not write mem");
                popup_set_header(appI2CToolReader->popup, "Error!", 64, 11, AlignCenter, AlignTop);
                popup_set_text(
                    appI2CToolReader->popup,
                    "Couldn't write data to chip",
                    64,
                    22,
                    AlignCenter,
                    AlignTop);
                view_dispatcher_switch_to_view(app->view_dispatcher, AppViewI2CToolWriterPopup);
            } else {
                popup_set_header(
                    appI2CToolReader->popup, "Success!", 64, 11, AlignCenter, AlignTop);
                popup_set_text(
                    appI2CToolReader->popup, "Chip programmed", 64, 22, AlignCenter, AlignTop);
                view_dispatcher_switch_to_view(app->view_dispatcher, AppViewI2CToolWriterPopup);
            }
            break;
        default:
            break;
        }
    }

    return consumed;
}
/**
 * @brief Set the offset to the model when offset's dialog is validated
 *
 * @param ctx App's CTX
 */
static void app_i2ctoolreader_input_readOffset_callback(void* ctx) {
    AppContext* app = (AppContext*)ctx;
    AppI2CToolReader* appI2CToolReader = app->sceneI2CToolReader;
    i2CToolReaderModel* model = view_get_model(appI2CToolReader->view);

    model->read_offset = input_read_offset;
    view_commit_model(appI2CToolReader->view, false);
    view_dispatcher_send_custom_event(app->view_dispatcher, I2CReaderEventByteInput);
}

/**
* @brief Set the len to the model when len's dialog is validated
 *
 * @param ctx App's CTX
 * @param number the offset
 */
static void app_i2ctoolreader_input_readLen_callback(void* ctx, int32_t number) {
    AppContext* app = (AppContext*)ctx;
    AppI2CToolReader* appI2CToolReader = app->sceneI2CToolReader;
    i2CToolReaderModel* model = view_get_model(appI2CToolReader->view);

    model->read_len = number;
    view_commit_model(appI2CToolReader->view, false);
    view_dispatcher_send_custom_event(app->view_dispatcher, I2CReaderEventNumberInput);
}

/**
 * @brief Allocate memory and initialize the app i2ctoolreader.
 *
 * @return Returns a pointer to the allocated AppI2CToolReader.
 */
AppI2CToolReader* app_i2ctoolreader_alloc(void* ctx) {
    furi_assert(ctx);
    AppContext* app = (AppContext*)ctx;
    AppI2CToolReader* appI2CToolReader = malloc(sizeof(AppI2CToolReader));

    appI2CToolReader->view = view_alloc();

    appI2CToolReader->popup = popup_alloc();
    popup_set_timeout(appI2CToolReader->popup, 1000);
    popup_enable_timeout(appI2CToolReader->popup);
    popup_set_context(appI2CToolReader->popup, app);
    popup_set_callback(appI2CToolReader->popup, app_i2ctoolreader_write_success_popup_callback);

    view_dispatcher_add_view(
        app->view_dispatcher, AppViewI2CToolWriterPopup, popup_get_view(appI2CToolReader->popup));

    view_allocate_model(appI2CToolReader->view, ViewModelTypeLockFree, sizeof(i2CToolReaderModel));

    view_set_context(appI2CToolReader->view, app);
    view_set_draw_callback(appI2CToolReader->view, app_i2ctoolreader_render_callback);
    view_set_input_callback(appI2CToolReader->view, app_i2ctoolreader_input_callback);

    appI2CToolReader->InputReadOffset = byte_input_alloc();

    byte_input_set_header_text(appI2CToolReader->InputReadOffset, "Register:");
    byte_input_set_result_callback(
        appI2CToolReader->InputReadOffset,
        app_i2ctoolreader_input_readOffset_callback,
        NULL,
        app,
        &input_read_offset,
        1);

    appI2CToolReader->InputReadLen = number_input_alloc();
    number_input_set_header_text(appI2CToolReader->InputReadLen, "Data len: Octets, max: 48)");
    number_input_set_result_callback(
        appI2CToolReader->InputReadLen, app_i2ctoolreader_input_readLen_callback, app, 1, 1, 48);

    view_dispatcher_add_view(
        app->view_dispatcher,
        AppViewI2CToolReaderByteInput,
        byte_input_get_view(appI2CToolReader->InputReadOffset));
    view_dispatcher_add_view(
        app->view_dispatcher,
        AppViewI2CToolReaderNumberInput,
        number_input_get_view(appI2CToolReader->InputReadLen));

    return appI2CToolReader;
}

/**
 * @brief Get the view associated with the app i2ctoolreader.
 *
 * @param appI2CToolReader The AppI2CToolReader for which the view is to be
 * fetched.
 * @return Returns a pointer to the View.
 */
View* app_i2ctoolreader_get_view(AppI2CToolReader* appI2CToolReader) {
    furi_assert(appI2CToolReader);
    return appI2CToolReader->view;
}

/**
 * @brief Callback when the app i2ctoolreader scene is entered.
 *
 * @param ctx The context passed to the callback.
 */
void app_scene_i2ctoolreader_on_enter(void* ctx) {
    AppContext* app = (AppContext*)ctx;
    AppI2CToolReader* appI2CToolReader = app->sceneI2CToolReader;
    with_view_model(
        appI2CToolReader->view,
        i2CToolReaderModel * model,
        {
            model->device = app->i2cToolDevice;
            model->data = NULL;
            model->data_offset = 0;
            model->data_len = 0;
        },
        false);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewI2CToolReaderByteInput);
}

/**
 * @brief Handle scene manager events for the app i2ctoolreader scene.
 *
 * @param context The context passed to the callback.
 * @param event The scene manager event.
 * @return Returns true if the event was consumed, otherwise false.
 */
bool app_scene_i2ctoolreader_on_event(void* ctx, SceneManagerEvent event) {
    AppContext* app = (AppContext*)ctx;
    AppI2CToolReader* appI2CToolReader = app->sceneI2CToolReader;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) { // Back button pressed
        switch(event.event) {
        case I2CReaderEventExitPopup:
            view_dispatcher_switch_to_view(app->view_dispatcher, AppViewI2CToolReader);
            break;
        case I2CReaderEventByteInput:
            view_dispatcher_switch_to_view(app->view_dispatcher, AppViewI2CToolReaderNumberInput);
            break;
        case I2CReaderEventNumberInput:
            with_view_model(
                appI2CToolReader->view,
                i2CToolReaderModel * model,
                {
                    model->data = calloc(model->read_len, sizeof(uint8_t));
                    model->data_len = model->read_len;
                    model->data_offset = 0;
                    furi_hal_i2c_acquire(I2C_BUS);
                    bool res = false;
                    res = furi_hal_i2c_read_mem(
                        I2C_BUS,
                        model->device->addr << 1,
                        model->read_offset,
                        model->data,
                        model->data_len,
                        I2C_MEM_I2C_TIMEOUT);
                    furi_hal_i2c_release(I2C_BUS);
                    if(res == false) {
                        FURI_LOG_I(TAG, "Could not read mem");
                    }
                },
                false);
            view_dispatcher_switch_to_view(app->view_dispatcher, AppViewI2CToolReader);
            break;
        }
        return true;
    }
    return consumed;
}

/**
 * @brief Callback when the app i2ctoolreader scene is exited.
 *
 * @param ctx The context passed to the callback.
 */
void app_scene_i2ctoolreader_on_exit(void* ctx) {
    AppContext* app = (AppContext*)ctx;
    AppI2CToolReader* appI2CToolReader = app->sceneI2CToolReader;
    with_view_model(
        appI2CToolReader->view,
        i2CToolReaderModel * model,
        {
            free(model->data);
            model->data = NULL;
            model->data_offset = 0;
            model->data_len = 0;
        },
        false);
}

/**
 * @brief Free the memory occupied by the app i2ctoolreader.
 *
 * @param appI2CToolReader The AppI2CToolReader to be freed.
 */
void app_i2ctoolreader_free(void* ctx) {
    furi_assert(ctx);
    AppContext* app = (AppContext*)ctx;
    AppI2CToolReader* appI2CToolReader = app->sceneI2CToolReader;

    view_dispatcher_remove_view(app->view_dispatcher, AppViewI2CToolWriterPopup);
    popup_free(appI2CToolReader->popup);
    view_dispatcher_remove_view(app->view_dispatcher, AppViewI2CToolReaderByteInput);
    byte_input_free(appI2CToolReader->InputReadOffset);
    view_dispatcher_remove_view(app->view_dispatcher, AppViewI2CToolReaderNumberInput);
    number_input_free(appI2CToolReader->InputReadLen);

    view_free_model(appI2CToolReader->view);
    view_free(appI2CToolReader->view);
    free(appI2CToolReader);
}
