#include "bars_view.h"

static void draw_progress_bar_with_temp(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t width,
    float progress,
    uint8_t usage,
    uint8_t temp) {
    char str[32];
    if(temp < 255) {
        // Two spaces before 'C' to ensure clear separation and no glyph clipping
        snprintf(str, sizeof(str), "%d%%  %d  C", usage, temp);
    } else {
        snprintf(str, sizeof(str), "%d%%", usage);
    }

    elements_progress_bar_with_text(canvas, x, y, width, progress, str);

    if(temp < 255) {
        canvas_set_font(canvas, FontSecondary);
        uint16_t full_len = canvas_string_width(canvas, str);
        uint16_t c_width = canvas_string_width(canvas, "C");
        int32_t text_left_x = (x + (int32_t)width / 2) - (full_len / 2);
        int32_t c_x = text_left_x + full_len - c_width;

        // Position degree box closer to 'C' and further from number, at top of 'C' (y + 2)
        int32_t deg_x = c_x - 4;
        int32_t deg_y = y + 2;

        canvas_set_color(canvas, ColorXOR);
        canvas_draw_frame(canvas, deg_x, deg_y, 3, 3);
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontKeyboard);
    }
}

void draw_bars_view(Canvas* canvas, void* ctx) {
    PcMonitorApp* app = ctx;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontKeyboard);

    uint8_t line = 0;
    uint8_t spacing = app->lines_count ? SCREEN_HEIGHT / app->lines_count : 0;
    uint8_t margin_top = spacing ? (spacing - LINE_HEIGHT) / 2 : 0;
    char str[32];

    if(app->data.cpu_usage <= 100) {
        if(app->lines_count) {
            canvas_draw_str(canvas, 1, margin_top + line * spacing + 9, "CPU");
            draw_progress_bar_with_temp(
                canvas,
                BAR_X,
                margin_top + line * spacing,
                BAR_WIDTH,
                app->data.cpu_usage / 100.0f,
                app->data.cpu_usage,
                app->data.cpu_temp);
        }

        line++;
    }

    if(app->data.ram_usage <= 100) {
        if(app->lines_count) {
            canvas_draw_str(canvas, 1, margin_top + line * spacing + 9, "RAM");
            snprintf(
                str,
                32,
                "%.1f/%.1f %s",
                (double)(app->data.ram_max * 0.1f * app->data.ram_usage * 0.01f),
                (double)(app->data.ram_max * 0.1f),
                app->data.ram_unit);
            elements_progress_bar_with_text(
                canvas,
                BAR_X,
                margin_top + line * spacing,
                BAR_WIDTH,
                app->data.ram_usage * 0.01f,
                str);
        }

        line++;
    }

    if(app->data.gpu_usage <= 100) {
        if(app->lines_count) {
            canvas_draw_str(canvas, 1, margin_top + line * spacing + 9, "GPU");
            draw_progress_bar_with_temp(
                canvas,
                BAR_X,
                margin_top + line * spacing,
                BAR_WIDTH,
                app->data.gpu_usage / 100.0f,
                app->data.gpu_usage,
                app->data.gpu_temp);
        }

        line++;
    }

    if(app->data.vram_usage <= 100) {
        if(app->lines_count) {
            canvas_draw_str(canvas, 1, margin_top + line * spacing + 9, "VRAM");
            snprintf(
                str,
                32,
                "%.1f/%.1f %s",
                (double)(app->data.vram_max * 0.1f * app->data.vram_usage * 0.01f),
                (double)(app->data.vram_max * 0.1f),
                app->data.vram_unit);
            elements_progress_bar_with_text(
                canvas,
                BAR_X,
                margin_top + line * spacing,
                BAR_WIDTH,
                app->data.vram_usage * 0.01f,
                str);
        }

        line++;
    }

    if(line == 0) app->usb_state = UsbStateInactive;
    app->lines_count = line;
}
