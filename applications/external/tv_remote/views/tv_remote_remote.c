/**
 * @file views/tv_remote_remote.c
 * @brief Remote control view – landscape d-pad layout.
 *
 * The display is rotated 90° CCW (ViewOrientationVerticalFlip) so the canvas
 * is 64 wide × 128 tall and the Flipper's d-pad sits on the right side.
 *
 * Physical key → IR action mapping:
 *   Press          Hold
 *   ─────          ────
 *   Up   → Up      Up   → Vol_up
 *   Down → Down    Down → Vol_dn
 *   Left → Left    Left → Ch_dn
 *   Right→ Right   Right→ Ch_up
 *   Ok   → Ok      Ok   → Home
 *   Back → Back    Back → exit app
 *   Back ×2 (double-tap) → Power
 */

#include "tv_remote_remote.h"
#include "../flipper_tv_remote.h"

#include <gui/elements.h>

/* ---- Layout constants (64×128 portrait canvas) ---- */

#define DISP_W 64

/* Concentric-circle radii (Home < Ok < D-pad < Hold) */
#define R_HOME  7
#define R_OK   14
#define R_DPAD 22
#define R_HOLD 30

/* Diagonal intercepts at 45° for divider lines: round(r * 0.707). */
#define D_OK   10
#define D_HOLD 21

/* Double-tap detection window (ms) */
#define DOUBLE_TAP_MS 400

/* Brief IR burst duration for single-shot buttons (ms) */
#define IR_BURST_MS 150

/* ---- Press / hold button mapping ---- */

typedef struct {
    uint8_t press_btn;  /**< TvButton index for short press. */
    uint8_t hold_btn;   /**< TvButton index for long press.  */
} ButtonMapping;

static const ButtonMapping key_map[] = {
    [InputKeyUp]    = {TvButtonUp,    TvButtonVolUp},
    [InputKeyDown]  = {TvButtonDown,  TvButtonVolDn},
    [InputKeyLeft]  = {TvButtonLeft,  TvButtonChDn},
    [InputKeyRight] = {TvButtonRight, TvButtonChUp},
    [InputKeyOk]    = {TvButtonOk,    TvButtonHome},
};

/* ---- View model ---- */

typedef struct {
    int8_t active_button;  /**< TvButton index being sent (-1 = idle). */
    bool active_is_hold;   /**< True when the hold action is active. */
    bool learned[TV_BUTTON_COUNT]; /**< Snapshot – which buttons have signals. */
    uint8_t pressed_keys;  /**< Bitmask of physically held d-pad/ok/back keys. */
    TvRemoteOrientation orientation; /**< Current screen orientation. */
    bool button_swap; /**< True: short press = vol/ch, hold = directional. */
} TvRemoteRemoteModel;

/* Bitmask bits for pressed_keys */
#define KEY_BIT_UP    (1u << 0)
#define KEY_BIT_DOWN  (1u << 1)
#define KEY_BIT_LEFT  (1u << 2)
#define KEY_BIT_RIGHT (1u << 3)
#define KEY_BIT_OK    (1u << 4)
#define KEY_BIT_BACK  (1u << 5)

static inline uint8_t key_to_bit(InputKey k) {
    switch(k) {
    case InputKeyUp:    return KEY_BIT_UP;
    case InputKeyDown:  return KEY_BIT_DOWN;
    case InputKeyLeft:  return KEY_BIT_LEFT;
    case InputKeyRight: return KEY_BIT_RIGHT;
    case InputKeyOk:    return KEY_BIT_OK;
    case InputKeyBack:  return KEY_BIT_BACK;
    default:            return 0;
    }
}

/* ---- IR worker TX callback ---- */

/*
 * TX callback – runs from the InfraredWorker thread.
 *
 * `app->tx_active` and `app->remote_selected` are written from the UI thread
 * only via tv_remote_tx_start (before the worker starts) and tv_remote_tx_stop
 * (after which the worker receives InfraredStatusDone and halts).  The boolean
 * flag `tx_active` therefore acts as a release/acquire barrier: it is set to
 * true before infrared_worker_tx_start and cleared before
 * infrared_worker_tx_stop, making concurrent access safe on ARM Cortex-M.
 */
static InfraredWorkerGetSignalResponse
    tv_remote_tx_callback(void* context, InfraredWorker* instance) {
    TvRemoteApp* app = context;

    if(!app->tx_active) {
        return InfraredWorkerGetSignalResponseStop;
    }

    uint8_t idx = app->remote_selected;
    if(idx < TV_BUTTON_COUNT && app->buttons[idx].learned) {
        TvRemoteIrSignal* sig = &app->buttons[idx].signal;
        if(sig->is_raw) {
            infrared_worker_set_raw_signal(
                instance, sig->timings, sig->timings_size, sig->frequency, sig->duty_cycle);
        } else {
            infrared_worker_set_decoded_signal(instance, &sig->message);
        }
        return InfraredWorkerGetSignalResponseNew;
    }
    return InfraredWorkerGetSignalResponseStop;
}

/* ---- TX start / stop helpers ---- */

static void tv_remote_tx_start(TvRemoteApp* app, uint8_t button_index) {
    if(app->worker_active) return;
    if(button_index >= TV_BUTTON_COUNT) return;
    if(!app->buttons[button_index].learned) return;

    app->remote_selected = button_index;
    app->tx_active = true;
    app->worker = infrared_worker_alloc();
    infrared_worker_tx_set_get_signal_callback(app->worker, tv_remote_tx_callback, app);
    infrared_worker_tx_start(app->worker);
    app->worker_active = true;

    notification_message(app->notifications, &sequence_blink_green_10);
}

static void tv_remote_tx_stop(TvRemoteApp* app) {
    if(!app->worker_active || !app->tx_active) return;
    infrared_worker_tx_stop(app->worker);
    infrared_worker_free(app->worker);
    app->worker = NULL;
    app->worker_active = false;
    app->tx_active = false;
}

/* ---- Concentric-circle drawing helpers ---- */

/** Quadrant directions for ring fills. */
#define QUAD_TOP    0
#define QUAD_BOTTOM 1
#define QUAD_LEFT   2
#define QUAD_RIGHT  3

/**
 * Fill a wedge-shaped quadrant of a ring (annulus sector).
 * Pixels exactly on the 45° diagonals are skipped, creating natural gap lines.
 */
static void draw_ring_quadrant_filled(
    Canvas* canvas,
    int cx,
    int cy,
    int r_in,
    int r_out,
    uint8_t direction) {
    int ri2 = r_in * r_in;
    int ro2 = r_out * r_out;
    for(int dy = -r_out; dy <= r_out; dy++) {
        for(int dx = -r_out; dx <= r_out; dx++) {
            int d2 = dx * dx + dy * dy;
            if(d2 < ri2 || d2 > ro2) continue;
            bool in_q = false;
            switch(direction) {
            case QUAD_TOP:
                in_q = (dy < 0 && dy * dy > dx * dx);
                break;
            case QUAD_BOTTOM:
                in_q = (dy > 0 && dy * dy > dx * dx);
                break;
            case QUAD_LEFT:
                in_q = (dx < 0 && dx * dx > dy * dy);
                break;
            case QUAD_RIGHT:
                in_q = (dx > 0 && dx * dx > dy * dy);
                break;
            }
            if(in_q) canvas_draw_dot(canvas, cx + dx, cy + dy);
        }
    }
}

/** Fill a full ring (annulus). */
static void draw_ring_filled(Canvas* canvas, int cx, int cy, int r_in, int r_out) {
    int ri2 = r_in * r_in;
    int ro2 = r_out * r_out;
    for(int dy = -r_out; dy <= r_out; dy++) {
        for(int dx = -r_out; dx <= r_out; dx++) {
            int d2 = dx * dx + dy * dy;
            if(d2 >= ri2 && d2 <= ro2) {
                canvas_draw_dot(canvas, cx + dx, cy + dy);
            }
        }
    }
}

/* ---- Horizontal draw (128×64, Flipper held normally) ---- */

#define H_CX     92
#define H_CY     32
#define H_R_HOME  5
#define H_R_OK   10
#define H_R_DPAD 17
#define H_R_HOLD 25
#define H_D_OK    7
#define H_D_HOLD 18

static void tv_remote_remote_draw_horizontal(Canvas* canvas, TvRemoteRemoteModel* model) {
    canvas_clear(canvas);
    int8_t ab   = model->active_button;
    uint8_t pk  = model->pressed_keys;
    bool is_hold = model->active_is_hold;

    bool swap = model->button_swap;
    bool home_active  = (ab == (int8_t)TvButtonHome);
    bool ok_active    = (ab == (int8_t)TvButtonOk)    || ((pk & KEY_BIT_OK)    && !is_hold);
    bool r3_top    = (ab == (int8_t)(swap ? TvButtonVolUp : TvButtonUp))    || ((pk & KEY_BIT_UP)    && !is_hold);
    bool r3_bottom = (ab == (int8_t)(swap ? TvButtonVolDn : TvButtonDown))  || ((pk & KEY_BIT_DOWN)  && !is_hold);
    bool r3_left   = (ab == (int8_t)(swap ? TvButtonChDn  : TvButtonLeft))  || ((pk & KEY_BIT_LEFT)  && !is_hold);
    bool r3_right  = (ab == (int8_t)(swap ? TvButtonChUp  : TvButtonRight)) || ((pk & KEY_BIT_RIGHT) && !is_hold);
    bool r4_top    = (ab == (int8_t)(swap ? TvButtonUp    : TvButtonVolUp));
    bool r4_bottom = (ab == (int8_t)(swap ? TvButtonDown  : TvButtonVolDn));
    bool r4_left   = (ab == (int8_t)(swap ? TvButtonLeft  : TvButtonChDn));
    bool r4_right  = (ab == (int8_t)(swap ? TvButtonRight : TvButtonChUp));

    canvas_set_font(canvas, FontSecondary);
    canvas_set_color(canvas, ColorBlack);

    if(r4_top)    draw_ring_quadrant_filled(canvas, H_CX, H_CY, H_R_DPAD, H_R_HOLD, QUAD_TOP);
    if(r4_bottom) draw_ring_quadrant_filled(canvas, H_CX, H_CY, H_R_DPAD, H_R_HOLD, QUAD_BOTTOM);
    if(r4_left)   draw_ring_quadrant_filled(canvas, H_CX, H_CY, H_R_DPAD, H_R_HOLD, QUAD_LEFT);
    if(r4_right)  draw_ring_quadrant_filled(canvas, H_CX, H_CY, H_R_DPAD, H_R_HOLD, QUAD_RIGHT);
    if(r3_top)    draw_ring_quadrant_filled(canvas, H_CX, H_CY, H_R_OK, H_R_DPAD, QUAD_TOP);
    if(r3_bottom) draw_ring_quadrant_filled(canvas, H_CX, H_CY, H_R_OK, H_R_DPAD, QUAD_BOTTOM);
    if(r3_left)   draw_ring_quadrant_filled(canvas, H_CX, H_CY, H_R_OK, H_R_DPAD, QUAD_LEFT);
    if(r3_right)  draw_ring_quadrant_filled(canvas, H_CX, H_CY, H_R_OK, H_R_DPAD, QUAD_RIGHT);
    if(ok_active)   draw_ring_filled(canvas, H_CX, H_CY, H_R_HOME, H_R_OK);
    if(home_active) canvas_draw_disc(canvas, H_CX, H_CY, H_R_HOME);

    canvas_draw_circle(canvas, H_CX, H_CY, H_R_HOME);
    canvas_draw_circle(canvas, H_CX, H_CY, H_R_OK);
    canvas_draw_circle(canvas, H_CX, H_CY, H_R_DPAD);
    canvas_draw_circle(canvas, H_CX, H_CY, H_R_HOLD);

    canvas_draw_line(canvas, H_CX + H_D_OK, H_CY - H_D_OK, H_CX + H_D_HOLD, H_CY - H_D_HOLD);
    canvas_draw_line(canvas, H_CX - H_D_OK, H_CY - H_D_OK, H_CX - H_D_HOLD, H_CY - H_D_HOLD);
    canvas_draw_line(canvas, H_CX + H_D_OK, H_CY + H_D_OK, H_CX + H_D_HOLD, H_CY + H_D_HOLD);
    canvas_draw_line(canvas, H_CX - H_D_OK, H_CY + H_D_OK, H_CX - H_D_HOLD, H_CY + H_D_HOLD);

    /* Home icon */
    canvas_set_color(canvas, home_active ? ColorWhite : ColorBlack);
    canvas_draw_line(canvas, H_CX,     H_CY - 4, H_CX - 3, H_CY - 1);
    canvas_draw_line(canvas, H_CX,     H_CY - 4, H_CX + 3, H_CY - 1);
    canvas_draw_line(canvas, H_CX - 2, H_CY - 1, H_CX - 2, H_CY + 3);
    canvas_draw_line(canvas, H_CX + 2, H_CY - 1, H_CX + 2, H_CY + 3);
    canvas_draw_line(canvas, H_CX - 2, H_CY + 3, H_CX + 2, H_CY + 3);
    canvas_set_color(canvas, ColorBlack);

    /* Back box (top-left) */
    const int bx = 2, bw = 24, bh = 27;
    const int b_back_y = 3, b_power_y = 34;
    bool back_active = (ab == (int8_t)TvButtonBack);
    if(back_active) {
        canvas_draw_rbox(canvas, bx, b_back_y, bw, bh, 3);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, bx, b_back_y, bw, bh, 3);
    }
    {
        int bx2 = bx + bw / 2, by2 = b_back_y + bh / 2;
        canvas_draw_line(canvas, bx2 - 6, by2 + 1, bx2 + 4, by2 + 1);
        canvas_draw_line(canvas, bx2 + 4, by2 + 1, bx2 + 6, by2 - 1);
        canvas_draw_line(canvas, bx2 + 6, by2 - 1, bx2 + 6, by2 - 3);
        canvas_draw_line(canvas, bx2 + 6, by2 - 3, bx2 + 4, by2 - 5);
        canvas_draw_line(canvas, bx2 + 4, by2 - 5, bx2 + 1, by2 - 5);
        canvas_draw_line(canvas, bx2 - 6, by2 + 1, bx2 - 3, by2 - 2);
        canvas_draw_line(canvas, bx2 - 6, by2 + 1, bx2 - 3, by2 + 4);
    }
    if(back_active) canvas_set_color(canvas, ColorBlack);

    /* Power box (bottom-left) */
    bool power_active = (ab == (int8_t)TvButtonPower);
    if(power_active) {
        canvas_draw_rbox(canvas, bx, b_power_y, bw, bh, 3);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, bx, b_power_y, bw, bh, 3);
    }
    canvas_draw_str_aligned(canvas, bx + bw / 2, b_power_y + 5, AlignCenter, AlignTop, "x2");
    canvas_draw_str_aligned(
        canvas, bx + bw / 2, b_power_y + bh - 4, AlignCenter, AlignBottom, "Pwr");
    if(power_active) canvas_set_color(canvas, ColorBlack);
}

/* ---- Draw callback ---- */

static void tv_remote_remote_draw_callback(Canvas* canvas, void* model_void) {
    TvRemoteRemoteModel* model = model_void;
    if(model->orientation == TvRemoteOrientationHorizontal) {
        tv_remote_remote_draw_horizontal(canvas, model);
        return;
    }
    canvas_clear(canvas);

    const int cx = DISP_W / 2;      /* 32 */
    const int cy = 45;              /* midpoint between top and Back/Power boxes */

    int8_t ab = model->active_button;
    uint8_t pk = model->pressed_keys;
    bool is_hold = model->active_is_hold;

    /* ── Determine which sections are active ── */

    /* Center: Home (hold-Ok) */
    bool swap = model->button_swap;
    bool home_active = (ab == (int8_t)TvButtonHome);
    /* Ring 2: Ok (press-Ok, but not when hold is active) */
    bool ok_active = (ab == (int8_t)TvButtonOk) ||
                     ((pk & KEY_BIT_OK) && !is_hold);

    /* Ring 3 quadrants: short-press actions */
    bool r3_top    = (ab == (int8_t)(swap ? TvButtonVolUp : TvButtonUp))    || ((pk & KEY_BIT_UP)    && !is_hold);
    bool r3_bottom = (ab == (int8_t)(swap ? TvButtonVolDn : TvButtonDown))  || ((pk & KEY_BIT_DOWN)  && !is_hold);
    bool r3_left   = (ab == (int8_t)(swap ? TvButtonChDn  : TvButtonLeft))  || ((pk & KEY_BIT_LEFT)  && !is_hold);
    bool r3_right  = (ab == (int8_t)(swap ? TvButtonChUp  : TvButtonRight)) || ((pk & KEY_BIT_RIGHT) && !is_hold);

    /* Ring 4 quadrants: hold actions */
    bool r4_top    = (ab == (int8_t)(swap ? TvButtonUp    : TvButtonVolUp));
    bool r4_bottom = (ab == (int8_t)(swap ? TvButtonDown  : TvButtonVolDn));
    bool r4_left   = (ab == (int8_t)(swap ? TvButtonLeft  : TvButtonChDn));
    bool r4_right  = (ab == (int8_t)(swap ? TvButtonRight : TvButtonChUp));

    canvas_set_font(canvas, FontSecondary);
    canvas_set_color(canvas, ColorBlack);

    /* ── 1. Fill active sections ── */

    /* Ring 4 (outermost – hold actions) */
    if(r4_top)    draw_ring_quadrant_filled(canvas, cx, cy, R_DPAD, R_HOLD, QUAD_TOP);
    if(r4_bottom) draw_ring_quadrant_filled(canvas, cx, cy, R_DPAD, R_HOLD, QUAD_BOTTOM);
    if(r4_left)   draw_ring_quadrant_filled(canvas, cx, cy, R_DPAD, R_HOLD, QUAD_LEFT);
    if(r4_right)  draw_ring_quadrant_filled(canvas, cx, cy, R_DPAD, R_HOLD, QUAD_RIGHT);

    /* Ring 3 (d-pad press actions) */
    if(r3_top)    draw_ring_quadrant_filled(canvas, cx, cy, R_OK, R_DPAD, QUAD_TOP);
    if(r3_bottom) draw_ring_quadrant_filled(canvas, cx, cy, R_OK, R_DPAD, QUAD_BOTTOM);
    if(r3_left)   draw_ring_quadrant_filled(canvas, cx, cy, R_OK, R_DPAD, QUAD_LEFT);
    if(r3_right)  draw_ring_quadrant_filled(canvas, cx, cy, R_OK, R_DPAD, QUAD_RIGHT);

    /* Ring 2: Ok */
    if(ok_active) draw_ring_filled(canvas, cx, cy, R_HOME, R_OK);

    /* Ring 1: Home */
    if(home_active) canvas_draw_disc(canvas, cx, cy, R_HOME);

    /* ── 2. Circle outlines ── */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_circle(canvas, cx, cy, R_HOME);
    canvas_draw_circle(canvas, cx, cy, R_OK);
    canvas_draw_circle(canvas, cx, cy, R_DPAD);
    canvas_draw_circle(canvas, cx, cy, R_HOLD);

    /* ── 3. Diagonal dividers (R_OK → R_HOLD at 45°) ── */
    canvas_draw_line(canvas, cx + D_OK, cy - D_OK, cx + D_HOLD, cy - D_HOLD); /* NE */
    canvas_draw_line(canvas, cx - D_OK, cy - D_OK, cx - D_HOLD, cy - D_HOLD); /* NW */
    canvas_draw_line(canvas, cx + D_OK, cy + D_OK, cx + D_HOLD, cy + D_HOLD); /* SE */
    canvas_draw_line(canvas, cx - D_OK, cy + D_OK, cx - D_HOLD, cy + D_HOLD); /* SW */

    /* ── 4. Home icon (house) inside inner circle ── */
    canvas_set_color(canvas, home_active ? ColorWhite : ColorBlack);
    /* Roof: inverted-V, apex at top, base at mid */
    canvas_draw_line(canvas, cx,     cy - 5, cx - 4, cy - 1); /* left slope  */
    canvas_draw_line(canvas, cx,     cy - 5, cx + 4, cy - 1); /* right slope */
    /* Body: walls + floor */
    canvas_draw_line(canvas, cx - 3, cy - 1, cx - 3, cy + 4); /* left wall   */
    canvas_draw_line(canvas, cx + 3, cy - 1, cx + 3, cy + 4); /* right wall  */
    canvas_draw_line(canvas, cx - 3, cy + 4, cx + 3, cy + 4); /* floor       */
    canvas_set_color(canvas, ColorBlack);

    /* ── Bottom bar ── */
    const int box_y = 90;
    const int box_h = 24;
    const int box_w = 28;
    const int box_gap = 2;

    /* Back box */
    bool back_active = (ab == (int8_t)TvButtonBack);
    if(back_active) {
        canvas_draw_rbox(canvas, box_gap, box_y, box_w, box_h, 3);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, box_gap, box_y, box_w, box_h, 3);
    }
    /* Curved return arrow (resembles Flipper Zero back button icon) */
    {
        const int bx = box_gap + box_w / 2; /* centre x of box */
        const int by = box_y + box_h / 2;   /* centre y of box */
        /* Horizontal shaft pointing left */
        canvas_draw_line(canvas, bx - 7, by + 2, bx + 5, by + 2);
        /* Curve up-right: short arc from shaft-right going up */
        canvas_draw_line(canvas, bx + 5, by + 2, bx + 7, by);
        canvas_draw_line(canvas, bx + 7, by, bx + 7, by - 4);
        canvas_draw_line(canvas, bx + 7, by - 4, bx + 5, by - 6);
        canvas_draw_line(canvas, bx + 5, by - 6, bx + 1, by - 6);
        /* Arrowhead at left end of shaft */
        canvas_draw_line(canvas, bx - 7, by + 2, bx - 4, by - 1);
        canvas_draw_line(canvas, bx - 7, by + 2, bx - 4, by + 5);
    }
    if(back_active) canvas_set_color(canvas, ColorBlack);

    /* Power box */
    bool power_active = (ab == (int8_t)TvButtonPower);
    const int power_box_x = DISP_W - box_w - box_gap;
    if(power_active) {
        canvas_draw_rbox(canvas, power_box_x, box_y, box_w, box_h, 3);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, power_box_x, box_y, box_w, box_h, 3);
    }
    canvas_draw_str_aligned(
        canvas, power_box_x + box_w / 2, box_y + 5, AlignCenter, AlignTop, "x2");
    canvas_draw_str_aligned(
        canvas, power_box_x + box_w / 2, box_y + box_h - 4, AlignCenter, AlignBottom, "Pwr");
    if(power_active) canvas_set_color(canvas, ColorBlack);

    /* Hint */
    canvas_draw_str_aligned(
        canvas, DISP_W / 2, 118, AlignCenter, AlignTop, "Hold for alt");
}

/* ---- Helper: update model from app state ---- */

static void tv_remote_remote_update_model(
    TvRemoteApp* app, int8_t active, bool is_hold, uint8_t pressed_keys) {
    with_view_model(
        app->remote_view,
        TvRemoteRemoteModel * model,
        {
            model->active_button = active;
            model->active_is_hold = is_hold;
            model->pressed_keys = pressed_keys;
            model->orientation = app->orientation;
            model->button_swap = app->button_swap;
            for(size_t i = 0; i < TV_BUTTON_COUNT; i++) {
                model->learned[i] = app->buttons[i].learned;
            }
        },
        true);
}

/* ---- Brief IR burst (for Back / Power single-shot) ---- */

static void tv_remote_ir_burst(TvRemoteApp* app, uint8_t button_index) {
    tv_remote_tx_start(app, button_index);
    if(app->tx_active) {
        furi_delay_ms(IR_BURST_MS);
        tv_remote_tx_stop(app);
    }
}

/* ---- Custom event callback (handles deferred back-tap timer) ---- */

static bool tv_remote_remote_custom_event_callback(void* context, uint32_t event) {
    TvRemoteApp* app = context;
    if(event == TvRemoteCustomEventBackTimeout) {
        if(app->back_pending) {
            app->back_pending = false;
            /* Timer expired – single tap → Back IR */
            tv_remote_remote_update_model(
                app, (int8_t)TvButtonBack, false, app->remote_pressed_keys);
            tv_remote_ir_burst(app, TvButtonBack);
            tv_remote_remote_update_model(app, -1, false, app->remote_pressed_keys);
        }
        return true;
    }
    return false;
}

/* ---- Input callback ---- */

static bool tv_remote_remote_input_callback(InputEvent* event, void* context) {
    TvRemoteApp* app = context;

    /* ── Back button: hold to exit, short = Back IR, double-tap = Power ── */
    if(event->key == InputKeyBack) {
        if(event->type == InputTypePress) {
            /* Consume silently – no visual until action is decided */
            return true;
        }
        if(event->type == InputTypeLong) {
            /* Hold back = exit: cancel any pending tap, let ViewDispatcher navigate */
            if(app->back_pending) {
                app->back_pending = false;
                furi_timer_stop(app->back_timer);
            }
            tv_remote_tx_stop(app);
            tv_remote_remote_update_model(app, -1, false, app->remote_pressed_keys);
            return false; /* ViewDispatcher handles exit */
        }
        if(event->type == InputTypeShort) {
            if(app->back_pending) {
                /* Double-tap within window → Power */
                app->back_pending = false;
                furi_timer_stop(app->back_timer);
                tv_remote_remote_update_model(
                    app, (int8_t)TvButtonPower, false, app->remote_pressed_keys);
                tv_remote_ir_burst(app, TvButtonPower);
                tv_remote_remote_update_model(app, -1, false, app->remote_pressed_keys);
            } else {
                /* First tap – wait for possible double-tap */
                app->back_pending = true;
                furi_timer_start(app->back_timer, furi_ms_to_ticks(DOUBLE_TAP_MS));
            }
            return true;
        }
        if(event->type == InputTypeRelease) {
            return true; /* consume release */
        }
        return false;
    }

    /* ── D-pad & Ok: press = primary, long = hold action ── */
    if(event->key > InputKeyMAX) return false;
    if(event->key == InputKeyBack) return false; /* handled above */

    /* Only handle keys with mappings */
    if(event->key != InputKeyUp && event->key != InputKeyDown &&
       event->key != InputKeyLeft && event->key != InputKeyRight &&
       event->key != InputKeyOk) {
        return false;
    }

    const ButtonMapping* map = &key_map[event->key];
    bool do_swap = app->button_swap && (event->key != InputKeyOk);
    uint8_t eff_press = do_swap ? map->hold_btn : map->press_btn;
    uint8_t eff_hold  = do_swap ? map->press_btn : map->hold_btn;

    if(event->type == InputTypePress) {
        /* Show ring section immediately on press */
        app->remote_pressed_keys |= key_to_bit(event->key);
        app->remote_held_long = false;
        tv_remote_remote_update_model(app, -1, false, app->remote_pressed_keys);
        return true;
    }

    if(event->type == InputTypeLong) {
        /* Start sending the hold/alt action */
        app->remote_held_long = true;
        tv_remote_tx_start(app, eff_hold);
        tv_remote_remote_update_model(app, (int8_t)eff_hold, true, app->remote_pressed_keys);
        return true;
    }

    if(event->type == InputTypeRelease) {
        app->remote_pressed_keys &= ~key_to_bit(event->key);
        if(app->remote_held_long) {
            /* Was holding alt action – just stop it */
            tv_remote_tx_stop(app);
        } else {
            /* Quick tap – show active ring section during burst */
            tv_remote_remote_update_model(
                app, (int8_t)eff_press, false, app->remote_pressed_keys);
            tv_remote_ir_burst(app, eff_press);
        }
        app->remote_held_long = false;
        tv_remote_remote_update_model(app, -1, false, app->remote_pressed_keys);
        return true;
    }

    /* InputTypeRepeat – continue sending (no change) */
    if(event->type == InputTypeRepeat) {
        return true;
    }

    return false;
}

/* ---- Enter / exit callbacks ---- */

static void tv_remote_remote_enter_callback(void* context) {
    TvRemoteApp* app = context;
    view_set_orientation(
        app->remote_view,
        app->orientation == TvRemoteOrientationHorizontal ?
            ViewOrientationHorizontal : ViewOrientationVertical);
    app->tx_active = false;
    app->remote_pressed_keys = 0;
    app->remote_held_long = false;
    app->back_pending = false;
    furi_timer_stop(app->back_timer);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, tv_remote_remote_custom_event_callback);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    tv_remote_remote_update_model(app, -1, false, 0);
}

static void tv_remote_remote_exit_callback(void* context) {
    TvRemoteApp* app = context;
    furi_timer_stop(app->back_timer);
    app->back_pending = false;
    tv_remote_tx_stop(app);
}

/* ---- Public API ---- */

View* tv_remote_remote_view_alloc(TvRemoteApp* app) {
    View* view = view_alloc();
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(TvRemoteRemoteModel));
    view_set_draw_callback(view, tv_remote_remote_draw_callback);
    view_set_input_callback(view, tv_remote_remote_input_callback);
    view_set_enter_callback(view, tv_remote_remote_enter_callback);
    view_set_exit_callback(view, tv_remote_remote_exit_callback);
    view_set_orientation(view, ViewOrientationVertical);

    with_view_model(
        view,
        TvRemoteRemoteModel * model,
        {
            model->active_button = -1;
            model->active_is_hold = false;
            model->button_swap = false;
            for(size_t i = 0; i < TV_BUTTON_COUNT; i++) {
                model->learned[i] = false;
            }
        },
        true);

    return view;
}

void tv_remote_remote_view_free(View* view) {
    view_free(view);
}
