#include "led_remote_view_remote.h"
#include <gui/elements.h>
#include <string.h>

#define TITLE_H 8
#define ROW_H   20
#define GRID_RADIUS 3

struct LedRemoteViewRemote {
    View* view;
};

typedef struct {
    uint8_t selected_col;
    uint8_t selected_row;
    bool learned[LED_REMOTE_MAX_BUTTONS];
    char profile_name[32];
    LedRemoteLayout layout;

    uint8_t button_count;
    uint8_t grid_cols;
    uint8_t grid_rows;
    const char* const* button_labels;

    LedRemoteViewRemoteCallback callback;
    void* context;
} LedRemoteViewRemoteModel;


static LedButton button_at(uint8_t col, uint8_t row, uint8_t grid_cols) {
    return (LedButton)(row * grid_cols + col);
}

static bool is_color_button(uint8_t row, uint8_t grid_cols) {
    if(grid_cols == 3) return (row >= 1 && row <= 3);
    return (row >= 1);
}


static void draw_circle_btn(
    Canvas* canvas,
    int16_t cx,
    int16_t cy,
    const char* label,
    bool color_btn,
    bool selected,
    bool learned,
    uint8_t r_learned,
    uint8_t r_unlearned) {
    int16_t r = (int16_t)(learned ? r_learned : r_unlearned);

    if(selected) {
        canvas_draw_circle(canvas, cx, cy, (uint8_t)(r + 1));
        canvas_draw_disc(canvas, cx, cy, (uint8_t)r);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, cx, cy, AlignCenter, AlignCenter, label);
        canvas_set_color(canvas, ColorBlack);
    } else if(color_btn && learned) {
        canvas_draw_disc(canvas, cx, cy, (uint8_t)r);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, cx, cy, AlignCenter, AlignCenter, label);
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_circle(canvas, cx, cy, (uint8_t)r);
        canvas_draw_str_aligned(canvas, cx, cy, AlignCenter, AlignCenter, label);
    }
}

static void draw_layout_circle(Canvas* canvas, LedRemoteViewRemoteModel* m) {
    canvas_set_font(canvas, FontKeyboard);

    uint8_t cell_w    = 64 / m->grid_cols;
    uint8_t r_learned   = (m->grid_cols == 3) ? 8 : 6;
    uint8_t r_unlearned = (m->grid_cols == 3) ? 6 : 5;

    for(uint8_t row = 0; row < m->grid_rows; row++) {
        bool color = is_color_button(row, m->grid_cols);
        for(uint8_t col = 0; col < m->grid_cols; col++) {
            LedButton btn = button_at(col, row, m->grid_cols);
            int16_t cx = (int16_t)(col * cell_w + cell_w / 2);
            int16_t cy = (int16_t)(TITLE_H + row * ROW_H + ROW_H / 2);
            bool sel     = (col == m->selected_col && row == m->selected_row);
            bool learned = m->learned[btn];

            const char* lbl = (m->button_labels && btn < m->button_count)
                                  ? m->button_labels[btn]
                                  : "?";
            draw_circle_btn(canvas, cx, cy, lbl, color, sel, learned, r_learned, r_unlearned);
        }
    }

    canvas_draw_line(canvas, 0, TITLE_H + ROW_H, 63, TITLE_H + ROW_H);
    // 18-Keys: extra separator between color rows and mode rows
    if(m->grid_cols == 3) {
        canvas_draw_line(canvas, 0, TITLE_H + 4 * ROW_H, 63, TITLE_H + 4 * ROW_H);
    }
}


static void draw_layout_grid(Canvas* canvas, LedRemoteViewRemoteModel* m) {
    canvas_set_font(canvas, FontKeyboard);

    uint8_t cell_w = 64 / m->grid_cols;

    for(uint8_t row = 0; row < m->grid_rows; row++) {
        for(uint8_t col = 0; col < m->grid_cols; col++) {
            LedButton btn = button_at(col, row, m->grid_cols);
            int16_t x  = (int16_t)(col * cell_w);
            int16_t y  = (int16_t)(TITLE_H + row * ROW_H);
            int16_t w  = (col < m->grid_cols - 1) ? (int16_t)cell_w : (int16_t)(64 - x);
            int16_t cx = (int16_t)(x + w / 2);
            int16_t cy = (int16_t)(y + ROW_H / 2);
            bool sel     = (col == m->selected_col && row == m->selected_row);
            bool learned = m->learned[btn];

            if(sel) {
                canvas_draw_rbox(canvas, x + 1, y + 1, (uint8_t)(w - 2), ROW_H - 2, GRID_RADIUS);
                canvas_set_color(canvas, ColorWhite);
            }

            const char* lbl = (m->button_labels && btn < m->button_count)
                                  ? m->button_labels[btn]
                                  : "?";
            if(learned) {
                canvas_draw_str_aligned(canvas, cx, cy, AlignCenter, AlignCenter, lbl);
            } else {
                char buf[5];
                buf[0] = '~';
                strncpy(buf + 1, lbl, 3);
                buf[4] = '\0';
                canvas_draw_str_aligned(canvas, cx, cy, AlignCenter, AlignCenter, buf);
            }

            if(sel) canvas_set_color(canvas, ColorBlack);
            canvas_draw_rframe(canvas, x + 1, y + 1, (uint8_t)(w - 2), ROW_H - 2, GRID_RADIUS);
        }
    }

    canvas_draw_line(canvas, 0, TITLE_H + ROW_H, 63, TITLE_H + ROW_H);
    if(m->grid_cols == 3) {
        canvas_draw_line(canvas, 0, TITLE_H + 4 * ROW_H, 63, TITLE_H + 4 * ROW_H);
    }
}


static void led_remote_view_remote_draw(Canvas* canvas, void* model_ptr) {
    LedRemoteViewRemoteModel* m = model_ptr;

    canvas_clear(canvas);

    canvas_set_font(canvas, FontSecondary);
    const char* name = (m->profile_name[0] != '\0') ? m->profile_name : "LED Remote";
    canvas_draw_str_aligned(canvas, 32, 1, AlignCenter, AlignTop, name);
    canvas_draw_line(canvas, 0, TITLE_H - 1, 63, TITLE_H - 1);

    if(m->grid_cols == 0) return;

    if(m->layout == LedRemoteLayoutCircle) {
        draw_layout_circle(canvas, m);
    } else {
        draw_layout_grid(canvas, m);
    }
}


static bool led_remote_view_remote_input(InputEvent* event, void* context) {
    LedRemoteViewRemote* view = context;

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    bool consumed = false;

    with_view_model(
        view->view,
        LedRemoteViewRemoteModel * m,
        {
            switch(event->key) {
            case InputKeyUp:
                if(m->selected_row > 0) m->selected_row--;
                consumed = true;
                break;
            case InputKeyDown:
                if(m->selected_row < m->grid_rows - 1) m->selected_row++;
                consumed = true;
                break;
            case InputKeyLeft:
                if(m->selected_col > 0) m->selected_col--;
                consumed = true;
                break;
            case InputKeyRight:
                if(m->selected_col < m->grid_cols - 1) m->selected_col++;
                consumed = true;
                break;
            case InputKeyOk:
                if(event->type == InputTypeShort && m->callback) {
                    LedButton btn = button_at(m->selected_col, m->selected_row, m->grid_cols);
                    m->callback(btn, m->context);
                }
                consumed = true;
                break;
            default:
                break;
            }
        },
        true);

    return consumed;
}


LedRemoteViewRemote* led_remote_view_remote_alloc(void) {
    LedRemoteViewRemote* view = malloc(sizeof(LedRemoteViewRemote));
    view->view = view_alloc();
    view_set_context(view->view, view);
    view_set_orientation(view->view, ViewOrientationVertical);
    view_allocate_model(view->view, ViewModelTypeLocking, sizeof(LedRemoteViewRemoteModel));
    view_set_draw_callback(view->view, led_remote_view_remote_draw);
    view_set_input_callback(view->view, led_remote_view_remote_input);

    with_view_model(
        view->view,
        LedRemoteViewRemoteModel * m,
        {
            m->grid_cols    = 3;
            m->grid_rows    = 6;
            m->button_count = 18;
        },
        false);

    return view;
}

void led_remote_view_remote_free(LedRemoteViewRemote* view) {
    view_free(view->view);
    free(view);
}

View* led_remote_view_remote_get_view(LedRemoteViewRemote* view) {
    return view->view;
}

void led_remote_view_remote_set_callback(
    LedRemoteViewRemote* view,
    LedRemoteViewRemoteCallback callback,
    void* context) {
    with_view_model(
        view->view,
        LedRemoteViewRemoteModel * m,
        {
            m->callback = callback;
            m->context  = context;
        },
        false);
}

void led_remote_view_remote_set_learned(LedRemoteViewRemote* view, const bool* learned) {
    with_view_model(
        view->view,
        LedRemoteViewRemoteModel * m,
        { memcpy(m->learned, learned, sizeof(bool) * LED_REMOTE_MAX_BUTTONS); },
        true);
}

void led_remote_view_remote_set_profile_name(LedRemoteViewRemote* view, const char* name) {
    with_view_model(
        view->view,
        LedRemoteViewRemoteModel * m,
        {
            strncpy(m->profile_name, name, sizeof(m->profile_name) - 1);
            m->profile_name[sizeof(m->profile_name) - 1] = '\0';
        },
        true);
}

void led_remote_view_remote_set_layout(LedRemoteViewRemote* view, LedRemoteLayout layout) {
    with_view_model(
        view->view,
        LedRemoteViewRemoteModel * m,
        { m->layout = layout; },
        true);
}

void led_remote_view_remote_set_remote_params(
    LedRemoteViewRemote* view,
    uint8_t button_count,
    uint8_t grid_cols,
    const char* const* button_labels_short) {
    with_view_model(
        view->view,
        LedRemoteViewRemoteModel * m,
        {
            m->button_count  = button_count;
            m->grid_cols     = grid_cols;
            m->grid_rows     = (grid_cols > 0) ? (button_count / grid_cols) : 6;
            m->button_labels = button_labels_short;
            // Clamp selection within new bounds
            if(m->selected_col >= grid_cols) m->selected_col = (uint8_t)(grid_cols - 1);
            if(m->grid_rows > 0 && m->selected_row >= m->grid_rows)
                m->selected_row = (uint8_t)(m->grid_rows - 1);
        },
        true);
}
