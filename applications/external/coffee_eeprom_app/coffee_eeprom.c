#include <furi.h>
#include <furi_hal.h>
#include <gui/elements.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <stdlib.h>
#include <stdio.h>
#include "coffee.h"
#include <math.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <lib/toolbox/name_generator.h>

#define TAG        "COFFEE EEPROM"
#define MAX_CREDIT 655.35
#define MIN_CREDIT 0.01

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    bool editor_mode;
    float digit_editor;
    float credit;
    // Plain storage, not a FuriString: the GUI thread reads this while the app thread
    // writes it, and a reallocating string hands the reader a freed buffer
    char status[32];
} CoffeeContext;

static void coffee_render_callback(Canvas* const canvas, void* ctx) {
    CoffeeContext* context = ctx;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    /* Format into locals and touch nothing shared: this runs on the GUI thread while
       the app thread is writing the same context */
    char line[32];

    if(context->credit >= 0.0) {
        snprintf(line, sizeof(line), "Credit: %.2f EUR", (double)context->credit);
        elements_button_left(canvas, "Load");
        elements_button_right(canvas, "Save");
        elements_button_center(canvas, "Edit (Hold)");
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignCenter, line);
        canvas_set_font(canvas, FontKeyboard);
        canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignCenter, context->status);
    } else {
        canvas_set_font(canvas, FontKeyboard);
        canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignCenter, "EEPROM not connected!");
    }
}

/* This function is called from the GUI thread. All it does is put the event
   into the application's queue so it can be processed later. */
static void coffee_input_callback(InputEvent* event, void* ctx) {
    CoffeeContext* context = ctx;
    furi_message_queue_put(context->event_queue, event, FuriWaitForever);
}
/* Allocate the memory and initialise the variables */
static CoffeeContext* coffee_context_alloc() {
    CoffeeContext* context = malloc(sizeof(CoffeeContext));

    // The draw callback reads status and credit, and can run as soon as the view port
    // goes on screen - neither may still be whatever malloc() returned
    context->status[0] = '\0';
    context->credit = 0.0;
    context->digit_editor = 0.01;
    context->editor_mode = false;

    context->view_port = view_port_alloc();
    view_port_draw_callback_set(context->view_port, coffee_render_callback, context);
    view_port_input_callback_set(context->view_port, coffee_input_callback, context);

    context->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    context->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(context->gui, context->view_port, GuiLayerFullscreen);
    return context;
}

void load_file_dump() {
    FuriString* file_path = furi_string_alloc();

    do {
        DialogsFileBrowserOptions browser_options;
        dialog_file_browser_set_basic_options(&browser_options, ".bin", NULL);
        browser_options.hide_ext = false;
        browser_options.base_path = "/ext";

        DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
        bool res = dialog_file_browser_show(dialogs, file_path, file_path, &browser_options);

        furi_record_close(RECORD_DIALOGS);
        if(!res) {
            FURI_LOG_E(TAG, "No file selected");
            break;
        }
        // Open storage
        Storage* storage = furi_record_open(RECORD_STORAGE);
        // Allocate file
        File* file = storage_file_alloc(storage);
        // Open file, write data and close it
        if(!storage_file_open(
               file, furi_string_get_cstr(file_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
            FURI_LOG_E(TAG, "Failed to open file");
        }
        furi_string_free(file_path);
        uint8_t buffer[256] = {0};

        uint16_t read = 0;
        uint16_t ret = 0;
        do {
            uint8_t temp[128] = {0};
            read += ret;
            ret = storage_file_read(file, temp, sizeof(temp) - 1);
            for(size_t i = 0; i < ret; i++) {
                buffer[i + read] = temp[i];
            }
        } while(ret > 0);
        storage_file_close(file);

        // Deallocate file
        storage_file_free(file);

        // Close storage
        furi_record_close(RECORD_STORAGE);
        if(read % 128 == 0) {
            FuriString* dump = furi_string_alloc();
            FURI_LOG_E(TAG, "START READ DUMP");
            for(size_t i = 0; i < read; i++) {
                furi_string_cat_printf(dump, "%.2X", buffer[i]);
            }
            FURI_LOG_E(TAG, "%s", furi_string_get_cstr(dump));
            FURI_LOG_E(TAG, "END READ DUMP");
            furi_string_free(dump);
            write_dump(buffer, (size_t)read);
            break;
        }
    } while(1);
}

FuriString* save_file_dump(float credit) {
    char file_name_buf[64];
    name_generator_make_random(file_name_buf, 64);
    // Open storag
    char* file_path = APP_DATA_PATH("Dump");

    FuriString* file_name =
        furi_string_alloc_printf("%s_%s_%d.bin", file_path, file_name_buf, (int)(credit * 100));
    Storage* storage = furi_record_open(RECORD_STORAGE);

    // Allocate file
    File* file = storage_file_alloc(storage);

    // Get the path to the current application data folder
    // That is: /ext/apps_data/<app_name>
    // And it will create folders in the path if they don't exist
    // In this example it will create /ext/apps_data/example_apps_data
    // And file will be /ext/apps_data/example_apps_data/test.txt

    // Open file, write data and close it
    if(!storage_file_open(file, furi_string_get_cstr(file_name), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_E(TAG, "Failed to open file");
    }
    uint8_t out[256];
    dump(out);
    if(!storage_file_write(file, out, 256)) {
        FURI_LOG_E(TAG, "Failed to write to file");
    }
    storage_file_close(file);

    // Deallocate file
    storage_file_free(file);

    // Close storage
    furi_record_close(RECORD_STORAGE);

    return file_name;
}

/* Starts the reader thread and handles the input */
static void coffee_run(CoffeeContext* context) {
    /* Start the reader thread. It will talk to the thermometer in the background. */
    context->credit = read_credit();
    context->digit_editor = 0.01;
    view_port_update(context->view_port);
    /* An endless loop which handles the input*/
    for(bool is_running = true; is_running;) {
        InputEvent event;
        /* Wait for an input event. Input events come from the GUI thread via a callback. */
        const FuriStatus status =
            furi_message_queue_get(context->event_queue, &event, FuriWaitForever);

        if(status == FuriStatusOk) {
            if(event.type == InputTypePress) {
                switch(event.key) {
                case InputKeyUp:
                    if(context->editor_mode &&
                       context->credit + context->digit_editor <= MAX_CREDIT) {
                        context->credit += context->digit_editor;
                        FURI_LOG_E(
                            TAG,
                            "%.2f   %.2f",
                            (double)context->credit,
                            (double)context->digit_editor);
                    }
                    break;
                case InputKeyDown:
                    if(context->editor_mode &&
                       context->credit - context->digit_editor >= MIN_CREDIT) {
                        context->credit -= context->digit_editor;
                        FURI_LOG_E(
                            TAG,
                            "%.2f   %.2f",
                            (double)context->credit,
                            (double)context->digit_editor);
                    }
                    break;
                case InputKeyMAX:
                    break;
                case InputKeyRight:
                    if(context->editor_mode && context->digit_editor >= 0.01) {
                        context->digit_editor /= 10;
                        FURI_LOG_E(
                            TAG,
                            "%.2f   %.2f",
                            (double)context->credit,
                            (double)context->digit_editor);
                    } else {
                        save_file_dump(context->credit);
                        snprintf(context->status, sizeof(context->status), "Dump saved!");
                    }
                    break;
                case InputKeyLeft:
                    if(context->editor_mode && context->digit_editor <= 100) {
                        context->digit_editor *= 10;
                        FURI_LOG_E(
                            TAG,
                            "%.2f   %.2f",
                            (double)context->credit,
                            (double)context->digit_editor);
                    } else {
                        //virgin();
                        load_file_dump();
                        context->credit = read_credit();
                        snprintf(context->status, sizeof(context->status), "Dump write done!");
                    }
                    break;
                case InputKeyOk:
                    if(context->editor_mode) {
                        write_credit(context->credit);
                        context->credit = read_credit();
                        snprintf(context->status, sizeof(context->status), "Write done!");
                        context->editor_mode = false;
                    }
                    break;
                case InputKeyBack:
                    if(context->editor_mode) {
                        context->status[0] = '\0';
                        context->editor_mode = false;
                    } else {
                        is_running = false;
                    }
                    break;
                }
            } else if(event.type == InputTypeLong && event.key == InputKeyOk) {
                snprintf(context->status, sizeof(context->status), "Editor Mode");
                context->editor_mode = true;
            }
        }

        /* Every branch above changes what the draw callback reads, and nothing else
           asks for a repaint */
        view_port_update(context->view_port);
    }
}

/* Release the unused resources and deallocate memory */
static void coffee_context_free(CoffeeContext* context) {
    view_port_enabled_set(context->view_port, false);
    gui_remove_view_port(context->gui, context->view_port);
    furi_message_queue_free(context->event_queue);
    view_port_free(context->view_port);
    furi_record_close(RECORD_GUI);
}

/* The application's entry point. Execution starts from here. */
int32_t coffee_eeprom_main(void* p) {
    UNUSED(p);

    /* Allocate all of the necessary structures */
    CoffeeContext* context = coffee_context_alloc();

    /* Start the applicaton's main loop. It won't return until the application was requested to exit. */
    coffee_run(context);

    /* Release all unneeded resources */
    coffee_context_free(context);

    return 0;
}
