#include "marauder_gui_app_i.h"
#include <gui/elements.h>
#include <stdio.h>
#include <string.h>

/* Generic replacement for Submenu everywhere in this app, so every menu can support a
   Right-key "read the description" overlay - Flipper's stock Submenu module has no such hook
   (it only exposes label/index/callback, nothing for arbitrary keys like Right). Ok still just
   sends a custom event with the selected index, same as Submenu's callback did, so scenes only
   need to change on_enter/on_exit, not on_event. */

#define MARAUDER_MENU_ROW_HEIGHT    13
#define MARAUDER_MENU_HEADER_HEIGHT 13
#define MARAUDER_MENU_VISIBLE_ROWS  4
#define MARAUDER_MENU_MARQUEE_TICKS 3
#define MARAUDER_MENU_MARQUEE_DELAY_TICKS \
    30 /* ~3s pause before a highlighted row starts scrolling */

void marauder_gui_menu_redraw(MarauderGuiApp* app) {
    with_view_model(app->menu_view, MarauderGuiApp * *model, { UNUSED(model); }, true);
}

/* Simple greedy word-wrap onto the raw canvas - Widget's multiline helpers aren't available
   here since this is a plain custom View, not a Widget. Hand-rolled word splitting because
   strtok isn't in the firmware's exposed API symbol set (link fails with "MissingImports" if
   used, found the hard way).

   Two-pass so long descriptions can be scrolled: call once with draw=false to measure how many
   lines the text wraps into (out_total_lines), clamp/adjust the scroll offset against that, then
   call again with draw=true to actually render just the visible window starting at
   scroll_offset. */
static void marauder_gui_menu_wrap(
    Canvas* canvas,
    int32_t x,
    int32_t y_start,
    int32_t max_width,
    int32_t line_height,
    int32_t visible_rows,
    int32_t scroll_offset,
    const char* text,
    int32_t* out_total_lines,
    bool draw) {
    char line[110] = "";
    const char* p = text;
    int32_t total = 0;

    while(*p) {
        while(*p == ' ')
            p++;
        if(!*p) break;

        const char* word_start = p;
        while(*p && *p != ' ')
            p++;
        int word_len = (int)(p - word_start);
        if(word_len > 100) word_len = 100;

        char trial[220];
        if(line[0] == '\0') {
            snprintf(trial, sizeof(trial), "%.*s", word_len, word_start);
        } else {
            snprintf(trial, sizeof(trial), "%s %.*s", line, word_len, word_start);
        }

        if(canvas_string_width(canvas, trial) <= (uint16_t)max_width) {
            strncpy(line, trial, sizeof(line) - 1);
            line[sizeof(line) - 1] = '\0';
        } else {
            if(draw && total >= scroll_offset && total < scroll_offset + visible_rows) {
                canvas_draw_str(canvas, x, y_start + (total - scroll_offset) * line_height, line);
            }
            total++;
            snprintf(line, sizeof(line), "%.*s", word_len, word_start);
        }
    }

    if(line[0] != '\0') {
        if(draw && total >= scroll_offset && total < scroll_offset + visible_rows) {
            canvas_draw_str(canvas, x, y_start + (total - scroll_offset) * line_height, line);
        }
        total++;
    }

    if(out_total_lines) *out_total_lines = total;
}

static void marauder_gui_menu_draw_description(Canvas* canvas, MarauderGuiApp* app) {
    const MarauderMenuItem* item = &app->menu_items[app->menu_selected];
    const int32_t visible_rows = 4;
    const int32_t line_height = 10;
    const int32_t text_y = 24;

    canvas_set_font(canvas, FontPrimary);
    FuriString* title = furi_string_alloc_set_str(marauder_gui_menu_item_label(app, item));
    elements_scrollable_text_line(canvas, 2, 9, canvas_width(canvas) - 4, title, 0, false);
    furi_string_free(title);

    canvas_draw_line(canvas, 0, 12, canvas_width(canvas), 12);

    canvas_set_font(canvas, FontSecondary);

    const char* description = marauder_gui_menu_item_description(app, item);
    int32_t total_lines = 0;
    marauder_gui_menu_wrap(
        canvas,
        2,
        text_y,
        canvas_width(canvas) - 4,
        line_height,
        visible_rows,
        0,
        description,
        &total_lines,
        false);

    int32_t max_scroll = total_lines - visible_rows;
    if(max_scroll < 0) max_scroll = 0;
    if(app->menu_description_scroll > (size_t)max_scroll) {
        app->menu_description_scroll = (size_t)max_scroll;
    }

    marauder_gui_menu_wrap(
        canvas,
        2,
        text_y,
        canvas_width(canvas) - 4,
        line_height,
        visible_rows,
        (int32_t)app->menu_description_scroll,
        description,
        NULL,
        true);

    if(total_lines > visible_rows) {
        elements_scrollbar_pos(
            canvas,
            canvas_width(canvas) - 3,
            text_y - line_height + 2,
            (uint32_t)(visible_rows * line_height),
            app->menu_description_scroll,
            (size_t)total_lines);
    }

    canvas_draw_str(canvas, 2, 63, marauder_gui_text(app, "Geri/Sag: kapat", "Back/Right: close"));
}

static void marauder_gui_menu_draw_callback(Canvas* canvas, void* model) {
    MarauderGuiApp* app = *(MarauderGuiApp**)model;

    canvas_clear(canvas);

    if(app->menu_showing_description) {
        marauder_gui_menu_draw_description(canvas, app);
        return;
    }

    /* Headers can be built at runtime (e.g. "Connected: <long ssid>"), so draw them the way the
       description view draws its title: elements_scrollable_text_line truncates with an ellipsis
       instead of running off the right edge like a plain canvas_draw_str does. */
    canvas_set_font(canvas, FontPrimary);
    FuriString* header = furi_string_alloc_set_str(app->menu_header);
    elements_scrollable_text_line(canvas, 2, 9, canvas_width(canvas) - 4, header, 0, false);
    furi_string_free(header);
    canvas_draw_line(canvas, 0, 11, canvas_width(canvas), 11);

    canvas_set_font(canvas, FontSecondary);

    for(size_t row = 0; row < MARAUDER_MENU_VISIBLE_ROWS; row++) {
        size_t idx = app->menu_scroll_offset + row;
        if(idx >= app->menu_item_count) break;

        int32_t y =
            MARAUDER_MENU_HEADER_HEIGHT + (int32_t)((row + 1) * MARAUDER_MENU_ROW_HEIGHT) - 2;
        bool selected = (idx == app->menu_selected);

        if(selected) {
            canvas_draw_box(
                canvas,
                0,
                MARAUDER_MENU_HEADER_HEIGHT + (int32_t)(row * MARAUDER_MENU_ROW_HEIGHT) + 1,
                canvas_width(canvas),
                MARAUDER_MENU_ROW_HEIGHT);
            canvas_set_color(canvas, ColorWhite);

            FuriString* text = furi_string_alloc_set_str(
                marauder_gui_menu_item_label(app, &app->menu_items[idx]));
            elements_scrollable_text_line(
                canvas, 2, y, canvas_width(canvas) - 4, text, app->menu_marquee_tick, false);
            furi_string_free(text);

            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str(
                canvas, 2, y, marauder_gui_menu_item_label(app, &app->menu_items[idx]));
        }
    }

    if(app->menu_item_count > MARAUDER_MENU_VISIBLE_ROWS) {
        elements_scrollbar_pos(
            canvas,
            canvas_width(canvas) - 3,
            MARAUDER_MENU_HEADER_HEIGHT + 1,
            canvas_height(canvas) - MARAUDER_MENU_HEADER_HEIGHT - 1,
            app->menu_selected,
            app->menu_item_count);
    }
}

static bool marauder_gui_menu_input_callback(InputEvent* event, void* context) {
    MarauderGuiApp* app = context;

    if(app->menu_showing_description) {
        if(event->type == InputTypeShort &&
           (event->key == InputKeyRight || event->key == InputKeyBack ||
            event->key == InputKeyOk)) {
            app->menu_showing_description = false;
            marauder_gui_menu_redraw(app);
            return true;
        }
        if((event->type == InputTypeShort || event->type == InputTypeRepeat) &&
           event->key == InputKeyDown) {
            app->menu_description_scroll++;
            marauder_gui_menu_redraw(app);
            return true;
        }
        if((event->type == InputTypeShort || event->type == InputTypeRepeat) &&
           event->key == InputKeyUp) {
            if(app->menu_description_scroll > 0) app->menu_description_scroll--;
            marauder_gui_menu_redraw(app);
            return true;
        }
        /* Swallow everything else so it doesn't leak through to the list or scene navigation
           while the description is open. */
        return true;
    }

    if(app->menu_item_count == 0) return false;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    if(event->key == InputKeyUp) {
        app->menu_selected = (app->menu_selected == 0) ? app->menu_item_count - 1 :
                                                         app->menu_selected - 1;
        app->menu_marquee_tick = 0;
        app->menu_marquee_hold = 0;
        app->menu_marquee_delay = MARAUDER_MENU_MARQUEE_DELAY_TICKS;
    } else if(event->key == InputKeyDown) {
        app->menu_selected =
            (app->menu_selected + 1 >= app->menu_item_count) ? 0 : app->menu_selected + 1;
        app->menu_marquee_tick = 0;
        app->menu_marquee_hold = 0;
        app->menu_marquee_delay = MARAUDER_MENU_MARQUEE_DELAY_TICKS;
    } else if(event->key == InputKeyOk) {
        view_dispatcher_send_custom_event(app->view_dispatcher, app->menu_selected);
        return true;
    } else if(event->key == InputKeyRight) {
        app->menu_showing_description = true;
        app->menu_description_scroll = 0;
        marauder_gui_menu_redraw(app);
        return true;
    } else {
        return false;
    }

    if(app->menu_selected < app->menu_scroll_offset) {
        app->menu_scroll_offset = app->menu_selected;
    } else if(app->menu_selected >= app->menu_scroll_offset + MARAUDER_MENU_VISIBLE_ROWS) {
        app->menu_scroll_offset = app->menu_selected - MARAUDER_MENU_VISIBLE_ROWS + 1;
    }

    marauder_gui_menu_redraw(app);
    return true;
}

static void marauder_gui_menu_tick(MarauderGuiApp* app) {
    if(app->menu_showing_description) return;

    if(app->menu_marquee_delay > 0) {
        app->menu_marquee_delay--;
    } else {
        app->menu_marquee_hold++;
        if(app->menu_marquee_hold >= MARAUDER_MENU_MARQUEE_TICKS) {
            app->menu_marquee_hold = 0;
            app->menu_marquee_tick++;
            marauder_gui_menu_redraw(app);
        }
    }
}

View* marauder_gui_menu_view_alloc(MarauderGuiApp* app) {
    View* view = view_alloc();
    view_allocate_model(view, ViewModelTypeLockFree, sizeof(MarauderGuiApp*));
    with_view_model(view, MarauderGuiApp * *model, { *model = app; }, false);
    view_set_draw_callback(view, marauder_gui_menu_draw_callback);
    view_set_input_callback(view, marauder_gui_menu_input_callback);
    view_set_context(view, app);
    return view;
}

void marauder_gui_menu_set_items(
    MarauderGuiApp* app,
    const MarauderMenuItem* items,
    size_t count,
    const char* header) {
    app->menu_items = items;
    app->menu_item_count = count;
    app->menu_header = header;
    app->menu_selected = 0;
    app->menu_scroll_offset = 0;
    app->menu_showing_description = false;
    app->menu_description_scroll = 0;
    app->menu_marquee_tick = 0;
    app->menu_marquee_hold = 0;
    app->menu_marquee_delay = MARAUDER_MENU_MARQUEE_DELAY_TICKS;

    app->tick_handler = marauder_gui_menu_tick;

    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewMenu);
}
