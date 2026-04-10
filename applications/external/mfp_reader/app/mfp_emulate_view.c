#include "mfp_emulate_view.h"

#include <gui/view.h>
#include <gui/elements.h>
#include <furi.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

struct MfpEmulateView {
    View* view;
};

/* ---- Rendering ---- */

static void draw_sector_grid(
    Canvas* canvas,
    const MfpEmulateViewModel* m,
    uint8_t x,
    uint8_t y,
    uint8_t max_width) {
    if(m->total_sectors == 0) return;

    /* Pick cell size to fit in the available width. Each cell has 1px
     * horizontal gap. Height = cell_size. Two rows if single-row doesn't fit. */
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
        uint8_t flags = m->sector_flags[s];

        if(flags & MFP_SECTOR_WRITTEN) {
            /* Solid filled box with inverted tick for "written" */
            canvas_draw_box(canvas, cx, cy, cell_size, cell_size);
        } else if(flags & MFP_SECTOR_READ) {
            /* Half-filled (top) for "read" */
            canvas_draw_box(canvas, cx, cy, cell_size, cell_size / 2);
            canvas_draw_frame(canvas, cx, cy, cell_size, cell_size);
        } else if(flags & MFP_SECTOR_AUTHED) {
            /* Outlined with single center dot for "authed only" */
            canvas_draw_frame(canvas, cx, cy, cell_size, cell_size);
            canvas_draw_dot(canvas, cx + cell_size / 2, cy + cell_size / 2);
        } else if(flags & MFP_SECTOR_LOADED) {
            /* Outlined only — loaded but untouched */
            canvas_draw_frame(canvas, cx, cy, cell_size, cell_size);
        } else {
            /* No keys — dim dotted outline */
            canvas_draw_dot(canvas, cx, cy);
            canvas_draw_dot(canvas, cx + cell_size - 1, cy);
            canvas_draw_dot(canvas, cx, cy + cell_size - 1);
            canvas_draw_dot(canvas, cx + cell_size - 1, cy + cell_size - 1);
        }
    }
}

static void draw_running(Canvas* canvas, const MfpEmulateViewModel* m) {
    /* Header line: card type + UID prefix */
    canvas_set_font(canvas, FontSecondary);
    char header[32];
    int n = snprintf(
        header, sizeof(header), "SL3 %s  ", m->card_size == MfpSize4K ? "4K" : "2K");
    for(uint8_t i = 0; i < m->uid_len && n < (int)sizeof(header) - 3; i++) {
        n += snprintf(header + n, sizeof(header) - (size_t)n, "%02X", m->uid[i]);
    }
    canvas_draw_str(canvas, 2, 8, header);
    canvas_draw_line(canvas, 0, 10, 127, 10);

    /* Counters row: Auth / Read / Write */
    canvas_set_font(canvas, FontPrimary);
    char buf[16];

    snprintf(buf, sizeof(buf), "A%lu", (unsigned long)m->auths);
    canvas_draw_str(canvas, 2, 22, buf);

    snprintf(buf, sizeof(buf), "R%lu", (unsigned long)m->reads);
    canvas_draw_str(canvas, 44, 22, buf);

    snprintf(buf, sizeof(buf), "W%lu", (unsigned long)m->writes);
    canvas_draw_str(canvas, 86, 22, buf);

    /* Last op line */
    canvas_set_font(canvas, FontSecondary);
    if(m->last_op == 'A') {
        snprintf(buf, sizeof(buf), "Auth s%02u", (unsigned)m->last_sector);
    } else if(m->last_op == 'R') {
        snprintf(buf, sizeof(buf), "Read b%02u s%02u",
                 (unsigned)m->last_block, (unsigned)m->last_sector);
    } else if(m->last_op == 'W') {
        snprintf(buf, sizeof(buf), "Write b%02u s%02u",
                 (unsigned)m->last_block, (unsigned)m->last_sector);
    } else {
        snprintf(buf, sizeof(buf), "Waiting...");
    }
    canvas_draw_str(canvas, 2, 32, buf);

    /* Sector activity grid below the last-op line */
    draw_sector_grid(canvas, m, 2, 36, 124);

    /* Footer: mode + back hint */
    canvas_draw_line(canvas, 0, 53, 127, 53);
    canvas_draw_str(canvas, 2, 62, m->allow_overwrite ? "Writable" : "Read-only");
    canvas_draw_str(canvas, 80, 62, "Back=stop");
}

static void draw_summary(Canvas* canvas, const MfpEmulateViewModel* m) {
    canvas_set_font(canvas, FontPrimary);
    const char* hdr;
    if(m->writes > 0 && m->allow_overwrite && m->modified_saved) {
        hdr = "Clone updated";
    } else if(m->writes > 0 && !m->allow_overwrite) {
        hdr = "Writes discarded";
    } else if(m->writes > 0) {
        hdr = "Save failed";
    } else {
        hdr = "Session done";
    }
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignCenter, hdr);

    canvas_set_font(canvas, FontSecondary);
    char buf[32];
    snprintf(
        buf, sizeof(buf), "A:%lu  R:%lu  W:%lu",
        (unsigned long)m->auths, (unsigned long)m->reads, (unsigned long)m->writes);
    canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignCenter, buf);

    if(m->writes > 0 && m->modified_saved && m->summary_path[0]) {
        canvas_draw_str_aligned(
            canvas, 64, 40, AlignCenter, AlignCenter, m->summary_path);
    } else if(m->writes > 0 && !m->allow_overwrite) {
        canvas_draw_str_aligned(
            canvas, 64, 40, AlignCenter, AlignCenter, "(read-only mode)");
    }

    canvas_draw_line(canvas, 0, 53, 127, 53);
    canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "Back to return");
}

static void emulate_view_draw(Canvas* canvas, void* ctx) {
    MfpEmulateViewModel* m = ctx;
    canvas_clear(canvas);
    if(m->state == MfpEmulateViewStateSummary) {
        draw_summary(canvas, m);
    } else {
        draw_running(canvas, m);
    }
}

/* ---- Public API ---- */

MfpEmulateView* mfp_emulate_view_alloc(void) {
    MfpEmulateView* inst = malloc(sizeof(MfpEmulateView));
    inst->view = view_alloc();
    view_allocate_model(inst->view, ViewModelTypeLocking, sizeof(MfpEmulateViewModel));
    view_set_context(inst->view, inst);
    view_set_draw_callback(inst->view, emulate_view_draw);
    return inst;
}

void mfp_emulate_view_free(MfpEmulateView* inst) {
    if(!inst) return;
    view_free(inst->view);
    free(inst);
}

View* mfp_emulate_view_get_view(MfpEmulateView* inst) {
    return inst->view;
}

void mfp_emulate_view_reset(
    MfpEmulateView* inst,
    MfpCardSize card_size,
    uint8_t total_sectors,
    const uint8_t* uid,
    uint8_t uid_len,
    const bool* sectors_loaded,
    bool allow_overwrite) {
    with_view_model(
        inst->view,
        MfpEmulateViewModel * m,
        {
            memset(m, 0, sizeof(*m));
            m->state = MfpEmulateViewStateRunning;
            m->card_size = card_size;
            m->total_sectors = total_sectors;
            m->uid_len = uid_len;
            memcpy(m->uid, uid, uid_len);
            m->allow_overwrite = allow_overwrite;
            for(uint8_t s = 0; s < total_sectors && s < MFP_SECTORS_4K; s++) {
                if(sectors_loaded && sectors_loaded[s]) {
                    m->sector_flags[s] = MFP_SECTOR_LOADED;
                }
            }
        },
        true);
}

void mfp_emulate_view_record(
    MfpEmulateView* inst,
    uint32_t auths,
    uint32_t reads,
    uint32_t writes,
    uint8_t last_op,
    uint8_t last_sector,
    uint8_t last_block) {
    with_view_model(
        inst->view,
        MfpEmulateViewModel * m,
        {
            m->auths = auths;
            m->reads = reads;
            m->writes = writes;
            m->last_op = last_op;
            m->last_sector = last_sector;
            m->last_block = last_block;
            if(last_sector < MFP_SECTORS_4K) {
                if(last_op == 'A') m->sector_flags[last_sector] |= MFP_SECTOR_AUTHED;
                else if(last_op == 'R')
                    m->sector_flags[last_sector] |= MFP_SECTOR_AUTHED | MFP_SECTOR_READ;
                else if(last_op == 'W')
                    m->sector_flags[last_sector] |=
                        MFP_SECTOR_AUTHED | MFP_SECTOR_WRITTEN;
            }
        },
        true);
}

void mfp_emulate_view_show_summary(
    MfpEmulateView* inst, bool modified_saved, const char* summary_path) {
    with_view_model(
        inst->view,
        MfpEmulateViewModel * m,
        {
            m->state = MfpEmulateViewStateSummary;
            m->modified_saved = modified_saved;
            if(summary_path) {
                strlcpy(m->summary_path, summary_path, sizeof(m->summary_path));
            } else {
                m->summary_path[0] = '\0';
            }
        },
        true);
}
