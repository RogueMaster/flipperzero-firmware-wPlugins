#include <furi.h>
#include <gui/view.h>
#include <gui/elements.h>
#include <gui/modules/variable_item_list.h>
#include <notification/notification.h>
#include <stratahero_icons.h>

#include "constants.h"
#include "types.h"
#include "stratagems.h"
#include "glyphs.h"
#include "notifications.h"
#include "settings.h"
#include "quiz.h"

#define MAX_QUIZ_ERRORS      0
#define QUIZ_OPTIONS_COUNT   4
#define QUIZ_ANSWER_HOLD_MS  2000
#define QUIZ_RESULTS_HOLD_MS 8000
#define QUIZ_CODE_Y          1
#define QUIZ_OPTIONS_Y       16
#define QUIZ_OPTION_COL_W    (SCREEN_WIDTH / QUIZ_OPTIONS_COUNT) // 32
#define QUIZ_TIME_BAR_Y      63
#define QUIZ_OPTION_H        (QUIZ_TIME_BAR_Y - QUIZ_OPTIONS_Y - 1) // 46

#define QUIZ_TIME_BAR_REFRESH_MS 100

static const uint32_t time_limit_values_ms[] = {0, 10000, 15000, 20000};
static const char* const time_limit_labels[] = {"No limit", "10s", "15s", "20s"};
#define TIME_LIMIT_COUNT 4

// Display column → option index: Left(2), Up(0), Down(3), Right(1)
static const int col_to_option[] = {2, 0, 3, 1};

typedef enum {
    QuizView_Question,
    QuizView_Answer,
    QuizView_Results,
} QuizViewState;

typedef struct {
    QuizViewState view_state;

    // Settings (synced from VariableItemList)
    int time_limit_index; // 0-3
    int max_errors;       // 0=no limit, 1-9

    // Time bar
    uint32_t question_start_tick;
    uint32_t time_limit_ticks; // 0 = no limit

    // Current question
    const Stratagem* options[QUIZ_OPTIONS_COUNT]; // 0=Up, 1=Right, 2=Left, 3=Down
    int correct_option;  // 0-3
    int selected_option; // -1=timeout/none, 0-3

    // Score
    int errors_remaining;
    int rounds_played;
    int correct_answers;
} QuizWidgetModel;

struct QuizWidget {
    View* view;
    VariableItemList* settings_list;
    VariableItem* time_limit_item;
    VariableItem* max_errors_item;
    NotificationApp* notification;
    StrataHeroSettings settings;
    FuriTimer* answer_timer;
    FuriTimer* results_timer;
    FuriTimer* time_limit_timer;
    FuriTimer* time_bar_timer;
    QuizStartCallback start_callback;
    void* start_callback_context;
    QuizWidgetNavigationCallback navigation_callback;
    void* navigation_callback_context;
};


// Forward declarations
static void quiz_generate_question(QuizWidget* widget);


static void quiz_navigate_back(QuizWidget* widget) {
    furi_timer_stop(widget->answer_timer);
    furi_timer_stop(widget->results_timer);
    furi_timer_stop(widget->time_limit_timer);
    furi_timer_stop(widget->time_bar_timer);
    if(widget->navigation_callback) {
        widget->navigation_callback(widget->navigation_callback_context);
    }
}

static void quiz_go_to_results(QuizWidget* widget) {
    furi_timer_stop(widget->answer_timer);
    furi_timer_stop(widget->time_limit_timer);
    furi_timer_stop(widget->time_bar_timer);
    with_view_model(widget->view, QuizWidgetModel* model, {
        model->view_state = QuizView_Results;
    }, true);
    furi_timer_start(widget->results_timer, QUIZ_RESULTS_HOLD_MS);
}

static void quiz_process_selection(QuizWidget* widget, int option_index) {
    bool is_correct = false;
    bool game_over = false;

    with_view_model(widget->view, QuizWidgetModel* model, {
        furi_timer_stop(widget->time_limit_timer);
        furi_timer_stop(widget->time_bar_timer);
        model->selected_option = option_index;
        model->view_state = QuizView_Answer;
        model->rounds_played++;
        is_correct = (option_index >= 0) && (option_index == model->correct_option);
        if(is_correct) {
            model->correct_answers++;
        } else if(model->max_errors > 0) {
            model->errors_remaining--;
        }
        game_over = (model->max_errors > 0) && (model->errors_remaining <= 0);
    }, true);

    if(is_correct) {
        stratahero_code_complete_notification(widget->notification, &widget->settings);
    } else {
        stratahero_code_glyph_entry_failure_notification(widget->notification, &widget->settings);
    }

    furi_timer_start(widget->answer_timer, QUIZ_ANSWER_HOLD_MS);
    UNUSED(game_over);
}


// Timer callbacks

static void quiz_answer_timer_callback(void* context) {
    QuizWidget* widget = context;
    bool game_over = false;
    with_view_model(widget->view, QuizWidgetModel* model, {
        game_over = (model->max_errors > 0) && (model->errors_remaining <= 0);
    }, false);
    if(game_over) {
        quiz_go_to_results(widget);
    } else {
        quiz_generate_question(widget);
    }
}

static void quiz_results_timer_callback(void* context) {
    quiz_navigate_back(context);
}

static void quiz_time_limit_timer_callback(void* context) {
    quiz_process_selection(context, -1);
}

static void quiz_time_bar_timer_callback(void* context) {
    QuizWidget* widget = context;
    with_view_model(widget->view, QuizWidgetModel* model, {
        UNUSED(model);
    }, true);
}


// Question generation

static void quiz_generate_question(QuizWidget* widget) {
    int time_limit_index;
    with_view_model(widget->view, QuizWidgetModel* model, {
        time_limit_index = model->time_limit_index;
    }, false);

    // Pick QUIZ_OPTIONS_COUNT unique stratagems
    const Stratagem* chosen[QUIZ_OPTIONS_COUNT] = {NULL};
    int chosen_count = 0;
    int retries = 200;

    while(chosen_count < QUIZ_OPTIONS_COUNT && retries-- > 0) {
        const Stratagem* candidate = stratagems[rand() % stratagems_count];
        if(!candidate || !candidate->icon) continue;
        bool dup = false;
        for(int j = 0; j < chosen_count; j++) {
            if(chosen[j] == candidate || chosen[j]->icon == candidate->icon) { dup = true; break; }
        }
        if(!dup) chosen[chosen_count++] = candidate;
    }
    // Fill gaps (very unlikely edge case) — scan in order, skip no-icon, already chosen, or duplicate icon
    for(int i = chosen_count; i < QUIZ_OPTIONS_COUNT; i++) {
        for(uint32_t j = 0; j < stratagems_count; j++) {
            const Stratagem* candidate = stratagems[j];
            if(!candidate->icon) continue;
            bool dup = false;
            for(int k = 0; k < i; k++) {
                if(chosen[k] == candidate || chosen[k]->icon == candidate->icon) { dup = true; break; }
            }
            if(!dup) { chosen[i] = candidate; break; }
        }
    }

    int correct = rand() % QUIZ_OPTIONS_COUNT;
    uint32_t limit_ms = time_limit_values_ms[time_limit_index];
    uint32_t limit_ticks = limit_ms; // 1 tick = 1ms on Flipper

    with_view_model(widget->view, QuizWidgetModel* model, {
        for(int i = 0; i < QUIZ_OPTIONS_COUNT; i++) model->options[i] = chosen[i];
        model->correct_option = correct;
        model->selected_option = -1;
        model->view_state = QuizView_Question;
        model->time_limit_ticks = limit_ticks;
        model->question_start_tick = furi_get_tick();
    }, true);

    if(limit_ms > 0) {
        furi_timer_start(widget->time_limit_timer, limit_ms);
        furi_timer_start(widget->time_bar_timer, QUIZ_TIME_BAR_REFRESH_MS);
    }
}


// Draw helpers

static const Icon* quiz_button_icon(int option_index) {
    switch(option_index) {
        case 0: return &I_up_button;
        case 1: return &I_right_button;
        case 2: return &I_left_button;
        case 3: return &I_down_button;
        default: return &I_up_button;
    }
}

static void quiz_draw_option(
    Canvas* canvas,
    int col,
    int option_idx,
    const Stratagem* stratagem,
    bool highlight,
    bool outline
) {
    int cx = col * QUIZ_OPTION_COL_W;

    if(highlight) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rbox(canvas, cx + 1, QUIZ_OPTIONS_Y + 1, QUIZ_OPTION_COL_W - 2, QUIZ_OPTION_H - 2, 3);
        canvas_set_color(canvas, ColorWhite);
    } else if(outline) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rframe(canvas, cx + 1, QUIZ_OPTIONS_Y + 1, QUIZ_OPTION_COL_W - 2, QUIZ_OPTION_H - 2, 3);
    }

    // Directional button icon at top of cell
    const Icon* btn = quiz_button_icon(option_idx);
    int btn_w = icon_get_width(btn);
    int btn_h = icon_get_height(btn);
    canvas_draw_icon(canvas, cx + (QUIZ_OPTION_COL_W - btn_w) / 2, QUIZ_OPTIONS_Y + 3, btn);

    // Stratagem icon below button icon
    const Icon* strat = stratagem->icon ? stratagem->icon : &I_no_icon_stratagem;
    int strat_w = icon_get_width(strat);
    int strat_h = icon_get_height(strat);
    int strat_y = QUIZ_OPTIONS_Y + 3 + btn_h + 2;
    int max_strat_y = QUIZ_OPTIONS_Y + QUIZ_OPTION_H - strat_h;
    if(strat_y > max_strat_y) strat_y = max_strat_y;
    canvas_draw_icon(canvas, cx + (QUIZ_OPTION_COL_W - strat_w) / 2, strat_y, strat);

    if(highlight) canvas_set_color(canvas, ColorBlack);
}

static void quiz_draw_question_screen(Canvas* canvas, QuizWidgetModel* model, bool in_answer) {
    // Code glyphs centered at top
    const Stratagem* correct_strat = model->options[model->correct_option];
    if(correct_strat) {
        const char* code = correct_strat->code;
        int code_len = strlen(code);
        int code_x = (SCREEN_WIDTH - CODE_GLYPH_WIDTH * code_len) / 2;
        if(code_x < 0) code_x = 0;
        for(int i = 0; i < code_len; i++) {
            const StrataHeroCodeGlyph* glyph = stratahero_get_code_glyph(code[i]);
            if(glyph) canvas_draw_icon(canvas, code_x + i * CODE_GLYPH_WIDTH, QUIZ_CODE_Y, glyph->black);
        }
    }

    // Errors remaining (top-right) — hidden when no limit
    if(model->max_errors > 0) {
        char err_buf[4];
        snprintf(err_buf, sizeof(err_buf) - 1, "%d", model->errors_remaining);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, SCREEN_WIDTH - 2, QUIZ_CODE_Y, AlignRight, AlignTop, err_buf);
    }

    // Options in a single row
    bool answer_correct = in_answer && (model->selected_option >= 0) &&
                          (model->selected_option == model->correct_option);
    bool timeout = in_answer && (model->selected_option < 0);
    bool answer_wrong = in_answer && !answer_correct && !timeout;

    for(int col = 0; col < QUIZ_OPTIONS_COUNT; col++) {
        int opt = col_to_option[col];
        if(!model->options[opt]) continue;

        bool highlight = false;
        bool outline = false;
        bool hidden = false;

        if(in_answer) {
            if(answer_correct) {
                highlight = (opt == model->selected_option);
            } else if(timeout) {
                outline = (opt == model->correct_option);
                hidden = (opt != model->correct_option);
            } else if(answer_wrong) {
                highlight = (opt == model->selected_option);
                outline = (opt == model->correct_option) && (opt != model->selected_option);
                hidden = (opt != model->selected_option) && (opt != model->correct_option);
            }
        }

        if(!hidden) {
            quiz_draw_option(canvas, col, opt, model->options[opt], highlight, outline);
        }
    }

    // Time bar at very bottom (Question state only)
    if(!in_answer && model->time_limit_ticks > 0) {
        uint32_t elapsed = furi_get_tick() - model->question_start_tick;
        if(elapsed < model->time_limit_ticks) {
            int bar_w = (int)((model->time_limit_ticks - elapsed) * SCREEN_WIDTH / model->time_limit_ticks);
            if(bar_w > 0) {
                canvas_draw_line(canvas, 0, QUIZ_TIME_BAR_Y, bar_w - 1, QUIZ_TIME_BAR_Y);
            }
        }
    }
}

static void quiz_draw_results(Canvas* canvas, QuizWidgetModel* model) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, SCREEN_WIDTH / 2, 6, AlignCenter, AlignTop, "Quiz Results");

    canvas_set_font(canvas, FontSecondary);
    char buf[32];
    snprintf(buf, sizeof(buf) - 1, "Rounds: %d", model->rounds_played);
    canvas_draw_str_aligned(canvas, SCREEN_WIDTH / 2, 26, AlignCenter, AlignTop, buf);
    snprintf(buf, sizeof(buf) - 1, "Correct: %d", model->correct_answers);
    canvas_draw_str_aligned(canvas, SCREEN_WIDTH / 2, 38, AlignCenter, AlignTop, buf);
}

static void quiz_draw_callback(Canvas* canvas, void* _model) {
    QuizWidgetModel* model = _model;
    canvas_clear(canvas);
    switch(model->view_state) {
        case QuizView_Question:
            quiz_draw_question_screen(canvas, model, false);
            break;
        case QuizView_Answer:
            quiz_draw_question_screen(canvas, model, true);
            break;
        case QuizView_Results:
            quiz_draw_results(canvas, model);
            break;
    }
}


// VariableItemList callbacks

static void quiz_time_limit_changed(VariableItem* item) {
    QuizWidget* widget = variable_item_get_context(item);
    int index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, time_limit_labels[index]);
    with_view_model(widget->view, QuizWidgetModel* model, {
        model->time_limit_index = index;
    }, false);
}

static void quiz_max_errors_changed(VariableItem* item) {
    QuizWidget* widget = variable_item_get_context(item);
    int index = variable_item_get_current_value_index(item);
    if(index == 0) {
        variable_item_set_current_value_text(item, "No limit");
    } else {
        char buf[4];
        snprintf(buf, sizeof(buf), "%d", index);
        variable_item_set_current_value_text(item, buf);
    }
    with_view_model(widget->view, QuizWidgetModel* model, {
        model->max_errors = index; // 0 = no limit, 1-9 = attempt count
    }, false);
}

static void quiz_settings_enter_callback(void* context, uint32_t index) {
    QuizWidget* widget = context;
    if(index == 2) { // "Start Quiz" item
        with_view_model(widget->view, QuizWidgetModel* model, {
            model->errors_remaining = model->max_errors;
            model->rounds_played = 0;
            model->correct_answers = 0;
        }, false);
        if(widget->start_callback) {
            widget->start_callback(widget->start_callback_context);
        }
    }
}


// Input callback

static bool quiz_input_callback(InputEvent* event, void* context) {
    QuizWidget* widget = context;
    if(event->type != InputTypeShort) return false;

    if(event->key == InputKeyBack) {
        quiz_navigate_back(widget);
        return true;
    }

    QuizViewState view_state;
    with_view_model(widget->view, QuizWidgetModel* model, {
        view_state = model->view_state;
    }, false);

    switch(view_state) {
        case QuizView_Question: {
            int option = -1;
            switch(event->key) {
                case InputKeyUp:    option = 0; break;
                case InputKeyRight: option = 1; break;
                case InputKeyLeft:  option = 2; break;
                case InputKeyDown:  option = 3; break;
                default: break;
            }
            if(option >= 0) {
                quiz_process_selection(widget, option);
                return true;
            }
            return false;
        }

        case QuizView_Answer:
            return false;

        case QuizView_Results:
            quiz_navigate_back(widget);
            return true;

        default:
            return false;
    }
}


// Lifecycle callbacks

static void quiz_enter_callback(void* context) {
    QuizWidget* widget = context;
    quiz_generate_question(widget);
}

static void quiz_exit_callback(void* context) {
    QuizWidget* widget = context;
    furi_timer_stop(widget->answer_timer);
    furi_timer_stop(widget->results_timer);
    furi_timer_stop(widget->time_limit_timer);
    furi_timer_stop(widget->time_bar_timer);
}


// Public API

QuizWidget* quiz_widget_alloc() {
    QuizWidget* widget = malloc(sizeof(QuizWidget));

    widget->view = view_alloc();
    view_set_context(widget->view, widget);
    view_set_draw_callback(widget->view, quiz_draw_callback);
    view_set_input_callback(widget->view, quiz_input_callback);
    view_set_enter_callback(widget->view, quiz_enter_callback);
    view_set_exit_callback(widget->view, quiz_exit_callback);
    view_allocate_model(widget->view, ViewModelTypeLockFree, sizeof(QuizWidgetModel));

    widget->settings_list = variable_item_list_alloc();

    widget->time_limit_item = variable_item_list_add(
        widget->settings_list, "Time Limit", TIME_LIMIT_COUNT, quiz_time_limit_changed, widget);
    variable_item_set_current_value_index(widget->time_limit_item, 0);
    variable_item_set_current_value_text(widget->time_limit_item, time_limit_labels[0]);

    widget->max_errors_item = variable_item_list_add(
        widget->settings_list, "Max Attempts", 10, quiz_max_errors_changed, widget);
    variable_item_set_current_value_index(widget->max_errors_item, MAX_QUIZ_ERRORS); // index = count (0=No limit)
    char err_text[4];
    snprintf(err_text, sizeof(err_text), "%d", MAX_QUIZ_ERRORS);
    variable_item_set_current_value_text(widget->max_errors_item, err_text);

    variable_item_list_add(widget->settings_list, "Start Quiz", 0, NULL, NULL);
    variable_item_list_set_enter_callback(
        widget->settings_list, quiz_settings_enter_callback, widget);

    widget->answer_timer = furi_timer_alloc(quiz_answer_timer_callback, FuriTimerTypeOnce, widget);
    widget->results_timer =
        furi_timer_alloc(quiz_results_timer_callback, FuriTimerTypeOnce, widget);
    widget->time_limit_timer =
        furi_timer_alloc(quiz_time_limit_timer_callback, FuriTimerTypeOnce, widget);
    widget->time_bar_timer =
        furi_timer_alloc(quiz_time_bar_timer_callback, FuriTimerTypePeriodic, widget);
    widget->notification = furi_record_open(RECORD_NOTIFICATION);
    widget->start_callback = NULL;
    widget->start_callback_context = NULL;
    widget->navigation_callback = NULL;
    widget->navigation_callback_context = NULL;

    with_view_model(widget->view, QuizWidgetModel* model, {
        model->view_state = QuizView_Question;
        model->time_limit_index = 0;
        model->max_errors = MAX_QUIZ_ERRORS;
        model->time_limit_ticks = 0;
        model->question_start_tick = 0;
        model->errors_remaining = MAX_QUIZ_ERRORS;
        model->rounds_played = 0;
        model->correct_answers = 0;
        model->correct_option = 0;
        model->selected_option = -1;
        for(int i = 0; i < QUIZ_OPTIONS_COUNT; i++) model->options[i] = NULL;
    }, false);

    return widget;
}

void quiz_widget_free(QuizWidget* widget) {
    furi_timer_stop(widget->answer_timer);
    furi_timer_free(widget->answer_timer);
    furi_timer_stop(widget->results_timer);
    furi_timer_free(widget->results_timer);
    furi_timer_stop(widget->time_limit_timer);
    furi_timer_free(widget->time_limit_timer);
    furi_timer_stop(widget->time_bar_timer);
    furi_timer_free(widget->time_bar_timer);
    furi_record_close(RECORD_NOTIFICATION);
    variable_item_list_free(widget->settings_list);
    view_free(widget->view);
    free(widget);
}

View* quiz_widget_get_view(QuizWidget* widget) {
    return widget->view;
}

View* quiz_widget_get_settings_view(QuizWidget* widget) {
    return variable_item_list_get_view(widget->settings_list);
}

void quiz_widget_set_settings(QuizWidget* widget, const StrataHeroSettings* settings) {
    widget->settings = *settings;
}

void quiz_widget_set_start_callback(
    QuizWidget* widget,
    QuizStartCallback callback,
    void* context
) {
    widget->start_callback = callback;
    widget->start_callback_context = context;
}

void quiz_widget_set_navigation_callback(
    QuizWidget* widget,
    QuizWidgetNavigationCallback callback,
    void* context
) {
    widget->navigation_callback = callback;
    widget->navigation_callback_context = context;
}
