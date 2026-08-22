#include "midea_main_view.h"
#include "../midea_ir_protocol.h"
#include <furi.h>
#include <gui/elements.h>
#include <stdlib.h>
#include <string.h>

// The whole screen is one 2-column grid. Slot order is fixed:
//   0 Mode        1 Fan
//   2 Temp -      3 Temp +
//   4.. toggles (every MideaToggle except PowerOff, in enum order)
//   then Extra, then Setup
// Everything below is derived from MideaToggleCount, so a port never edits
// this file - it changes the enum in the protocol module.
#define SLOT_MODE      0
#define SLOT_FAN       1
#define SLOT_TEMP_DOWN 2
#define SLOT_TEMP_UP   3
#define SLOT_TOGGLE_0  4

// Toggle count excluding PowerOff, which is reached through Mode = Off
#define TOGGLE_SLOTS ((int)MideaToggleCount - 1)
#define SLOT_EXTRA   (SLOT_TOGGLE_0 + TOGGLE_SLOTS)
#define SLOT_SETUP   (SLOT_EXTRA + 1)
#define SLOT_COUNT   (SLOT_SETUP + 1)

_Static_assert(
    TOGGLE_SLOTS <= MIDEA_MAX_MAIN_TOGGLES,
    "Too many main-screen toggles - move some to MideaExtra");

// Button geometry. Labels that do not fit on one line wrap onto two, which
// needs a taller button and more row pitch. That only works while the extra
// height still clears the footer, so an app with many buttons keeps the
// compact single-line layout and one with a few gets the roomy one.
#define BTN_W      30
#define BTN_H_1    12
#define BTN_H_2    18
#define BTN_ROW_H1 15
#define BTN_ROW_H2 21
#define BTN_ROW_Y  49
#define COL_X(col) ((col) == 0 ? 1 : 33)
#define FOOTER_Y   116

// Grid rows the buttons occupy, and whether the tall variant still fits
#define BTN_ROWS ((SLOT_COUNT - SLOT_TOGGLE_0 + 1) / 2)
#define TWO_LINE_FITS \
    ((BTN_ROW_Y + (BTN_ROWS - 1) * BTN_ROW_H2 + BTN_H_2) <= (FOOTER_Y - 4))

// Dropdown picker
#define PICKER_ITEM_H 11
#define PICKER_Y      23

typedef enum {
    PickerNone = 0,
    PickerMode,
    PickerFan,
} PickerKind;

typedef struct {
    MideaState* state;
    int focus;
    bool is_sending;
    int send_anim_frame;

    bool last_was_toggle;
    MideaToggle last_toggle;

    bool state_pending;

    PickerKind picker_kind;
    uint8_t picker_index;
} MideaMainViewModel;

struct MideaMainView {
    View* view;
    MideaMainViewSendCallback send_callback;
    void* send_context;
    MideaMainViewNavigateCallback extra_callback;
    void* extra_context;
    MideaMainViewNavigateCallback setup_callback;
    void* setup_context;
    FuriTimer* debounce_timer;
};

static void midea_main_view_debounce_callback(void* context);

// ---------------------------------------------------------------- drawing --

// Two independent cues, so neither is ambiguous:
//   inverted -> this button has the cursor on it
//   thick    -> this toggle is believed to be on
// The active ring is drawn one pixel OUTSIDE the button so it never touches
// the label; an inner ring clipped the descenders of the centred text. It is
// always black, because it sits on the white background rather than on the
// button body, and so stays visible whether or not the button is inverted.
// Columns are placed to leave the one pixel of margin it needs on both sides.
// Leaves the canvas colour set for the caller's content, so every caller
// resets to ColorBlack when done.
static void draw_button_body(
    Canvas* canvas,
    int16_t x,
    int16_t y,
    int16_t w,
    int16_t h,
    bool inverted,
    bool thick) {
    if(thick) canvas_draw_rframe(canvas, x - 1, y - 1, w + 2, h + 2, 3);
    if(inverted) {
        canvas_draw_rbox(canvas, x, y, w, h, 2);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, x, y, w, h, 2);
    }
}

static void draw_selector_button(
    Canvas* canvas,
    int16_t x,
    int16_t y,
    int16_t w,
    const char* label,
    const char* value,
    bool focused) {
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, x + w / 2, y, AlignCenter, AlignTop, label);

    int16_t btn_y = y + 10;
    draw_button_body(canvas, x, btn_y, w, 13, focused, false);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, x + w / 2, btn_y + 10, AlignCenter, AlignBottom, value);
    canvas_set_color(canvas, ColorBlack);
}

static void draw_temp_icon(Canvas* canvas, int16_t box_x, int16_t box_y, bool plus) {
    int16_t cx = box_x + 7;
    int16_t cy = box_y + 7;
    canvas_draw_box(canvas, cx - 4, cy - 1, 8, 2);
    if(plus) canvas_draw_box(canvas, cx - 1, cy - 4, 2, 8);
}

#define TEMP_BTN_W 14
#define TEMP_BTN_L 2
#define TEMP_BTN_R 48

static void draw_temperature(
    Canvas* canvas,
    int16_t y,
    uint8_t temp,
    bool focus_down,
    bool focus_up) {
    char temp_str[8];
    snprintf(temp_str, sizeof(temp_str), "%d", temp);

    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, 32, y, AlignCenter, AlignTop, temp_str);

    int16_t btn_y = y + 2;

    draw_button_body(canvas, TEMP_BTN_L, btn_y, TEMP_BTN_W, TEMP_BTN_W, focus_down, false);
    draw_temp_icon(canvas, TEMP_BTN_L, btn_y, false);
    canvas_set_color(canvas, ColorBlack);

    draw_button_body(canvas, TEMP_BTN_R, btn_y, TEMP_BTN_W, TEMP_BTN_W, focus_up, false);
    draw_temp_icon(canvas, TEMP_BTN_R, btn_y, true);
    canvas_set_color(canvas, ColorBlack);
}

/// Longest prefix of `label` that fits `maxw`, preferring a break at a space.
/// Returns 0 when the whole string fits.
static size_t label_break(Canvas* canvas, const char* label, int16_t maxw) {
    char buf[24];
    size_t len = strlen(label);
    if(len >= sizeof(buf)) len = sizeof(buf) - 1;

    if(canvas_string_width(canvas, label) <= (uint16_t)maxw) return 0;

    size_t best = 0;
    for(size_t i = 1; i <= len; i++) {
        memcpy(buf, label, i);
        buf[i] = '\0';
        if(canvas_string_width(canvas, buf) > (uint16_t)maxw) break;
        best = i;
    }
    if(best == 0) best = 1; // always make progress

    // Prefer splitting on a space inside the part that fits
    for(size_t i = best; i > 0; i--) {
        if(label[i - 1] == ' ') return i - 1;
    }
    return best;
}

/// Draw the label centred in the button, wrapping onto a second line when it
/// does not fit and the button is tall enough to hold one.
static void
    draw_button_label(Canvas* canvas, int16_t x, int16_t y, int16_t h, const char* label) {
    canvas_set_font(canvas, FontSecondary);
    int16_t maxw = BTN_W - 4;
    size_t brk = (h >= BTN_H_2) ? label_break(canvas, label, maxw) : 0;

    if(brk == 0) {
        canvas_draw_str_aligned(
            canvas, x + BTN_W / 2, y + (h - 8) / 2, AlignCenter, AlignTop, label);
        return;
    }

    char line[24];
    size_t n = brk < sizeof(line) ? brk : sizeof(line) - 1;
    memcpy(line, label, n);
    line[n] = '\0';
    canvas_draw_str_aligned(canvas, x + BTN_W / 2, y + 1, AlignCenter, AlignTop, line);

    const char* rest = label + brk;
    while(*rest == ' ') rest++;
    canvas_draw_str_aligned(canvas, x + BTN_W / 2, y + 9, AlignCenter, AlignTop, rest);
}

// A doubled border marks a toggle the app believes is on. The AC never
// reports back, so it is a local guess and nothing more. It has to be
// separate from the inverted fill, which means "cursor is here".
static void draw_button(
    Canvas* canvas,
    int16_t x,
    int16_t y,
    int16_t h,
    const char* label,
    bool focused,
    bool active) {
    draw_button_body(canvas, x, y, BTN_W, h, focused, active);
    draw_button_label(canvas, x, y, h, label);
    canvas_set_color(canvas, ColorBlack);
}

// Protocol name plus a sending animation. Arrow columns are placed from the
// measured text width so a longer protocol name never collides with them.
static void draw_footer(Canvas* canvas, bool sending, int frame) {
    const char* name = midea_ir_get_protocol_name();

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 32, FOOTER_Y, AlignCenter, AlignTop, name);
    if(!sending) return;

    int phase = frame % 6;
    int arrows = (phase <= 3) ? phase : (6 - phase);

    int16_t half = canvas_string_width(canvas, name) / 2;
    int16_t cy = FOOTER_Y + 5;
    int16_t left = 32 - half - 3;
    int16_t right = 32 + half + 3;

    for(int i = 0; i < arrows; i++) {
        int16_t tip = left - i * 4;
        if(tip - 3 >= 0) {
            canvas_draw_line(canvas, tip - 3, cy - 2, tip, cy);
            canvas_draw_line(canvas, tip - 3, cy + 2, tip, cy);
        }
        tip = right + i * 4;
        if(tip + 3 < 64) {
            canvas_draw_line(canvas, tip + 3, cy - 2, tip, cy);
            canvas_draw_line(canvas, tip + 3, cy + 2, tip, cy);
        }
    }
}

static uint8_t picker_count(PickerKind k) {
    return k == PickerMode ? MideaModeCount : MideaFanCount;
}

static const char* picker_name(PickerKind k, uint8_t i) {
    return k == PickerMode ? midea_ir_get_mode_name((MideaMode)i) :
                             midea_ir_get_fan_name((MideaFan)i);
}

static void draw_picker(Canvas* canvas, MideaMainViewModel* m) {
    uint8_t count = picker_count(m->picker_kind);
    int16_t x = (m->picker_kind == PickerMode) ? 1 : 33;
    int16_t h = count * PICKER_ITEM_H + 2;

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, x, PICKER_Y, BTN_W, h);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, x, PICKER_Y, BTN_W, h);

    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < count; i++) {
        int16_t row_y = PICKER_Y + 1 + i * PICKER_ITEM_H;
        int16_t ty = row_y + PICKER_ITEM_H / 2;
        const char* nm = picker_name(m->picker_kind, i);
        if(i == m->picker_index) {
            canvas_draw_box(canvas, x + 1, row_y, BTN_W - 2, PICKER_ITEM_H - 1);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str_aligned(canvas, x + BTN_W / 2, ty, AlignCenter, AlignCenter, nm);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str_aligned(canvas, x + BTN_W / 2, ty, AlignCenter, AlignCenter, nm);
        }
    }
}

static void midea_main_view_draw(Canvas* canvas, void* model) {
    MideaMainViewModel* m = model;
    if(!m->state) return;

    canvas_clear(canvas);

    // Row 0: Mode and Fan
    const char* fan_value = midea_ir_get_fan_name(m->state->fan);
    if(m->state->mode == MideaModeOff) {
        fan_value = "--";
    } else if(midea_ir_mode_locks_fan(m->state->mode)) {
        fan_value = midea_ir_get_fan_name(MideaFanAuto);
    }
    draw_selector_button(
        canvas, COL_X(0), 0, BTN_W, "Mode", midea_ir_get_mode_name(m->state->mode),
        m->focus == SLOT_MODE);
    draw_selector_button(canvas, COL_X(1), 0, BTN_W, "Fan", fan_value, m->focus == SLOT_FAN);

    // Row 1: temperature
    if(!midea_state_can_change_temp(m->state)) {
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, 32, 28, AlignCenter, AlignTop, "--");
    } else {
        draw_temperature(
            canvas, 28, m->state->temp, m->focus == SLOT_TEMP_DOWN, m->focus == SLOT_TEMP_UP);
    }

    // Rows 2+: toggles, then Extra and Setup. Go tall only if some label
    // actually needs a second line and the layout can afford the height.
    canvas_set_font(canvas, FontSecondary);
    bool wrap_needed = false;
    if(TWO_LINE_FITS) {
        for(int slot = SLOT_TOGGLE_0; slot < SLOT_EXTRA; slot++) {
            MideaToggle t = (MideaToggle)(slot - SLOT_TOGGLE_0 + 1);
            if(canvas_string_width(canvas, midea_ir_get_toggle_name(t)) > BTN_W - 4) {
                wrap_needed = true;
                break;
            }
        }
    }
    int16_t btn_h = wrap_needed ? BTN_H_2 : BTN_H_1;
    int16_t row_h = wrap_needed ? BTN_ROW_H2 : BTN_ROW_H1;

    for(int slot = SLOT_TOGGLE_0; slot < SLOT_COUNT; slot++) {
        int grid = slot - SLOT_TOGGLE_0;
        int16_t x = COL_X(grid % 2);
        int16_t y = BTN_ROW_Y + (grid / 2) * row_h;
        bool focused = m->focus == slot;

        if(slot == SLOT_EXTRA) {
            draw_button(canvas, x, y, btn_h, "Extra", focused, false);
        } else if(slot == SLOT_SETUP) {
            draw_button(canvas, x, y, btn_h, "Setup", focused, false);
        } else {
            MideaToggle t = (MideaToggle)(slot - SLOT_TOGGLE_0 + 1);
            draw_button(
                canvas, x, y, btn_h, midea_ir_get_toggle_name(t), focused,
                midea_state_toggle_active(m->state, t));
        }
    }

    draw_footer(canvas, m->is_sending, m->send_anim_frame);

    if(m->picker_kind != PickerNone) draw_picker(canvas, m);
}

// ----------------------------------------------------------------- input ---

static void schedule_state_send(MideaMainView* view) {
    with_view_model(view->view, MideaMainViewModel * m, { m->state_pending = true; }, false);
    furi_timer_stop(view->debounce_timer);
    furi_timer_start(view->debounce_timer, MIDEA_SEND_DEBOUNCE_MS);
}

static void send_toggle_immediate(MideaMainView* view, MideaToggle toggle) {
    furi_timer_stop(view->debounce_timer);
    with_view_model(
        view->view,
        MideaMainViewModel * m,
        {
            m->state_pending = false;
            m->last_was_toggle = true;
            m->last_toggle = toggle;
        },
        false);
    if(view->send_callback) view->send_callback(view->send_context);
}

static void send_state_immediate(MideaMainView* view) {
    furi_timer_stop(view->debounce_timer);
    with_view_model(
        view->view,
        MideaMainViewModel * m,
        {
            m->state_pending = false;
            m->last_was_toggle = false;
        },
        false);
    if(view->send_callback) view->send_callback(view->send_context);
}

// Grid navigation. Slots are a uniform 2-column grid, so this is arithmetic
// rather than a hand-written adjacency table.
static int nav_left(int i) {
    return (i % 2 == 1) ? i - 1 : i;
}

static int nav_right(int i) {
    return (i % 2 == 0 && i + 1 < SLOT_COUNT) ? i + 1 : i;
}

static int nav_up(int i) {
    return (i - 2 >= 0) ? i - 2 : i;
}

static int nav_down(int i) {
    if(i + 2 < SLOT_COUNT) return i + 2;
    // Last row may hold a single item; step onto it rather than stalling.
    if(i + 1 < SLOT_COUNT && (i + 1) / 2 > i / 2) return i + 1;
    return i;
}

static bool midea_main_view_input(InputEvent* event, void* context) {
    MideaMainView* view = context;
    bool consumed = false;

    with_view_model(
        view->view,
        MideaMainViewModel * m,
        {
            if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
                // fall through, nothing to do
            } else if(m->picker_kind != PickerNone) {
                switch(event->key) {
                case InputKeyUp:
                    if(m->picker_index > 0) m->picker_index--;
                    consumed = true;
                    break;
                case InputKeyDown:
                    if(m->picker_index + 1 < picker_count(m->picker_kind)) m->picker_index++;
                    consumed = true;
                    break;
                case InputKeyOk:
                    if(event->type == InputTypeShort) {
                        if(m->picker_kind == PickerMode) {
                            midea_state_set_mode(m->state, (MideaMode)m->picker_index);
                        } else {
                            midea_state_set_fan(m->state, (MideaFan)m->picker_index);
                        }
                        m->picker_kind = PickerNone;
                        send_state_immediate(view);
                    }
                    consumed = true;
                    break;
                case InputKeyBack:
                    if(event->type == InputTypeShort) m->picker_kind = PickerNone;
                    consumed = true;
                    break;
                default:
                    consumed = true;
                    break;
                }
            } else {
                switch(event->key) {
                case InputKeyLeft:
                    m->focus = nav_left(m->focus);
                    consumed = true;
                    break;
                case InputKeyRight:
                    m->focus = nav_right(m->focus);
                    consumed = true;
                    break;
                case InputKeyUp:
                    m->focus = nav_up(m->focus);
                    consumed = true;
                    break;
                case InputKeyDown:
                    m->focus = nav_down(m->focus);
                    consumed = true;
                    break;
                case InputKeyOk: {
                    if(event->type != InputTypeShort) {
                        consumed = true;
                        break;
                    }
                    int f = m->focus;
                    if(f == SLOT_MODE) {
                        m->picker_kind = PickerMode;
                        m->picker_index = (uint8_t)m->state->mode;
                    } else if(f == SLOT_FAN) {
                        if(midea_state_can_change_fan(m->state)) {
                            m->picker_kind = PickerFan;
                            m->picker_index = (uint8_t)m->state->fan;
                        }
                    } else if(f == SLOT_TEMP_DOWN) {
                        if(midea_state_can_change_temp(m->state)) {
                            midea_state_temp_down(m->state);
                            schedule_state_send(view);
                        }
                    } else if(f == SLOT_TEMP_UP) {
                        if(midea_state_can_change_temp(m->state)) {
                            midea_state_temp_up(m->state);
                            schedule_state_send(view);
                        }
                    } else if(f == SLOT_EXTRA) {
                        if(view->extra_callback) view->extra_callback(view->extra_context);
                    } else if(f == SLOT_SETUP) {
                        if(view->setup_callback) view->setup_callback(view->setup_context);
                    } else {
                        MideaToggle t = (MideaToggle)(f - SLOT_TOGGLE_0 + 1);
                        midea_state_toggle(m->state, t);
                        send_toggle_immediate(view, t);
                    }
                    consumed = true;
                    break;
                }
                default:
                    break;
                }
            }
        },
        consumed);

    return consumed;
}

static void midea_main_view_debounce_callback(void* context) {
    MideaMainView* view = context;
    bool should_send = false;
    with_view_model(
        view->view,
        MideaMainViewModel * m,
        {
            if(m->state_pending) {
                m->state_pending = false;
                m->last_was_toggle = false;
                should_send = true;
            }
        },
        false);
    if(should_send && view->send_callback) view->send_callback(view->send_context);
}

// ------------------------------------------------------------- lifecycle ---

MideaMainView* midea_main_view_alloc(void) {
    MideaMainView* view = malloc(sizeof(MideaMainView));
    view->view = view_alloc();
    view->send_callback = NULL;
    view->send_context = NULL;
    view->extra_callback = NULL;
    view->extra_context = NULL;
    view->setup_callback = NULL;
    view->setup_context = NULL;

    view_allocate_model(view->view, ViewModelTypeLocking, sizeof(MideaMainViewModel));
    view_set_context(view->view, view);
    view_set_draw_callback(view->view, midea_main_view_draw);
    view_set_input_callback(view->view, midea_main_view_input);
    view_set_orientation(view->view, ViewOrientationVertical);

    view->debounce_timer =
        furi_timer_alloc(midea_main_view_debounce_callback, FuriTimerTypeOnce, view);

    with_view_model(
        view->view,
        MideaMainViewModel * m,
        {
            m->state = NULL;
            m->focus = SLOT_MODE;
            m->is_sending = false;
            m->send_anim_frame = 0;
            m->last_was_toggle = false;
            m->last_toggle = MideaTogglePowerOff;
            m->state_pending = false;
            m->picker_kind = PickerNone;
            m->picker_index = 0;
        },
        true);

    return view;
}

void midea_main_view_free(MideaMainView* view) {
    if(view) {
        furi_timer_stop(view->debounce_timer);
        furi_timer_free(view->debounce_timer);
        view_free(view->view);
        free(view);
    }
}

View* midea_main_view_get_view(MideaMainView* view) {
    return view->view;
}

void midea_main_view_set_state(MideaMainView* view, MideaState* state) {
    with_view_model(view->view, MideaMainViewModel * m, { m->state = state; }, true);
}

void midea_main_view_set_send_callback(
    MideaMainView* view,
    MideaMainViewSendCallback callback,
    void* context) {
    view->send_callback = callback;
    view->send_context = context;
}

void midea_main_view_set_extra_callback(
    MideaMainView* view,
    MideaMainViewNavigateCallback callback,
    void* context) {
    view->extra_callback = callback;
    view->extra_context = context;
}

void midea_main_view_set_setup_callback(
    MideaMainView* view,
    MideaMainViewNavigateCallback callback,
    void* context) {
    view->setup_callback = callback;
    view->setup_context = context;
}

void midea_main_view_start_sending(MideaMainView* view) {
    with_view_model(
        view->view,
        MideaMainViewModel * m,
        {
            m->is_sending = true;
            m->send_anim_frame = 0;
        },
        true);
}

void midea_main_view_update_sending(MideaMainView* view) {
    with_view_model(
        view->view, MideaMainViewModel * m, { if(m->is_sending) m->send_anim_frame++; }, true);
}

void midea_main_view_stop_sending(MideaMainView* view) {
    with_view_model(
        view->view,
        MideaMainViewModel * m,
        {
            m->is_sending = false;
            m->send_anim_frame = 0;
        },
        true);
}

int midea_main_view_get_last_command(MideaMainView* view, MideaToggle* out_toggle) {
    int result = 0;
    with_view_model(
        view->view,
        MideaMainViewModel * m,
        {
            result = m->last_was_toggle ? 1 : 0;
            if(out_toggle) *out_toggle = m->last_toggle;
        },
        false);
    return result;
}
