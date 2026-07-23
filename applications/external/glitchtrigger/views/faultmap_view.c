#include "faultmap_view.h"
#include "../helpers/glitch_config.h"
#include <gui/elements.h>
#include <furi.h>
#include <string.h>
#include <stdio.h>

#define PLOT_X 12
#define PLOT_Y 13
#define PLOT_W 114
#define PLOT_H 37

struct FaultmapView {
    View* view;
    FaultmapViewCallback cb;
    void* ctx;
};

typedef struct {
    GlitchFaultMap map;
    uint16_t cur_col;
    uint16_t cur_row;
} FaultmapModel;

static int cell_px(const GlitchFaultMap* m, uint16_t col) {
    if(m->cols <= 1) return PLOT_X + PLOT_W / 2;
    return PLOT_X + 1 + (int)((uint32_t)col * (PLOT_W - 3) / (m->cols - 1));
}
static int cell_py(const GlitchFaultMap* m, uint16_t row) {
    if(m->rows <= 1) return PLOT_Y + PLOT_H / 2;
    return PLOT_Y + 1 + (int)((uint32_t)row * (PLOT_H - 3) / (m->rows - 1));
}

static void faultmap_view_draw(Canvas* canvas, void* model) {
    FaultmapModel* mo = model;
    GlitchFaultMap* m = &mo->map;
    canvas_clear(canvas);

    /* header */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Fault Map");
    canvas_set_font(canvas, FontSecondary);
    char hits[16];
    snprintf(hits, sizeof(hits), "hits %lu", (unsigned long)m->hits);
    canvas_draw_str_aligned(canvas, 126, 9, AlignRight, AlignBottom, hits);

    if(m->cols == 0) {
        elements_multiline_text_aligned(
            canvas, 64, 36, AlignCenter, AlignCenter, "No sweep data yet.\nRun a Sweep first.");
        return;
    }

    /* plot frame + axis hints */
    canvas_draw_frame(canvas, PLOT_X, PLOT_Y, PLOT_W, PLOT_H);
    canvas_draw_str(canvas, PLOT_X - 10, PLOT_Y + 6, "d");
    canvas_draw_str(canvas, PLOT_X + PLOT_W - 8, PLOT_Y + PLOT_H + 8, "w");

    /* hit cells */
    for(uint16_t r = 0; r < m->rows; r++) {
        int py = cell_py(m, r);
        for(uint16_t c = 0; c < m->cols; c++) {
            if(!glitch_map_get(m, c, r)) continue;
            int px = cell_px(m, c);
            canvas_draw_dot(canvas, px, py);
            if(m->rows == 1) { // 1D: emphasise with a short vertical bar
                canvas_draw_line(canvas, px, py - 2, px, py + 2);
            }
        }
    }

    /* cursor crosshair */
    int cx = cell_px(m, mo->cur_col);
    int cy = cell_py(m, mo->cur_row);
    for(int y = PLOT_Y + 1; y < PLOT_Y + PLOT_H - 1; y += 2)
        canvas_draw_dot(canvas, cx, y);
    for(int x = PLOT_X + 1; x < PLOT_X + PLOT_W - 1; x += 2)
        canvas_draw_dot(canvas, x, cy);
    canvas_draw_frame(canvas, cx - 1, cy - 1, 3, 3);

    /* readout for the cell under the cursor */
    char w[16], line[40];
    uint32_t wv = glitch_map_col_width(m, mo->cur_col);
    glitch_fmt_ns(wv, w, sizeof(w));
    bool hit = glitch_map_get(m, mo->cur_col, mo->cur_row);
    if(m->is_2d) {
        char d[16];
        uint32_t dv = glitch_map_row_delay(m, mo->cur_row);
        glitch_fmt_us(dv, d, sizeof(d));
        snprintf(line, sizeof(line), "w %s d %s %s", w, d, hit ? "HIT" : "-");
    } else {
        snprintf(line, sizeof(line), "w %s %s", w, hit ? "HIT" : "-");
    }
    canvas_draw_str(canvas, 2, PLOT_Y + PLOT_H + 8, line);
    canvas_draw_str_aligned(canvas, 126, 63, AlignRight, AlignBottom, "OK:save");
}

static bool faultmap_view_input(InputEvent* event, void* context) {
    FaultmapView* v = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    bool handled = false;
    with_view_model(
        v->view,
        FaultmapModel * m,
        {
            switch(event->key) {
            case InputKeyLeft:
                if(m->cur_col > 0) m->cur_col--;
                handled = true;
                break;
            case InputKeyRight:
                if(m->cur_col + 1 < m->map.cols) m->cur_col++;
                handled = true;
                break;
            case InputKeyUp:
                if(m->cur_row > 0) m->cur_row--;
                handled = true;
                break;
            case InputKeyDown:
                if(m->cur_row + 1 < m->map.rows) m->cur_row++;
                handled = true;
                break;
            default:
                break;
            }
        },
        true);

    if(!handled && event->key == InputKeyOk && event->type == InputTypeShort) {
        if(v->cb) v->cb(FaultmapViewEventExport, v->ctx);
        return true;
    }
    return handled; // Back propagates to the scene manager
}

FaultmapView* faultmap_view_alloc(void) {
    FaultmapView* v = malloc(sizeof(FaultmapView));
    v->cb = NULL;
    v->ctx = NULL;
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(FaultmapModel));
    view_set_draw_callback(v->view, faultmap_view_draw);
    view_set_input_callback(v->view, faultmap_view_input);
    return v;
}

void faultmap_view_free(FaultmapView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* faultmap_view_get_view(FaultmapView* v) {
    furi_assert(v);
    return v->view;
}

void faultmap_view_set_callback(FaultmapView* v, FaultmapViewCallback cb, void* ctx) {
    v->cb = cb;
    v->ctx = ctx;
}

void faultmap_view_set_map(FaultmapView* v, const GlitchFaultMap* map) {
    with_view_model(
        v->view,
        FaultmapModel * m,
        {
            m->map = *map;
            if(m->cur_col >= m->map.cols) m->cur_col = m->map.cols ? m->map.cols - 1 : 0;
            if(m->cur_row >= m->map.rows) m->cur_row = m->map.rows ? m->map.rows - 1 : 0;
        },
        true);
}
