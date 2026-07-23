#include "result_view.h"

#include <gui/elements.h>
#include <string.h>

typedef struct {
    ResultEntry entries[RESULT_MAX_ENTRIES];
    uint8_t count;
    uint8_t selected;
    char detail[28];
    bool unverified;
} ResultViewModel;

struct ResultView {
    View* view;
    ResultViewCallback callback;
    void* context;
};

/* --------------------------------------------------------------- draw ----- */

/* Vertical budget below the header, all of it tight:
 *   12..30  the rate, in big numerals (baseline 30)
 *   32..38  confidence bar
 *   41..48  provenance line (baseline 48)
 *   51..62  candidate ladder, and the OK hint beside it
 * The ladder's tallest chip stops at 51 so it cannot strike through the line
 * above it. */
#define RESULT_BAR_Y           (32)
#define RESULT_DETAIL_BASELINE (48)
#define RESULT_LADDER_BASE     (62)
#define RESULT_LADDER_MAX_H    (11)

static void result_draw_confidence(Canvas* canvas, uint8_t confidence) {
    const int x = 2, y = RESULT_BAR_Y, w = 88, h = 7;

    canvas_draw_frame(canvas, x, y, w, h);
    const int fill = ((w - 2) * confidence) / 100;
    if(fill > 0) canvas_draw_box(canvas, x + 1, y + 1, fill, h - 2);

    char buf[12];
    snprintf(buf, sizeof(buf), "%u%%", confidence);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, x + w + 4, y + h - 1, buf);
}

/** The runner-ups, as a row of chips. Height encodes confidence, so a close
 *  second is visible at a glance rather than hidden behind a keypress. */
static void result_draw_ladder(Canvas* canvas, const ResultViewModel* m) {
    const int y_base = RESULT_LADDER_BASE;
    const int chip_w = 7;
    const int gap = 3;
    const int x0 = 2;

    for(uint8_t i = 0; i < m->count; i++) {
        const int x = x0 + i * (chip_w + gap);
        int h = 2 + ((RESULT_LADDER_MAX_H - 2) * m->entries[i].confidence) / 100;
        if(h < 2) h = 2;

        if(i == m->selected) {
            canvas_draw_box(canvas, x, y_base - h, chip_w, h);
        } else {
            canvas_draw_frame(canvas, x, y_base - h, chip_w, h);
        }
    }
}

static void result_view_draw(Canvas* canvas, void* model) {
    const ResultViewModel* m = model;
    char buf[24];

    canvas_clear(canvas);

    if(m->count == 0) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignTop, "Nothing found");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignTop, "the line stayed quiet");
        elements_button_center(canvas, "Retry");
        return;
    }

    const ResultEntry* e = &m->entries[m->selected];

    /* header */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, (m->selected == 0) ? "BEST MATCH" : "ALTERNATIVE");
    canvas_draw_str_aligned(
        canvas, 126, 8, AlignRight, AlignBottom, e->verified ? "verified" : "timing only");
    canvas_draw_line(canvas, 0, 10, 127, 10);

    /* the answer */
    canvas_set_font(canvas, FontBigNumbers);
    snprintf(buf, sizeof(buf), "%lu", e->baud);
    canvas_draw_str(canvas, 2, 30, buf);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas, 126, 30, AlignRight, AlignBottom, hermes_framing_name(e->framing));

    result_draw_confidence(canvas, e->confidence);

    /* provenance - never just assert a number, say where it came from */
    canvas_set_font(canvas, FontSecondary);
    if(e->verified) {
        snprintf(buf, sizeof(buf), "%u B  %u%% text", e->bytes, e->printable_pct);
        canvas_draw_str(canvas, 2, RESULT_DETAIL_BASELINE, buf);
    } else if(m->detail[0]) {
        canvas_draw_str(canvas, 2, RESULT_DETAIL_BASELINE, m->detail);
    }

    if(e->note && e->note[0]) {
        canvas_draw_str_aligned(
            canvas, 126, RESULT_DETAIL_BASELINE, AlignRight, AlignBottom, e->note);
    }

    result_draw_ladder(canvas, m);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 126, 62, AlignRight, AlignBottom, "OK: console");
}

/* -------------------------------------------------------------- input ----- */

static bool result_view_input(InputEvent* event, void* context) {
    ResultView* rv = context;
    bool consumed = false;

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    if(event->key == InputKeyRight || event->key == InputKeyDown) {
        with_view_model(
            rv->view,
            ResultViewModel * m,
            {
                if(m->count > 0 && m->selected + 1 < m->count) m->selected++;
            },
            true);
        consumed = true;
    } else if(event->key == InputKeyLeft || event->key == InputKeyUp) {
        with_view_model(
            rv->view,
            ResultViewModel * m,
            {
                if(m->selected > 0) m->selected--;
            },
            true);
        consumed = true;
    } else if(event->key == InputKeyOk) {
        uint32_t index = 0;
        bool have = false;
        with_view_model(
            rv->view,
            ResultViewModel * m,
            {
                index = m->selected;
                have = m->count > 0;
            },
            false);
        if(rv->callback) rv->callback(rv->context, have ? index : UINT32_MAX);
        consumed = true;
    }

    return consumed;
}

/* ---------------------------------------------------------------- api ----- */

ResultView* result_view_alloc(void) {
    ResultView* rv = malloc(sizeof(ResultView));
    memset(rv, 0, sizeof(ResultView));
    rv->view = view_alloc();
    view_allocate_model(rv->view, ViewModelTypeLocking, sizeof(ResultViewModel));
    view_set_context(rv->view, rv);
    view_set_draw_callback(rv->view, result_view_draw);
    view_set_input_callback(rv->view, result_view_input);
    result_view_reset(rv);
    return rv;
}

void result_view_free(ResultView* rv) {
    furi_assert(rv);
    view_free(rv->view);
    free(rv);
}

View* result_view_get_view(ResultView* rv) {
    furi_assert(rv);
    return rv->view;
}

void result_view_set_callback(ResultView* rv, ResultViewCallback cb, void* context) {
    furi_assert(rv);
    rv->callback = cb;
    rv->context = context;
}

void result_view_reset(ResultView* rv) {
    furi_assert(rv);
    with_view_model(
        rv->view, ResultViewModel * m, { memset(m, 0, sizeof(ResultViewModel)); }, true);
}

void result_view_add(ResultView* rv, const ResultEntry* entry) {
    furi_assert(rv);
    furi_assert(entry);
    with_view_model(
        rv->view,
        ResultViewModel * m,
        {
            if(m->count < RESULT_MAX_ENTRIES) m->entries[m->count++] = *entry;
        },
        true);
}

bool result_view_get_entry(const ResultView* rv, uint32_t index, ResultEntry* out) {
    furi_assert(rv);
    furi_assert(out);

    bool found = false;
    with_view_model(
        rv->view,
        ResultViewModel * m,
        {
            if(index < m->count) {
                *out = m->entries[index];
                found = true;
            }
        },
        false);
    return found;
}

void result_view_set_detail(ResultView* rv, const char* detail) {
    furi_assert(rv);
    with_view_model(
        rv->view,
        ResultViewModel * m,
        {
            strncpy(m->detail, detail, sizeof(m->detail) - 1);
            m->detail[sizeof(m->detail) - 1] = '\0';
        },
        true);
}

void result_view_set_unverified(ResultView* rv, bool unverified) {
    furi_assert(rv);
    with_view_model(rv->view, ResultViewModel * m, { m->unverified = unverified; }, true);
}
