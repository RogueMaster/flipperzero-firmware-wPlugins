#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_infrared.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

// --- Configuration ---
#define LASKO_IR_FREQ 38000
#define LASKO_IR_DUTY 0.33f

// Helper macro for development debugging (Unit: ms)
// Set to 1 to use real times (20m/40m), 0 for fast test (Seconds instead of
// minutes)
#define USE_REAL_TIMES 1

#define DEFAULT_ON_MIN  20
#define DEFAULT_OFF_MIN 40

// Lasko Protocol Timings (micros)
#define BIT_1_MARK  1246
#define BIT_1_SPACE 434
#define BIT_0_MARK  404
#define BIT_0_SPACE 1276
#define REPEAT_GAP  7107
#define REPEATS     4
#define BITS_LEN    12

// Power Code: 0b110110000001 (0xD81)
const uint16_t CMD_POWER = 0x0D81;

typedef enum {
    StateIdle,
    StateRunningOn, // Fan is ON, timer counting down
    StateRunningOff, // Fan is OFF, timer counting down
    StateAbout // About Page
} AppState;

typedef struct {
    AppState state;
    uint32_t state_start_tick; // Tick when the current state started
    FuriMutex* mutex;
    ViewPort* view_port;
    Gui* gui;
    NotificationApp* notification;

    // Config
    int32_t duration_on_min;
    int32_t duration_off_min;
    int32_t menu_index; // 0: Start, 1: On, 2: Off, 3: About
    bool screen_off;
} AppContext;

// --- IR Logic ---

typedef struct {
    uint32_t* timings;
    size_t count;
    size_t index;
} InfraredTxContext;

static FuriHalInfraredTxGetDataState
    infrared_tx_callback(void* context, uint32_t* duration, bool* level) {
    InfraredTxContext* ctx = (InfraredTxContext*)context;

    if(ctx->index >= ctx->count) {
        return FuriHalInfraredTxGetDataStateLastDone;
    }

    *duration = ctx->timings[ctx->index];
    *level = ((ctx->index % 2) == 0); // even = mark (high), odd = space (low)

    ctx->index++;

    if(ctx->index >= ctx->count) {
        return FuriHalInfraredTxGetDataStateLastDone;
    }

    return FuriHalInfraredTxGetDataStateOk;
}

static void send_lasko_signal(uint16_t code) {
    uint32_t* duration_buffer = malloc(sizeof(uint32_t) * BITS_LEN * 2 * REPEATS);
    size_t index = 0;

    for(int r = 0; r < REPEATS; r++) {
        for(int i = BITS_LEN - 1; i >= 0; i--) {
            // Read bit (MSB first)
            bool bit = (code >> i) & 1;

            uint32_t mark = bit ? BIT_1_MARK : BIT_0_MARK;
            uint32_t space = bit ? BIT_1_SPACE : BIT_0_SPACE;

            // Mark gap check
            if(i == 0 && r < REPEATS - 1) {
                space += REPEAT_GAP;
            }

            duration_buffer[index++] = mark;
            duration_buffer[index++] = space;
        }
    }

    if(furi_hal_infrared_is_busy()) {
        furi_hal_infrared_async_tx_wait_termination();
    }

    InfraredTxContext tx_ctx = {.timings = duration_buffer, .count = index, .index = 0};

    furi_hal_infrared_async_tx_set_data_isr_callback(infrared_tx_callback, &tx_ctx);
    furi_hal_infrared_async_tx_start(LASKO_IR_FREQ, LASKO_IR_DUTY);
    furi_hal_infrared_async_tx_wait_termination();

    free(duration_buffer);
}

// --- App Logic ---

static void app_update_timer(AppContext* ctx) {
    furi_mutex_acquire(ctx->mutex, FuriWaitForever);

    if(ctx->state == StateRunningOn || ctx->state == StateRunningOff) {
        uint32_t now = furi_get_tick();
        uint32_t elapsed = now - ctx->state_start_tick;

        uint32_t duration_ms = (ctx->state == StateRunningOn) ?
                                   (ctx->duration_on_min * 60UL * 1000UL) :
                                   (ctx->duration_off_min * 60UL * 1000UL);

#if !USE_REAL_TIMES
        duration_ms /= 60; // Use seconds for debug
#endif

        if(elapsed >= duration_ms) {
            // Time to switch!
            furi_mutex_release(ctx->mutex); // Release around IR sent

            notification_message(ctx->notification, &sequence_blink_blue_100);
            send_lasko_signal(CMD_POWER);

            furi_mutex_acquire(ctx->mutex, FuriWaitForever);

            // Toggle State
            if(ctx->state == StateRunningOn) {
                ctx->state = StateRunningOff;
            } else {
                ctx->state = StateRunningOn;
            }
            ctx->state_start_tick = furi_get_tick();
        }
    }

    furi_mutex_release(ctx->mutex);
}

static void render_callback(Canvas* canvas, void* ctx_ptr) {
    AppContext* ctx = (AppContext*)ctx_ptr;
    furi_mutex_acquire(ctx->mutex, FuriWaitForever);

    canvas_clear(canvas);

    if(ctx->screen_off) {
        furi_mutex_release(ctx->mutex);
        return;
    }

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Lasko 2519 Timer");

    canvas_set_font(canvas, FontSecondary);
    char buffer[64];

    if(ctx->state == StateIdle) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 19, "Start with Fan OFF!");

        // Selection Indicator
        int y_base = 30;
        int y_step = 11;
        int y_sel = y_base + (ctx->menu_index * y_step);

        canvas_draw_str(canvas, 2, y_sel, ">");

        // Items
        canvas_draw_str(canvas, 10, y_base, "Start Automation");

        snprintf(buffer, sizeof(buffer), "On Time: %ld min", ctx->duration_on_min);
        canvas_draw_str(canvas, 10, y_base + y_step, buffer);
        if(ctx->menu_index == 1) {
            canvas_draw_str(canvas, 115, y_base + y_step, "<>");
        }

        snprintf(buffer, sizeof(buffer), "Off Time: %ld min", ctx->duration_off_min);
        canvas_draw_str(canvas, 10, y_base + (y_step * 2), buffer);
        if(ctx->menu_index == 2) {
            canvas_draw_str(canvas, 115, y_base + (y_step * 2), "<>");
        }

        canvas_draw_str(canvas, 10, y_base + (y_step * 3), "About");
        if(ctx->menu_index == 3) {
            canvas_draw_str(canvas, 2, y_base + (y_step * 3), ">");
        }

    } else if(ctx->state == StateAbout) {
        canvas_clear(canvas);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 10, 20, "Lasko 2519 Timer");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 10, 40, "Copyright 2026");
        canvas_draw_str(canvas, 10, 52, "Made by LN4CY");
        canvas_draw_str(canvas, 10, 62, "Back/Left to Exit");

    } else {
        uint32_t now = furi_get_tick();
        uint32_t elapsed = now - ctx->state_start_tick;

        uint32_t duration_ms = (ctx->state == StateRunningOn) ?
                                   (ctx->duration_on_min * 60UL * 1000UL) :
                                   (ctx->duration_off_min * 60UL * 1000UL);

#if !USE_REAL_TIMES
        duration_ms /= 60;
#endif

        int32_t remaining = (duration_ms > elapsed) ? (duration_ms - elapsed) : 0;
        int32_t rem_sec = remaining / 1000;
        int32_t rem_min = rem_sec / 60;
        rem_sec %= 60;

        if(ctx->state == StateRunningOn) {
            canvas_draw_str(canvas, 10, 30, "State: FAN ON");
            snprintf(buffer, sizeof(buffer), "Time Left: %02ld:%02ld", rem_min, rem_sec);
        } else {
            canvas_draw_str(canvas, 10, 30, "State: FAN OFF");
            snprintf(buffer, sizeof(buffer), "Next ON in: %02ld:%02ld", rem_min, rem_sec);
        }
        canvas_draw_str(canvas, 10, 50, buffer);

        canvas_draw_str(canvas, 40, 62, "Back to Stop");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 10, 20, "Right: Screen Off");
    }

    furi_mutex_release(ctx->mutex);
}

static void input_callback(InputEvent* input_event, void* ctx_ptr) {
    FuriMessageQueue* event_queue = (FuriMessageQueue*)ctx_ptr;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

int32_t lasko_automator_app(void* p) {
    UNUSED(p);
    AppContext* ctx = malloc(sizeof(AppContext));
    ctx->state = StateIdle;
    ctx->duration_on_min = DEFAULT_ON_MIN;
    ctx->duration_off_min = DEFAULT_OFF_MIN;
    ctx->menu_index = 0;
    ctx->screen_off = false;

    ctx->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    ctx->notification = furi_record_open(RECORD_NOTIFICATION);

    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    ctx->view_port = view_port_alloc();
    view_port_draw_callback_set(ctx->view_port, render_callback, ctx);
    view_port_input_callback_set(ctx->view_port, input_callback, event_queue);

    ctx->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(ctx->gui, ctx->view_port, GuiLayerFullscreen);

    InputEvent event;
    bool running = true;

    while(running) {
        FuriStatus status = furi_message_queue_get(event_queue, &event, 250); // 250ms tick

        if(status == FuriStatusOk) {
            furi_mutex_acquire(ctx->mutex, FuriWaitForever);

            if(event.type == InputTypeShort || event.type == InputTypeRepeat) {
                if(event.key == InputKeyBack ||
                   (event.key == InputKeyLeft && ctx->state == StateAbout)) {
                    if(ctx->state == StateIdle) {
                        if(event.key == InputKeyBack) running = false;
                    } else if(ctx->state == StateAbout) {
                        ctx->state = StateIdle;
                    } else {
                        // Stop and go back to config
                        ctx->state = StateIdle;
                        if(ctx->screen_off) {
                            ctx->screen_off = false;
                            notification_message(
                                ctx->notification, &sequence_display_backlight_on);
                        }
                    }
                }

                if(ctx->state == StateIdle) {
                    if(event.key == InputKeyUp) {
                        ctx->menu_index--;
                        if(ctx->menu_index < 0) ctx->menu_index = 3;
                    }
                    if(event.key == InputKeyDown) {
                        ctx->menu_index++;
                        if(ctx->menu_index > 3) ctx->menu_index = 0;
                    }

                    if(event.key == InputKeyLeft) {
                        if(ctx->menu_index == 1 && ctx->duration_on_min > 1)
                            ctx->duration_on_min--;
                        if(ctx->menu_index == 2 && ctx->duration_off_min > 1)
                            ctx->duration_off_min--;
                    }
                    if(event.key == InputKeyRight) {
                        if(ctx->menu_index == 1 && ctx->duration_on_min < 240)
                            ctx->duration_on_min++;
                        if(ctx->menu_index == 2 && ctx->duration_off_min < 240)
                            ctx->duration_off_min++;
                    }

                    if(event.key == InputKeyOk ||
                       (event.key == InputKeyRight &&
                        (ctx->menu_index == 0 || ctx->menu_index == 3))) {
                        if(ctx->menu_index == 0) {
                            // Start!
                            furi_mutex_release(ctx->mutex);

                            send_lasko_signal(CMD_POWER);
                            notification_message(ctx->notification, &sequence_success);

                            furi_mutex_acquire(ctx->mutex, FuriWaitForever);
                            ctx->state = StateRunningOn;
                            ctx->state_start_tick = furi_get_tick();
                        }
                        if(ctx->menu_index == 3) {
                            ctx->state = StateAbout;
                        }
                    }
                } else if(ctx->state == StateRunningOn || ctx->state == StateRunningOff) {
                    if(event.key == InputKeyRight) {
                        ctx->screen_off = !ctx->screen_off;
                        if(ctx->screen_off) {
                            notification_message(
                                ctx->notification, &sequence_display_backlight_off);
                        } else {
                            notification_message(
                                ctx->notification, &sequence_display_backlight_on);
                        }
                    }
                }
            }
            furi_mutex_release(ctx->mutex);
        }

        // Timer Check Loop
        app_update_timer(ctx);
        view_port_update(ctx->view_port);
    }

    gui_remove_view_port(ctx->gui, ctx->view_port);
    view_port_free(ctx->view_port);
    furi_message_queue_free(event_queue);
    furi_mutex_free(ctx->mutex);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(ctx);

    return 0;
}
