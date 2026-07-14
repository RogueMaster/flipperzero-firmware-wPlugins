#include <furi.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <gui/view.h>
#include <input/input.h>
#include <stdlib.h>
#include <string.h>

#include "saved_passwords.h"
#include "../badusb/badusb.h"

static void saved_passwords_draw_callback(Canvas* canvas, void* model) {
    AppContext** model_ = model;
    AppContext* app = *model_;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    // Draw header
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_str(canvas, 20, 10, "Saved Passwords");
    canvas_draw_line(canvas, 0, 12, 128, 12);

    size_t max_visible = 4;
    size_t start = app->scroll_offset;
    size_t end = start + max_visible;
    if(end > app->credentials_number) end = app->credentials_number;

    for(size_t i = start; i < end; i++) {
        int y = 25 + (i - start) * 12;
        if(i == app->selected) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, 0, y - 10, 120, 12);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }
        canvas_draw_str(canvas, 5, y, app->credentials[i].name);
    }

    // Draw scroll bar
    if(app->credentials_number > max_visible) {
        int bar_x = 124;
        int bar_y = 14;
        int bar_height = 48; // total scroll area height
        int indicator_height = bar_height * max_visible / app->credentials_number;
        if(indicator_height < 6) indicator_height = 6; // minimum size for visibility

        int scroll_range = app->credentials_number - max_visible;
        int indicator_y =
            bar_y + (bar_height - indicator_height) * app->scroll_offset / scroll_range;

        // Draw scroll indicator
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, bar_x, indicator_y, 3, indicator_height);
    }
}

static bool saved_passwords_input_callback(InputEvent* event, void* context) {
    AppContext* app = context;

    if(event->type == InputTypeShort) {
        if(event->key == InputKeyUp) {
            if(app->selected > 0) app->selected--;

            // Scroll up if selected goes above visible area
            if(app->selected < app->scroll_offset) {
                app->scroll_offset--;
            }

            return true;
        } else if(event->key == InputKeyDown) {
            if(app->selected + 1 < app->credentials_number) app->selected++;

            // Scroll down if selected goes beyond visible area
            if(app->selected >= app->scroll_offset + 4) {
                app->scroll_offset++;
            }

            return true;
        } else if(event->key == InputKeyBack) {
            app->selected = 0;
            app->scroll_offset = 0;
            return false;
        } else if(event->key == InputKeyOk) {
            initialize_hid();
            if(strlen(app->credentials[app->selected].username)) {
                type_string(app->credentials[app->selected].username);
                press_key(HID_KEYBOARD_TAB);
            }
            type_string(app->credentials[app->selected].password);
            press_key(HID_KEYBOARD_RETURN);
            release_all_keys();
            return true;
        }
    }

    return false;
}

View* saved_passwords_view_alloc(AppContext* app_context) {
    View* view = view_alloc();

    AppContext* app = app_context;
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLockFree, sizeof(AppContext*));
    AppContext** app_view = view_get_model(view);
    *app_view = app;
    view_set_draw_callback(view, saved_passwords_draw_callback);
    view_set_input_callback(view, saved_passwords_input_callback);

    return view;
}
