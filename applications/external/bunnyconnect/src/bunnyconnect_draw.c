#include "../lib/bunnyconnect_draw.h"
#include <gui/icon_i.h>
#include <furi.h>

void bunnyconnect_draw_logo(Canvas* canvas, uint8_t x, uint8_t y) {
    // Draw a simple logo - rabbit silhouette
    canvas_set_color(canvas, ColorBlack);

    // Draw ears
    canvas_draw_line(canvas, x + 3, y + 5, x + 3, y);
    canvas_draw_line(canvas, x + 3, y, x + 5, y + 3);
    canvas_draw_line(canvas, x + 5, y + 3, x + 7, y);
    canvas_draw_line(canvas, x + 7, y, x + 7, y + 5);

    // Draw head
    canvas_draw_circle(canvas, x + 5, y + 7, 3);

    // Draw eye
    canvas_draw_disc(canvas, x + 4, y + 6, 1);
}

void bunnyconnect_draw_connection_status(Canvas* canvas, bool connected, uint8_t x, uint8_t y) {
    canvas_set_color(canvas, ColorBlack);
    if(connected) {
        // Draw connected icon (checkmark)
        canvas_draw_line(canvas, x, y + 3, x + 2, y + 5);
        canvas_draw_line(canvas, x + 2, y + 5, x + 6, y);
    } else {
        // Draw disconnected icon (X)
        canvas_draw_line(canvas, x, y, x + 5, y + 5);
        canvas_draw_line(canvas, x + 5, y, x, y + 5);
    }
}

void bunnyconnect_draw_signal_strength(Canvas* canvas, uint8_t strength, uint8_t x, uint8_t y) {
    canvas_set_color(canvas, ColorBlack);

    uint8_t bars = (strength * 4) / 100;
    bars = (bars > 4) ? 4 : bars; // Ensure maximum 4 bars

    // Draw signal bars
    for(uint8_t i = 0; i < 4; i++) {
        if(i < bars) {
            // Filled bar for active signal
            canvas_draw_box(canvas, x + (i * 3), y - (i * 2), 2, 5 + (i * 2));
        } else {
            // Empty bar for inactive signal
            canvas_draw_frame(canvas, x + (i * 3), y - (i * 2), 2, 5 + (i * 2));
        }
    }
}

void bunnyconnect_draw_loading_animation(Canvas* canvas, uint8_t frame, uint8_t x, uint8_t y) {
    const uint8_t frame_count = 8;
    frame = frame % frame_count;

    // Draw a rotating line as a simple loading animation
    canvas_set_color(canvas, ColorBlack);

    // Calculate angle based on frame - use simpler integer math to avoid float issues
    int8_t dx[] = {0, 4, 3, 0, -4, -3, 0, 3};
    int8_t dy[] = {-5, -3, 0, 4, 3, 0, -4, -3};

    int8_t end_x = x + dx[frame];
    int8_t end_y = y + dy[frame];

    canvas_draw_line(canvas, x, y, end_x, end_y);
    canvas_draw_disc(canvas, x, y, 1);
}

void bunnyconnect_draw_battery_status(
    Canvas* canvas,
    uint8_t percentage,
    bool charging,
    uint8_t x,
    uint8_t y) {
    canvas_set_color(canvas, ColorBlack);

    // Battery outline
    canvas_draw_box(canvas, x + 9, y + 1, 2, 4);
    canvas_draw_frame(canvas, x, y, 9, 6);

    // Calculate filled portion based on percentage (0-100)
    uint8_t fill_width = (percentage * 7) / 100;
    fill_width = (fill_width > 7) ? 7 : fill_width;

    // Draw battery fill level
    if(fill_width > 0) {
        canvas_draw_box(canvas, x + 1, y + 1, fill_width, 4);
    }

    // Draw charging symbol
    if(charging) {
        // Lightning bolt
        canvas_draw_line(canvas, x + 6, y + 1, x + 4, y + 3);
        canvas_draw_line(canvas, x + 4, y + 3, x + 5, y + 3);
        canvas_draw_line(canvas, x + 5, y + 3, x + 3, y + 5);
        canvas_draw_line(canvas, x + 3, y + 5, x + 5, y + 3);
    }
}

void bunnyconnect_draw_menu_item(
    Canvas* canvas,
    const char* text,
    bool selected,
    uint8_t x,
    uint8_t y) {
    if(selected) {
        // Draw selection background
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, x - 2, y - 8, 122, 11);

        // Draw text in inverted color
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_set_color(canvas, ColorBlack);
    }

    // Draw the text
    canvas_draw_str(canvas, x, y, text);
}
