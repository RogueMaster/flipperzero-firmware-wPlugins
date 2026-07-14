#include "../jayx.h"

void draw_system_view(Canvas* canvas, JayxApp* app) {
    JayxLivePacket* d = &app->data;
    char value[32];
    char unit[4];

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(d->cpu_temp_c != JAYX_VAL_NA) {
        snprintf(value, sizeof(value), "%u%%  %uC", d->cpu_usage, d->cpu_temp_c);
    } else {
        snprintf(value, sizeof(value), "%u%%", d->cpu_usage);
    }
    jayx_draw_metric_row(canvas, 0, "CPU", value, d->cpu_usage / 100.0f);

    jayx_unit_cstr(unit, sizeof(unit), d->ram_unit);
    snprintf(
        value,
        sizeof(value),
        "%.1f/%.1f%s",
        (double)(d->ram_max * 0.1f * d->ram_usage * 0.01f),
        (double)(d->ram_max * 0.1f),
        unit);
    jayx_draw_metric_row(canvas, 1, "RAM", value, d->ram_usage * 0.01f);

    if(d->gpu_usage <= 100) {
        if(d->gpu_temp_c != JAYX_VAL_NA) {
            snprintf(value, sizeof(value), "%u%%  %uC", d->gpu_usage, d->gpu_temp_c);
        } else {
            snprintf(value, sizeof(value), "%u%%", d->gpu_usage);
        }
        jayx_draw_metric_row(canvas, 2, "GPU", value, d->gpu_usage / 100.0f);
    } else {
        jayx_draw_metric_row(canvas, 2, "GPU", "N/A", -1.0f);
    }

    if(d->vram_usage <= 100) {
        jayx_unit_cstr(unit, sizeof(unit), d->vram_unit);
        snprintf(
            value,
            sizeof(value),
            "%.1f/%.1f%s",
            (double)(d->vram_max * 0.1f * d->vram_usage * 0.01f),
            (double)(d->vram_max * 0.1f),
            unit);
        jayx_draw_metric_row(canvas, 3, "VRAM", value, d->vram_usage * 0.01f);
    } else {
        jayx_draw_metric_row(canvas, 3, "VRAM", "N/A", -1.0f);
    }

    jayx_draw_page_dots(canvas, JayxPageSystem);
}
