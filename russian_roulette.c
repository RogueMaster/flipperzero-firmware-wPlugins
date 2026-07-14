#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_random.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define CHAMBERS 6
#define MSG_DISPLAY_TICKS 20
#define TICK_HZ 8

typedef enum {
    ScreenTitle,
    ScreenReady,
    ScreenClick,
    ScreenBang,
    ScreenSpin,
} Screen;

typedef enum {
    EventTypeInput,
    EventTypeTick,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} RouletteEvent;

typedef struct {
    Screen screen;
    uint8_t bullet_position;
    uint8_t current_chamber;
    uint8_t pulls_survived;
    uint32_t msg_ticks_left;
    FuriMutex* mutex;
} RouletteState;

// Cocking the hammer at game start — two rising clicks.
static const NotificationSequence sequence_start = {
    &message_note_a5,
    &message_delay_25,
    &message_sound_off,
    &message_delay_50,
    &message_note_e6,
    &message_delay_25,
    &message_sound_off,
    NULL,
};

// Trigger pull on an empty chamber — a sharp click + vibro, then a soft "phew".
static const NotificationSequence sequence_click = {
    &message_vibro_on,
    &message_note_e6,
    &message_delay_10,
    &message_sound_off,
    &message_vibro_off,
    &message_delay_50,
    &message_note_c5,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

// The shot — a loud low bang with a red flash, then a descending game-over motif.
static const NotificationSequence sequence_bang = {
    &message_vibro_on,
    &message_red_255,
    &message_note_c3,
    &message_delay_250,
    &message_sound_off,
    &message_vibro_off,
    &message_delay_100,
    &message_note_gs3,
    &message_delay_100,
    &message_sound_off,
    &message_note_f3,
    &message_delay_100,
    &message_sound_off,
    &message_note_c3,
    &message_delay_250,
    &message_sound_off,
    &message_red_0,
    NULL,
};

// Spinning the cylinder — a fast descending ratchet of clicks.
static const NotificationSequence sequence_spin = {
    &message_note_c6,  &message_delay_10, &message_sound_off, &message_delay_10,
    &message_note_a5,  &message_delay_10, &message_sound_off, &message_delay_10,
    &message_note_fs5, &message_delay_10, &message_sound_off, &message_delay_10,
    &message_note_d5,  &message_delay_10, &message_sound_off, &message_delay_10,
    &message_note_a4,  &message_delay_10, &message_sound_off, &message_delay_10,
    &message_note_e4,  &message_delay_10, &message_sound_off,
    NULL,
};

static void roulette_spin(RouletteState* state) {
    state->bullet_position = furi_hal_random_get() % CHAMBERS;
    state->current_chamber = 0;
    state->pulls_survived = 0;
}

static void draw_cylinder(Canvas* canvas, RouletteState* state, int cx, int cy, int r) {
    canvas_draw_circle(canvas, cx, cy, r);
    for(int i = 0; i < CHAMBERS; i++) {
        double angle = (6.28318530718 * i) / CHAMBERS - 1.5707963;
        int px = cx + (int)((r - 3) * cos(angle));
        int py = cy + (int)((r - 3) * sin(angle));
        if(i == state->current_chamber) {
            canvas_draw_disc(canvas, px, py, 2);
        } else {
            canvas_draw_circle(canvas, px, py, 2);
        }
    }
}

static void render_callback(Canvas* canvas, void* ctx) {
    furi_assert(ctx);
    RouletteState* state = ctx;
    furi_mutex_acquire(state->mutex, FuriWaitForever);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    switch(state->screen) {
    case ScreenTitle:
        canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignCenter, "ROULETTE");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignCenter, "OK: Spin & Start");
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, "Hold BACK: Exit");
        break;

    case ScreenReady: {
        draw_cylinder(canvas, state, 32, 32, 20);
        canvas_set_font(canvas, FontSecondary);
        char buf[24];
        snprintf(buf, sizeof(buf), "Chamber %d/%d", state->current_chamber + 1, CHAMBERS);
        canvas_draw_str(canvas, 60, 18, buf);
        snprintf(buf, sizeof(buf), "Survived: %d", state->pulls_survived);
        canvas_draw_str(canvas, 60, 30, buf);
        canvas_draw_str(canvas, 60, 46, "OK: Pull");
        canvas_draw_str(canvas, 60, 58, "LEFT: Spin");
        break;
    }

    case ScreenClick:
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "*click*");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, "You live.");
        {
            char buf[24];
            snprintf(buf, sizeof(buf), "Survived: %d", state->pulls_survived);
            canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignCenter, buf);
        }
        break;

    case ScreenBang:
        canvas_draw_box(canvas, 0, 0, 128, 64);
        canvas_invert_color(canvas);
        canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignCenter, "BANG!");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, "Game Over");
        {
            char buf[24];
            snprintf(buf, sizeof(buf), "Score: %d", state->pulls_survived);
            canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignCenter, buf);
        }
        canvas_invert_color(canvas);
        break;

    case ScreenSpin:
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, "Spinning...");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignCenter, "New chamber loaded");
        break;
    }

    furi_mutex_release(state->mutex);
}

static void input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* queue = ctx;
    RouletteEvent event = {.type = EventTypeInput, .input = *input_event};
    furi_message_queue_put(queue, &event, FuriWaitForever);
}

static void tick_callback(void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* queue = ctx;
    RouletteEvent event = {.type = EventTypeTick};
    furi_message_queue_put(queue, &event, 0);
}

int32_t russian_roulette_app(void* p) {
    UNUSED(p);

    RouletteState* state = malloc(sizeof(RouletteState));
    state->screen = ScreenTitle;
    state->bullet_position = 0;
    state->current_chamber = 0;
    state->pulls_survived = 0;
    state->msg_ticks_left = 0;
    state->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!state->mutex) {
        free(state);
        return 255;
    }

    FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(RouletteEvent));

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, render_callback, state);
    view_port_input_callback_set(view_port, input_callback, queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);

    FuriTimer* timer = furi_timer_alloc(tick_callback, FuriTimerTypePeriodic, queue);
    furi_timer_start(timer, furi_kernel_get_tick_frequency() / TICK_HZ);

    furi_hal_random_init();

    bool running = true;
    RouletteEvent event;

    while(running) {
        if(furi_message_queue_get(queue, &event, FuriWaitForever) != FuriStatusOk) continue;

        furi_mutex_acquire(state->mutex, FuriWaitForever);

        if(event.type == EventTypeTick) {
            if(state->msg_ticks_left > 0) {
                state->msg_ticks_left--;
                if(state->msg_ticks_left == 0) {
                    if(state->screen == ScreenClick) {
                        state->current_chamber = (state->current_chamber + 1) % CHAMBERS;
                        state->screen = ScreenReady;
                    } else if(state->screen == ScreenBang) {
                        state->screen = ScreenTitle;
                    } else if(state->screen == ScreenSpin) {
                        state->screen = ScreenReady;
                    }
                }
            }
        } else if(event.type == EventTypeInput) {
            InputEvent* in = &event.input;

            if(in->type == InputTypeLong && in->key == InputKeyBack) {
                running = false;
            } else if(in->type == InputTypeShort) {
                switch(state->screen) {
                case ScreenTitle:
                    if(in->key == InputKeyOk) {
                        roulette_spin(state);
                        state->screen = ScreenReady;
                        notification_message(notifications, &sequence_start);
                    } else if(in->key == InputKeyBack) {
                        running = false;
                    }
                    break;

                case ScreenReady:
                    if(in->key == InputKeyOk) {
                        if(state->current_chamber == state->bullet_position) {
                            state->screen = ScreenBang;
                            state->msg_ticks_left = MSG_DISPLAY_TICKS * 2;
                            notification_message(notifications, &sequence_bang);
                        } else {
                            state->pulls_survived++;
                            state->screen = ScreenClick;
                            state->msg_ticks_left = MSG_DISPLAY_TICKS;
                            notification_message(notifications, &sequence_click);
                        }
                    } else if(in->key == InputKeyLeft || in->key == InputKeyRight) {
                        roulette_spin(state);
                        state->screen = ScreenSpin;
                        state->msg_ticks_left = MSG_DISPLAY_TICKS / 2;
                        notification_message(notifications, &sequence_spin);
                    } else if(in->key == InputKeyBack) {
                        state->screen = ScreenTitle;
                    }
                    break;

                case ScreenClick:
                case ScreenBang:
                case ScreenSpin:
                    break;
                }
            }
        }

        furi_mutex_release(state->mutex);
        view_port_update(view_port);
    }

    furi_timer_stop(timer);
    furi_timer_free(timer);
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_message_queue_free(queue);
    furi_mutex_free(state->mutex);
    free(state);
    return 0;
}
