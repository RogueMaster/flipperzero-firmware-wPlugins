/**
 * @file pq_ui_theme.c
 * @brief Canvas 绘制 helper 实现.
 */
#include "pq_ui_theme.h"

void pq_ui_draw_header(Canvas* canvas, const char* title) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, title);
    canvas_draw_line(canvas, 0, 12, 127, 12);
}

void pq_ui_draw_footer(Canvas* canvas, const char* hint) {
    canvas_draw_line(canvas, 0, 52, 127, 52);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignTop, hint);
}

void pq_ui_draw_splash(Canvas* canvas, uint8_t progress) {
    canvas_clear(canvas);

    /* Logo 区:大字 PINGEQUA + 小字 RF Lab. */
    canvas_set_font(canvas, FontBigNumbers);
    /* "PINGEQUA" 8 字符 BigNumbers 太宽,改回 Primary. */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignTop, "PINGEQUA");
    canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignTop, "RF Lab");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, "Initializing...");

    /* 进度条:外框 100x6,内填 progress 像素. */
    if(progress > 100) progress = 100;
    canvas_draw_frame(canvas, 14, 52, 100, 6);
    if(progress > 0) {
        canvas_draw_box(canvas, 15, 53, progress, 4);
    }
}
