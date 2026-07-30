#define _POSIX_C_SOURCE 200809L

#include "morse_flipper_icr.h"
#include "morse_flipper_icr_api.h"

#include <dialogs/dialogs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ICR_PHASE_GRAPH_WAIT      0U
#define ICR_PHASE_PLAYBACK        1U
#define ICR_PHASE_RECOGNITION     2U
#define ICR_PHASE_RECOGNIZED_HOLD 3U
#define ICR_PHASE_ANSWER_GUARD    4U
#define ICR_PHASE_ANSWER          5U
#define ICR_PHASE_RESULT          6U
#define ICR_FEEDBACK_TIMEOUT      4U
#define ICR_WAIT_MS               1000U
#define ICR_GUARD_MS              100U
#define ICR_RESULT_MS             1000U
#define ICR_PRESS_BLACK_MS        300U
#define ICR_TRACE_MS              10000U

static unsigned g_checks;
static DialogMessageButton dialog_responses[4];
static DialogMessage dialog_seen[4];
static uint8_t dialog_response_count;
static uint8_t dialog_show_count;

#define CHECK(expr)                                                         \
    do {                                                                    \
        g_checks++;                                                         \
        if(!(expr)) {                                                       \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
            exit(1);                                                        \
        }                                                                   \
    } while(0)

DialogMessage* dialog_message_alloc(void) {
    return calloc(1U, sizeof(DialogMessage));
}

void dialog_message_free(DialogMessage* message) {
    free(message);
}

void dialog_message_set_header(
    DialogMessage* message,
    const char* text,
    uint8_t x,
    uint8_t y,
    Align horizontal,
    Align vertical) {
    (void)x;
    (void)y;
    (void)horizontal;
    (void)vertical;
    message->header = text;
}

void dialog_message_set_text(
    DialogMessage* message,
    const char* text,
    uint8_t x,
    uint8_t y,
    Align horizontal,
    Align vertical) {
    (void)x;
    (void)y;
    (void)horizontal;
    (void)vertical;
    message->text = text;
}

void dialog_message_set_buttons(
    DialogMessage* message,
    const char* left,
    const char* center,
    const char* right) {
    message->left = left;
    message->center = center;
    message->right = right;
}

DialogMessageButton dialog_message_show(DialogsApp* context, const DialogMessage* message) {
    (void)context;
    CHECK(dialog_show_count < dialog_response_count);
    dialog_seen[dialog_show_count] = *message;
    return dialog_responses[dialog_show_count++];
}

void* morse_flipper_icr_runtime_alloc(void);
void morse_flipper_icr_runtime_free(void* state);
void morse_flipper_icr_runtime_leave(void* state);
bool morse_flipper_icr_runtime_enter(
    void* state,
    const MorseFlipperIcrEnterArgs* args,
    MorseFlipperIcrResult* initial);
MorseFlipperIcrResult
    morse_flipper_icr_runtime_input(void* state, const InputEvent* event, uint32_t now_ms);
MorseFlipperIcrResult morse_flipper_icr_runtime_tick(void* state, uint32_t now_ms);
void morse_flipper_icr_runtime_draw(void* state, Canvas* canvas, uint32_t now_ms);

void canvas_set_font(Canvas* canvas, Font font) {
    (void)canvas;
    (void)font;
}

void canvas_draw_str_aligned(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    Align horizontal,
    Align vertical,
    const char* text) {
    (void)canvas;
    (void)x;
    (void)y;
    (void)horizontal;
    (void)vertical;
    if(text[0] == '\0') return;
    if(canvas->strings < TEST_CANVAS_TEXT_CAPACITY) {
        snprintf(canvas->text[canvas->strings], TEST_CANVAS_TEXT_LENGTH, "%s", text);
        canvas->text_x[canvas->strings] = x;
        canvas->text_y[canvas->strings] = y;
    }
    canvas->strings++;
    if(canvas->current_color == ColorWhite) canvas->white_strings++;
}

void canvas_draw_dot(Canvas* canvas, int32_t x, int32_t y) {
    (void)x;
    (void)y;
    canvas->dots++;
}

void canvas_draw_box(Canvas* canvas, int32_t x, int32_t y, int32_t width, int32_t height) {
    if(canvas->boxes < TEST_CANVAS_BOX_CAPACITY) {
        canvas->box_x[canvas->boxes] = x;
        canvas->box_y[canvas->boxes] = y;
        canvas->box_width[canvas->boxes] = width;
        canvas->box_height[canvas->boxes] = height;
        canvas->box_color[canvas->boxes] = canvas->current_color;
    }
    canvas->boxes++;
}

void canvas_draw_line(Canvas* canvas, int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    canvas->lines++;
    if(x1 == 57 && y1 == 0 && x2 == 57 && y2 == 63) canvas->full_height_divider = true;
}

void canvas_draw_bitmap(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t width,
    size_t height,
    const uint8_t* data) {
    size_t data_size;
    uint32_t hash = 2166136261U;

    if(data[0] == 1U) {
        data_size = 4U + data[2] + ((size_t)data[3] << 8U);
    } else {
        data_size = 1U + (((width + 7U) / 8U) * height);
    }
    for(size_t i = 0U; i < data_size; i++) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    if(canvas->bitmaps < TEST_CANVAS_BITMAP_CAPACITY) {
        canvas->bitmap_hash[canvas->bitmaps] = hash;
        canvas->bitmap_x[canvas->bitmaps] = x;
        canvas->bitmap_y[canvas->bitmaps] = y;
        canvas->bitmap_width[canvas->bitmaps] = width;
        canvas->bitmap_height[canvas->bitmaps] = height;
    }
    canvas->bitmaps++;
}

void canvas_set_color(Canvas* canvas, Color color) {
    canvas->current_color = color;
}

typedef struct {
    uint8_t phase[ICR_TRACE_MS + 1U];
    bool playback_mark[ICR_TRACE_MS + 1U];
    bool redraw[ICR_TRACE_MS + 1U];
    uint32_t dots[ICR_TRACE_MS + 1U];
    uint32_t boxes[ICR_TRACE_MS + 1U];
    uint32_t timeout_at;
} IcrTrace;

static void trace_timeout(uint32_t start, IcrTrace* trace) {
    MorseFlipperIcrEnterArgs args = {.now_ms = start, .rng_seed = 0x1A2B3C4DU};
    MorseFlipperIcrResult initial;
    void* state = morse_flipper_icr_runtime_alloc();

    CHECK(state != NULL);
    CHECK(morse_flipper_icr_runtime_enter(state, &args, &initial));
    CHECK(initial.phase == ICR_PHASE_GRAPH_WAIT);
    for(uint32_t elapsed = 0U; elapsed <= ICR_TRACE_MS; elapsed++) {
        Canvas canvas = {0};
        MorseFlipperIcrResult result = morse_flipper_icr_runtime_tick(state, start + elapsed);

        morse_flipper_icr_runtime_draw(state, &canvas, start + elapsed);
        trace->phase[elapsed] = result.phase;
        trace->playback_mark[elapsed] = result.playback_mark;
        trace->redraw[elapsed] = result.redraw;
        trace->dots[elapsed] = canvas.dots;
        trace->boxes[elapsed] = canvas.boxes;
        if(result.feedback == ICR_FEEDBACK_TIMEOUT && trace->timeout_at == 0U)
            trace->timeout_at = elapsed;
    }
    CHECK(trace->timeout_at != 0U);
    CHECK(trace->phase[trace->timeout_at + ICR_RESULT_MS] == ICR_PHASE_GRAPH_WAIT);
    morse_flipper_icr_runtime_free(state);
}

static void test_timeout_trace_wrap_equivalence(void) {
    IcrTrace ordinary = {0};
    IcrTrace wrapped = {0};

    trace_timeout(100000U, &ordinary);
    trace_timeout(UINT32_MAX - 500U, &wrapped);
    CHECK(ordinary.timeout_at == wrapped.timeout_at);
    for(uint32_t elapsed = 0U; elapsed <= ICR_TRACE_MS; elapsed++) {
        CHECK(ordinary.phase[elapsed] == wrapped.phase[elapsed]);
        CHECK(ordinary.playback_mark[elapsed] == wrapped.playback_mark[elapsed]);
        CHECK(ordinary.redraw[elapsed] == wrapped.redraw[elapsed]);
        CHECK(ordinary.dots[elapsed] == wrapped.dots[elapsed]);
        CHECK(ordinary.boxes[elapsed] == wrapped.boxes[elapsed]);
    }
    CHECK(ordinary.phase[ICR_WAIT_MS - 1U] == ICR_PHASE_GRAPH_WAIT);
    CHECK(ordinary.phase[ICR_WAIT_MS] == ICR_PHASE_PLAYBACK);
    CHECK(ordinary.phase[ordinary.timeout_at] == ICR_PHASE_RESULT);
    CHECK(ordinary.phase[ordinary.timeout_at + ICR_RESULT_MS] == ICR_PHASE_GRAPH_WAIT);
}

static void test_answer_trace_wrap_equivalence(void) {
    const uint32_t starts[] = {100000U, UINT32_MAX - 500U};
    uint8_t phases[2][5];

    for(uint8_t run = 0U; run < 2U; run++) {
        MorseFlipperIcrEnterArgs args = {.now_ms = starts[run], .rng_seed = 0x55667788U};
        MorseFlipperIcrResult result;
        InputEvent press = {.key = InputKeyOk, .type = InputTypePress};
        InputEvent release = {.key = InputKeyOk, .type = InputTypeRelease};
        InputEvent answer = {.key = InputKeyUp, .type = InputTypeRelease};
        void* state = morse_flipper_icr_runtime_alloc();
        uint32_t elapsed;

        CHECK(state != NULL);
        CHECK(morse_flipper_icr_runtime_enter(state, &args, &result));
        for(elapsed = 0U; elapsed < ICR_TRACE_MS; elapsed++) {
            result = morse_flipper_icr_runtime_tick(state, starts[run] + elapsed);
            if(result.phase == ICR_PHASE_RECOGNITION) break;
        }
        CHECK(result.phase == ICR_PHASE_RECOGNITION);
        phases[run][0] = result.phase;
        result = morse_flipper_icr_runtime_input(state, &press, starts[run] + elapsed);
        phases[run][1] = result.phase;
        result = morse_flipper_icr_runtime_input(state, &release, starts[run] + elapsed + 1U);
        phases[run][2] = result.phase;
        result = morse_flipper_icr_runtime_tick(state, starts[run] + elapsed + 1U + ICR_GUARD_MS);
        phases[run][3] = result.phase;
        result = morse_flipper_icr_runtime_input(
            state, &answer, starts[run] + elapsed + 2U + ICR_GUARD_MS);
        CHECK(result.phase == ICR_PHASE_RESULT);
        result = morse_flipper_icr_runtime_tick(
            state, starts[run] + elapsed + 2U + ICR_GUARD_MS + ICR_RESULT_MS);
        phases[run][4] = result.phase;
        morse_flipper_icr_runtime_free(state);
    }

    CHECK(memcmp(phases[0], phases[1], sizeof(phases[0])) == 0);
    CHECK(phases[0][0] == ICR_PHASE_RECOGNITION);
    CHECK(phases[0][1] == ICR_PHASE_RECOGNIZED_HOLD);
    CHECK(phases[0][2] == ICR_PHASE_ANSWER_GUARD);
    CHECK(phases[0][3] == ICR_PHASE_ANSWER);
    CHECK(phases[0][4] == ICR_PHASE_GRAPH_WAIT);
}

static void test_zero_deadlines_remain_active(void) {
    IcrTrace ordinary = {0};
    MorseFlipperIcrEnterArgs args = {.now_ms = UINT32_MAX - 999U, .rng_seed = 0x12345678U};
    MorseFlipperIcrResult result;
    void* state = morse_flipper_icr_runtime_alloc();
    uint32_t flash_start;

    CHECK(state != NULL);
    CHECK(morse_flipper_icr_runtime_enter(state, &args, &result));
    result = morse_flipper_icr_runtime_tick(state, args.now_ms);
    CHECK(result.phase == ICR_PHASE_GRAPH_WAIT);
    result = morse_flipper_icr_runtime_tick(state, args.now_ms + ICR_WAIT_MS - 1U);
    CHECK(result.phase == ICR_PHASE_GRAPH_WAIT);
    result = morse_flipper_icr_runtime_tick(state, args.now_ms + ICR_WAIT_MS);
    CHECK(result.phase == ICR_PHASE_PLAYBACK);
    morse_flipper_icr_runtime_free(state);

    trace_timeout(100000U, &ordinary);
    flash_start = UINT32_MAX - 249U - ordinary.timeout_at - ICR_RESULT_MS;
    args.now_ms = flash_start;
    args.rng_seed = 0x1A2B3C4DU;
    state = morse_flipper_icr_runtime_alloc();
    CHECK(state != NULL);
    CHECK(morse_flipper_icr_runtime_enter(state, &args, &result));
    for(uint32_t elapsed = 0U; elapsed <= ordinary.timeout_at + ICR_RESULT_MS; elapsed++)
        result = morse_flipper_icr_runtime_tick(state, flash_start + elapsed);
    CHECK(result.phase == ICR_PHASE_GRAPH_WAIT);
    result = morse_flipper_icr_runtime_tick(
        state, flash_start + ordinary.timeout_at + ICR_RESULT_MS + 250U);
    CHECK(result.phase == ICR_PHASE_GRAPH_WAIT);
    CHECK(result.redraw);
    morse_flipper_icr_runtime_free(state);
}

static InputKey key_for_choice(uint8_t choice) {
    static const InputKey keys[MORSE_FLIPPER_ICR_CHOICE_COUNT] = {
        InputKeyUp,
        InputKeyDown,
        InputKeyLeft,
        InputKeyRight,
        InputKeyOk,
    };

    CHECK(choice < MORSE_FLIPPER_ICR_CHOICE_COUNT);
    return keys[choice];
}

static bool canvas_has_text(const Canvas* canvas, const char* text) {
    uint32_t count = canvas->strings < TEST_CANVAS_TEXT_CAPACITY ? canvas->strings :
                                                                   TEST_CANVAS_TEXT_CAPACITY;

    for(uint32_t i = 0U; i < count; i++) {
        if(strcmp(canvas->text[i], text) == 0) return true;
    }
    return false;
}

static bool canvas_has_box(
    const Canvas* canvas,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    Color color) {
    uint32_t count = canvas->boxes < TEST_CANVAS_BOX_CAPACITY ? canvas->boxes :
                                                                TEST_CANVAS_BOX_CAPACITY;

    for(uint32_t i = 0U; i < count; i++) {
        if(canvas->box_x[i] == x && canvas->box_y[i] == y && canvas->box_width[i] == width &&
           canvas->box_height[i] == height && canvas->box_color[i] == color)
            return true;
    }
    return false;
}

static void test_answer_choice_layout(void) {
    const uint32_t start = 100000U;
    static const uint32_t pressed_black_hash[MORSE_FLIPPER_ICR_CHOICE_COUNT] = {
        0x0517D78DU,
        0xC94475C4U,
        0x282E81B0U,
        0xF074FFF5U,
        0xA0AC7AE4U,
    };
    static const uint32_t pressed_white_hash[MORSE_FLIPPER_ICR_CHOICE_COUNT] = {
        0x61B1B824U,
        0x6666BE6FU,
        0x363F840CU,
        0xD630DF5CU,
        0x201CCF2DU,
    };
    static const int32_t selected_x[MORSE_FLIPPER_ICR_CHOICE_COUNT] = {20, 20, 2, 44, 20};
    static const int32_t selected_y[MORSE_FLIPPER_ICR_CHOICE_COUNT] = {5, 45, 22, 22, 22};
    static const size_t selected_width[MORSE_FLIPPER_ICR_CHOICE_COUNT] = {19, 19, 14, 14, 20};
    static const size_t selected_height[MORSE_FLIPPER_ICR_CHOICE_COUNT] = {14, 14, 19, 19, 20};

    for(uint8_t choice = 0U; choice < MORSE_FLIPPER_ICR_CHOICE_COUNT; choice++) {
        MorseFlipperIcrEnterArgs args = {
            .now_ms = start,
            .rng_seed = 0x55667788U,
        };
        MorseFlipperIcrResult result;
        InputEvent press = {.key = InputKeyOk, .type = InputTypePress};
        InputEvent release = {.key = InputKeyOk, .type = InputTypeRelease};
        InputEvent answer_press = {.key = key_for_choice(choice), .type = InputTypePress};
        InputEvent answer_release = {.key = key_for_choice(choice), .type = InputTypeRelease};
        void* state = morse_flipper_icr_runtime_alloc();
        uint32_t elapsed;
        uint32_t answer_at;

        CHECK(state != NULL);
        CHECK(morse_flipper_icr_runtime_enter(state, &args, &result));
        for(elapsed = 0U; elapsed < ICR_TRACE_MS; elapsed++) {
            result = morse_flipper_icr_runtime_tick(state, start + elapsed);
            if(result.phase == ICR_PHASE_RECOGNITION) break;
        }
        CHECK(result.phase == ICR_PHASE_RECOGNITION);

        result = morse_flipper_icr_runtime_input(state, &press, start + elapsed);
        CHECK(result.phase == ICR_PHASE_RECOGNIZED_HOLD);
        result = morse_flipper_icr_runtime_input(state, &release, start + elapsed + 1U);
        CHECK(result.phase == ICR_PHASE_ANSWER_GUARD);
        if(choice == 0U) {
            Canvas canvas = {0};

            morse_flipper_icr_runtime_draw(state, &canvas, start + elapsed + 1U);
            CHECK(!canvas.full_height_divider);
            CHECK(canvas.lines == 0U);
            CHECK(canvas.bitmaps == 1U);
            CHECK(canvas.bitmap_hash[0] == 0xBDF95AD9U);
            CHECK(canvas.bitmap_x[0] == 2 && canvas.bitmap_y[0] == 5);
            CHECK(canvas.bitmap_width[0] == 56U && canvas.bitmap_height[0] == 54U);
            CHECK(canvas.white_strings == 0U);
            CHECK(canvas.text_y[0] == 13);
            CHECK(canvas.text_y[1] == 51);
            CHECK(canvas.text_y[2] == 32);
            CHECK(canvas.text_y[3] == 32);
            CHECK(canvas.text_y[4] == 32);
            CHECK(canvas_has_text(&canvas, "Press"));
            CHECK(canvas_has_text(&canvas, "the"));
            CHECK(canvas_has_text(&canvas, "joystick"));
            CHECK(canvas_has_text(&canvas, "for"));
            CHECK(canvas_has_text(&canvas, "your"));
            CHECK(canvas_has_text(&canvas, "answer"));
        }

        result = morse_flipper_icr_runtime_tick(state, start + elapsed + 1U + ICR_GUARD_MS);
        CHECK(result.phase == ICR_PHASE_ANSWER);
        answer_at = start + elapsed + 2U + ICR_GUARD_MS;
        result = morse_flipper_icr_runtime_input(state, &answer_press, answer_at);
        CHECK(result.phase == ICR_PHASE_ANSWER);
        CHECK(result.redraw);
        {
            Canvas canvas = {0};

            morse_flipper_icr_runtime_draw(state, &canvas, answer_at);
            CHECK(canvas.bitmaps == 2U);
            CHECK(canvas.bitmap_hash[0] == 0xBDF95AD9U);
            CHECK(canvas.bitmap_hash[1] == pressed_black_hash[choice]);
            CHECK(canvas_has_box(
                &canvas,
                selected_x[choice],
                selected_y[choice],
                selected_width[choice],
                selected_height[choice],
                ColorWhite));
            CHECK(canvas.white_strings == 1U);
            CHECK(canvas_has_text(&canvas, "joystick"));
        }

        result = morse_flipper_icr_runtime_input(state, &answer_release, answer_at + 1U);
        CHECK(result.phase == ICR_PHASE_RESULT);
        {
            Canvas canvas = {0};

            morse_flipper_icr_runtime_draw(state, &canvas, answer_at + 1U);
            CHECK(!canvas.full_height_divider);
            CHECK(canvas.lines == 0U);
            CHECK(canvas.bitmaps == 2U);
            CHECK(canvas.bitmap_hash[0] == 0xBDF95AD9U);
            CHECK(canvas.bitmap_hash[1] == pressed_black_hash[choice]);
            CHECK(canvas.bitmap_x[1] == selected_x[choice]);
            CHECK(canvas.bitmap_y[1] == selected_y[choice]);
            CHECK(canvas.bitmap_width[1] == selected_width[choice]);
            CHECK(canvas.bitmap_height[1] == selected_height[choice]);
            CHECK(canvas_has_box(
                &canvas,
                selected_x[choice],
                selected_y[choice],
                selected_width[choice],
                selected_height[choice],
                ColorWhite));
            CHECK(canvas.white_strings == 1U);
            CHECK(!canvas_has_text(&canvas, "Press"));
            CHECK(!canvas_has_text(&canvas, "joystick"));
            CHECK(!canvas_has_text(&canvas, "answer"));
            CHECK(canvas.dots > 0U);
        }

        result = morse_flipper_icr_runtime_tick(state, answer_at + ICR_PRESS_BLACK_MS);
        CHECK(result.phase == ICR_PHASE_RESULT);
        CHECK(result.redraw);
        {
            Canvas canvas = {0};

            morse_flipper_icr_runtime_draw(state, &canvas, answer_at + ICR_PRESS_BLACK_MS);
            CHECK(canvas.bitmaps == 2U);
            CHECK(canvas.bitmap_hash[0] == 0xBDF95AD9U);
            CHECK(canvas.bitmap_hash[1] == pressed_white_hash[choice]);
            CHECK(canvas_has_box(
                &canvas,
                selected_x[choice],
                selected_y[choice],
                selected_width[choice],
                selected_height[choice],
                ColorWhite));
            CHECK(canvas.white_strings == 0U);
            CHECK(canvas.dots > 0U);
        }

        morse_flipper_icr_runtime_free(state);
    }
}

static void test_graph_bars_are_solid_with_level_gaps(void) {
    MorseFlipperIcrStats stats;
    MorseFlipperIcrEnterArgs args = {.now_ms = 100U, .rng_seed = 0x12345678U};
    MorseFlipperIcrResult result;
    Canvas canvas = {0};
    void* state;

    morse_flipper_icr_stats_reset(&stats);
    stats.avg_ms20[0] = 1U;
    stats.avg_ms20[1] = 40U;
    stats.avg_ms20[2] = 60U;
    CHECK(morse_flipper_icr_stats_save(&stats));

    state = morse_flipper_icr_runtime_alloc();
    CHECK(state != NULL);
    CHECK(morse_flipper_icr_runtime_enter(state, &args, &result));
    CHECK(result.phase == ICR_PHASE_GRAPH_WAIT);
    morse_flipper_icr_runtime_draw(state, &canvas, args.now_ms);

    CHECK(canvas_has_box(&canvas, 4, 16, 2, 44, ColorBlack));
    CHECK(canvas_has_box(&canvas, 4, 26, 2, 1, ColorWhite));
    CHECK(canvas_has_box(&canvas, 4, 48, 2, 1, ColorWhite));
    CHECK(canvas_has_box(&canvas, 7, 38, 2, 22, ColorBlack));
    CHECK(canvas_has_box(&canvas, 7, 48, 2, 1, ColorWhite));
    CHECK(canvas_has_box(&canvas, 10, 50, 2, 10, ColorBlack));
    CHECK(canvas.dots == 74U);
    CHECK(canvas.current_color == ColorBlack);

    morse_flipper_icr_runtime_free(state);
    morse_flipper_icr_stats_reset(&stats);
    CHECK(morse_flipper_icr_stats_save(&stats));
}

static uint32_t recognition_elapsed(uint32_t start, uint32_t seed) {
    MorseFlipperIcrEnterArgs args = {.now_ms = start, .rng_seed = seed};
    MorseFlipperIcrResult result;
    void* state = morse_flipper_icr_runtime_alloc();
    uint32_t elapsed;

    CHECK(state != NULL);
    CHECK(morse_flipper_icr_runtime_enter(state, &args, &result));
    for(elapsed = 0U; elapsed < ICR_TRACE_MS; elapsed++) {
        result = morse_flipper_icr_runtime_tick(state, start + elapsed);
        if(result.phase == ICR_PHASE_RECOGNITION) break;
    }
    CHECK(result.phase == ICR_PHASE_RECOGNITION);
    morse_flipper_icr_runtime_free(state);
    return elapsed;
}

static void test_zero_reaction_start_records_elapsed_time(void) {
    const uint32_t seed = 0x55667788U;
    MorseFlipperIcrStats expected;
    MorseFlipperIcrStats saved;
    uint8_t choices[MORSE_FLIPPER_ICR_CHOICE_COUNT];
    uint32_t rng_state = seed;
    uint32_t elapsed = recognition_elapsed(100000U, seed);
    uint32_t start = 0U - elapsed;
    uint8_t target;
    uint8_t correct_choice = MORSE_FLIPPER_ICR_NO_CHOICE;
    MorseFlipperIcrEnterArgs args = {.now_ms = start, .rng_seed = seed};
    MorseFlipperIcrResult result;
    InputEvent press = {.key = InputKeyOk, .type = InputTypePress};
    InputEvent release = {.key = InputKeyOk, .type = InputTypeRelease};
    InputEvent answer;
    void* state = morse_flipper_icr_runtime_alloc();

    morse_flipper_icr_stats_reset(&expected);
    target =
        morse_flipper_icr_pick_target_except(&expected, &rng_state, MORSE_FLIPPER_ICR_NO_CHOICE);
    morse_flipper_icr_build_choices(&expected, target, &rng_state, choices);
    for(uint8_t i = 0U; i < MORSE_FLIPPER_ICR_CHOICE_COUNT; i++) {
        if(choices[i] == target) {
            correct_choice = i;
            break;
        }
    }

    CHECK(correct_choice != MORSE_FLIPPER_ICR_NO_CHOICE);
    CHECK(state != NULL);
    CHECK(morse_flipper_icr_runtime_enter(state, &args, &result));
    for(uint32_t i = 0U; i <= elapsed; i++)
        result = morse_flipper_icr_runtime_tick(state, start + i);
    CHECK(result.phase == ICR_PHASE_RECOGNITION);
    CHECK(start + elapsed == 0U);

    result = morse_flipper_icr_runtime_input(state, &press, 100U);
    CHECK(result.phase == ICR_PHASE_RECOGNIZED_HOLD);
    result = morse_flipper_icr_runtime_input(state, &release, 101U);
    CHECK(result.phase == ICR_PHASE_ANSWER_GUARD);
    result = morse_flipper_icr_runtime_tick(state, 101U + ICR_GUARD_MS);
    CHECK(result.phase == ICR_PHASE_ANSWER);
    answer = (InputEvent){
        .key = key_for_choice(correct_choice),
        .type = InputTypeRelease,
    };
    result = morse_flipper_icr_runtime_input(state, &answer, 102U + ICR_GUARD_MS);
    CHECK(result.phase == ICR_PHASE_RESULT);
    morse_flipper_icr_runtime_leave(state);
    morse_flipper_icr_runtime_free(state);

    CHECK(morse_flipper_icr_stats_load(&saved));
    CHECK(saved.attempts[target] == 1U);
    CHECK(saved.correct[target] == 1U);
    CHECK(saved.avg_ms20[target] == morse_flipper_icr_reaction_bucket(100U));
}

static void test_settings_reset_confirmation_and_persistence(void) {
    DialogsApp dialogs = {0};
    MorseFlipperIcrEnterArgs args = {
        .now_ms = 100U,
        .entry_kind = MorseFlipperIcrEntrySettings,
        .dialogs = &dialogs,
    };
    MorseFlipperIcrStats stats;
    MorseFlipperIcrResult result;
    void* state = morse_flipper_icr_runtime_alloc();

    morse_flipper_icr_stats_reset(&stats);
    stats.attempts[0] = 4U;
    stats.correct[0] = 3U;
    CHECK(morse_flipper_icr_stats_save(&stats));

    dialog_responses[0] = DialogMessageButtonLeft;
    dialog_response_count = 1U;
    dialog_show_count = 0U;
    CHECK(state != NULL);
    CHECK(morse_flipper_icr_runtime_enter(state, &args, &result));
    CHECK(result.handled && result.request_exit);
    CHECK(dialog_show_count == 1U);
    CHECK(strcmp(dialog_seen[0].header, "ICR") == 0);
    CHECK(strcmp(dialog_seen[0].text, "Reset statistics?") == 0);
    CHECK(strcmp(dialog_seen[0].left, "No") == 0);
    CHECK(strcmp(dialog_seen[0].center, "Yes") == 0);
    CHECK(dialog_seen[0].right == NULL);
    CHECK(morse_flipper_icr_stats_load(&stats));
    CHECK(stats.attempts[0] == 4U && stats.correct[0] == 3U);
    morse_flipper_icr_runtime_free(state);

    dialog_responses[0] = DialogMessageButtonCenter;
    dialog_responses[1] = DialogMessageButtonCenter;
    dialog_response_count = 2U;
    dialog_show_count = 0U;
    state = morse_flipper_icr_runtime_alloc();
    CHECK(state != NULL);
    CHECK(morse_flipper_icr_runtime_enter(state, &args, &result));
    CHECK(result.handled && result.request_exit);
    CHECK(dialog_show_count == 2U);
    CHECK(strcmp(dialog_seen[1].text, "Statistics reset") == 0);
    CHECK(dialog_seen[1].left == NULL);
    CHECK(strcmp(dialog_seen[1].center, "OK") == 0);
    CHECK(dialog_seen[1].right == NULL);
    CHECK(morse_flipper_icr_stats_load(&stats));
    CHECK(morse_flipper_icr_stats_valid(&stats));
    CHECK(stats.attempts[0] == 0U && stats.correct[0] == 0U);
    morse_flipper_icr_runtime_leave(state);
    morse_flipper_icr_runtime_free(state);
    CHECK(morse_flipper_icr_stats_load(&stats));
    CHECK(morse_flipper_icr_stats_valid(&stats) && stats.attempts[0] == 0U);
}

int main(void) {
    char tmp[] = "/tmp/morse_icr_runtime_test_XXXXXX";

    CHECK(mkdtemp(tmp) != NULL);
    CHECK(chdir(tmp) == 0);
    test_timeout_trace_wrap_equivalence();
    test_answer_trace_wrap_equivalence();
    test_zero_deadlines_remain_active();
    test_answer_choice_layout();
    test_graph_bars_are_solid_with_level_gaps();
    test_zero_reaction_start_records_elapsed_time();
    test_settings_reset_confirmation_and_persistence();
    printf("test_icr_runtime: %u checks passed\n", g_checks);
    return 0;
}
