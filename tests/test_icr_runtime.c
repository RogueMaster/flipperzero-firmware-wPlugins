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
    (void)text;
}

void canvas_draw_dot(Canvas* canvas, int32_t x, int32_t y) {
    (void)x;
    (void)y;
    canvas->dots++;
}

void canvas_draw_box(Canvas* canvas, int32_t x, int32_t y, int32_t width, int32_t height) {
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    canvas->boxes++;
}

void canvas_draw_line(Canvas* canvas, int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    (void)canvas;
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
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
    test_zero_reaction_start_records_elapsed_time();
    test_settings_reset_confirmation_and_persistence();
    printf("test_icr_runtime: %u checks passed\n", g_checks);
    return 0;
}
