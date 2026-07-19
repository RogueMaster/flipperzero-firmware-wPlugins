#include "console_view.h"

#include <gui/elements.h>
#include <string.h>

/* FontKeyboard is 5x7 on a 6px pitch: 21 columns, and 6 rows once the status
 * bar has had its say. */
#define CONSOLE_ROWS (6u)
#define CONSOLE_CHAR_W (6)
#define CONSOLE_ROW_H (9)
#define CONSOLE_TOP (11)

#define HEX_BYTES_PER_ROW (5u)

typedef struct {
    Term* term;
    uint32_t baud;
    char framing[6];
    bool tx_enabled;
    bool logging;
    uint32_t autoboot_left; // seconds remaining in a key burst, 0 when idle

    bool hex_mode;
    /* Lines scrolled back from the newest. 0 means live-follow, which is what
     * you want while a board is booting. */
    uint16_t scroll_back;
} ConsoleViewModel;

struct ConsoleView {
    View* view;
    ConsoleViewCallback callback;
    void* context;
};

/* ---------------------------------------------------------------- draw ---- */

static void console_draw_status(Canvas* canvas, const ConsoleViewModel* m) {
    char buf[24];

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 9);
    canvas_set_color(canvas, ColorWhite);

    canvas_set_font(canvas, FontSecondary);

    /* A filled dot before the rate means the session is being written to the
     * card - the one piece of state you would hate to be wrong about. */
    int x = 2;
    if(m->logging) {
        canvas_draw_disc(canvas, x + 2, 4, 2);
        x += 7;
    }
    snprintf(buf, sizeof(buf), "%lu %s", m->baud, m->framing);
    canvas_draw_str(canvas, x, 7, buf);

    /* Right side: the burst countdown outranks everything while it runs, since
     * it is the only transient state here and the user is waiting on it. */
    const char* mode = m->hex_mode ? "HEX" : "TXT";
    if(m->autoboot_left > 0) {
        snprintf(buf, sizeof(buf), "BREAKING %lus", m->autoboot_left);
    } else if(m->scroll_back > 0) {
        snprintf(buf, sizeof(buf), "%s  -%u", mode, m->scroll_back);
    } else {
        snprintf(buf, sizeof(buf), "%s  %s", mode, m->tx_enabled ? "RW" : "RO");
    }
    canvas_draw_str_aligned(canvas, 126, 7, AlignRight, AlignBottom, buf);

    canvas_set_color(canvas, ColorBlack);
}

static void console_draw_text(Canvas* canvas, const ConsoleViewModel* m) {
    const size_t total = term_line_count(m->term);

    /* Newest line that should appear at the bottom of the window. */
    size_t last = (total > 0) ? (total - 1) : 0;
    if(m->scroll_back < last) {
        last -= m->scroll_back;
    } else {
        last = 0;
    }

    const size_t rows = (last + 1 < CONSOLE_ROWS) ? (last + 1) : CONSOLE_ROWS;
    const size_t first = last + 1 - rows;

    canvas_set_font(canvas, FontKeyboard);
    for(size_t r = 0; r < rows; r++) {
        const char* line = term_line(m->term, first + r);
        const int y = CONSOLE_TOP + (int)r * CONSOLE_ROW_H;
        canvas_draw_str(canvas, 1, y + 7, line);
    }

    if(total > CONSOLE_ROWS) {
        elements_scrollbar(canvas, last, total);
    }
}

static void console_draw_hex(Canvas* canvas, const ConsoleViewModel* m) {
    const size_t total_bytes = term_raw_count(m->term);
    const size_t total_rows = (total_bytes + HEX_BYTES_PER_ROW - 1) / HEX_BYTES_PER_ROW;

    size_t last_row = (total_rows > 0) ? (total_rows - 1) : 0;
    if(m->scroll_back < last_row) {
        last_row -= m->scroll_back;
    } else {
        last_row = 0;
    }

    const size_t rows = (last_row + 1 < CONSOLE_ROWS) ? (last_row + 1) : CONSOLE_ROWS;
    const size_t first_row = last_row + 1 - rows;

    canvas_set_font(canvas, FontKeyboard);
    for(size_t r = 0; r < rows; r++) {
        char line[TERM_COLS + 1];
        size_t pos = 0;
        const size_t base = (first_row + r) * HEX_BYTES_PER_ROW;

        for(size_t i = 0; i < HEX_BYTES_PER_ROW; i++) {
            if(base + i >= total_bytes) break;
            const uint8_t b = term_raw_at(m->term, base + i);
            pos += snprintf(&line[pos], sizeof(line) - pos, "%02X ", b);
        }
        /* ASCII gutter, so a familiar string still jumps out of the hex. */
        if(pos < sizeof(line) - 1) {
            line[pos++] = ' ';
            for(size_t i = 0; i < HEX_BYTES_PER_ROW && pos < sizeof(line) - 1; i++) {
                if(base + i >= total_bytes) break;
                const uint8_t b = term_raw_at(m->term, base + i);
                line[pos++] = (b >= 0x20 && b <= 0x7E) ? (char)b : '.';
            }
        }
        line[pos] = '\0';

        canvas_draw_str(canvas, 1, CONSOLE_TOP + (int)r * CONSOLE_ROW_H + 7, line);
    }

    if(total_rows > CONSOLE_ROWS) {
        elements_scrollbar(canvas, last_row, total_rows);
    }
}

static void console_view_draw(Canvas* canvas, void* model) {
    ConsoleViewModel* m = model;

    canvas_clear(canvas);
    console_draw_status(canvas, m);

    if(!m->term) return;

    if(m->hex_mode) {
        console_draw_hex(canvas, m);
    } else {
        console_draw_text(canvas, m);
    }
}

/* --------------------------------------------------------------- input ---- */

static size_t console_scroll_limit(const ConsoleViewModel* m) {
    if(!m->term) return 0;
    if(m->hex_mode) {
        const size_t bytes = term_raw_count(m->term);
        const size_t rows = (bytes + HEX_BYTES_PER_ROW - 1) / HEX_BYTES_PER_ROW;
        return (rows > CONSOLE_ROWS) ? (rows - CONSOLE_ROWS) : 0;
    }
    const size_t lines = term_line_count(m->term);
    return (lines > CONSOLE_ROWS) ? (lines - CONSOLE_ROWS) : 0;
}

static bool console_view_input(InputEvent* event, void* context) {
    ConsoleView* cv = context;
    bool consumed = false;

    const bool pressed = (event->type == InputTypeShort) || (event->type == InputTypeRepeat);

    if(pressed && event->key == InputKeyUp) {
        with_view_model(
            cv->view,
            ConsoleViewModel * m,
            {
                const size_t limit = console_scroll_limit(m);
                if(m->scroll_back < limit) m->scroll_back++;
            },
            true);
        consumed = true;

    } else if(pressed && event->key == InputKeyDown) {
        with_view_model(
            cv->view,
            ConsoleViewModel * m,
            {
                if(m->scroll_back > 0) m->scroll_back--;
            },
            true);
        consumed = true;

    } else if(pressed && event->key == InputKeyRight) {
        with_view_model(
            cv->view,
            ConsoleViewModel * m,
            {
                m->hex_mode = !m->hex_mode;
                m->scroll_back = 0; // the two views count in different units
            },
            true);
        consumed = true;

    } else if(pressed && event->key == InputKeyLeft) {
        if(cv->callback) cv->callback(cv->context, ConsoleEventTypeCtrl);
        consumed = true;

    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(cv->callback) cv->callback(cv->context, ConsoleEventTypeText);
        consumed = true;

    } else if(event->type == InputTypeLong && event->key == InputKeyOk) {
        if(cv->callback) cv->callback(cv->context, ConsoleEventTypeEnter);
        consumed = true;
    }

    return consumed;
}

/* ----------------------------------------------------------------- api ---- */

ConsoleView* console_view_alloc(void) {
    ConsoleView* cv = malloc(sizeof(ConsoleView));
    memset(cv, 0, sizeof(ConsoleView));
    cv->view = view_alloc();
    view_allocate_model(cv->view, ViewModelTypeLocking, sizeof(ConsoleViewModel));
    view_set_context(cv->view, cv);
    view_set_draw_callback(cv->view, console_view_draw);
    view_set_input_callback(cv->view, console_view_input);
    return cv;
}

void console_view_free(ConsoleView* cv) {
    furi_assert(cv);
    view_free(cv->view);
    free(cv);
}

View* console_view_get_view(ConsoleView* cv) {
    furi_assert(cv);
    return cv->view;
}

void console_view_set_callback(ConsoleView* cv, ConsoleViewCallback cb, void* context) {
    furi_assert(cv);
    cv->callback = cb;
    cv->context = context;
}

void console_view_set_term(ConsoleView* cv, Term* term) {
    furi_assert(cv);
    with_view_model(
        cv->view,
        ConsoleViewModel * m,
        {
            m->term = term;
            m->scroll_back = 0;
        },
        true);
}

void console_view_set_link(ConsoleView* cv, uint32_t baud, const char* framing, bool tx_enabled) {
    furi_assert(cv);
    with_view_model(
        cv->view,
        ConsoleViewModel * m,
        {
            m->baud = baud;
            strncpy(m->framing, framing, sizeof(m->framing) - 1);
            m->framing[sizeof(m->framing) - 1] = '\0';
            m->tx_enabled = tx_enabled;
        },
        true);
}

void console_view_set_logging(ConsoleView* cv, bool logging) {
    furi_assert(cv);
    with_view_model(cv->view, ConsoleViewModel * m, { m->logging = logging; }, true);
}

void console_view_set_autoboot(ConsoleView* cv, uint32_t seconds_left) {
    furi_assert(cv);
    with_view_model(cv->view, ConsoleViewModel * m, { m->autoboot_left = seconds_left; }, true);
}

void console_view_notify_rx(ConsoleView* cv) {
    furi_assert(cv);
    /* Only repaint. Scroll position is deliberately untouched: if the user has
     * scrolled back to read something, new output must not yank them away. */
    with_view_model(cv->view, ConsoleViewModel * m, { UNUSED(m); }, true);
}
