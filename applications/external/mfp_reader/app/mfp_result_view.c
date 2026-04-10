#include "mfp_result_view.h"

#include <gui/view.h>
#include <gui/elements.h>
#include <furi.h>
#include <input/input.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

struct MfpResultView {
    View* view;
    MfpResultViewActionsCallback actions_cb;
    void* actions_ctx;
};

/* ---- Drawing ---- */

static void draw_sector_grid(
    Canvas* canvas, const MfpResultViewModel* m, uint8_t x, uint8_t y, uint8_t max_width) {
    if(m->total_sectors == 0) return;

    uint8_t cell_size = 6;
    uint8_t cols = m->total_sectors;
    if((cell_size + 1) * cols > max_width) {
        cols = (m->total_sectors + 1) / 2;
        if((cell_size + 1) * cols > max_width) {
            cell_size = 4;
        }
    }

    for(uint8_t s = 0; s < m->total_sectors; s++) {
        uint8_t col = s % cols;
        uint8_t row = s / cols;
        uint8_t cx = x + col * (cell_size + 1);
        uint8_t cy = y + row * (cell_size + 1);
        uint8_t state = m->sector_states[s];

        if(state == MFP_RESULT_SECTOR_KEY_AB) {
            /* Both keys — fully filled */
            canvas_draw_box(canvas, cx, cy, cell_size, cell_size);
        } else if(state == MFP_RESULT_SECTOR_KEY_A) {
            /* Only Key A — top half filled */
            canvas_draw_frame(canvas, cx, cy, cell_size, cell_size);
            canvas_draw_box(canvas, cx + 1, cy + 1, cell_size - 2, cell_size / 2 - 1);
        } else if(state == MFP_RESULT_SECTOR_KEY_B) {
            /* Only Key B — bottom half filled */
            canvas_draw_frame(canvas, cx, cy, cell_size, cell_size);
            canvas_draw_box(canvas, cx + 1, cy + cell_size / 2, cell_size - 2, cell_size / 2 - 1);
        } else {
            /* No keys — dotted corners */
            canvas_draw_dot(canvas, cx, cy);
            canvas_draw_dot(canvas, cx + cell_size - 1, cy);
            canvas_draw_dot(canvas, cx, cy + cell_size - 1);
            canvas_draw_dot(canvas, cx + cell_size - 1, cy + cell_size - 1);
        }
    }
}

static void draw_header(Canvas* canvas, const MfpResultViewModel* m) {
    /* Matches the dump view header: single-line FontSecondary "SLx <size>  <UID>"
     * followed by a horizontal separator. Keeps the visual style
     * consistent across the scan/result/emulate screens. */
    canvas_set_font(canvas, FontSecondary);
    char header[32];
    int n = snprintf(
        header, sizeof(header), "SL%u %s  ",
        (unsigned)m->sl, m->card_size == MfpSize4K ? "4K" : "2K");
    for(uint8_t i = 0; i < m->uid_len && n < (int)sizeof(header) - 3; i++) {
        n += snprintf(header + n, sizeof(header) - (size_t)n, "%02X", m->uid[i]);
    }
    canvas_draw_str(canvas, 2, 8, header);
    canvas_draw_line(canvas, 0, 10, 127, 10);
}

static void draw_stats(Canvas* canvas, const MfpResultViewModel* m) {
    /* Fraction + label on one line. Measure the fraction in
     * FontPrimary so the label starts right after it with a clean
     * gap, instead of a hardcoded x that collides for wide fonts. */
    canvas_set_font(canvas, FontPrimary);
    char frac[16];
    snprintf(
        frac, sizeof(frac), "%u/%u",
        (unsigned)m->sectors_ok, (unsigned)m->total_sectors);
    canvas_draw_str(canvas, 2, 20, frac);
    uint16_t frac_width = canvas_string_width(canvas, frac);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2 + frac_width + 4, 20, "sectors read");

    /* Key-type breakdown on the next line */
    if(m->sectors_ab > 0 || m->sectors_a_only > 0 || m->sectors_b_only > 0) {
        char ks[32];
        snprintf(
            ks, sizeof(ks), "AB:%u  A:%u  B:%u",
            (unsigned)m->sectors_ab,
            (unsigned)m->sectors_a_only,
            (unsigned)m->sectors_b_only);
        canvas_draw_str(canvas, 2, 29, ks);
    }
}

static void result_view_draw(Canvas* canvas, void* ctx) {
    MfpResultViewModel* m = ctx;
    canvas_clear(canvas);

    draw_header(canvas, m);
    draw_stats(canvas, m);

    /* Sector activity grid below the stats lines */
    draw_sector_grid(canvas, m, 2, 34, 124);

    /* Right button hint — has its own framing, no extra divider. */
    elements_button_right(canvas, "Actions");
}

static bool result_view_input(InputEvent* event, void* ctx) {
    MfpResultView* inst = ctx;
    if(event->type == InputTypeShort && event->key == InputKeyRight) {
        if(inst->actions_cb) {
            inst->actions_cb(inst->actions_ctx);
            return true;
        }
    }
    return false;
}

/* ---- Public API ---- */

MfpResultView* mfp_result_view_alloc(void) {
    MfpResultView* inst = malloc(sizeof(MfpResultView));
    memset(inst, 0, sizeof(*inst));
    inst->view = view_alloc();
    view_allocate_model(inst->view, ViewModelTypeLocking, sizeof(MfpResultViewModel));
    view_set_context(inst->view, inst);
    view_set_draw_callback(inst->view, result_view_draw);
    view_set_input_callback(inst->view, result_view_input);
    return inst;
}

void mfp_result_view_free(MfpResultView* inst) {
    if(!inst) return;
    view_free(inst->view);
    free(inst);
}

View* mfp_result_view_get_view(MfpResultView* inst) {
    return inst->view;
}

void mfp_result_view_set_actions_callback(
    MfpResultView* inst, MfpResultViewActionsCallback cb, void* ctx) {
    inst->actions_cb = cb;
    inst->actions_ctx = ctx;
}

void mfp_result_view_update(
    MfpResultView* inst,
    MfpCardSize card_size,
    MfpSecurityLevel sl,
    const uint8_t* uid,
    uint8_t uid_len,
    uint8_t total_sectors,
    const uint8_t* sector_states) {
    with_view_model(
        inst->view,
        MfpResultViewModel * m,
        {
            memset(m, 0, sizeof(*m));
            m->card_size = card_size;
            m->sl = sl;
            m->uid_len = uid_len;
            if(uid_len > sizeof(m->uid)) uid_len = sizeof(m->uid);
            memcpy(m->uid, uid, uid_len);
            m->total_sectors = total_sectors;

            for(uint8_t s = 0; s < total_sectors && s < MFP_SECTORS_4K; s++) {
                uint8_t st = sector_states[s];
                m->sector_states[s] = st;
                if(st == MFP_RESULT_SECTOR_KEY_AB) {
                    m->sectors_ok++;
                    m->sectors_ab++;
                } else if(st == MFP_RESULT_SECTOR_KEY_A) {
                    m->sectors_ok++;
                    m->sectors_a_only++;
                } else if(st == MFP_RESULT_SECTOR_KEY_B) {
                    m->sectors_ok++;
                    m->sectors_b_only++;
                }
            }
        },
        true);
}
