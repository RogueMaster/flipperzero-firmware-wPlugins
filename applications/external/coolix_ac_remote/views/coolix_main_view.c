#include "coolix_main_view.h"
#include "../coolix_ir_protocol.h"
#include <furi.h>
#include <gui/elements.h>
#include <stdlib.h>

// Standard button width for toggle buttons
#define BTN_WIDTH  30
#define BTN_HEIGHT 12

// Temperature +/- boxes
#define TEMP_BTN_W 14
#define TEMP_BTN_L 2
#define TEMP_BTN_R 48

// Dropdown picker row height and vertical position (right below row 1 buttons)
#define PICKER_ITEM_H 11
#define PICKER_Y      23

// Focus items enumeration
typedef enum {
    FocusMode = 0,
    FocusFan,
    FocusTempDown,
    FocusTempUp,
    FocusSwing,
    FocusDirect,
    FocusTurbo,
    FocusLed,
    FocusSleep,
    FocusExtra,
    FocusSetup,
    FocusCount
} FocusItem;

// Which parameter the dropdown picker is currently showing, if any
typedef enum {
    CoolixPickerNone = 0,
    CoolixPickerMode,
    CoolixPickerFan,
} CoolixPickerKind;

// Model for the view
typedef struct {
    CoolixState* state;
    FocusItem focus;
    bool is_sending;
    int send_anim_frame;

    // Last command info
    bool last_was_toggle;
    CoolixToggle last_toggle;

    // Debounce: pending state send
    bool state_pending;

    // Dropdown picker for Mode/Fan
    CoolixPickerKind picker_kind;
    uint8_t picker_index;
} CoolixMainViewModel;

struct CoolixMainView {
    View* view;
    CoolixMainViewSendCallback send_callback;
    void* send_context;
    CoolixMainViewNavigateCallback extra_callback;
    void* extra_context;
    CoolixMainViewNavigateCallback setup_callback;
    void* setup_context;

    // Debounce timer for state commands
    FuriTimer* debounce_timer;
};

// Forward declarations
static void coolix_main_view_draw(Canvas* canvas, void* model);
static bool coolix_main_view_input(InputEvent* event, void* context);
static void coolix_main_view_debounce_callback(void* context);

// Two independent cues, so neither is ambiguous:
//   inverted -> this button has the cursor on it
//   thick    -> this toggle is believed to be on
// The active ring is drawn one pixel OUTSIDE the button so it never touches
// the label; an inner ring clipped the descenders of the centred text. It is
// always black, sitting on the white background rather than on the button
// body, so it stays visible whether or not the button is inverted.
// Leaves the canvas colour set for the caller's content; callers reset to
// ColorBlack when done.
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

// Helper: Draw button with label above (for Mode/Fan)
static void draw_selector_button(
    Canvas* canvas,
    int16_t x,
    int16_t y,
    int16_t w,
    const char* label,
    const char* value,
    bool focused) {
    // Label above button
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, x + w / 2, y, AlignCenter, AlignTop, label);

    // Button below label
    int16_t btn_y = y + 10;
    draw_button_body(canvas, x, btn_y, w, 13, focused, false);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, x + w / 2, btn_y + 10, AlignCenter, AlignBottom, value);
    canvas_set_color(canvas, ColorBlack);
}

// Helper: Draw a +/- icon with uniform stroke weight, centered in a box
static void draw_temp_icon(Canvas* canvas, int16_t box_x, int16_t box_y, bool plus) {
    int16_t cx = box_x + 7;
    int16_t cy = box_y + 7;
    canvas_draw_box(canvas, cx - 4, cy - 1, 8, 2);
    if(plus) {
        canvas_draw_box(canvas, cx - 1, cy - 4, 2, 8);
    }
}

// Helper: Draw temperature with +/- buttons
static void draw_temperature(
    Canvas* canvas,
    int16_t y,
    uint8_t temp,
    bool focus_down,
    bool focus_up,
    bool can_change) {
    char temp_str[8];
    snprintf(temp_str, sizeof(temp_str), "%d", temp);

    // Big temperature number centered
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, 32, y, AlignCenter, AlignTop, temp_str);

    if(!can_change) return;

    int16_t btn_y = y + 2;
    draw_button_body(canvas, TEMP_BTN_L, btn_y, TEMP_BTN_W, TEMP_BTN_W, focus_down, false);
    draw_temp_icon(canvas, TEMP_BTN_L, btn_y, false);
    canvas_set_color(canvas, ColorBlack);

    draw_button_body(canvas, TEMP_BTN_R, btn_y, TEMP_BTN_W, TEMP_BTN_W, focus_up, false);
    draw_temp_icon(canvas, TEMP_BTN_R, btn_y, true);
    canvas_set_color(canvas, ColorBlack);
}

// Helper: Draw standard toggle button. A filled dot marks an active toggle,
// since the AC never reports back what it actually did.
static void draw_toggle_btn(
    Canvas* canvas,
    int16_t x,
    int16_t y,
    const char* label,
    bool focused,
    bool active) {
    canvas_set_font(canvas, FontSecondary);
    draw_button_body(canvas, x, y, BTN_WIDTH, BTN_HEIGHT, focused, active);
    canvas_draw_str_aligned(canvas, x + BTN_WIDTH / 2, y + 2, AlignCenter, AlignTop, label);
    canvas_set_color(canvas, ColorBlack);
}

// Helper: Draw menu button at position
static void
    draw_menu_btn_at(Canvas* canvas, int16_t x, int16_t y, const char* label, bool focused) {
    canvas_set_font(canvas, FontSecondary);
    draw_button_body(canvas, x, y, BTN_WIDTH, BTN_HEIGHT, focused, false);
    canvas_draw_str_aligned(canvas, x + BTN_WIDTH / 2, y + 2, AlignCenter, AlignTop, label);
    canvas_set_color(canvas, ColorBlack);
}

// Helper: Draw menu button centered
static void draw_menu_btn(Canvas* canvas, int16_t y, const char* label, bool focused) {
    draw_menu_btn_at(canvas, 32 - BTN_WIDTH / 2, y, label, focused);
}

// Helper: Draw the protocol name with a sending animation on either side.
// Arrow columns are placed from the measured text width so the name can be
// changed without them colliding with it.
static void draw_logo(Canvas* canvas, int16_t y, bool sending, int frame) {
    static const char* const name = "Coolix";

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 32, y, AlignCenter, AlignTop, name);

    if(!sending) return;

    // Arrows grow 0->1->2->3->2->1 and repeat
    int phase = frame % 6;
    int num_arrows = (phase <= 3) ? phase : (6 - phase);

    int16_t half_text = canvas_string_width(canvas, name) / 2;
    int16_t center_y = y + 5;
    int16_t left_edge = 32 - half_text - 3; // rightmost arrow tip on the left
    int16_t right_edge = 32 + half_text + 3; // leftmost arrow tip on the right

    for(int i = 0; i < num_arrows; i++) {
        // Left side: ">" pointing in towards the name
        int16_t tip = left_edge - i * 4;
        canvas_draw_line(canvas, tip - 3, center_y - 2, tip, center_y);
        canvas_draw_line(canvas, tip - 3, center_y + 2, tip, center_y);

        // Right side: "<" pointing in towards the name
        tip = right_edge + i * 4;
        canvas_draw_line(canvas, tip + 3, center_y - 2, tip, center_y);
        canvas_draw_line(canvas, tip + 3, center_y + 2, tip, center_y);
    }
}

// Helper: Number of items in a picker's list
static uint8_t picker_item_count(CoolixPickerKind kind) {
    return kind == CoolixPickerMode ? CoolixModeCount : CoolixFanCount;
}

// Helper: Name of a picker item by index
static const char* picker_item_name(CoolixPickerKind kind, uint8_t index) {
    if(kind == CoolixPickerMode) {
        return coolix_ir_get_mode_name((CoolixMode)index);
    }
    return coolix_ir_get_fan_name((CoolixFan)index);
}

// Helper: Draw dropdown picker overlay on top of the current screen
static void draw_picker(Canvas* canvas, CoolixMainViewModel* m) {
    uint8_t count = picker_item_count(m->picker_kind);
    int16_t box_x = (m->picker_kind == CoolixPickerMode) ? 1 : 33;
    int16_t box_w = BTN_WIDTH;
    int16_t box_h = count * PICKER_ITEM_H + 2;

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, box_x, PICKER_Y, box_w, box_h);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, box_x, PICKER_Y, box_w, box_h);

    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < count; i++) {
        int16_t row_y = PICKER_Y + 1 + i * PICKER_ITEM_H;
        int16_t text_y = row_y + PICKER_ITEM_H / 2;
        const char* name = picker_item_name(m->picker_kind, i);

        if(i == m->picker_index) {
            canvas_draw_box(canvas, box_x + 1, row_y, box_w - 2, PICKER_ITEM_H - 1);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str_aligned(
                canvas, box_x + box_w / 2, text_y, AlignCenter, AlignCenter, name);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str_aligned(
                canvas, box_x + box_w / 2, text_y, AlignCenter, AlignCenter, name);
        }
    }
}

static void coolix_main_view_draw(Canvas* canvas, void* model) {
    CoolixMainViewModel* m = model;
    if(!m->state) return;

    canvas_clear(canvas);

    const char* fan_value = coolix_ir_get_fan_name(m->state->fan);
    if(m->state->mode == CoolixModeOff) {
        fan_value = "--";
    } else if(!coolix_state_can_change_fan(m->state)) {
        fan_value = "Auto";
    }

    // Row 1: Mode and Fan with labels above (y=0)
    draw_selector_button(
        canvas, 1, 0, 30, "Mode", coolix_ir_get_mode_name(m->state->mode), m->focus == FocusMode);
    draw_selector_button(canvas, 33, 0, 30, "Fan", fan_value, m->focus == FocusFan);

    // Row 2: Temperature (y=28)
    bool can_temp = coolix_state_can_change_temp(m->state);
    if(m->state->mode == CoolixModeFan || m->state->mode == CoolixModeOff) {
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, 32, 28, AlignCenter, AlignTop, "--");
    } else {
        draw_temperature(
            canvas,
            28,
            m->state->temp,
            m->focus == FocusTempDown,
            m->focus == FocusTempUp,
            can_temp);
    }

    // Row 3: Swing, Direct (y=49)
    draw_toggle_btn(canvas, 1, 49, "Swing", m->focus == FocusSwing, m->state->swing);
    draw_toggle_btn(canvas, 33, 49, "Direct", m->focus == FocusDirect, false);

    // Row 4: Turbo, LED (y=64)
    draw_toggle_btn(canvas, 1, 64, "Turbo", m->focus == FocusTurbo, m->state->turbo);
    draw_toggle_btn(canvas, 33, 64, "LED", m->focus == FocusLed, m->state->led);

    // Row 5: Sleep and Extra (y=79)
    draw_toggle_btn(canvas, 1, 79, "Sleep", m->focus == FocusSleep, m->state->sleep);
    draw_menu_btn_at(canvas, 33, 79, "Extra", m->focus == FocusExtra);

    // Row 6: Setup button (y=94)
    draw_menu_btn(canvas, 94, "Setup", m->focus == FocusSetup);

    // Row 7: protocol name at bottom (y=116)
    draw_logo(canvas, 116, m->is_sending, m->send_anim_frame);

    // Dropdown picker overlay, drawn last so it sits on top without shifting anything
    if(m->picker_kind != CoolixPickerNone) {
        draw_picker(canvas, m);
    }
}

// Debounce timer callback
static void coolix_main_view_debounce_callback(void* context) {
    CoolixMainView* view = context;

    bool should_send = false;
    with_view_model(
        view->view,
        CoolixMainViewModel * m,
        {
            if(m->state_pending) {
                m->state_pending = false;
                m->last_was_toggle = false;
                should_send = true;
            }
        },
        false);

    if(should_send && view->send_callback) {
        view->send_callback(view->send_context);
    }
}

// Schedule debounced state send
static void schedule_state_send(CoolixMainView* view) {
    with_view_model(view->view, CoolixMainViewModel * m, { m->state_pending = true; }, false);

    furi_timer_stop(view->debounce_timer);
    furi_timer_start(view->debounce_timer, COOLIX_SEND_DEBOUNCE_MS);
}

// Send toggle immediately
static void send_toggle_immediate(CoolixMainView* view, CoolixToggle toggle) {
    furi_timer_stop(view->debounce_timer);

    with_view_model(
        view->view,
        CoolixMainViewModel * m,
        {
            m->state_pending = false;
            m->last_was_toggle = true;
            m->last_toggle = toggle;
        },
        false);

    if(view->send_callback) {
        view->send_callback(view->send_context);
    }
}

// Send a confirmed picker selection immediately (no debounce)
static void send_state_immediate(CoolixMainView* view) {
    furi_timer_stop(view->debounce_timer);

    with_view_model(
        view->view,
        CoolixMainViewModel * m,
        {
            m->state_pending = false;
            m->last_was_toggle = false;
        },
        false);

    if(view->send_callback) {
        view->send_callback(view->send_context);
    }
}

static bool coolix_main_view_input(InputEvent* event, void* context) {
    CoolixMainView* view = context;
    bool consumed = false;

    with_view_model(
        view->view,
        CoolixMainViewModel * m,
        {
            if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
                if(m->picker_kind != CoolixPickerNone) {
                    // Dropdown picker is open: Up/Down browse, Ok confirms & sends, Back cancels
                    switch(event->key) {
                    case InputKeyUp:
                        if(m->picker_index > 0) {
                            m->picker_index--;
                        }
                        consumed = true;
                        break;
                    case InputKeyDown:
                        if(m->picker_index + 1 < picker_item_count(m->picker_kind)) {
                            m->picker_index++;
                        }
                        consumed = true;
                        break;
                    case InputKeyOk:
                        if(event->type == InputTypeShort) {
                            if(m->picker_kind == CoolixPickerMode) {
                                coolix_state_set_mode(m->state, (CoolixMode)m->picker_index);
                            } else {
                                coolix_state_set_fan(m->state, (CoolixFan)m->picker_index);
                            }
                            m->picker_kind = CoolixPickerNone;
                            send_state_immediate(view);
                        }
                        consumed = true;
                        break;
                    case InputKeyBack:
                        if(event->type == InputTypeShort) {
                            m->picker_kind = CoolixPickerNone;
                        }
                        consumed = true;
                        break;
                    default:
                        consumed = true;
                        break;
                    }
                } else {
                    switch(event->key) {
                    case InputKeyLeft:
                        // Navigation only
                        if(m->focus == FocusFan) {
                            m->focus = FocusMode;
                        } else if(m->focus == FocusTempUp) {
                            m->focus = FocusTempDown;
                        } else if(m->focus == FocusDirect) {
                            m->focus = FocusSwing;
                        } else if(m->focus == FocusLed) {
                            m->focus = FocusTurbo;
                        } else if(m->focus == FocusExtra) {
                            m->focus = FocusSleep;
                        }
                        consumed = true;
                        break;

                    case InputKeyRight:
                        // Navigation only
                        if(m->focus == FocusMode) {
                            m->focus = FocusFan;
                        } else if(m->focus == FocusTempDown) {
                            m->focus = FocusTempUp;
                        } else if(m->focus == FocusSwing) {
                            m->focus = FocusDirect;
                        } else if(m->focus == FocusTurbo) {
                            m->focus = FocusLed;
                        } else if(m->focus == FocusSleep) {
                            m->focus = FocusExtra;
                        }
                        consumed = true;
                        break;

                    case InputKeyUp:
                        // Preserve column position when navigating up
                        if(m->focus == FocusMode || m->focus == FocusFan) {
                            // Already at top, do nothing
                        } else if(m->focus == FocusTempDown) {
                            m->focus = FocusMode;
                        } else if(m->focus == FocusTempUp) {
                            m->focus = FocusFan;
                        } else if(m->focus == FocusSwing) {
                            m->focus = FocusTempDown;
                        } else if(m->focus == FocusDirect) {
                            m->focus = FocusTempUp;
                        } else if(m->focus == FocusTurbo) {
                            m->focus = FocusSwing;
                        } else if(m->focus == FocusLed) {
                            m->focus = FocusDirect;
                        } else if(m->focus == FocusSleep) {
                            m->focus = FocusTurbo;
                        } else if(m->focus == FocusExtra) {
                            m->focus = FocusLed;
                        } else if(m->focus == FocusSetup) {
                            m->focus = FocusSleep;
                        }
                        consumed = true;
                        break;

                    case InputKeyDown:
                        // Preserve column position when navigating down
                        if(m->focus == FocusMode) {
                            m->focus = FocusTempDown;
                        } else if(m->focus == FocusFan) {
                            m->focus = FocusTempUp;
                        } else if(m->focus == FocusTempDown) {
                            m->focus = FocusSwing;
                        } else if(m->focus == FocusTempUp) {
                            m->focus = FocusDirect;
                        } else if(m->focus == FocusSwing) {
                            m->focus = FocusTurbo;
                        } else if(m->focus == FocusDirect) {
                            m->focus = FocusLed;
                        } else if(m->focus == FocusTurbo) {
                            m->focus = FocusSleep;
                        } else if(m->focus == FocusLed) {
                            m->focus = FocusExtra;
                        } else if(m->focus == FocusSleep || m->focus == FocusExtra) {
                            m->focus = FocusSetup;
                        }
                        // Setup is at bottom, do nothing
                        consumed = true;
                        break;

                    case InputKeyOk:
                        if(event->type != InputTypeShort) {
                            consumed = true;
                            break;
                        }
                        switch(m->focus) {
                        case FocusMode:
                            m->picker_kind = CoolixPickerMode;
                            m->picker_index = (uint8_t)m->state->mode;
                            break;
                        case FocusFan:
                            if(coolix_state_can_change_fan(m->state)) {
                                m->picker_kind = CoolixPickerFan;
                                m->picker_index = (uint8_t)m->state->fan;
                            }
                            break;
                        case FocusTempDown:
                            if(coolix_state_can_change_temp(m->state)) {
                                coolix_state_temp_down(m->state);
                                schedule_state_send(view);
                            }
                            break;
                        case FocusTempUp:
                            if(coolix_state_can_change_temp(m->state)) {
                                coolix_state_temp_up(m->state);
                                schedule_state_send(view);
                            }
                            break;
                        case FocusSwing:
                            coolix_state_toggle(m->state, CoolixToggleSwing);
                            send_toggle_immediate(view, CoolixToggleSwing);
                            break;
                        case FocusDirect:
                            coolix_state_toggle(m->state, CoolixToggleDirect);
                            send_toggle_immediate(view, CoolixToggleDirect);
                            break;
                        case FocusTurbo:
                            coolix_state_toggle(m->state, CoolixToggleTurbo);
                            send_toggle_immediate(view, CoolixToggleTurbo);
                            break;
                        case FocusLed:
                            coolix_state_toggle(m->state, CoolixToggleLed);
                            send_toggle_immediate(view, CoolixToggleLed);
                            break;
                        case FocusSleep:
                            coolix_state_toggle(m->state, CoolixToggleSleep);
                            send_toggle_immediate(view, CoolixToggleSleep);
                            break;
                        case FocusExtra:
                            if(view->extra_callback) {
                                view->extra_callback(view->extra_context);
                            }
                            break;
                        case FocusSetup:
                            if(view->setup_callback) {
                                view->setup_callback(view->setup_context);
                            }
                            break;
                        default:
                            break;
                        }
                        consumed = true;
                        break;

                    default:
                        break;
                    }
                }
            }
        },
        consumed);

    return consumed;
}

CoolixMainView* coolix_main_view_alloc(void) {
    CoolixMainView* view = malloc(sizeof(CoolixMainView));
    view->view = view_alloc();

    view_allocate_model(view->view, ViewModelTypeLocking, sizeof(CoolixMainViewModel));
    view_set_context(view->view, view);
    view_set_draw_callback(view->view, coolix_main_view_draw);
    view_set_input_callback(view->view, coolix_main_view_input);
    view_set_orientation(view->view, ViewOrientationVertical);

    view->debounce_timer =
        furi_timer_alloc(coolix_main_view_debounce_callback, FuriTimerTypeOnce, view);

    with_view_model(
        view->view,
        CoolixMainViewModel * m,
        {
            m->state = NULL;
            m->focus = FocusMode;
            m->is_sending = false;
            m->send_anim_frame = 0;
            m->last_was_toggle = false;
            m->last_toggle = CoolixTogglePowerOff;
            m->state_pending = false;
            m->picker_kind = CoolixPickerNone;
            m->picker_index = 0;
        },
        true);

    return view;
}

void coolix_main_view_free(CoolixMainView* view) {
    if(view) {
        furi_timer_stop(view->debounce_timer);
        furi_timer_free(view->debounce_timer);
        view_free(view->view);
        free(view);
    }
}

View* coolix_main_view_get_view(CoolixMainView* view) {
    return view->view;
}

void coolix_main_view_set_state(CoolixMainView* view, CoolixState* state) {
    with_view_model(view->view, CoolixMainViewModel * m, { m->state = state; }, true);
}

void coolix_main_view_set_send_callback(
    CoolixMainView* view,
    CoolixMainViewSendCallback callback,
    void* context) {
    view->send_callback = callback;
    view->send_context = context;
}

void coolix_main_view_set_extra_callback(
    CoolixMainView* view,
    CoolixMainViewNavigateCallback callback,
    void* context) {
    view->extra_callback = callback;
    view->extra_context = context;
}

void coolix_main_view_set_setup_callback(
    CoolixMainView* view,
    CoolixMainViewNavigateCallback callback,
    void* context) {
    view->setup_callback = callback;
    view->setup_context = context;
}

void coolix_main_view_start_sending(CoolixMainView* view) {
    with_view_model(
        view->view,
        CoolixMainViewModel * m,
        {
            m->is_sending = true;
            m->send_anim_frame = 0;
        },
        true);
}

void coolix_main_view_update_sending(CoolixMainView* view) {
    with_view_model(
        view->view,
        CoolixMainViewModel * m,
        {
            if(m->is_sending) {
                m->send_anim_frame++;
            }
        },
        true);
}

void coolix_main_view_stop_sending(CoolixMainView* view) {
    with_view_model(
        view->view,
        CoolixMainViewModel * m,
        {
            m->is_sending = false;
            m->send_anim_frame = 0;
        },
        true);
}

int coolix_main_view_get_last_command(CoolixMainView* view, CoolixToggle* out_toggle) {
    int result = 0;
    with_view_model(
        view->view,
        CoolixMainViewModel * m,
        {
            result = m->last_was_toggle ? 1 : 0;
            if(out_toggle) {
                *out_toggle = m->last_toggle;
            }
        },
        false);
    return result;
}
