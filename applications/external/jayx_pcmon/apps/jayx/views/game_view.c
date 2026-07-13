#include "../jayx.h"

static void draw_fit_str(Canvas* canvas, int x, int y, Align h, Align v, const char* text, size_t max_w) {
    FuriString* fs = furi_string_alloc_set(text);
    elements_string_fit_width(canvas, fs, max_w);
    canvas_draw_str_aligned(canvas, x, y, h, v, furi_string_get_cstr(fs));
    furi_string_free(fs);
}

void draw_game_view(Canvas* canvas, JayxApp* app) {
    JayxLivePacket* d = &app->data;
    char name[JAYX_GAME_NAME_LEN + 1];
    char fps_buf[12];
    char ms_buf[16];

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    jayx_copy_field(name, sizeof(name), d->game_name, JAYX_GAME_NAME_LEN);
    if(name[0] == '\0') {
        snprintf(name, sizeof(name), "Desktop");
    }

    /* Title strip */
    canvas_draw_box(canvas, 0, 0, 128, 11);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    draw_fit_str(canvas, 64, 8, AlignCenter, AlignBottom, name, 120);
    canvas_set_color(canvas, ColorBlack);

    if(d->fps > 0) {
        snprintf(fps_buf, sizeof(fps_buf), "%u", d->fps);
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, fps_buf);

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignBottom, "FPS");

        float ms = 1000.0f / (float)d->fps;
        snprintf(ms_buf, sizeof(ms_buf), "%.1fms", (double)ms);
        canvas_draw_str(canvas, 2, 56, ms_buf);
        canvas_draw_str_aligned(canvas, 126, 56, AlignRight, AlignBottom, "RTSS");

        /* Target bar (relative to 240 FPS) */
        float pct = (float)d->fps / 240.0f;
        if(pct > 1.0f) pct = 1.0f;
        canvas_draw_frame(canvas, 40, 50, 48, 4);
        uint8_t fill = (uint8_t)(46.0f * pct);
        if(fill > 0) canvas_draw_box(canvas, 41, 51, fill, 2);
    } else {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, "--");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignBottom, "No FPS signal");
        canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignBottom, "RTSS + focus game");
    }

    jayx_draw_page_dots(canvas, JayxPageGame);
}
