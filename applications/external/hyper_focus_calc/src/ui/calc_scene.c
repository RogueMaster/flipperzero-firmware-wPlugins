#include "include/ui/calc_scene.h"
#include "include/domain/hyperfocal.h"
#include <gui/canvas.h>
#include <math.h>
#include <stdio.h>

static void draw_dotted_hline(Canvas *canvas, int x0, int x1, int y) {
    canvas_set_color(canvas, ColorBlack);
    if (x0 > x1) {
        const int t = x0;
        x0 = x1;
        x1 = t;
    }
    for (int x = x0; x <= x1; x += 3) {
        canvas_draw_dot(canvas, x, y);
    }
}

/** Simple camera icon (left). */
static void draw_camera(Canvas *canvas, int cx, int cy) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, cx - 8, cy - 5, 16, 10);
    canvas_draw_disc(canvas, cx, cy, 3);
    canvas_draw_dot(canvas, cx - 6, cy - 6);
}

/** Simple tree icon (right). */
static void draw_tree(Canvas *canvas, int cx, int cy) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, cx, cy + 6, cx, cy + 12);
    const int tip = cy - 4;
    canvas_draw_line(canvas, cx - 8, cy + 4, cx, tip);
    canvas_draw_line(canvas, cx + 8, cy + 4, cx, tip);
    canvas_draw_line(canvas, cx - 8, cy + 4, cx + 8, cy + 4);
}

static void draw_tick_down(Canvas *canvas, int x, int y_line) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, x - 3, y_line - 6, x, y_line - 1);
    canvas_draw_line(canvas, x + 3, y_line - 6, x, y_line - 1);
}

void calc_scene_draw(Canvas *canvas, const CalcSceneModel *model) {
    if (!canvas || !model) {
        return;
    }

    canvas_clear(canvas);

    canvas_set_font(canvas, FontSecondary);
    char top[48];
    const float fstop = hyperfocal_fstop_at_index(model->fstop_idx);
    snprintf(top, sizeof(top), "Focal: %umm  f/%.1f", (unsigned)model->focal_mm, (double)fstop);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, top);

    char coc_line[40];
    snprintf(coc_line, sizeof(coc_line), "CoC: %.3f mm", (double)model->coc_mm);
    canvas_draw_str_aligned(canvas, 64, 11, AlignCenter, AlignTop, coc_line);

    const int mid_y = 34;
    const int x_cam = 18;
    const int x_tree = 110;
    draw_camera(canvas, x_cam, mid_y);
    draw_tree(canvas, x_tree, mid_y);

    const int y_line = mid_y;
    draw_dotted_hline(canvas, x_cam + 10, x_tree - 10, y_line);

    const float t = fminf(fmaxf(model->h_meters / 100.0f, 0.0f), 1.0f);
    const int x_l = x_cam + 10;
    const int x_r = x_tree - 10;
    const int tick_x = x_l + (int)((float)(x_r - x_l) * t);
    draw_tick_down(canvas, tick_x, y_line);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignBottom, model->result_text);
}
