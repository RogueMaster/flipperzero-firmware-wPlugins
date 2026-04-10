#include "mfp_dump_view.h"

#include <gui/view.h>
#include <gui/elements.h>
#include <furi.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

struct MfpDumpView {
    View* view;
};

/* ---- Drawing ---- */

static void draw_sector_grid(
    Canvas* canvas, const MfpDumpViewModel* m, uint8_t x, uint8_t y, uint8_t max_width) {
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

        switch(m->sector_states[s]) {
        case MFP_DUMP_SECTOR_OK:
            canvas_draw_box(canvas, cx, cy, cell_size, cell_size);
            break;
        case MFP_DUMP_SECTOR_FAILED:
            /* Outlined box with diagonal cross */
            canvas_draw_frame(canvas, cx, cy, cell_size, cell_size);
            canvas_draw_line(
                canvas, cx + 1, cy + 1, cx + cell_size - 2, cy + cell_size - 2);
            canvas_draw_line(
                canvas, cx + cell_size - 2, cy + 1, cx + 1, cy + cell_size - 2);
            break;
        case MFP_DUMP_SECTOR_TRYING: {
            /* Animated marker: outlined box with center dot. The scene
             * forces periodic redraws so the "current" sector feels live. */
            canvas_draw_frame(canvas, cx, cy, cell_size, cell_size);
            canvas_draw_box(
                canvas, cx + cell_size / 2 - 1, cy + cell_size / 2 - 1, 2, 2);
            break;
        }
        case MFP_DUMP_SECTOR_PENDING:
        default:
            /* Empty sector slot — dotted corners */
            canvas_draw_dot(canvas, cx, cy);
            canvas_draw_dot(canvas, cx + cell_size - 1, cy);
            canvas_draw_dot(canvas, cx, cy + cell_size - 1);
            canvas_draw_dot(canvas, cx + cell_size - 1, cy + cell_size - 1);
            break;
        }
    }
}

static void draw_header(Canvas* canvas, const MfpDumpViewModel* m) {
    canvas_set_font(canvas, FontSecondary);
    char header[32];
    int n = snprintf(
        header, sizeof(header), "SL3 %s  ", m->card_size == MfpSize4K ? "4K" : "2K");
    for(uint8_t i = 0; i < m->uid_len && n < (int)sizeof(header) - 3; i++) {
        n += snprintf(header + n, sizeof(header) - (size_t)n, "%02X", m->uid[i]);
    }
    canvas_draw_str(canvas, 2, 8, header);
    canvas_draw_line(canvas, 0, 10, 127, 10);
}

static void draw_scanning(Canvas* canvas, const MfpDumpViewModel* m) {
    draw_header(canvas, m);

    /* Status line with progress fraction */
    canvas_set_font(canvas, FontPrimary);
    char buf[32];
    snprintf(
        buf, sizeof(buf), "%u/%u sectors",
        (unsigned)m->sectors_done, (unsigned)m->total_sectors);
    canvas_draw_str(canvas, 2, 22, buf);

    /* Keys info (right aligned) */
    canvas_set_font(canvas, FontSecondary);
    snprintf(
        buf, sizeof(buf), "%lu keys",
        (unsigned long)(m->default_keys + m->dict_keys));
    canvas_draw_str_aligned(canvas, 125, 22, AlignRight, AlignBottom, buf);

    /* Sector grid */
    draw_sector_grid(canvas, m, 2, 28, 124);

    /* Progress bar for a second visual cue */
    uint8_t bar_y = 44;
    canvas_draw_frame(canvas, 2, bar_y, 124, 4);
    if(m->total_sectors > 0) {
        uint16_t fill = (uint16_t)m->sectors_done * 122u / m->total_sectors;
        if(fill > 0) canvas_draw_box(canvas, 3, bar_y + 1, fill, 2);
    }

    /* Footer */
    canvas_draw_line(canvas, 0, 53, 127, 53);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 62, "Hold card steady");
    canvas_draw_str_aligned(canvas, 125, 62, AlignRight, AlignBottom, "Back=stop");
}

static void draw_terminal(Canvas* canvas, const MfpDumpViewModel* m, const char* title) {
    draw_header(canvas, m);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignBottom, title);

    /* Sector grid in the middle even on the terminal screens —
     * users can glance at it while the scene is transitioning. */
    draw_sector_grid(canvas, m, 2, 28, 124);

    canvas_set_font(canvas, FontSecondary);
    char buf[32];
    snprintf(
        buf, sizeof(buf), "%u/%u sectors read",
        (unsigned)m->sectors_ok, (unsigned)m->total_sectors);
    canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignBottom, buf);
}

static void dump_view_draw(Canvas* canvas, void* ctx) {
    MfpDumpViewModel* m = ctx;
    canvas_clear(canvas);
    switch(m->state) {
    case MfpDumpViewStateComplete:
        draw_terminal(canvas, m, "Done");
        break;
    case MfpDumpViewStateCardLost:
        draw_terminal(canvas, m, "Card lost");
        break;
    case MfpDumpViewStateNoKeys:
        draw_terminal(canvas, m, "No keys");
        break;
    case MfpDumpViewStateScanning:
    default:
        draw_scanning(canvas, m);
        break;
    }
}

/* ---- Public API ---- */

MfpDumpView* mfp_dump_view_alloc(void) {
    MfpDumpView* inst = malloc(sizeof(MfpDumpView));
    inst->view = view_alloc();
    view_allocate_model(inst->view, ViewModelTypeLocking, sizeof(MfpDumpViewModel));
    view_set_draw_callback(inst->view, dump_view_draw);
    return inst;
}

void mfp_dump_view_free(MfpDumpView* inst) {
    if(!inst) return;
    view_free(inst->view);
    free(inst);
}

View* mfp_dump_view_get_view(MfpDumpView* inst) {
    return inst->view;
}

void mfp_dump_view_reset(
    MfpDumpView* inst,
    MfpCardSize card_size,
    uint8_t total_sectors,
    const uint8_t* uid,
    uint8_t uid_len,
    uint32_t default_keys,
    uint32_t dict_keys) {
    with_view_model(
        inst->view,
        MfpDumpViewModel * m,
        {
            memset(m, 0, sizeof(*m));
            m->state = MfpDumpViewStateScanning;
            m->card_size = card_size;
            m->total_sectors = total_sectors;
            m->uid_len = uid_len;
            memcpy(m->uid, uid, uid_len);
            m->default_keys = default_keys;
            m->dict_keys = dict_keys;
        },
        true);
}

void mfp_dump_view_sync(MfpDumpView* inst, const MfpApp* app) {
    with_view_model(
        inst->view,
        MfpDumpViewModel * m,
        {
            m->sectors_done = app->scan_sectors_done;
            m->sectors_ok = app->scan_sectors_ok;
            m->current_sector = app->scan_sectors_done; /* next to be scanned */
            for(uint8_t s = 0; s < m->total_sectors && s < MFP_SECTORS_4K; s++) {
                MfpSectorStatus st = app->sector_results[s].status;
                if(st == MfpSectorOk) {
                    m->sector_states[s] = MFP_DUMP_SECTOR_OK;
                } else if(st == MfpSectorAuthFail || st == MfpSectorReadFail) {
                    m->sector_states[s] = MFP_DUMP_SECTOR_FAILED;
                } else if(s == m->current_sector && m->state == MfpDumpViewStateScanning) {
                    m->sector_states[s] = MFP_DUMP_SECTOR_TRYING;
                } else {
                    m->sector_states[s] = MFP_DUMP_SECTOR_PENDING;
                }
            }
        },
        true);
}

void mfp_dump_view_set_state(MfpDumpView* inst, MfpDumpViewState state) {
    with_view_model(
        inst->view,
        MfpDumpViewModel * m,
        {
            m->state = state;
        },
        true);
}
