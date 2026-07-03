#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <toolbox/stream/file_stream.h>

#include "fordradiocode_icons.h"

#define TAG         "FordRadioCodeApp"
#define DATA_FOLDER "/ext/apps_data/fordradiocode"
#define FILE_PATH   DATA_FOLDER "/radiocodes.bin"
#define BUFFER_SIZE 8

typedef struct {
    int16_t result;
    char input_buffer[BUFFER_SIZE];
    bool error;
    uint8_t cursor_pos;
    char error_msg[32];
    bool file_missing;
} FordRadioCodeApp;

static void draw_m_symbol(Canvas* canvas, int x) {
    int y = 21;
    // Left vertical
    canvas_draw_line(canvas, x, y, x, y + 13);
    canvas_draw_line(canvas, x + 1, y, x + 1, y + 13);
    // Right vertical
    canvas_draw_line(canvas, x + 8, y, x + 8, y + 13);
    canvas_draw_line(canvas, x + 9, y, x + 9, y + 13);
    // Diagonals
    canvas_draw_line(canvas, x, y, x + 4, y + 13);
    canvas_draw_line(canvas, x + 1, y, x + 5, y + 13);
    canvas_draw_line(canvas, x + 4, y + 13, x + 8, y);
    canvas_draw_line(canvas, x + 5, y + 13, x + 9, y);
}

static void draw_v_symbol(Canvas* canvas, int x) {
    int y = 21;
    // Diagonals
    canvas_draw_line(canvas, x, y, x + 4, y + 13);
    canvas_draw_line(canvas, x + 1, y, x + 5, y + 13);
    canvas_draw_line(canvas, x + 4, y + 13, x + 8, y);
    canvas_draw_line(canvas, x + 5, y + 13, x + 9, y);
}

static void render_callback(Canvas* canvas, void* ctx) {
    FordRadioCodeApp* app = (FordRadioCodeApp*)ctx;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "Ford Radio Codes");

    if(app->file_missing) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 0, 20, "File not found!");
        canvas_draw_str(canvas, 0, 40, "Copy radiocodes.bin to SD Card");
        canvas_draw_str(canvas, 0, 50, "/apps_data/fordradiocode/");
    } else if(app->error) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 0, 30, app->error_msg);
    } else {
        canvas_set_font(canvas, FontBigNumbers);
        int x = 23;

        // Display the input characters
        for(uint8_t i = 0; i < 7; i++) {
            char current_char = app->input_buffer[i];

            // Draw the 'M' and 'V' manually, to match the size of FontBigNumbers
            if(current_char == 'M') {
                draw_m_symbol(canvas, x);
            } else if(current_char == 'V') {
                draw_v_symbol(canvas, x);
            } else {
                char char_str[2] = {current_char, '\0'};
                canvas_draw_str(canvas, x, 35, char_str);
            }

            // Display arrows for the current cursor position
            if(i == app->cursor_pos) {
                canvas_draw_icon(canvas, x, 12, &I_arrow_up);
                canvas_draw_icon(canvas, x, 36, &I_arrow_down);
            }

            x += 12;
        }

        // Display result
        char result_str[8];
        snprintf(result_str, sizeof(result_str), "%04d", app->result);
        canvas_draw_str_aligned(canvas, 64, 61, AlignCenter, AlignBottom, result_str);
    }
}

static void input_callback(InputEvent* input_event, void* ctx) {
    furi_message_queue_put((FuriMessageQueue*)ctx, input_event, FuriWaitForever);
}

static void process_data(FordRadioCodeApp* app) {
    app->error = false;
    memset(app->error_msg, 0, sizeof(app->error_msg));

    char* endptr;
    long index = strtol(app->input_buffer + 1, &endptr, 10);

    size_t offset = (size_t)index * 2;
    if(app->input_buffer[0] == 'V') offset += 2000000;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* file_stream = file_stream_alloc(storage);

    if(!file_stream_open(file_stream, FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        snprintf(app->error_msg, sizeof(app->error_msg), "File read error");
        app->error = true;
        goto cleanup;
    }

    stream_seek(file_stream, offset, StreamOffsetFromStart);
    uint8_t buffer[2];
    if(stream_read(file_stream, buffer, sizeof(buffer)) != sizeof(buffer)) {
        snprintf(app->error_msg, sizeof(app->error_msg), "Read error");
        app->error = true;
    } else {
        app->result = (int16_t)((buffer[1] << 8) | buffer[0]);
    }

cleanup:
    file_stream_close(file_stream);
    stream_free(file_stream);
    furi_record_close(RECORD_STORAGE);
}

static void ensure_data_folder_exists(FordRadioCodeApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    if(!storage_dir_exists(storage, DATA_FOLDER)) {
        storage_common_mkdir(storage, DATA_FOLDER);
    }

    app->file_missing = !storage_file_exists(storage, FILE_PATH);
    furi_record_close(RECORD_STORAGE);
}

int32_t fordradiocode_app_entry(void* p) {
    UNUSED(p);

    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    FordRadioCodeApp* app = malloc(sizeof(FordRadioCodeApp));
    memset(app, 0, sizeof(FordRadioCodeApp));

    snprintf(app->input_buffer, BUFFER_SIZE, "M000000");

    ensure_data_folder_exists(app);

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, render_callback, app);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    InputEvent event;
    bool running = true;

    if(!app->file_missing) process_data(app);

    while(running) {
        if(furi_message_queue_get(event_queue, &event, FuriWaitForever) == FuriStatusOk) {
            if(event.type == InputTypeShort || event.type == InputTypeRepeat) {
                switch(event.key) {
                case InputKeyUp:
                    if(app->cursor_pos == 0) {
                        app->input_buffer[0] = (app->input_buffer[0] == 'M') ? 'V' : 'M';
                    } else {
                        app->input_buffer[app->cursor_pos] =
                            (app->input_buffer[app->cursor_pos] == '9') ?
                                '0' :
                                app->input_buffer[app->cursor_pos] + 1;
                    }
                    if(!app->file_missing) process_data(app);
                    break;

                case InputKeyDown:
                    if(app->cursor_pos == 0) {
                        app->input_buffer[0] = (app->input_buffer[0] == 'M') ? 'V' : 'M';
                    } else {
                        app->input_buffer[app->cursor_pos] =
                            (app->input_buffer[app->cursor_pos] == '0') ?
                                '9' :
                                app->input_buffer[app->cursor_pos] - 1;
                    }
                    if(!app->file_missing) process_data(app);
                    break;

                case InputKeyLeft:
                    if(app->cursor_pos > 0) app->cursor_pos--;
                    break;

                case InputKeyRight:
                    if(app->cursor_pos < 6) app->cursor_pos++;
                    break;

                case InputKeyBack:
                    running = false;
                    break;

                default:
                    break;
                }
                view_port_update(view_port);
            }
        }
    }

    // Cleanup
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_record_close(RECORD_GUI);
    free(app);
    return 0;
}
