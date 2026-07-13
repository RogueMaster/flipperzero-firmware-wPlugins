#include "../jayx.h"

#define ROW_H 13

static void draw_metric_row(
    Canvas* canvas,
    uint8_t row,
    const char* label,
    const char* value,
    float percent) {
    int32_t y0 = row * ROW_H;

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_disc(canvas, 3, y0 + 4, 1);
    canvas_draw_str(canvas, 7, y0 + 7, label);

    uint16_t val_w = canvas_string_width(canvas, value);
    canvas_draw_str(canvas, 125 - val_w, y0 + 7, value);

    if(percent >= 0.0f) {
        if(percent > 1.0f) percent = 1.0f;
        canvas_draw_frame(canvas, 2, y0 + 8, 124, 3);
        uint8_t fill = (uint8_t)(122.0f * percent);
        if(fill > 0) canvas_draw_box(canvas, 3, y0 + 9, fill, 1);
    }
}

static void unit_cstr(char* out, size_t n, const char unit[4]) {
    size_t i = 0;
    for(; i < 3 && i + 1 < n && unit[i]; i++) {
        out[i] = unit[i];
    }
    out[i] = '\0';
}

void draw_system_view(Canvas* canvas, JayxApp* app) {
    JayxLivePacket* d = &app->data;
    char value[32];
    char unit[4];

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(d->cpu_temp_c != 0xFF) {
        snprintf(value, sizeof(value), "%u%%  %uC", d->cpu_usage, d->cpu_temp_c);
    } else {
        snprintf(value, sizeof(value), "%u%%", d->cpu_usage);
    }
    draw_metric_row(canvas, 0, "CPU", value, d->cpu_usage / 100.0f);

    unit_cstr(unit, sizeof(unit), d->ram_unit);
    snprintf(
        value,
        sizeof(value),
        "%.1f/%.1f%s",
        (double)(d->ram_max * 0.1f * d->ram_usage * 0.01f),
        (double)(d->ram_max * 0.1f),
        unit);
    draw_metric_row(canvas, 1, "RAM", value, d->ram_usage * 0.01f);

    if(d->gpu_usage <= 100) {
        if(d->gpu_temp_c != 0xFF) {
            snprintf(value, sizeof(value), "%u%%  %uC", d->gpu_usage, d->gpu_temp_c);
        } else {
            snprintf(value, sizeof(value), "%u%%", d->gpu_usage);
        }
        draw_metric_row(canvas, 2, "GPU", value, d->gpu_usage / 100.0f);
    } else {
        draw_metric_row(canvas, 2, "GPU", "N/A", -1.0f);
    }

    if(d->vram_usage <= 100) {
        unit_cstr(unit, sizeof(unit), d->vram_unit);
        snprintf(
            value,
            sizeof(value),
            "%.1f/%.1f%s",
            (double)(d->vram_max * 0.1f * d->vram_usage * 0.01f),
            (double)(d->vram_max * 0.1f),
            unit);
        draw_metric_row(canvas, 3, "VRAM", value, d->vram_usage * 0.01f);
    } else {
        draw_metric_row(canvas, 3, "VRAM", "N/A", -1.0f);
    }

    jayx_draw_page_dots(canvas, JayxPageSystem);
}
