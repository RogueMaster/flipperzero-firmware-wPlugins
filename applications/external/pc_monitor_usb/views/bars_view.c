#include "bars_view.h"

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
            snprintf(str, 32, "%d%%", app->data.cpu_usage);
            elements_progress_bar_with_text(
                canvas,
                BAR_X,
                margin_top + line * spacing,
                BAR_WIDTH,
                app->data.cpu_usage / 100.0f,
                str);
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
            snprintf(str, 32, "%d%%", app->data.gpu_usage);
            elements_progress_bar_with_text(
                canvas,
                BAR_X,
                margin_top + line * spacing,
                BAR_WIDTH,
                app->data.gpu_usage / 100.0f,
                str);
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
