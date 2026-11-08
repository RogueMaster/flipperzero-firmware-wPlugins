#include <stdio.h>
#include <furi.h>
#include "renderer.h"

#include <gui/elements.h>
#include <string.h>

/* Vertical canvas: 64 x 128 */
#define LOTERIA_SCREEN_W      64
#define LOTERIA_SCREEN_H      128
#define LOTERIA_LINE_H        9
#define LOTERIA_MAX_LINES     24
#define LOTERIA_RHYME_VISIBLE 5
#define LOTERIA_RHYME_Y       72
#define LOTERIA_RHYME_WIDTH   60

static size_t loteria_wrap_words(
    Canvas* canvas,
    const char* text,
    int32_t width,
    FuriString** lines,
    size_t max_lines) {
    size_t line_count = 0;
    size_t len = strlen(text);
    size_t i = 0;
    FuriString* current = furi_string_alloc();

    while(i < len && line_count < max_lines) {
        while(i < len && text[i] == ' ')
            i++;
        if(i >= len) break;

        size_t word_start = i;
        while(i < len && text[i] != ' ')
            i++;
        size_t word_len = i - word_start;

        FuriString* trial = furi_string_alloc_set(current);
        if(furi_string_size(trial) > 0) {
            furi_string_push_back(trial, ' ');
        }
        for(size_t k = 0; k < word_len; k++) {
            furi_string_push_back(trial, text[word_start + k]);
        }

        if(canvas_string_width(canvas, furi_string_get_cstr(trial)) > (size_t)width) {
            if(furi_string_size(current) > 0) {
                lines[line_count++] = furi_string_alloc_set(current);
                furi_string_reset(current);
                furi_string_free(trial);
                i = word_start;
                continue;
            }
            furi_string_reset(current);
            for(size_t k = 0; k < word_len; k++) {
                furi_string_push_back(current, text[word_start + k]);
                if(canvas_string_width(canvas, furi_string_get_cstr(current)) > (size_t)width &&
                   furi_string_size(current) > 1) {
                    furi_string_left(current, furi_string_size(current) - 1);
                    lines[line_count++] = furi_string_alloc_set(current);
                    furi_string_reset(current);
                    furi_string_push_back(current, text[word_start + k]);
                    if(line_count >= max_lines) break;
                }
            }
            furi_string_free(trial);
        } else {
            furi_string_set(current, trial);
            furi_string_free(trial);
        }
    }

    if(furi_string_size(current) > 0 && line_count < max_lines) {
        lines[line_count++] = furi_string_alloc_set(current);
    }
    furi_string_free(current);
    return line_count;
}

void loteria_renderer_draw_card(Canvas* canvas, const LoteriaRenderModel* model) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(model->finished) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 32, 40, AlignCenter, AlignTop, "GAME OVER");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 32, 58, AlignCenter, AlignTop, "May every board");
        canvas_draw_str_aligned(canvas, 32, 68, AlignCenter, AlignTop, "find a winner!");
        return;
    }

    if(!model->card) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 32, 60, AlignCenter, AlignTop, "No card");
        return;
    }

    /* Progress */
    char progress[16];
    snprintf(progress, sizeof(progress), "%u/%u", model->sung, model->total);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 32, 1, AlignCenter, AlignTop, progress);

    /* Name under progress */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 32, 11, AlignCenter, AlignTop, model->card->name);

    /* Image centered in upper half */
    const Icon* icon = model->card->icon;
    uint16_t iw = icon_get_width(icon);
    uint16_t ih = icon_get_height(icon);
    int32_t ix = (LOTERIA_SCREEN_W - (int32_t)iw) / 2;
    int32_t iy = 24;
    if(iy + (int32_t)ih > LOTERIA_RHYME_Y - 2) {
        iy = LOTERIA_RHYME_Y - 2 - (int32_t)ih;
        if(iy < 22) iy = 22;
    }
    canvas_draw_icon(canvas, ix, iy, icon);

    /* Separator before rhyme */
    canvas_draw_line(canvas, 2, LOTERIA_RHYME_Y - 3, LOTERIA_SCREEN_W - 3, LOTERIA_RHYME_Y - 3);

    /* Full-width rhyme zone (scrollable with UP/DOWN) */
    canvas_set_font(canvas, FontSecondary);
    FuriString* lines[LOTERIA_MAX_LINES] = {0};
    size_t line_count = loteria_wrap_words(
        canvas, model->card->rhyme, LOTERIA_RHYME_WIDTH, lines, LOTERIA_MAX_LINES);

    int8_t scroll = model->rhyme_scroll;
    if(scroll < 0) scroll = 0;
    int8_t max_scroll = 0;
    if(line_count > LOTERIA_RHYME_VISIBLE) {
        max_scroll = (int8_t)(line_count - LOTERIA_RHYME_VISIBLE);
    }
    if(scroll > max_scroll) scroll = max_scroll;

    size_t drawn = 0;
    for(size_t i = (size_t)scroll; i < line_count && drawn < LOTERIA_RHYME_VISIBLE; i++) {
        canvas_draw_str_aligned(
            canvas,
            32,
            LOTERIA_RHYME_Y + (int32_t)drawn * LOTERIA_LINE_H,
            AlignCenter,
            AlignTop,
            furi_string_get_cstr(lines[i]));
        drawn++;
    }

    for(size_t i = 0; i < line_count; i++) {
        furi_string_free(lines[i]);
    }

    /* Scroll hints */
    if(scroll > 0) {
        canvas_draw_str_aligned(canvas, 60, LOTERIA_RHYME_Y - 2, AlignRight, AlignTop, "^");
    }
    if(scroll < max_scroll) {
        canvas_draw_str_aligned(canvas, 60, 118, AlignRight, AlignTop, "v");
    }

    if(model->showing_previous) {
        canvas_draw_str_aligned(canvas, 4, 118, AlignLeft, AlignTop, "<prev");
    } else if(model->paused) {
        canvas_draw_str_aligned(canvas, 4, 118, AlignLeft, AlignTop, "PAUSE");
    }
}
