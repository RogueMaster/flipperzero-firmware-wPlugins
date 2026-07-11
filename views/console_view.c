#include "console_view.h"
#include "trident_icons.h" // generated from icons/ by fbt; on the app include path
#include <furi.h>
#include <string.h>

#define CONSOLE_LINES   64 // ring-buffer depth
#define CONSOLE_COLS    48 // stored chars per line (screen clips the rest)
#define VISIBLE_ROWS    5
#define BODY_TOP        20 // baseline of the first body row
#define ROW_PITCH       8
#define BODY_DIVIDER    54

struct ConsoleView {
    View* view;
    ConsoleViewCallback ok_cb;
    void* ok_ctx;
};

typedef struct {
    char lines[CONSOLE_LINES][CONSOLE_COLS];
    uint16_t count; // number of stored lines (<= CONSOLE_LINES)
    uint16_t head; // physical index of the oldest stored line
    int16_t scroll; // rows scrolled up from the newest (0 = pinned to bottom)
    bool autoscroll;
    bool live;
    char title[24];
    char chan[8];
    uint8_t anim;
} ConsoleModel;

static uint16_t phys_index(const ConsoleModel* m, uint16_t logical) {
    return (uint16_t)((m->head + logical) % CONSOLE_LINES);
}

static int16_t scroll_max(const ConsoleModel* m) {
    int16_t over = (int16_t)m->count - VISIBLE_ROWS;
    return over > 0 ? over : 0;
}

/* ---------- drawing ---------- */

static void console_view_draw(Canvas* canvas, void* model) {
    ConsoleModel* m = model;

    /* ---- header ---- */
    canvas_draw_icon(canvas, 0, 1, &I_trident_10px);
    canvas_set_font(canvas, FontSecondary);
    char title[22];
    strncpy(title, m->title[0] ? m->title : "Console", sizeof(title) - 1);
    title[sizeof(title) - 1] = '\0';
    canvas_draw_str(canvas, 13, 9, title);

    // link status: filled dot + streaming blink when live
    const char* state = m->live ? "LIVE" : "IDLE";
    canvas_draw_str_aligned(canvas, 110, 9, AlignRight, AlignBottom, state);
    if(m->live) {
        if(m->anim & 1) canvas_draw_disc(canvas, 124, 5, 2);
        canvas_draw_circle(canvas, 124, 5, 2);
    } else {
        canvas_draw_circle(canvas, 124, 5, 2);
    }
    canvas_draw_line(canvas, 0, 12, 127, 12);

    /* ---- body ---- */
    canvas_set_font(canvas, FontKeyboard);
    int16_t smax = scroll_max(m);
    if(m->scroll > smax) m->scroll = smax;
    if(m->scroll < 0) m->scroll = 0;

    if(m->count == 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 4, 32, "Waiting for the board...");
        canvas_draw_str(canvas, 4, 44, "OK to send a command");
    } else {
        int bottom_logical = (int)m->count - 1 - m->scroll;
        int top_logical = bottom_logical - (VISIBLE_ROWS - 1);
        for(int r = 0; r < VISIBLE_ROWS; r++) {
            int logical = top_logical + r;
            if(logical < 0 || logical >= (int)m->count) continue;
            canvas_draw_str(
                canvas, 1, BODY_TOP + r * ROW_PITCH, m->lines[phys_index(m, (uint16_t)logical)]);
        }
    }

    // scrollbar / "more above" affordance
    if(m->count > VISIBLE_ROWS) {
        if(m->scroll < smax) canvas_draw_str(canvas, 122, 20, "^");
        if(m->scroll > 0) canvas_draw_str(canvas, 122, 52, "v");
    }

    /* ---- footer ---- */
    canvas_draw_line(canvas, 0, BODY_DIVIDER, 127, BODY_DIVIDER);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 63, "OK:cmd");
    if(m->scroll > 0) {
        canvas_draw_str_aligned(canvas, 63, 63, AlignCenter, AlignBottom, "PAUSED");
    }
    char right[16];
    snprintf(right, sizeof(right), "UART %s", m->chan[0] ? m->chan : "?");
    canvas_draw_str_aligned(canvas, 126, 63, AlignRight, AlignBottom, right);
}

static bool console_view_input(InputEvent* event, void* context) {
    ConsoleView* v = context;
    bool consumed = false;

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        if(event->key == InputKeyUp) {
            with_view_model(
                v->view, ConsoleModel * m, { m->scroll++; }, true);
            consumed = true;
        } else if(event->key == InputKeyDown) {
            with_view_model(
                v->view,
                ConsoleModel * m,
                {
                    if(m->scroll > 0) m->scroll--;
                },
                true);
            consumed = true;
        } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
            if(v->ok_cb) v->ok_cb(v->ok_ctx);
            consumed = true;
        }
    }
    return consumed;
}

/* ---------- lifecycle ---------- */

ConsoleView* console_view_alloc(void) {
    ConsoleView* v = malloc(sizeof(ConsoleView));
    v->ok_cb = NULL;
    v->ok_ctx = NULL;
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_set_draw_callback(v->view, console_view_draw);
    view_set_input_callback(v->view, console_view_input);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(ConsoleModel));
    with_view_model(
        v->view,
        ConsoleModel * m,
        {
            memset(m, 0, sizeof(ConsoleModel));
            m->autoscroll = true;
            strcpy(m->chan, "13/14");
        },
        false);
    return v;
}

void console_view_free(ConsoleView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* console_view_get_view(ConsoleView* v) {
    furi_assert(v);
    return v->view;
}

void console_view_set_ok_callback(ConsoleView* v, ConsoleViewCallback cb, void* context) {
    furi_assert(v);
    v->ok_cb = cb;
    v->ok_ctx = context;
}

void console_view_clear(ConsoleView* v) {
    furi_assert(v);
    with_view_model(
        v->view,
        ConsoleModel * m,
        {
            m->count = 0;
            m->head = 0;
            m->scroll = 0;
        },
        true);
}

void console_view_push_line(ConsoleView* v, const char* line) {
    furi_assert(v);
    if(!line) return;
    with_view_model(
        v->view,
        ConsoleModel * m,
        {
            uint16_t slot;
            if(m->count < CONSOLE_LINES) {
                slot = (uint16_t)((m->head + m->count) % CONSOLE_LINES);
                m->count++;
            } else {
                slot = m->head;
                m->head = (uint16_t)((m->head + 1) % CONSOLE_LINES);
                if(m->scroll > 0 && m->scroll < scroll_max(m) + 1) {
                    // keep the user's viewport steady as old lines fall off the top
                    m->scroll++;
                    if(m->scroll > scroll_max(m)) m->scroll = scroll_max(m);
                }
            }
            strncpy(m->lines[slot], line, CONSOLE_COLS - 1);
            m->lines[slot][CONSOLE_COLS - 1] = '\0';
            if(m->autoscroll && m->scroll == 0) {
                // stay pinned to the newest line
            }
        },
        false); // repaint happens on the scene tick, decoupling data rate from FPS
}

void console_view_set_header(ConsoleView* v, const char* title) {
    furi_assert(v);
    with_view_model(
        v->view,
        ConsoleModel * m,
        {
            strncpy(m->title, title ? title : "", sizeof(m->title) - 1);
            m->title[sizeof(m->title) - 1] = '\0';
        },
        true);
}

void console_view_set_channel(ConsoleView* v, const char* chan) {
    furi_assert(v);
    with_view_model(
        v->view,
        ConsoleModel * m,
        {
            strncpy(m->chan, chan ? chan : "?", sizeof(m->chan) - 1);
            m->chan[sizeof(m->chan) - 1] = '\0';
        },
        true);
}

void console_view_set_live(ConsoleView* v, bool live) {
    furi_assert(v);
    with_view_model(
        v->view, ConsoleModel * m, { m->live = live; }, false);
}

void console_view_set_autoscroll(ConsoleView* v, bool autoscroll) {
    furi_assert(v);
    with_view_model(
        v->view, ConsoleModel * m, { m->autoscroll = autoscroll; }, false);
}

void console_view_tick(ConsoleView* v) {
    furi_assert(v);
    with_view_model(
        v->view, ConsoleModel * m, { m->anim++; }, true);
}
