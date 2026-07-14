#include "fas_list_view.h"

#include <furi.h>
#include <gui/canvas.h>
#include <gui/elements.h>
#include <input/input.h>
#include <string.h>
#include <stdio.h>

/* ── Layout constants ─────────────────────────────────────────────────── */
#define VISIBLE_ROWS 4
#define ROW_HEIGHT   14
#define LIST_Y_START 2
#define CHECKBOX_W   10
#define CHECKBOX_H   10

/* ── Model (stored inside the View) ──────────────────────────────────── */
typedef struct {
    char labels[FAS_LIST_MAX_ITEMS][FAS_LIST_LABEL_LEN];
    bool has_checkbox[FAS_LIST_MAX_ITEMS];
    bool checked[FAS_LIST_MAX_ITEMS];
    int count;
    int cursor;
    int scroll; /* index of the topmost visible item */

    FasListCallback callback;
    void* callback_ctx;
} FasListViewModel;

struct FasListView {
    View* view;
};

/* ── Draw ─────────────────────────────────────────────────────────────── */
static void fas_list_draw(Canvas* canvas, void* model_ptr) {
    FasListViewModel* m = model_ptr;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    for(int row = 0; row < VISIBLE_ROWS; row++) {
        int idx = m->scroll + row;
        if(idx >= m->count) break;

        int y_top = LIST_Y_START + row * ROW_HEIGHT;
        int y_text = y_top + ROW_HEIGHT - 3; /* baseline for FontSecondary */
        bool sel = (idx == m->cursor);

        if(sel) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, 0, y_top, 128, ROW_HEIGHT);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }

        int x = 2;
        if(m->has_checkbox[idx]) {
            /* Draw checkbox border */
            int cb_y = y_top + (ROW_HEIGHT - CHECKBOX_H) / 2;
            if(sel) {
                canvas_set_color(canvas, ColorWhite);
            }
            canvas_draw_rframe(canvas, x, cb_y, CHECKBOX_W, CHECKBOX_H, 1);
            if(m->checked[idx]) {
                /* Draw a small X inside */
                canvas_draw_line(
                    canvas, x + 2, cb_y + 2, x + CHECKBOX_W - 3, cb_y + CHECKBOX_H - 3);
                canvas_draw_line(
                    canvas, x + CHECKBOX_W - 3, cb_y + 2, x + 2, cb_y + CHECKBOX_H - 3);
            }
            if(sel) canvas_set_color(canvas, ColorWhite);
            x += CHECKBOX_W + 3;
        }

        /* Truncate label to fit available width */
        int max_chars = m->has_checkbox[idx] ? 17 : 20;
        char display[FAS_LIST_LABEL_LEN];
        snprintf(display, sizeof(display), "%.*s", max_chars, m->labels[idx]);
        canvas_draw_str(canvas, x, y_text, display);

        if(sel) canvas_set_color(canvas, ColorBlack);
    }

    /* Scroll indicators */
    if(m->scroll > 0) {
        canvas_draw_str(canvas, 120, 10, "^");
    }
    if(m->scroll + VISIBLE_ROWS < m->count) {
        canvas_draw_str(canvas, 120, 60, "v");
    }
}

/* ── Input ────────────────────────────────────────────────────────────── */
static bool fas_list_input(InputEvent* event, void* context) {
    FasListView* lv = context;
    bool consumed = false;

    /* ── Navigation (short + repeat for smooth scrolling) ─────────────── */
    if((event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        if(event->key == InputKeyUp) {
            with_view_model(
                lv->view,
                FasListViewModel * m,
                {
                    if(m->cursor > 0) {
                        m->cursor--;
                        if(m->cursor < m->scroll) m->scroll = m->cursor;
                    }
                },
                true);
            consumed = true;
        } else if(event->key == InputKeyDown) {
            with_view_model(
                lv->view,
                FasListViewModel * m,
                {
                    if(m->cursor < m->count - 1) {
                        m->cursor++;
                        if(m->cursor >= m->scroll + VISIBLE_ROWS)
                            m->scroll = m->cursor - VISIBLE_ROWS + 1;
                    }
                },
                true);
            consumed = true;
        }
    }

    /* ── OK short: toggle checkbox, fire callback ─────────────────────── */
    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        int cursor = 0;
        FasListCallback cb = NULL;
        void* cb_ctx = NULL;

        with_view_model(
            lv->view,
            FasListViewModel * m,
            {
                cursor = m->cursor;
                cb = m->callback;
                cb_ctx = m->callback_ctx;
                if(m->count > 0 && m->has_checkbox[m->cursor]) {
                    m->checked[m->cursor] = !m->checked[m->cursor];
                }
            },
            true);

        if(cb) cb(cb_ctx, cursor, FasListEvtOkShort);
        consumed = true;
    }

    /* ── OK long: fire callback (no checkbox toggle) ──────────────────── */
    if(event->type == InputTypeLong && event->key == InputKeyOk) {
        int cursor = 0;
        FasListCallback cb = NULL;
        void* cb_ctx = NULL;

        with_view_model(
            lv->view,
            FasListViewModel * m,
            {
                cursor = m->cursor;
                cb = m->callback;
                cb_ctx = m->callback_ctx;
            },
            false);

        if(cb) cb(cb_ctx, cursor, FasListEvtOkLong);
        consumed = true;
    }

    /* ── Right arrow: "Done / Proceed" ───────────────────────────────── */
    if(event->type == InputTypeShort && event->key == InputKeyRight) {
        int cursor = 0;
        FasListCallback cb = NULL;
        void* cb_ctx = NULL;

        with_view_model(
            lv->view,
            FasListViewModel * m,
            {
                cursor = m->cursor;
                cb = m->callback;
                cb_ctx = m->callback_ctx;
            },
            false);

        if(cb) cb(cb_ctx, cursor, FasListEvtRight);
        consumed = true;
    }

    return consumed;
}

/* ── Public API ───────────────────────────────────────────────────────── */

FasListView* fas_list_view_alloc(void) {
    FasListView* lv = malloc(sizeof(FasListView));
    lv->view = view_alloc();
    view_allocate_model(lv->view, ViewModelTypeLocking, sizeof(FasListViewModel));
    view_set_draw_callback(lv->view, fas_list_draw);
    view_set_input_callback(lv->view, fas_list_input);
    view_set_context(lv->view, lv);

    with_view_model(
        lv->view, FasListViewModel * m, { memset(m, 0, sizeof(FasListViewModel)); }, false);

    return lv;
}

void fas_list_view_free(FasListView* lv) {
    view_free(lv->view);
    free(lv);
}

View* fas_list_view_get_view(FasListView* lv) {
    return lv->view;
}

void fas_list_view_set_callback(FasListView* lv, FasListCallback cb, void* ctx) {
    with_view_model(
        lv->view,
        FasListViewModel * m,
        {
            m->callback = cb;
            m->callback_ctx = ctx;
        },
        false);
}

void fas_list_view_reset(FasListView* lv) {
    with_view_model(
        lv->view,
        FasListViewModel * m,
        {
            m->count = 0;
            m->cursor = 0;
            m->scroll = 0;
        },
        true);
}

void fas_list_view_add_item(FasListView* lv, const char* label, bool has_checkbox, bool checked) {
    with_view_model(
        lv->view,
        FasListViewModel * m,
        {
            if(m->count < FAS_LIST_MAX_ITEMS) {
                int i = m->count;
                strncpy(m->labels[i], label, FAS_LIST_LABEL_LEN - 1);
                m->labels[i][FAS_LIST_LABEL_LEN - 1] = '\0';
                m->has_checkbox[i] = has_checkbox;
                m->checked[i] = checked;
                m->count++;
            }
        },
        true);
}

void fas_list_view_set_checked(FasListView* lv, int index, bool checked) {
    with_view_model(
        lv->view,
        FasListViewModel * m,
        {
            if(index >= 0 && index < m->count) m->checked[index] = checked;
        },
        true);
}

bool fas_list_view_get_checked(FasListView* lv, int index) {
    bool result = false;
    with_view_model(
        lv->view,
        FasListViewModel * m,
        {
            if(index >= 0 && index < m->count) result = m->checked[index];
        },
        false);
    return result;
}

int fas_list_view_get_cursor(FasListView* lv) {
    int c = 0;
    with_view_model(lv->view, FasListViewModel * m, { c = m->cursor; }, false);
    return c;
}

void fas_list_view_set_cursor(FasListView* lv, int index) {
    with_view_model(
        lv->view,
        FasListViewModel * m,
        {
            if(index >= 0 && index < m->count) {
                m->cursor = index;
                /* Adjust scroll so cursor is visible */
                if(m->cursor < m->scroll) m->scroll = m->cursor;
                if(m->cursor >= m->scroll + VISIBLE_ROWS) m->scroll = m->cursor - VISIBLE_ROWS + 1;
            }
        },
        true);
}
