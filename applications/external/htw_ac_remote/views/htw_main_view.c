#include "htw_main_view.h"
#include "../htw_ir_protocol.h"
#include <furi.h>
#include <gui/elements.h>
#include <stdlib.h>

// Standard button width for toggle buttons
#define BTN_WIDTH  30
#define BTN_HEIGHT 12

// Focus items enumeration
typedef enum {
    FocusMode = 0,
    FocusFan,
    FocusTempDown,
    FocusTempUp,
    FocusSwing,
    FocusTurbo,
    FocusFresh,
    FocusLed,
    FocusClean,
    FocusTimer,
    FocusSetup,
    FocusCount
} FocusItem;

// Model for the view
typedef struct {
    HtwState* state;
    FocusItem focus;
    bool is_sending;
    int send_anim_frame;

    // Last command info
    bool last_was_toggle;
    HtwToggle last_toggle;

    // Debounce: pending state send
    bool state_pending;
} HtwMainViewModel;

struct HtwMainView {
    View* view;
    HtwMainViewSendCallback send_callback;
    void* send_context;
    HtwMainViewNavigateCallback timer_callback;
    void* timer_context;
    HtwMainViewNavigateCallback setup_callback;
    void* setup_context;

    // Debounce timer for state commands
    FuriTimer* debounce_timer;
};

// Forward declarations
static void htw_main_view_draw(Canvas* canvas, void* model);
static bool htw_main_view_input(InputEvent* event, void* context);
static void htw_main_view_debounce_callback(void* context);

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
    if(focused) {
        canvas_draw_rbox(canvas, x, btn_y, w, 13, 2);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, x, btn_y, w, 13, 2);
    }

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, x + w / 2, btn_y + 10, AlignCenter, AlignBottom, value);
    canvas_set_color(canvas, ColorBlack);
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

    // Minus button (left)
    int16_t btn_y = y + 2;
    if(focus_down) {
        canvas_draw_rbox(canvas, 2, btn_y, 14, 14, 2);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, 2, btn_y, 14, 14, 2);
    }
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 6, btn_y + 11, "-");
    canvas_set_color(canvas, ColorBlack);

    // Plus button (right)
    if(focus_up) {
        canvas_draw_rbox(canvas, 48, btn_y, 14, 14, 2);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, 48, btn_y, 14, 14, 2);
    }
    canvas_draw_str(canvas, 51, btn_y + 11, "+");
    canvas_set_color(canvas, ColorBlack);
}

// Helper: Draw standard toggle button
static void
    draw_toggle_btn(Canvas* canvas, int16_t x, int16_t y, const char* label, bool focused) {
    canvas_set_font(canvas, FontSecondary);

    if(focused) {
        canvas_draw_rbox(canvas, x, y, BTN_WIDTH, BTN_HEIGHT, 2);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, x + BTN_WIDTH / 2, y + 2, AlignCenter, AlignTop, label);
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_rframe(canvas, x, y, BTN_WIDTH, BTN_HEIGHT, 2);
        canvas_draw_str_aligned(canvas, x + BTN_WIDTH / 2, y + 2, AlignCenter, AlignTop, label);
    }
}

// Helper: Draw menu button at position
static void
    draw_menu_btn_at(Canvas* canvas, int16_t x, int16_t y, const char* label, bool focused) {
    canvas_set_font(canvas, FontSecondary);

    if(focused) {
        canvas_draw_rbox(canvas, x, y, BTN_WIDTH, BTN_HEIGHT, 2);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, x + BTN_WIDTH / 2, y + 2, AlignCenter, AlignTop, label);
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_rframe(canvas, x, y, BTN_WIDTH, BTN_HEIGHT, 2);
        canvas_draw_str_aligned(canvas, x + BTN_WIDTH / 2, y + 2, AlignCenter, AlignTop, label);
    }
}

// Helper: Draw menu button centered
static void draw_menu_btn(Canvas* canvas, int16_t y, const char* label, bool focused) {
    draw_menu_btn_at(canvas, 32 - BTN_WIDTH / 2, y, label, focused);
}

// Helper: Draw HTW logo with sending animation
static void draw_logo(Canvas* canvas, int16_t y, bool sending, int frame) {
    canvas_set_font(canvas, FontPrimary);
    int16_t center_y = y + 5;

    // Draw HTW text
    canvas_draw_str_aligned(canvas, 32, y, AlignCenter, AlignTop, "HTW");

    if(sending) {
        // Animation: arrows grow 0->1->2->3->2->1->0 and repeat
        int phase = frame % 6;
        int num_arrows;
        if(phase <= 3) {
            num_arrows = phase; // 0, 1, 2, 3
        } else {
            num_arrows = 6 - phase; // 2, 1
        }

        // Left side: arrows pointing right (>) towards HTW
        for(int i = 0; i < num_arrows; i++) {
            int x = 4 + i * 4; // 4, 8, 12 (from edge towards center)
            // Draw > shape: tip points right
            canvas_draw_line(canvas, x, center_y - 2, x + 3, center_y);
            canvas_draw_line(canvas, x, center_y + 2, x + 3, center_y);
        }

        // Right side: arrows pointing left (<) towards HTW
        for(int i = 0; i < num_arrows; i++) {
            int x = 60 - i * 4; // 60, 56, 52 (from edge towards center)
            // Draw < shape: tip points left
            canvas_draw_line(canvas, x, center_y - 2, x - 3, center_y);
            canvas_draw_line(canvas, x, center_y + 2, x - 3, center_y);
        }
    }
}

static void htw_main_view_draw(Canvas* canvas, void* model) {
    HtwMainViewModel* m = model;
    if(!m->state) return;

    canvas_clear(canvas);

    const char* fan_value = htw_ir_get_fan_name(m->state->fan);
    if(m->state->mode == HtwModeOff) {
        fan_value = "--";
    } else if(!htw_state_can_change_fan(m->state)) {
        fan_value = "Auto";
    }

    // Row 1: Mode and Fan with labels above (y=0)
    draw_selector_button(
        canvas, 1, 0, 30, "Mode", htw_ir_get_mode_name(m->state->mode), m->focus == FocusMode);
    draw_selector_button(canvas, 33, 0, 30, "Fan", fan_value, m->focus == FocusFan);

    // Row 2: Temperature (y=26)
    bool can_temp = htw_state_can_change_temp(m->state);
    if(m->state->mode == HtwModeFan || m->state->mode == HtwModeOff) {
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, 32, 26, AlignCenter, AlignTop, "--");
    } else {
        draw_temperature(
            canvas,
            26,
            m->state->temp,
            m->focus == FocusTempDown,
            m->focus == FocusTempUp,
            can_temp);
    }

    // Row 3: Toggle buttons - Swing, Turbo (y=46)
    draw_toggle_btn(canvas, 2, 46, "Swing", m->focus == FocusSwing);
    draw_toggle_btn(canvas, 34, 46, "Turbo", m->focus == FocusTurbo);

    // Row 4: Toggle buttons - Fresh, LED (y=60)
    draw_toggle_btn(canvas, 2, 60, "Fresh", m->focus == FocusFresh);
    draw_toggle_btn(canvas, 34, 60, "LED", m->focus == FocusLed);

    // Row 5: Clean and Timer buttons (y=74)
    draw_toggle_btn(canvas, 2, 74, "Clean", m->focus == FocusClean);
    draw_menu_btn_at(canvas, 34, 74, "Timer", m->focus == FocusTimer);

    // Row 6: Setup button (y=88)
    draw_menu_btn(canvas, 88, "Setup", m->focus == FocusSetup);

    // Row 7: HTW logo at bottom (y=116)
    draw_logo(canvas, 116, m->is_sending, m->send_anim_frame);
}

// Debounce timer callback
static void htw_main_view_debounce_callback(void* context) {
    HtwMainView* view = context;

    bool should_send = false;
    with_view_model(
        view->view,
        HtwMainViewModel * m,
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
static void schedule_state_send(HtwMainView* view) {
    with_view_model(view->view, HtwMainViewModel * m, { m->state_pending = true; }, false);

    furi_timer_stop(view->debounce_timer);
    furi_timer_start(view->debounce_timer, HTW_SEND_DEBOUNCE_MS);
}

// Send toggle immediately
static void send_toggle_immediate(HtwMainView* view, HtwToggle toggle) {
    furi_timer_stop(view->debounce_timer);

    with_view_model(
        view->view,
        HtwMainViewModel * m,
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

static bool htw_main_view_input(InputEvent* event, void* context) {
    HtwMainView* view = context;
    bool consumed = false;

    with_view_model(
        view->view,
        HtwMainViewModel * m,
        {
            if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
                switch(event->key) {
                case InputKeyLeft:
                    // Navigation only
                    if(m->focus == FocusFan) {
                        m->focus = FocusMode;
                    } else if(m->focus == FocusTempUp) {
                        m->focus = FocusTempDown;
                    } else if(m->focus == FocusTurbo) {
                        m->focus = FocusSwing;
                    } else if(m->focus == FocusLed) {
                        m->focus = FocusFresh;
                    } else if(m->focus == FocusTimer) {
                        m->focus = FocusClean;
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
                        m->focus = FocusTurbo;
                    } else if(m->focus == FocusFresh) {
                        m->focus = FocusLed;
                    } else if(m->focus == FocusClean) {
                        m->focus = FocusTimer;
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
                    } else if(m->focus == FocusTurbo) {
                        m->focus = FocusTempUp;
                    } else if(m->focus == FocusFresh) {
                        m->focus = FocusSwing;
                    } else if(m->focus == FocusLed) {
                        m->focus = FocusTurbo;
                    } else if(m->focus == FocusClean) {
                        m->focus = FocusFresh;
                    } else if(m->focus == FocusTimer) {
                        m->focus = FocusLed;
                    } else if(m->focus == FocusSetup) {
                        m->focus = FocusClean;
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
                        m->focus = FocusTurbo;
                    } else if(m->focus == FocusSwing) {
                        m->focus = FocusFresh;
                    } else if(m->focus == FocusTurbo) {
                        m->focus = FocusLed;
                    } else if(m->focus == FocusFresh) {
                        m->focus = FocusClean;
                    } else if(m->focus == FocusLed) {
                        m->focus = FocusTimer;
                    } else if(m->focus == FocusClean || m->focus == FocusTimer) {
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
                        htw_state_mode_next(m->state);
                        schedule_state_send(view);
                        break;
                    case FocusFan:
                        if(htw_state_can_change_fan(m->state)) {
                            htw_state_fan_next(m->state);
                            schedule_state_send(view);
                        }
                        break;
                    case FocusTempDown:
                        if(htw_state_can_change_temp(m->state)) {
                            htw_state_temp_down(m->state);
                            schedule_state_send(view);
                        }
                        break;
                    case FocusTempUp:
                        if(htw_state_can_change_temp(m->state)) {
                            htw_state_temp_up(m->state);
                            schedule_state_send(view);
                        }
                        break;
                    case FocusSwing:
                        htw_state_toggle(m->state, HtwToggleSwing);
                        send_toggle_immediate(view, HtwToggleSwing);
                        break;
                    case FocusTurbo:
                        htw_state_toggle(m->state, HtwToggleTurbo);
                        send_toggle_immediate(view, HtwToggleTurbo);
                        break;
                    case FocusFresh:
                        htw_state_toggle(m->state, HtwToggleFresh);
                        send_toggle_immediate(view, HtwToggleFresh);
                        break;
                    case FocusLed:
                        htw_state_toggle(m->state, HtwToggleLed);
                        send_toggle_immediate(view, HtwToggleLed);
                        break;
                    case FocusClean:
                        htw_state_toggle(m->state, HtwToggleClean);
                        send_toggle_immediate(view, HtwToggleClean);
                        break;
                    case FocusTimer:
                        if(view->timer_callback) {
                            view->timer_callback(view->timer_context);
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
        },
        consumed);

    return consumed;
}

HtwMainView* htw_main_view_alloc(void) {
    HtwMainView* view = malloc(sizeof(HtwMainView));
    view->view = view_alloc();

    view_allocate_model(view->view, ViewModelTypeLocking, sizeof(HtwMainViewModel));
    view_set_context(view->view, view);
    view_set_draw_callback(view->view, htw_main_view_draw);
    view_set_input_callback(view->view, htw_main_view_input);
    view_set_orientation(view->view, ViewOrientationVertical);

    view->debounce_timer =
        furi_timer_alloc(htw_main_view_debounce_callback, FuriTimerTypeOnce, view);

    with_view_model(
        view->view,
        HtwMainViewModel * m,
        {
            m->state = NULL;
            m->focus = FocusMode;
            m->is_sending = false;
            m->send_anim_frame = 0;
            m->last_was_toggle = false;
            m->last_toggle = HtwTogglePowerOff;
            m->state_pending = false;
        },
        true);

    return view;
}

void htw_main_view_free(HtwMainView* view) {
    if(view) {
        furi_timer_stop(view->debounce_timer);
        furi_timer_free(view->debounce_timer);
        view_free(view->view);
        free(view);
    }
}

View* htw_main_view_get_view(HtwMainView* view) {
    return view->view;
}

void htw_main_view_set_state(HtwMainView* view, HtwState* state) {
    with_view_model(view->view, HtwMainViewModel * m, { m->state = state; }, true);
}

void htw_main_view_set_send_callback(
    HtwMainView* view,
    HtwMainViewSendCallback callback,
    void* context) {
    view->send_callback = callback;
    view->send_context = context;
}

void htw_main_view_set_timer_callback(
    HtwMainView* view,
    HtwMainViewNavigateCallback callback,
    void* context) {
    view->timer_callback = callback;
    view->timer_context = context;
}

void htw_main_view_set_setup_callback(
    HtwMainView* view,
    HtwMainViewNavigateCallback callback,
    void* context) {
    view->setup_callback = callback;
    view->setup_context = context;
}

void htw_main_view_start_sending(HtwMainView* view) {
    with_view_model(
        view->view,
        HtwMainViewModel * m,
        {
            m->is_sending = true;
            m->send_anim_frame = 0;
        },
        true);
}

void htw_main_view_update_sending(HtwMainView* view) {
    with_view_model(
        view->view,
        HtwMainViewModel * m,
        {
            if(m->is_sending) {
                m->send_anim_frame++;
            }
        },
        true);
}

void htw_main_view_stop_sending(HtwMainView* view) {
    with_view_model(
        view->view,
        HtwMainViewModel * m,
        {
            m->is_sending = false;
            m->send_anim_frame = 0;
        },
        true);
}

int htw_main_view_get_last_command(HtwMainView* view, HtwToggle* out_toggle) {
    int result = 0;
    with_view_model(
        view->view,
        HtwMainViewModel * m,
        {
            result = m->last_was_toggle ? 1 : 0;
            if(out_toggle) {
                *out_toggle = m->last_toggle;
            }
        },
        false);
    return result;
}
