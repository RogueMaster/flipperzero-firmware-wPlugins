#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <gui/elements.h>
#include <math.h>
// Make sure this file exists in your assets folder
#include <bzzbzz_icons.h>

#define PATTERN_LENGTH 5
#define VIBRATION_MS   25
#define TIMEOUT_MS     4000

// Difficulty: 0.30 = 30% tolerance (Tuned for good feel)
#define DIFFICULTY_TOLERANCE 0.30f

// Possible intervals (ms)
const uint32_t INTERVALS[] = {200, 500, 800};

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} AppEvent;

typedef enum {
    GameStateTutorial,
    GameStateShowingPattern,
    GameStateWaitingForInput,
    GameStateGameOver
} GameState;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    NotificationApp* notification;
    FuriMessageQueue* event_queue;
    FuriTimer* timer;

    GameState state;

    // Game Data
    uint32_t pattern[PATTERN_LENGTH - 1];
    uint32_t input_times[PATTERN_LENGTH];
    uint8_t current_input_index;

    // Scoring
    uint32_t correct_in_a_row;
    uint32_t hi_score;
    float last_accuracy;

    // Timing
    uint32_t last_interaction_time;
    bool timeout_replay_done;

    // Tutorial
    int tutorial_page; // -1 for Start Screen, 0-3 for Pages

} AppContext;

// --- HELPER: Process Queue during delays ---
// This allows the Back button to work even while the device is vibrating
// Returns 'false' if the delay was interrupted by a Back press
bool responsive_delay(AppContext* ctx, uint32_t ms) {
    uint32_t start = furi_get_tick();
    while(furi_get_tick() - start < ms) {
        AppEvent event;
        // Wait up to 10ms for an event
        FuriStatus status = furi_message_queue_get(ctx->event_queue, &event, 10);

        if(status == FuriStatusOk) {
            // Check for Back Button
            if(event.type == EventTypeKey && event.input.key == InputKeyBack &&
               event.input.type == InputTypeShort) {
                // Signal the main loop to exit
                ctx->state = GameStateGameOver;
                return false; // Interrupted!
            }
            // We intentionally ignore/consume other buttons (OK, Left, Right)
            // during vibration/delays so the user doesn't accidentally buffer inputs.
        }
    }
    return true; // Completed successfully
}

static void render_callback(Canvas* canvas, void* ctx) {
    AppContext* context = ctx;

    canvas_clear(canvas);

    // --- START SCREEN ---
    if(context->state == GameStateTutorial && context->tutorial_page < 0) {
        canvas_draw_icon(canvas, 0, 5, &I_start);
        elements_button_center(canvas, "Start");
        elements_button_right(canvas, "Info");
        return;
    }

    // --- TUTORIAL ---
    if(context->state == GameStateTutorial && context->tutorial_page >= 0) {
        static const char* TUTORIAL_TEXT[3][3] = {
            {"BzzBzz vibrates", "in a 5 step pattern,", "and you need to repeat it."},
            {"Missed it?", "The pattern will replay", "after a few seconds."},
            {"v0.3", "by Koray Er", "mail@korayer.de"}};

        canvas_set_font(canvas, FontSecondary);
        char page_num[16];
        snprintf(page_num, sizeof(page_num), "%d/3", context->tutorial_page + 1);
        canvas_draw_str_aligned(canvas, 127, 0, AlignRight, AlignTop, page_num);

        if(context->tutorial_page < 3) {
            for(int i = 0; i < 3; i++) {
                if(TUTORIAL_TEXT[context->tutorial_page][i]) {
                    canvas_draw_str_aligned(
                        canvas,
                        64,
                        12 + (i * 10),
                        AlignCenter,
                        AlignTop,
                        TUTORIAL_TEXT[context->tutorial_page][i]);
                }
            }
        }

        elements_button_center(canvas, "Start");
        if(context->tutorial_page >= 0) elements_button_left(canvas, "Prev");
        if(context->tutorial_page < 2) elements_button_right(canvas, "Next");
        return;
    }

    // --- GAMEPLAY ---
    canvas_draw_icon(canvas, 0, 0, &I_game);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 0, 0, AlignLeft, AlignTop, "STEP");
    canvas_draw_str_aligned(canvas, 0, 33, AlignLeft, AlignTop, "ACC%");
    canvas_draw_str_aligned(canvas, 128, 0, AlignRight, AlignTop, "SCORE");

    canvas_set_font(canvas, FontBigNumbers);
    char buffer[16];

    // Accuracy
    snprintf(buffer, sizeof(buffer), "%02d", (int)context->last_accuracy);
    canvas_draw_str_aligned(canvas, 0, 44, AlignLeft, AlignTop, buffer);

    // Score
    snprintf(buffer, sizeof(buffer), "%02lu", context->correct_in_a_row);
    canvas_draw_str_aligned(canvas, 128, 12, AlignRight, AlignTop, buffer);

    // High Score
    snprintf(buffer, sizeof(buffer), "%02lu", context->hi_score);
    canvas_draw_str_aligned(canvas, 128, 44, AlignRight, AlignTop, buffer);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 128, 33, AlignRight, AlignTop, "BEST");

    // Indicator Dial
    static const float ANGLES[] = {0, -M_PI, -3 * M_PI / 4.0f, -M_PI / 2.0f, -M_PI / 4.0f, 0};

    if(context->current_input_index <= 5) {
        float angle = ANGLES[context->current_input_index];
        int cx = 11, cy = 22, len = 9;
        int end_x = cx + (int)(len * cosf(angle));
        int end_y = cy + (int)(len * sinf(angle));
        canvas_draw_line(canvas, cx, cy, end_x, end_y);
    }
}

static void input_callback(InputEvent* input_event, void* ctx) {
    AppContext* context = ctx;
    AppEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(context->event_queue, &event, 0);
}

static void timer_callback(void* ctx) {
    AppContext* context = ctx;
    AppEvent event = {.type = EventTypeTick};
    furi_message_queue_put(context->event_queue, &event, 0);
}

void generate_new_pattern(AppContext* ctx) {
    for(int i = 0; i < PATTERN_LENGTH - 1; i++) {
        ctx->pattern[i] = INTERVALS[rand() % 3];
    }
    ctx->current_input_index = 0;
    ctx->timeout_replay_done = false;
}

// Play vibration sequence
// Checks responsive_delay to allow instant exit
void play_pattern(AppContext* ctx) {
    // 1. Initial vibration
    notification_message(ctx->notification, &sequence_single_vibro);
    if(!responsive_delay(ctx, VIBRATION_MS)) return;

    // 2. Play pattern intervals
    for(int i = 0; i < PATTERN_LENGTH - 1; i++) {
        uint32_t wait = ctx->pattern[i];

        // Loop in chunks to keep checking input
        while(wait > 0) {
            uint32_t chunk = (wait > 50) ? 50 : wait;
            // If Back pressed, return immediately
            if(!responsive_delay(ctx, chunk)) return;
            wait -= chunk;
        }

        notification_message(ctx->notification, &sequence_single_vibro);
        if(!responsive_delay(ctx, VIBRATION_MS)) return;
    }
    ctx->last_interaction_time = furi_get_tick();
}

int32_t bzzbzz_app(void* p) {
    UNUSED(p);
    AppContext* ctx = malloc(sizeof(AppContext));

    ctx->event_queue = furi_message_queue_alloc(8, sizeof(AppEvent));
    ctx->gui = furi_record_open(RECORD_GUI);
    ctx->notification = furi_record_open(RECORD_NOTIFICATION);
    ctx->view_port = view_port_alloc();

    view_port_draw_callback_set(ctx->view_port, render_callback, ctx);
    view_port_input_callback_set(ctx->view_port, input_callback, ctx);
    gui_add_view_port(ctx->gui, ctx->view_port, GuiLayerFullscreen);

    ctx->timer = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, ctx);
    furi_timer_start(ctx->timer, 500);

    srand(furi_get_tick());
    ctx->state = GameStateTutorial;
    ctx->tutorial_page = -1; // Start Screen
    ctx->hi_score = 0;
    ctx->correct_in_a_row = 0;
    ctx->last_accuracy = 0.0f;

    AppEvent event;
    while(ctx->state != GameStateGameOver) {
        if(furi_message_queue_get(ctx->event_queue, &event, FuriWaitForever) == FuriStatusOk) {
            // --- TIMER ---
            if(event.type == EventTypeTick) {
                if(ctx->state == GameStateWaitingForInput) {
                    uint32_t now = furi_get_tick();
                    if(!ctx->timeout_replay_done &&
                       (now - ctx->last_interaction_time > TIMEOUT_MS)) {
                        ctx->current_input_index = 0;
                        view_port_update(ctx->view_port);
                        if(responsive_delay(ctx, 500)) {
                            play_pattern(ctx);
                        }
                        ctx->timeout_replay_done = true;
                    }
                }
                continue;
            }

            // --- INPUT ---
            if(event.type == EventTypeKey) {
                InputEvent* input = &event.input;

                if(input->key == InputKeyBack && input->type == InputTypeShort) {
                    if(ctx->state == GameStateTutorial)
                        ctx->state = GameStateGameOver;
                    else {
                        // Reset to tutorial/start screen on back
                        ctx->state = GameStateTutorial;
                        ctx->tutorial_page = -1;
                    }
                } else if(input->type == InputTypeShort) {
                    // 1. TUTORIAL / MENU
                    if(ctx->state == GameStateTutorial) {
                        if(ctx->tutorial_page < 0) {
                            if(input->key == InputKeyOk) {
                                generate_new_pattern(ctx);
                                ctx->state = GameStateShowingPattern;
                                // Wait 1 second before playing the vibration pattern
                                if(responsive_delay(ctx, 1000)) {
                                    play_pattern(ctx);
                                }
                                if(ctx->state != GameStateGameOver) {
                                    ctx->state = GameStateWaitingForInput;
                                }
                            } else if(input->key == InputKeyRight) {
                                ctx->tutorial_page = 0;
                            }
                        } else {
                            if(input->key == InputKeyRight && ctx->tutorial_page < 2)
                                ctx->tutorial_page++;
                            else if(input->key == InputKeyLeft && ctx->tutorial_page > 0)
                                ctx->tutorial_page--;
                            else if(input->key == InputKeyLeft && ctx->tutorial_page == 0)
                                ctx->tutorial_page = -1;
                            else if(input->key == InputKeyOk) {
                                generate_new_pattern(ctx);
                                ctx->state = GameStateShowingPattern;
                                // Wait 1 second before playing the vibration pattern
                                if(responsive_delay(ctx, 1000)) {
                                    play_pattern(ctx);
                                }
                                if(ctx->state != GameStateGameOver) {
                                    ctx->state = GameStateWaitingForInput;
                                }
                            }
                        }
                    }
                    // 2. GAMEPLAY
                    else if(ctx->state == GameStateWaitingForInput && input->key == InputKeyOk) {
                        uint32_t now = furi_get_tick();

                        if(ctx->current_input_index < PATTERN_LENGTH) {
                            ctx->input_times[ctx->current_input_index] = now;
                            ctx->current_input_index++;
                            ctx->last_interaction_time = now;
                            ctx->timeout_replay_done = false;
                        }

                        if(ctx->current_input_index >= PATTERN_LENGTH) {
                            float total_score = 0;
                            for(int i = 0; i < PATTERN_LENGTH - 1; i++) {
                                uint32_t actual = ctx->input_times[i + 1] - ctx->input_times[i];
                                uint32_t expected = ctx->pattern[i];
                                float limit = expected * DIFFICULTY_TOLERANCE;
                                float diff = abs((int32_t)actual - (int32_t)expected);
                                float score = 100.0f - ((diff / limit) * 50.0f);
                                if(score < 0) score = 0;
                                if(score > 100) score = 100;
                                total_score += score;
                            }

                            ctx->last_accuracy = total_score / (PATTERN_LENGTH - 1);

                            if(ctx->last_accuracy >= 50.0f) {
                                // PASS
                                ctx->correct_in_a_row++;
                                if(ctx->correct_in_a_row > ctx->hi_score)
                                    ctx->hi_score = ctx->correct_in_a_row;
                                generate_new_pattern(ctx);
                                view_port_update(ctx->view_port);
                                if(responsive_delay(ctx, 500)) {
                                    play_pattern(ctx);
                                }
                            } else {
                                // FAIL
                                ctx->correct_in_a_row = 0;
                                // 1. Play Error Vibes
                                for(int k = 0; k < 3; k++) {
                                    notification_message(
                                        ctx->notification, &sequence_single_vibro);
                                    if(!responsive_delay(ctx, 150)) break;
                                }

                                // 2. Wait 1.2 Seconds (Gap)
                                if(ctx->state != GameStateGameOver) {
                                    responsive_delay(ctx, 1200);
                                }

                                // 3. Replay Pattern
                                ctx->current_input_index = 0;
                                ctx->timeout_replay_done = false;
                                if(ctx->state != GameStateGameOver) {
                                    play_pattern(ctx);
                                }
                            }

                            if(ctx->state != GameStateGameOver) {
                                ctx->state = GameStateWaitingForInput;
                            }
                        }
                    }
                }
                view_port_update(ctx->view_port);
            }
        }
    }

    furi_timer_free(ctx->timer);
    gui_remove_view_port(ctx->gui, ctx->view_port);
    view_port_free(ctx->view_port);
    furi_message_queue_free(ctx->event_queue);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(ctx);

    return 0;
}
