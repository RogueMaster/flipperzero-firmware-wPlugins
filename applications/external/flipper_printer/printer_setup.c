#include "flipper_printer.h"
#include <furi.h>
#include <gui/elements.h>

typedef struct {
    uint8_t page;
} PrinterSetupModel;

static void printer_setup_view_draw_callback(Canvas* canvas, void* model) {
    PrinterSetupModel* m = model;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    if(m->page == 0) {
        // Page 1: Method 1 (RX/TX) Connection
        canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Method 1: RX/TX");

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 15, "T7-US -> Flipper:");
        canvas_draw_str(canvas, 2, 27, "VCC  -> 3V3");
        canvas_draw_str(canvas, 2, 39, "GND  -> GND");
        canvas_draw_str(canvas, 2, 51, "RX   -> RX (Pin 13)");
        canvas_draw_str(canvas, 2, 63, "TX   -> TX (Pin 14)");

        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "OK:Next  BACK:Exit");
    } else if(m->page == 1) {
        // Page 2: Method 2 (C0/C1) Connection
        canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Method 2: C0/C1");

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 15, "T7-US -> Flipper:");
        canvas_draw_str(canvas, 2, 27, "VCC  -> 3V3");
        canvas_draw_str(canvas, 2, 39, "GND  -> GND");
        canvas_draw_str(canvas, 2, 51, "RX   -> C0 (Pin 15)");
        canvas_draw_str(canvas, 2, 63, "TX   -> C1 (Pin 16)");

        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "OK:Next  BACK:Exit");
    } else if(m->page == 2) {
        // Page 3: Setup Instructions
        canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Setup Steps");

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 15, "1. Power OFF Flipper");
        canvas_draw_str(canvas, 2, 27, "2. Connect wires");
        canvas_draw_str(canvas, 2, 39, "3. Check connections");
        canvas_draw_str(canvas, 2, 51, "4. Power ON Flipper");
        canvas_draw_str(canvas, 2, 63, "5. Launch app");

        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "OK:Next  BACK:Exit");
    } else {
        // Page 4: Troubleshooting
        canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Troubleshooting");

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 15, "No response?");
        canvas_draw_str(canvas, 2, 27, "- Check all wires");
        canvas_draw_str(canvas, 2, 39, "- Load paper");
        canvas_draw_str(canvas, 2, 51, "- 9600 baud rate");
        canvas_draw_str(canvas, 2, 63, "- Power cycle");

        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "OK:Page 1  BACK:Exit");
    }
}

static bool printer_setup_view_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    bool consumed = false;

    if(event->type == InputTypePress) {
        if(event->key == InputKeyOk) {
            with_view_model(
                context, PrinterSetupModel * model, { model->page = (model->page + 1) % 4; }, true);
            consumed = true;
        }
    }

    return consumed;
}

View* printer_setup_view_alloc() {
    View* view = view_alloc();
    view_allocate_model(view, ViewModelTypeLocking, sizeof(PrinterSetupModel));
    view_set_draw_callback(view, printer_setup_view_draw_callback);
    view_set_input_callback(view, printer_setup_view_input_callback);

    with_view_model(view, PrinterSetupModel * model, { model->page = 0; }, true);

    return view;
}

void printer_setup_view_free(View* view) {
    furi_assert(view);
    view_free(view);
}
