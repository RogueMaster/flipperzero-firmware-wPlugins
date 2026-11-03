#include <furi.h>
#include <furi_hal_random.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>
#include <notification/notification_messages.h>

#define TAG "CubeTimer"

#define SCRAMBLE_LEN 20
#define INSPECTION_MS 15000
#define INSPECTION_DNF_MS 17000 /* WCA: 15-17s is +2, over 17s is DNF */
#define WARN_1_MS 8000 /* "8 seconds" call */
#define WARN_2_MS 12000 /* "12 seconds" call */

#define HISTORY_MAX 12
#define RESULT_NONE 0 /* not enough solves yet */
#define RESULT_DNF UINT32_MAX

#define STATS_VISIBLE_ROWS 6
#define STATS_ROW_HEIGHT 10
#define STATS_MAX_ROWS (4 + HISTORY_MAX / 2)

typedef enum {
    StateScramble,
    StateInspection,
    StateSolving,
    StateDone,
    StateStats,
} TimerState;

typedef enum {
    PenaltyNone,
    PenaltyPlus2,
    PenaltyDnf,
} Penalty;

typedef struct {
    FuriMutex* mutex;
    TimerState state;
    TimerState stats_return; /* screen to go back to when leaving stats */
    uint8_t stats_scroll;
    char scramble[SCRAMBLE_LEN * 3 + 1];
    uint32_t inspection_start;
    uint32_t solve_start;
    uint32_t solve_ms;
    Penalty penalty;
    bool warned_1;
    bool warned_2;

    /* Session results, newest first. Times include +2, DNF is RESULT_DNF */
    uint32_t history[HISTORY_MAX];
    uint8_t history_len;
    uint32_t solve_count;
    uint32_t best_ms;
    uint32_t best_ao5;
    uint32_t best_ao12;
} CubeTimer;

static uint32_t now_ms(void) {
    return (uint32_t)((uint64_t)furi_get_tick() * 1000 / furi_kernel_get_tick_frequency());
}

static void generate_scramble(char* out) {
    static const char faces[] = "UDRLFB"; /* axis = index / 2 */
    static const char* suffix[] = {"", "'", "2"};
    int prev = -1, prev2 = -1;
    out[0] = '\0';

    for(int i = 0; i < SCRAMBLE_LEN; i++) {
        int face;
        while(true) {
            face = furi_hal_random_get() % 6;
            /* No repeating the same face (R R) */
            if(face == prev) continue;
            /* No A B A on the same axis (R L R) */
            if(prev2 >= 0 && face / 2 == prev / 2 && face / 2 == prev2 / 2) continue;
            break;
        }
        prev2 = prev;
        prev = face;

        size_t len = strlen(out);
        snprintf(
            out + len,
            SCRAMBLE_LEN * 3 + 1 - len,
            "%s%c%s",
            i ? " " : "",
            faces[face],
            suffix[furi_hal_random_get() % 3]);
    }
}

static void format_time(char* buf, size_t size, uint32_t ms) {
    uint32_t cs = (ms / 10) % 100;
    uint32_t s = (ms / 1000) % 60;
    uint32_t m = ms / 60000;
    if(m) {
        snprintf(buf, size, "%lu:%02lu.%02lu", m, s, cs);
    } else {
        snprintf(buf, size, "%lu.%02lu", s, cs);
    }
}

/* Like format_time, but handles "no result yet" and DNF */
static void format_result(char* buf, size_t size, uint32_t value) {
    if(value == RESULT_NONE) {
        strlcpy(buf, "--", size);
    } else if(value == RESULT_DNF) {
        strlcpy(buf, "DNF", size);
    } else {
        format_time(buf, size, value);
    }
}

static uint32_t result_ms(CubeTimer* app) {
    return app->solve_ms + (app->penalty == PenaltyPlus2 ? 2000 : 0);
}

/* WCA average of the last n solves: drop best and worst, mean of the rest.
 * More than one DNF makes the whole average a DNF. */
static uint32_t average_of(CubeTimer* app, uint8_t n) {
    if(app->history_len < n) return RESULT_NONE;

    uint32_t sorted[HISTORY_MAX];
    memcpy(sorted, app->history, n * sizeof(uint32_t));
    for(uint8_t i = 1; i < n; i++) {
        uint32_t v = sorted[i];
        int8_t j = i - 1;
        while(j >= 0 && sorted[j] > v) {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = v;
    }

    if(sorted[n - 2] == RESULT_DNF) return RESULT_DNF;

    uint64_t sum = 0;
    for(uint8_t i = 1; i < n - 1; i++)
        sum += sorted[i];
    uint32_t avg = sum / (n - 2);
    /* Round to the nearest hundredth, as displayed */
    return (avg + 5) / 10 * 10;
}

static bool is_better(uint32_t candidate, uint32_t current_best) {
    if(candidate == RESULT_NONE || candidate == RESULT_DNF) return false;
    return current_best == RESULT_NONE || candidate < current_best;
}

static void record_result(CubeTimer* app, uint32_t result) {
    memmove(&app->history[1], &app->history[0], (HISTORY_MAX - 1) * sizeof(uint32_t));
    app->history[0] = result;
    if(app->history_len < HISTORY_MAX) app->history_len++;
    app->solve_count++;

    if(is_better(result, app->best_ms)) app->best_ms = result;
    uint32_t ao5 = average_of(app, 5);
    if(is_better(ao5, app->best_ao5)) app->best_ao5 = ao5;
    uint32_t ao12 = average_of(app, 12);
    if(is_better(ao12, app->best_ao12)) app->best_ao12 = ao12;
}

/* Current ao5 / ao12 as one line, e.g. "ao5 12.34  ao12 --" */
static void format_averages(CubeTimer* app, char* buf, size_t size) {
    char ao5[16], ao12[16];
    format_result(ao5, sizeof(ao5), average_of(app, 5));
    format_result(ao12, sizeof(ao12), average_of(app, 12));
    snprintf(buf, size, "ao5 %s   ao12 %s", ao5, ao12);
}

typedef enum {
    HintGlyphNone,
    HintGlyphRight, /* 4x7 filled arrow */
    HintGlyphDown, /* 7x4 filled arrow */
} HintGlyph;

typedef struct {
    HintGlyph glyph;
    const char* label;
} HintItem;

static int32_t hint_glyph_width(HintGlyph glyph) {
    switch(glyph) {
    case HintGlyphRight:
        return 4 + 2;
    case HintGlyphDown:
        return 7 + 2;
    default:
        return 0;
    }
}

/* Bottom-of-screen hint bar: centered items, each an optional arrow plus a label */
static void draw_hints(Canvas* canvas, const HintItem* items, size_t count) {
    const int32_t baseline = 61;
    const int32_t gap = 7;

    canvas_set_font(canvas, FontSecondary);
    int32_t total = gap * (int32_t)(count - 1);
    for(size_t i = 0; i < count; i++)
        total += hint_glyph_width(items[i].glyph) + canvas_string_width(canvas, items[i].label);

    int32_t x = (128 - total) / 2;
    for(size_t i = 0; i < count; i++) {
        if(items[i].glyph == HintGlyphRight) {
            for(int32_t c = 0; c < 4; c++)
                canvas_draw_line(canvas, x + c, baseline - 7 + c, x + c, baseline - 1 - c);
        } else if(items[i].glyph == HintGlyphDown) {
            for(int32_t r = 0; r < 4; r++)
                canvas_draw_line(canvas, x + r, baseline - 5 + r, x + 6 - r, baseline - 5 + r);
        }
        x += hint_glyph_width(items[i].glyph);
        canvas_draw_str(canvas, x, baseline, items[i].label);
        x += canvas_string_width(canvas, items[i].label) + gap;
    }
}

/* Word-wrap the scramble across lines of the screen width */
static void draw_scramble(Canvas* canvas, const char* scramble, int32_t y) {
    char line[64] = "";
    const char* p = scramble;
    while(*p) {
        const char* end = strchr(p, ' ');
        size_t tok_len = end ? (size_t)(end - p) : strlen(p);

        char candidate[128];
        snprintf(candidate, sizeof(candidate), "%s%s%.*s", line, line[0] ? " " : "", (int)tok_len, p);
        if(line[0] && canvas_string_width(canvas, candidate) > 124) {
            canvas_draw_str_aligned(canvas, 64, y, AlignCenter, AlignTop, line);
            y += 10;
            snprintf(line, sizeof(line), "%.*s", (int)tok_len, p);
        } else {
            strlcpy(line, candidate, sizeof(line));
        }
        p += tok_len;
        while(*p == ' ')
            p++;
    }
    if(line[0]) canvas_draw_str_aligned(canvas, 64, y, AlignCenter, AlignTop, line);
}

typedef struct {
    char left[32];
    char right[32]; /* drawn as a second column, may be empty */
} StatsRow;

static uint8_t build_stats_rows(CubeTimer* app, StatsRow* rows) {
    char a[16], b[16];
    uint8_t count = 0;
    memset(rows, 0, sizeof(StatsRow) * STATS_MAX_ROWS);

    format_result(a, sizeof(a), app->best_ms);
    snprintf(rows[count].left, sizeof(rows[count].left), "Best: %s", a);
    snprintf(rows[count].right, sizeof(rows[count].right), "Solves: %lu", app->solve_count);
    count++;

    format_result(a, sizeof(a), average_of(app, 5));
    format_result(b, sizeof(b), app->best_ao5);
    snprintf(rows[count].left, sizeof(rows[count].left), "ao5: %s", a);
    snprintf(rows[count].right, sizeof(rows[count].right), "best %s", b);
    count++;

    format_result(a, sizeof(a), average_of(app, 12));
    format_result(b, sizeof(b), app->best_ao12);
    snprintf(rows[count].left, sizeof(rows[count].left), "ao12: %s", a);
    snprintf(rows[count].right, sizeof(rows[count].right), "best %s", b);
    count++;

    if(app->history_len == 0) {
        strlcpy(rows[count++].left, "No solves yet", sizeof(rows[0].left));
        return count;
    }

    strlcpy(rows[count++].left, "Recent (newest first):", sizeof(rows[0].left));

    /* Two columns: 1-6 on the left, 7-12 on the right */
    uint8_t list_start = count;
    uint8_t list_rows = app->history_len < HISTORY_MAX / 2 ? app->history_len : HISTORY_MAX / 2;
    for(uint8_t i = 0; i < app->history_len; i++) {
        StatsRow* row = &rows[list_start + i % list_rows];
        char* cell = i < list_rows ? row->left : row->right;
        format_result(a, sizeof(a), app->history[i]);
        snprintf(cell, sizeof(row->left), "%u. %s", i + 1, a);
    }
    return count + list_rows;
}

static void draw_stats(Canvas* canvas, CubeTimer* app) {
    StatsRow rows[STATS_MAX_ROWS];
    uint8_t total = build_stats_rows(app, rows);

    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < STATS_VISIBLE_ROWS && app->stats_scroll + i < total; i++) {
        StatsRow* row = &rows[app->stats_scroll + i];
        int32_t y = 1 + i * STATS_ROW_HEIGHT;
        canvas_draw_str_aligned(canvas, 1, y, AlignLeft, AlignTop, row->left);
        if(row->right[0]) canvas_draw_str_aligned(canvas, 66, y, AlignLeft, AlignTop, row->right);
    }
    if(total > STATS_VISIBLE_ROWS) {
        elements_scrollbar(canvas, app->stats_scroll, total - STATS_VISIBLE_ROWS + 1);
    }
}

static uint8_t stats_max_scroll(CubeTimer* app) {
    StatsRow rows[STATS_MAX_ROWS];
    uint8_t total = build_stats_rows(app, rows);
    return total > STATS_VISIBLE_ROWS ? total - STATS_VISIBLE_ROWS : 0;
}

static void draw_callback(Canvas* canvas, void* ctx) {
    CubeTimer* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);

    char buf[48];
    canvas_clear(canvas);

    switch(app->state) {
    case StateScramble:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "Scramble");
        canvas_set_font(canvas, FontSecondary);
        draw_scramble(canvas, app->scramble, 13);
        if(app->solve_count) {
            format_averages(app, buf, sizeof(buf));
            canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignTop, buf);
        }
        draw_hints(
            canvas,
            (HintItem[]){
                {HintGlyphNone, "OK:inspect"},
                {HintGlyphRight, "new"},
                {HintGlyphDown, "stats"},
            },
            3);
        break;

    case StateInspection: {
        uint32_t elapsed = now_ms() - app->inspection_start;
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "Inspection");
        if(elapsed < INSPECTION_MS) {
            uint32_t left = (INSPECTION_MS - elapsed + 999) / 1000;
            snprintf(buf, sizeof(buf), "%lu", left);
            canvas_set_font(canvas, FontBigNumbers);
            canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, buf);
        } else {
            canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "+2 penalty!");
        }
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 61, AlignCenter, AlignBottom, "OK: start solve");
        break;
    }

    case StateSolving:
        format_time(buf, sizeof(buf), now_ms() - app->solve_start);
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, buf);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 61, AlignCenter, AlignBottom, "Any key: stop");
        break;

    case StateDone:
        if(app->penalty == PenaltyDnf) {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "DNF");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(
                canvas, 64, 34, AlignCenter, AlignCenter, "Inspection over 17s");
        } else {
            format_time(buf, sizeof(buf), result_ms(app));
            canvas_set_font(canvas, FontBigNumbers);
            canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, buf);
            canvas_set_font(canvas, FontSecondary);
            if(app->penalty == PenaltyPlus2) {
                char raw[16];
                format_time(raw, sizeof(raw), app->solve_ms);
                snprintf(buf, sizeof(buf), "(%s +2)", raw);
                canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, buf);
            }
        }
        canvas_set_font(canvas, FontSecondary);
        format_averages(app, buf, sizeof(buf));
        canvas_draw_str_aligned(canvas, 64, 47, AlignCenter, AlignCenter, buf);
        draw_hints(
            canvas,
            (HintItem[]){
                {HintGlyphNone, "OK:next"},
                {HintGlyphDown, "stats"},
            },
            2);
        break;

    case StateStats:
        draw_stats(canvas, app);
        break;
    }

    furi_mutex_release(app->mutex);
}

static void input_callback(InputEvent* event, void* ctx) {
    FuriMessageQueue* queue = ctx;
    furi_message_queue_put(queue, event, FuriWaitForever);
}

static void new_scramble(CubeTimer* app) {
    generate_scramble(app->scramble);
    app->state = StateScramble;
    FURI_LOG_I(TAG, "Scramble: %s", app->scramble);
}

static void finish_solve(CubeTimer* app) {
    app->state = StateDone;
    record_result(app, app->penalty == PenaltyDnf ? RESULT_DNF : result_ms(app));
}

static void open_stats(CubeTimer* app) {
    app->stats_return = app->state;
    app->stats_scroll = 0;
    app->state = StateStats;
}

int32_t cube_timer_app(void* p) {
    UNUSED(p);

    CubeTimer* app = malloc(sizeof(CubeTimer));
    memset(app, 0, sizeof(CubeTimer));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    new_scramble(app);

    FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, app);
    view_port_input_callback_set(view_port, input_callback, queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);
    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);

    InputEvent event;
    bool running = true;
    while(running) {
        /* Poll fast while a clock is running so the display stays live */
        bool active = app->state == StateInspection || app->state == StateSolving;
        FuriStatus status = furi_message_queue_get(queue, &event, active ? 30 : 500);

        furi_mutex_acquire(app->mutex, FuriWaitForever);

        if(app->state == StateInspection) {
            uint32_t elapsed = now_ms() - app->inspection_start;
            if(!app->warned_1 && elapsed >= WARN_1_MS) {
                app->warned_1 = true;
                notification_message(notifications, &sequence_single_vibro);
            }
            if(!app->warned_2 && elapsed >= WARN_2_MS) {
                app->warned_2 = true;
                notification_message(notifications, &sequence_double_vibro);
            }
            if(elapsed >= INSPECTION_DNF_MS) {
                app->penalty = PenaltyDnf;
                finish_solve(app);
                notification_message(notifications, &sequence_display_backlight_enforce_auto);
                notification_message(notifications, &sequence_error);
            }
        }

        if(status == FuriStatusOk && event.type == InputTypePress) {
            switch(app->state) {
            case StateScramble:
                if(event.key == InputKeyOk) {
                    app->state = StateInspection;
                    app->inspection_start = now_ms();
                    app->warned_1 = false;
                    app->warned_2 = false;
                    app->penalty = PenaltyNone;
                    notification_message(notifications, &sequence_display_backlight_enforce_on);
                } else if(event.key == InputKeyRight) {
                    new_scramble(app);
                } else if(event.key == InputKeyDown) {
                    open_stats(app);
                } else if(event.key == InputKeyBack) {
                    running = false;
                }
                break;

            case StateInspection:
                if(event.key == InputKeyOk) {
                    uint32_t elapsed = now_ms() - app->inspection_start;
                    app->penalty = elapsed >= INSPECTION_MS ? PenaltyPlus2 : PenaltyNone;
                    app->state = StateSolving;
                    app->solve_start = now_ms();
                    notification_message(notifications, &sequence_single_vibro);
                } else if(event.key == InputKeyBack) {
                    app->state = StateScramble;
                    notification_message(notifications, &sequence_display_backlight_enforce_auto);
                }
                break;

            case StateSolving:
                if(event.key == InputKeyBack) {
                    /* Cancel the solve without recording it */
                    app->state = StateScramble;
                } else {
                    app->solve_ms = now_ms() - app->solve_start;
                    finish_solve(app);
                    char t[16];
                    format_time(t, sizeof(t), result_ms(app));
                    FURI_LOG_I(TAG, "Solve: %s", t);
                    notification_message(notifications, &sequence_success);
                }
                notification_message(notifications, &sequence_display_backlight_enforce_auto);
                break;

            case StateDone:
                if(event.key == InputKeyOk || event.key == InputKeyBack) {
                    new_scramble(app);
                } else if(event.key == InputKeyDown) {
                    open_stats(app);
                }
                break;

            case StateStats:
                if(event.key == InputKeyDown) {
                    if(app->stats_scroll < stats_max_scroll(app)) app->stats_scroll++;
                } else if(event.key == InputKeyUp) {
                    if(app->stats_scroll > 0) app->stats_scroll--;
                } else if(event.key == InputKeyBack || event.key == InputKeyOk) {
                    app->state = app->stats_return;
                }
                break;
            }
        }

        furi_mutex_release(app->mutex);
        view_port_update(view_port);
    }

    notification_message(notifications, &sequence_display_backlight_enforce_auto);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(queue);
    furi_mutex_free(app->mutex);
    free(app);

    return 0;
}
