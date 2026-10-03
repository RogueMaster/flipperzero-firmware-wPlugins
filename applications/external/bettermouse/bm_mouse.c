#include "bm_mouse.h"

#include <furi_hal_usb_hid.h>
#include <gui/elements.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <math.h>

#include "bettermouse_icons.h"

/* Movement is driven by a fixed-rate motion loop, not by the GUI input service
   150 ms repeat events.

   Key state comes straight off the input service pubsub rather than the view
   input callback. The GUI service is a single thread that dispatches input AND
   redraws, so a key release queued behind a redraw arrives late -- which showed
   up as the pointer overshooting after you let go. Subscribing directly means
   release is seen the moment it happens, whatever the screen is doing. */

#define BM_DIAG       0.70710678f /* keep diagonals the same speed as axes */
#define BM_EXIT_HOLD  800u /* ms holding Back alone before we drop to the menu */
#define BM_REPORT_MAX 127 /* HID reports are int8 */
#define BM_ICE_STOP   8.0f /* px/s below which a glide is over */
#define BM_BAR_MIN_MS 250u /* min gap between speed bar repaints */

#define BM_FLAG_STOP (1u << 0)

/* Only what the screen shows. Written by the motion thread, read by the draw
   callback, and deliberately updated as rarely as possible. */
typedef struct {
    bool up;
    bool down;
    bool left;
    bool right;
    bool back_down;
    bool scroll_mode;
    bool drag_lock;
    bool btn_left;
    bool btn_right;
    bool connected;
    bool ice;
    bool gliding;
    bool exit_warn; /* Back held alone, quit is coming */
    uint8_t speed_pct;
} BmMouseModel;

/* Live key state. Plain bytes and counters, written on the input service thread
   and read by the motion thread; no lock, so neither can ever stall the other.
   Byte-sized loads and stores are atomic on Cortex-M4. */
typedef struct {
    volatile bool up;
    volatile bool down;
    volatile bool left;
    volatile bool right;

    volatile bool back_held;
    volatile bool back_consumed; /* Back already did something, so no right click */
    volatile bool ok_held;
    volatile bool ok_swallowed; /* this OK press already did something else */
    volatile bool drag_lock; /* owned by the motion thread, read here */

    /* Requests handed to the motion thread, which owns all USB traffic. The
       input service thread must never block on a HID report. */
    volatile uint8_t click_left;
    volatile uint8_t click_right;
    volatile uint8_t toggle_scroll;
    volatile uint8_t drag_press;
    volatile uint8_t drag_release;
} BmInput;

struct BmMouse {
    View* view;
    const BmSettings* settings;
    NotificationApp* notifications;

    FuriThread* thread;
    FuriPubSub* input_events;
    FuriPubSubSubscription* input_subscription;

    BmMouseExitCallback exit_callback;
    void* exit_context;

    BmInput input;

    /* Owned by the motion thread. */
    float vx; /* px/s, only meaningful in ice mode */
    float vy;
    float acc_x; /* sub-pixel remainders */
    float acc_y;
    float acc_scroll;
    uint32_t hold_ms;
    uint32_t back_hold_ms;
    bool scroll_mode;
    bool exit_requested;
};

/* ------------------------------------------------------------------ drawing */

static void bm_mouse_draw_callback(Canvas* canvas, void* context) {
    furi_assert(context);
    BmMouseModel* model = context;

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, "BMouse");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 22, model->connected ? "USB ready" : "No USB host");

    const char* mode = "Pointer";
    if(model->scroll_mode) {
        mode = "Scroll";
    } else if(model->ice) {
        mode = model->gliding ? "Ice: glide" : "Ice";
    }
    canvas_draw_str(canvas, 0, 33, mode);

    if(model->drag_lock) canvas_draw_str(canvas, 0, 44, "DRAG");

    /* Speed bar: where you are on the ramp. */
    canvas_draw_frame(canvas, 0, 47, 52, 5);
    if(model->speed_pct > 0) {
        uint8_t w = (uint8_t)((50u * model->speed_pct) / 100u);
        if(w > 0) canvas_draw_box(canvas, 1, 48, w, 3);
    }

    canvas_draw_icon(canvas, 0, 55, &I_Pin_back_arrow_10x8);
    canvas_draw_str(canvas, 13, 63, model->exit_warn ? "MENU..." : "Hold = menu");

    canvas_draw_icon(canvas, 58, 3, &I_OutCircles_70x51);

    if(model->up) {
        canvas_set_bitmap_mode(canvas, true);
        canvas_draw_icon(canvas, 68, 6, &I_S_UP_31x15);
        canvas_set_bitmap_mode(canvas, false);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_icon(canvas, 80, 8, &I_Pin_arrow_up_7x9);
    canvas_set_color(canvas, ColorBlack);

    if(model->down) {
        canvas_set_bitmap_mode(canvas, true);
        canvas_draw_icon(canvas, 68, 36, &I_S_DOWN_31x15);
        canvas_set_bitmap_mode(canvas, false);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_icon(canvas, 80, 40, &I_Pin_arrow_down_7x9);
    canvas_set_color(canvas, ColorBlack);

    if(model->left) {
        canvas_set_bitmap_mode(canvas, true);
        canvas_draw_icon(canvas, 61, 13, &I_S_LEFT_15x31);
        canvas_set_bitmap_mode(canvas, false);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_icon(canvas, 63, 25, &I_Pin_arrow_left_9x7);
    canvas_set_color(canvas, ColorBlack);

    if(model->right) {
        canvas_set_bitmap_mode(canvas, true);
        canvas_draw_icon(canvas, 91, 13, &I_S_RIGHT_15x31);
        canvas_set_bitmap_mode(canvas, false);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_icon(canvas, 95, 25, &I_Pin_arrow_right_9x7);
    canvas_set_color(canvas, ColorBlack);

    if(model->btn_left) {
        canvas_set_bitmap_mode(canvas, true);
        canvas_draw_icon(canvas, 74, 19, &I_Pressed_Button_19x19);
        canvas_set_bitmap_mode(canvas, false);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_icon(canvas, 79, 24, &I_Left_mouse_icon_9x9);
    canvas_set_color(canvas, ColorBlack);

    if(model->btn_right) {
        canvas_set_bitmap_mode(canvas, true);
        canvas_draw_icon(canvas, 107, 33, &I_Pressed_Button_19x19);
        canvas_set_bitmap_mode(canvas, false);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_icon(canvas, 112, 38, &I_Right_mouse_icon_9x9);
    canvas_set_color(canvas, ColorBlack);
}

/* ---------------------------------------------------------------- input tap */

static void bm_mouse_input_event(const void* value, void* context) {
    const InputEvent* event = value;
    BmMouse* bm_mouse = context;
    BmInput* in = &bm_mouse->input;

    switch(event->key) {
    case InputKeyUp:
    case InputKeyDown:
    case InputKeyLeft:
    case InputKeyRight: {
        if(event->type != InputTypePress && event->type != InputTypeRelease) break;
        bool pressed = (event->type == InputTypePress);

        if(event->key == InputKeyUp) in->up = pressed;
        if(event->key == InputKeyDown) in->down = pressed;
        if(event->key == InputKeyLeft) in->left = pressed;
        if(event->key == InputKeyRight) in->right = pressed;

        break;
    }

    case InputKeyOk:
        if(event->type == InputTypePress) {
            if(in->back_held) {
                in->back_consumed = true;
                in->ok_swallowed = true;
                in->toggle_scroll++;
            } else {
                in->ok_held = true;
                in->ok_swallowed = false;
            }
        } else if(event->type == InputTypeLong) {
            if(!in->back_held && !in->drag_lock && !in->ok_swallowed) in->drag_press++;
        } else if(event->type == InputTypeShort) {
            /* Short arrives before Release, so dropping the latch here means
               the Release below correctly does not also fire a click. */
            if(in->drag_lock) {
                in->drag_release++;
                in->ok_swallowed = true;
            }
        } else if(event->type == InputTypeRelease) {
            if(in->ok_held && !in->ok_swallowed && !in->drag_lock) in->click_left++;
            in->ok_held = false;
        }
        break;

    case InputKeyBack:
        if(event->type == InputTypePress) {
            in->back_held = true;
            in->back_consumed = false;
        } else if(event->type == InputTypeRelease) {
            bool consumed = in->back_consumed;
            in->back_held = false;
            if(!consumed) in->click_right++;
        }
        break;

    default:
        break;
    }
}

/* ------------------------------------------------------------ motion engine */

static int8_t bm_take_whole(float* accumulator) {
    /* Truncate toward zero and keep the remainder, so speeds below one pixel
       per tick still move the pointer instead of rounding away to nothing. */
    float whole = (*accumulator < 0.0f) ? ceilf(*accumulator) : floorf(*accumulator);

    /* Clamp to the int8 report range, leaving anything over in the accumulator
       so a delayed tick spreads over the next few instead of being dropped. */
    if(whole > (float)BM_REPORT_MAX) whole = (float)BM_REPORT_MAX;
    if(whole < (float)-BM_REPORT_MAX) whole = (float)-BM_REPORT_MAX;

    *accumulator -= whole;
    return (int8_t)whole;
}

static void bm_mouse_vibro(BmMouse* bm_mouse) {
    if(bm_mouse->settings->haptic) {
        notification_message(bm_mouse->notifications, &sequence_single_vibro);
    }
}

/* Act on everything the input thread asked for. All USB traffic happens here. */
static void bm_mouse_service_requests(BmMouse* bm_mouse, bool connected) {
    BmInput* in = &bm_mouse->input;

    while(in->click_left > 0) {
        in->click_left--;
        if(connected) {
            furi_hal_hid_mouse_press(HID_MOUSE_BTN_LEFT);
            furi_hal_hid_mouse_release(HID_MOUSE_BTN_LEFT);
        }
        bm_mouse_vibro(bm_mouse);
    }

    while(in->click_right > 0) {
        in->click_right--;
        if(connected) {
            furi_hal_hid_mouse_press(HID_MOUSE_BTN_RIGHT);
            furi_hal_hid_mouse_release(HID_MOUSE_BTN_RIGHT);
        }
        bm_mouse_vibro(bm_mouse);
    }

    while(in->drag_press > 0) {
        in->drag_press--;
        if(connected) furi_hal_hid_mouse_press(HID_MOUSE_BTN_LEFT);
        in->drag_lock = true;
        bm_mouse_vibro(bm_mouse);
    }

    while(in->drag_release > 0) {
        in->drag_release--;
        if(connected) furi_hal_hid_mouse_release(HID_MOUSE_BTN_LEFT);
        in->drag_lock = false;
    }

    while(in->toggle_scroll > 0) {
        in->toggle_scroll--;
        bm_mouse->scroll_mode = !bm_mouse->scroll_mode;
    }
}

static int32_t bm_mouse_thread(void* context) {
    BmMouse* bm_mouse = context;
    BmInput* in = &bm_mouse->input;

    /* Redraws are throttled: the GUI service redraws on the same thread that
       dispatches input, so repainting every tick is what made releases land
       late in the first place. */
    uint8_t last_drawn_pct = 255;
    uint32_t last_bar_tick = 0;

    /* Scheduling deadline and the timestamp we measure dt against. Waiting a
       relative tick_ms each pass would let the loop free-run: the period would
       be tick_ms plus however long the work took, and FreeRTOS relative delays
       quantise to +/-1 tick depending on where in the 1 ms tick we started. The
       phase against the system tick then drifts slowly, so the real period
       wanders between about 20 and 22 ms. Pair that with integrating a fixed
       nominal dt and the delivered speed is const_px / varying_seconds, which
       reads as a slow sine on top of a steady glide. Absolute deadlines plus a
       measured dt kill both halves of that. */
    uint32_t next_deadline = furi_get_tick();
    uint32_t last_tick = next_deadline;

    while(true) {
        const BmSettings* settings = bm_mouse->settings;

        uint16_t tick_hz = settings->tick_hz;
        if(tick_hz < 10) tick_hz = 10;
        uint32_t tick_ms = 1000u / tick_hz;
        if(tick_ms < 1) tick_ms = 1;

        /* Absolute cadence: the next wake is a fixed step from the last target,
           not from now, so work time does not push the period out. */
        next_deadline += tick_ms;
        uint32_t now = furi_get_tick();
        int32_t remaining = (int32_t)(next_deadline - now);

        /* If we fell far behind, or the rate setting just changed, resync
           rather than trying to catch up with a burst of zero-length waits. */
        if(remaining < 0 || remaining > (int32_t)(tick_ms * 4)) {
            next_deadline = now + tick_ms;
            remaining = (int32_t)tick_ms;
        }

        uint32_t flags =
            furi_thread_flags_wait(BM_FLAG_STOP, FuriFlagWaitAny, (uint32_t)remaining);
        if((flags & BM_FLAG_STOP) != 0) break;

        /* Integrate against the time that actually passed. Whole milliseconds
           quantise each step, but the remainder is never discarded, so distance
           over any window stays exact and the modulation has nowhere to live. */
        now = furi_get_tick();
        uint32_t elapsed_ms = now - last_tick;
        last_tick = now;
        if(elapsed_ms == 0) elapsed_ms = 1;
        if(elapsed_ms > 200) elapsed_ms = 200; /* do not teleport after a stall */
        float dt = (float)elapsed_ms / 1000.0f;

        bool connected = furi_hal_hid_is_connected();
        bm_mouse_service_requests(bm_mouse, connected);

        /* Sample the keys once per tick. */
        int8_t dir_x = (int8_t)((in->right ? 1 : 0) - (in->left ? 1 : 0));
        int8_t dir_y = 0;
        int8_t scroll_dir = 0;

        if(bm_mouse->scroll_mode) {
            scroll_dir = (int8_t)((in->up ? 1 : 0) - (in->down ? 1 : 0));
        } else {
            dir_y = (int8_t)((in->down ? 1 : 0) - (in->up ? 1 : 0));
        }

        if(settings->invert_y) dir_y = (int8_t)-dir_y;
        if(settings->invert_scroll) scroll_dir = (int8_t)-scroll_dir;

        bool moving = (dir_x != 0 || dir_y != 0);

        /* Back held alone for long enough leaves the app. Timed here rather
           than off InputTypeLong, so we can warn before it fires. */
        bool quit = false;
        if(in->back_held && !in->back_consumed) {
            bm_mouse->back_hold_ms += elapsed_ms;
            if(bm_mouse->back_hold_ms >= BM_EXIT_HOLD && !bm_mouse->exit_requested) {
                bm_mouse->exit_requested = true;
                in->back_consumed = true; /* so releasing Back is not a click */
                quit = true;
            }
        } else if(!in->back_held) {
            bm_mouse->back_hold_ms = 0;
        }

        /* Warn once we are a quarter of the way to quitting, so a hold that is
           about to exit is never a surprise. */
        bool exit_warn = in->back_held && !in->back_consumed &&
                         (bm_mouse->back_hold_ms >= BM_EXIT_HOLD / 4);

        float max_speed = (float)settings->max_speed;
        float min_speed = (float)settings->min_speed;
        /* Ramp: eases in from min_speed to max_speed over accel_ms. */
        if(moving) {
            bm_mouse->hold_ms += elapsed_ms;
        } else {
            bm_mouse->hold_ms = 0;
        }

        float ramp = 1.0f;
        if(settings->accel_ms > 0) {
            ramp = (float)bm_mouse->hold_ms / (float)settings->accel_ms;
            if(ramp > 1.0f) ramp = 1.0f;
            ramp = ramp * ramp; /* gentle at first, quick once moving */
        }

        float speed = min_speed + (max_speed - min_speed) * ramp;
        if(speed < 0.0f) speed = 0.0f;

        float target_vx = (float)dir_x * speed;
        float target_vy = (float)dir_y * speed;

        /* Without this, diagonals are 1.41x faster than the axes. */
        if(dir_x != 0 && dir_y != 0) {
            target_vx *= BM_DIAG;
            target_vy *= BM_DIAG;
        }

        bool gliding = false;

        if(settings->ice) {
            /* Ice mode: velocity has momentum. Keys push it toward the target,
               and letting go coasts to a stop instead of cutting out. */
            float accel_ms = (float)settings->accel_ms;
            if(accel_ms < 1.0f) accel_ms = 1.0f;
            float max_delta = max_speed * (dt * 1000.0f / accel_ms);

            float ddx = target_vx - bm_mouse->vx;
            float ddy = target_vy - bm_mouse->vy;

            if(moving) {
                if(ddx > max_delta) ddx = max_delta;
                if(ddx < -max_delta) ddx = -max_delta;
                if(ddy > max_delta) ddy = max_delta;
                if(ddy < -max_delta) ddy = -max_delta;
                bm_mouse->vx += ddx;
                bm_mouse->vy += ddy;
            } else {
                /* Exponential coast: halves every ice_glide_ms. */
                float glide_ms = (float)settings->ice_glide_ms;
                if(glide_ms < 1.0f) glide_ms = 1.0f;
                float decay = powf(0.5f, (dt * 1000.0f) / glide_ms);
                bm_mouse->vx *= decay;
                bm_mouse->vy *= decay;

                if(fabsf(bm_mouse->vx) < BM_ICE_STOP) bm_mouse->vx = 0.0f;
                if(fabsf(bm_mouse->vy) < BM_ICE_STOP) bm_mouse->vy = 0.0f;
                gliding = (bm_mouse->vx != 0.0f || bm_mouse->vy != 0.0f);
            }
        } else {
            /* Default: velocity follows the keys exactly, so releasing stops
               dead on the very next tick. No coasting, no overshoot. */
            bm_mouse->vx = target_vx;
            bm_mouse->vy = target_vy;

            if(!moving) {
                bm_mouse->acc_x = 0.0f;
                bm_mouse->acc_y = 0.0f;
            }
        }

        bm_mouse->acc_x += bm_mouse->vx * dt;
        bm_mouse->acc_y += bm_mouse->vy * dt;

        int8_t dx = bm_take_whole(&bm_mouse->acc_x);
        int8_t dy = bm_take_whole(&bm_mouse->acc_y);

        /* One combined report per tick, so diagonals are real diagonals. */
        if(connected && (dx != 0 || dy != 0)) furi_hal_hid_mouse_move(dx, dy);

        if(scroll_dir != 0) {
            float scroll_speed = (float)settings->scroll_speed;
            bm_mouse->acc_scroll += (float)scroll_dir * scroll_speed * dt;
            int8_t notches = bm_take_whole(&bm_mouse->acc_scroll);
            if(connected && notches != 0) furi_hal_hid_mouse_scroll(notches);
        } else {
            bm_mouse->acc_scroll = 0.0f;
        }

        /* Mirror state to the screen, quantised to 20%. The bar is the only
           thing that changes while you hold a direction, and a repaint of this
           screen is not cheap, so it also gets a minimum interval. Button and
           direction changes still repaint immediately: those are things you
           just did, and they need to feel instant. */
        float shown = sqrtf(bm_mouse->vx * bm_mouse->vx + bm_mouse->vy * bm_mouse->vy);
        uint8_t speed_pct = 0;
        if(max_speed > 0.0f) {
            float pct = (shown * 100.0f) / max_speed;
            if(pct > 100.0f) pct = 100.0f;
            speed_pct = (uint8_t)(((uint8_t)pct / 20u) * 20u);
        }

        bool bar_due = (speed_pct != last_drawn_pct) &&
                       ((uint32_t)(now - last_bar_tick) >= BM_BAR_MIN_MS || speed_pct == 0 ||
                        speed_pct == 100);
        if(bar_due) last_bar_tick = now;

        bool redraw = false;
        with_view_model(
            bm_mouse->view,
            BmMouseModel * model,
            {
                /* One declarator per line: the preprocessor does not treat
                   braces as protecting commas, only parentheses, so a comma
                   here would split the macro argument. */
                bool up = in->up;
                bool down = in->down;
                bool left = in->left;
                bool right = in->right;
                bool back_down = in->back_held;
                bool drag = in->drag_lock;

                if(model->up != up || model->down != down || model->left != left ||
                   model->right != right || model->back_down != back_down ||
                   model->drag_lock != drag || model->btn_left != (in->ok_held || drag) ||
                   model->btn_right != back_down || model->scroll_mode != bm_mouse->scroll_mode ||
                   model->connected != connected || model->gliding != gliding ||
                   model->exit_warn != exit_warn || model->ice != (bool)settings->ice || bar_due) {
                    model->up = up;
                    model->down = down;
                    model->left = left;
                    model->right = right;
                    model->back_down = back_down;
                    model->drag_lock = drag;
                    model->btn_left = (in->ok_held || drag);
                    model->btn_right = back_down;
                    model->scroll_mode = bm_mouse->scroll_mode;
                    model->connected = connected;
                    model->gliding = gliding;
                    model->exit_warn = exit_warn;
                    model->ice = settings->ice;
                    model->speed_pct = speed_pct;
                    last_drawn_pct = speed_pct;
                    redraw = true;
                }
            },
            redraw);

        if(quit && bm_mouse->exit_callback) bm_mouse->exit_callback(bm_mouse->exit_context);
    }

    return 0;
}

/* --------------------------------------------------------------------- view */

static bool bm_mouse_input_callback(InputEvent* event, void* context) {
    UNUSED(event);
    UNUSED(context);
    /* State is taken straight from the input service; this only stops the view
       dispatcher from treating Back as "go back". */
    return true;
}

static void bm_mouse_enter_callback(void* context) {
    BmMouse* bm_mouse = context;

    memset(&bm_mouse->input, 0, sizeof(BmInput));
    bm_mouse->vx = 0.0f;
    bm_mouse->vy = 0.0f;
    bm_mouse->acc_x = 0.0f;
    bm_mouse->acc_y = 0.0f;
    bm_mouse->acc_scroll = 0.0f;
    bm_mouse->hold_ms = 0;
    bm_mouse->back_hold_ms = 0;
    bm_mouse->scroll_mode = false;
    bm_mouse->exit_requested = false;

    with_view_model(
        bm_mouse->view,
        BmMouseModel * model,
        {
            memset(model, 0, sizeof(BmMouseModel));
            model->connected = furi_hal_hid_is_connected();
            model->ice = bm_mouse->settings->ice;
        },
        true);

    bm_mouse->input_subscription =
        furi_pubsub_subscribe(bm_mouse->input_events, bm_mouse_input_event, bm_mouse);

    bm_mouse->thread = furi_thread_alloc_ex("BmMouseMotion", 1024, bm_mouse_thread, bm_mouse);
    /* Above the GUI service, so a tick lands on time even mid-redraw. Safe to
       sit here because the loop sleeps between ticks and never spins. */
    furi_thread_set_priority(bm_mouse->thread, FuriThreadPriorityHigh);
    furi_thread_start(bm_mouse->thread);
}

static void bm_mouse_exit_callback(void* context) {
    BmMouse* bm_mouse = context;

    if(bm_mouse->input_subscription) {
        furi_pubsub_unsubscribe(bm_mouse->input_events, bm_mouse->input_subscription);
        bm_mouse->input_subscription = NULL;
    }

    if(bm_mouse->thread) {
        furi_thread_flags_set(furi_thread_get_id(bm_mouse->thread), BM_FLAG_STOP);
        furi_thread_join(bm_mouse->thread);
        furi_thread_free(bm_mouse->thread);
        bm_mouse->thread = NULL;
    }

    /* Never leave a button stuck down on the host. */
    if(furi_hal_hid_is_connected()) {
        furi_hal_hid_mouse_release(HID_MOUSE_BTN_LEFT);
        furi_hal_hid_mouse_release(HID_MOUSE_BTN_RIGHT);
    }
}

BmMouse* bm_mouse_alloc(const BmSettings* settings, NotificationApp* notifications) {
    BmMouse* bm_mouse = malloc(sizeof(BmMouse));
    memset(bm_mouse, 0, sizeof(BmMouse));

    bm_mouse->settings = settings;
    bm_mouse->notifications = notifications;
    bm_mouse->input_events = furi_record_open(RECORD_INPUT_EVENTS);

    bm_mouse->view = view_alloc();
    view_set_context(bm_mouse->view, bm_mouse);
    /* Lock free, not Locking. view_draw() holds the model mutex for the whole
       draw callback, and every canvas_draw_icon() decompresses its icon, so a
       redraw of this screen keeps that mutex for milliseconds. With a locking
       model the motion thread blocked on it once per tick, which stalled the
       pointer exactly while the speed bar was animating during the ramp.
       Nothing here is bigger than a byte and only the motion thread writes it,
       so the worst a torn read can do is draw one field a frame late. */
    view_allocate_model(bm_mouse->view, ViewModelTypeLockFree, sizeof(BmMouseModel));
    view_set_draw_callback(bm_mouse->view, bm_mouse_draw_callback);
    view_set_input_callback(bm_mouse->view, bm_mouse_input_callback);
    view_set_enter_callback(bm_mouse->view, bm_mouse_enter_callback);
    view_set_exit_callback(bm_mouse->view, bm_mouse_exit_callback);

    return bm_mouse;
}

void bm_mouse_free(BmMouse* bm_mouse) {
    furi_assert(bm_mouse);
    furi_record_close(RECORD_INPUT_EVENTS);
    view_free(bm_mouse->view);
    free(bm_mouse);
}

View* bm_mouse_get_view(BmMouse* bm_mouse) {
    furi_assert(bm_mouse);
    return bm_mouse->view;
}

void bm_mouse_set_exit_callback(BmMouse* bm_mouse, BmMouseExitCallback callback, void* context) {
    furi_assert(bm_mouse);
    bm_mouse->exit_callback = callback;
    bm_mouse->exit_context = context;
}
