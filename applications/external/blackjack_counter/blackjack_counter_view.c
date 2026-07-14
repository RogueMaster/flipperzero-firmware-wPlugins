#include "blackjack_counter.h"

struct BlackjackCounterView {
    View* view;
};

typedef struct {
    int running_count;
    int side_count;
    int decks_remaining;
    BlackjackCounterMethod method;
} BlackjackCounterViewModel;

static void vibration_hot_count() {
    furi_hal_vibro_on(true);
    furi_delay_ms(200);
    furi_hal_vibro_on(false);
}

static void blackjack_counter_view_draw_callback(Canvas* canvas, void* _model) {
    BlackjackCounterViewModel* model = _model;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Title
    const char* titles[] = {
        "Hi-Lo Classic",
        "Hi-Opt I",
        "Hi-Opt II",
        "Omega II",
        "", // True Count - no title to save space
        "Hi-Lo Side",
        "Hi-Opt I Side",
        "", // Hi-Opt II Side - no title to save space
    };

    canvas_set_font(canvas, FontBatteryPercent);
    if(strlen(titles[model->method]) > 0) {
        canvas_draw_str(canvas, 2, 7, titles[model->method]);
    }

    // Running Count - position adjusted for methods without title
    char rc_str[32];
    snprintf(rc_str, sizeof(rc_str), "RC: %d", model->running_count);
    canvas_set_font(canvas, FontPrimary);
    int rc_y = (strlen(titles[model->method]) > 0) ? 22 : 12;
    canvas_draw_str(canvas, 5, rc_y, rc_str);

    // Side count or True count - layout migliorato
    if(model->method == BlackjackCounterMethodTrueCount) {
        char tc_str[32];
        if(model->decks_remaining > 0) {
            double true_count = (double)model->running_count / (double)model->decks_remaining;
            snprintf(tc_str, sizeof(tc_str), "TC: %.1f", true_count);
        } else {
            snprintf(tc_str, sizeof(tc_str), "TC: N/A");
        }
        canvas_draw_str(canvas, 70, rc_y, tc_str); // TC sulla stessa riga di RC

        char decks_str[32];
        snprintf(decks_str, sizeof(decks_str), "Decks: %d", model->decks_remaining);
        canvas_set_font(canvas, FontBatteryPercent);
        canvas_draw_str(canvas, 5, rc_y + 13, decks_str);
    } else if(model->method >= BlackjackCounterMethodHiLoSide) {
        char sc_str[32];
        snprintf(sc_str, sizeof(sc_str), "Aces: %d", model->side_count);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 70, rc_y, sc_str); // Aces sulla stessa riga di RC
    }

    // Instructions - posizioni aggiustate per il nuovo layout
    canvas_set_font(canvas, FontBatteryPercent);
    int y_start;
    if(strlen(titles[model->method]) > 0) {
        // Methods with title
        y_start = (model->method == BlackjackCounterMethodTrueCount) ? 45 :
                  (model->method >= BlackjackCounterMethodHiLoSide)  ? 35 :
                                                                       35;
    } else {
        // Methods without title (True Count and Hi-Opt II Side)
        y_start = (model->method == BlackjackCounterMethodTrueCount) ? 35 : 25;
    }

    switch(model->method) {
    case BlackjackCounterMethodHiLo:
    case BlackjackCounterMethodTrueCount:
    case BlackjackCounterMethodHiLoSide:
        canvas_draw_str(canvas, 2, y_start, "Up: 2-6 (+1)");
        canvas_draw_str(canvas, 2, y_start + 8, "Ok: 7,8,9 (0)");
        canvas_draw_str(canvas, 2, y_start + 16, "Down: 10-K,A (-1)");
        if(model->method == BlackjackCounterMethodTrueCount) {
            canvas_draw_str(canvas, 2, y_start + 24, "Left/Right: Decks");
        } else if(model->method == BlackjackCounterMethodHiLoSide) {
            canvas_draw_str(canvas, 2, y_start + 24, "Right: Ace (side)");
        }
        break;

    case BlackjackCounterMethodHiOpt1:
    case BlackjackCounterMethodHiOpt1Side:
        canvas_draw_str(canvas, 2, y_start, "Up: 3-6 (+1)");
        canvas_draw_str(canvas, 2, y_start + 8, "Ok: 2,7,8,9 (0)");
        canvas_draw_str(canvas, 2, y_start + 16, "Down: 10-K (-1)");
        if(model->method == BlackjackCounterMethodHiOpt1Side) {
            canvas_draw_str(canvas, 2, y_start + 24, "Right: Ace (side)");
        }
        break;

    case BlackjackCounterMethodHiOpt2:
    case BlackjackCounterMethodHiOpt2Side:
        canvas_draw_str(canvas, 2, y_start, "Up: 2,3,6,7 (+1)");
        canvas_draw_str(canvas, 2, y_start + 8, "Right: 4,5 (+2)");
        canvas_draw_str(canvas, 2, y_start + 16, "Ok: 8,9,A (0)");
        canvas_draw_str(canvas, 2, y_start + 24, "Down: 10-K (-2)");
        if(model->method == BlackjackCounterMethodHiOpt2Side) {
            canvas_draw_str(canvas, 2, y_start + 32, "Left: Ace (side)");
        }
        break;

    case BlackjackCounterMethodOmega2:
        canvas_draw_str(canvas, 2, y_start, "Up: 2,3,7 (+1)");
        canvas_draw_str(canvas, 2, y_start + 8, "Right: 4,5,6 (+2)");
        canvas_draw_str(canvas, 2, y_start + 16, "Ok: 9 (-1)");
        canvas_draw_str(canvas, 2, y_start + 24, "Down: 10-K (-2)");
        break;
    }
}

static bool blackjack_counter_view_input_callback(InputEvent* event, void* context) {
    BlackjackCounterView* counter_view = context;
    bool consumed = false;

    if(event->type == InputTypePress || event->type == InputTypeRepeat) {
        with_view_model(
            counter_view->view,
            BlackjackCounterViewModel * model,
            {
                bool update_count = true;
                int threshold = HOT_COUNT_THRESHOLD_CLASSIC;

                switch(model->method) {
                case BlackjackCounterMethodHiLo:
                case BlackjackCounterMethodTrueCount:
                case BlackjackCounterMethodHiLoSide:
                    switch(event->key) {
                    case InputKeyUp:
                        model->running_count += 1;
                        break;
                    case InputKeyOk:
                        update_count = false;
                        break;
                    case InputKeyDown:
                        model->running_count -= 1;
                        break;
                    case InputKeyRight:
                        if(model->method == BlackjackCounterMethodTrueCount) {
                            model->decks_remaining += 1;
                            update_count = false;
                        } else if(model->method == BlackjackCounterMethodHiLoSide) {
                            model->side_count += 1;
                            update_count = false;
                        }
                        break;
                    case InputKeyLeft:
                        if(model->method == BlackjackCounterMethodTrueCount &&
                           model->decks_remaining > 1) {
                            model->decks_remaining -= 1;
                        }
                        update_count = false;
                        break;
                    default:
                        update_count = false;
                        break;
                    }
                    threshold = HOT_COUNT_THRESHOLD_CLASSIC;
                    break;

                case BlackjackCounterMethodHiOpt1:
                case BlackjackCounterMethodHiOpt1Side:
                    switch(event->key) {
                    case InputKeyUp:
                        model->running_count += 1;
                        break;
                    case InputKeyOk:
                        update_count = false;
                        break;
                    case InputKeyDown:
                        model->running_count -= 1;
                        break;
                    case InputKeyRight:
                        if(model->method == BlackjackCounterMethodHiOpt1Side) {
                            model->side_count += 1;
                        }
                        update_count = false;
                        break;
                    default:
                        update_count = false;
                        break;
                    }
                    threshold = HOT_COUNT_THRESHOLD_HIOPT;
                    break;

                case BlackjackCounterMethodHiOpt2:
                case BlackjackCounterMethodHiOpt2Side:
                    switch(event->key) {
                    case InputKeyUp:
                        model->running_count += 1;
                        break;
                    case InputKeyRight:
                        if(model->method == BlackjackCounterMethodHiOpt2Side) {
                            model->side_count += 1;
                            update_count = false;
                        } else {
                            model->running_count += 2;
                        }
                        break;
                    case InputKeyOk:
                        update_count = false;
                        break;
                    case InputKeyDown:
                        model->running_count -= 2;
                        break;
                    case InputKeyLeft:
                        if(model->method == BlackjackCounterMethodHiOpt2Side) {
                            model->side_count += 1;
                        }
                        update_count = false;
                        break;
                    default:
                        update_count = false;
                        break;
                    }
                    threshold = HOT_COUNT_THRESHOLD_HIOPT2;
                    break;

                case BlackjackCounterMethodOmega2:
                    switch(event->key) {
                    case InputKeyUp:
                        model->running_count += 1;
                        break;
                    case InputKeyRight:
                        model->running_count += 2;
                        break;
                    case InputKeyOk:
                        model->running_count -= 1;
                        break;
                    case InputKeyDown:
                        model->running_count -= 2;
                        break;
                    default:
                        update_count = false;
                        break;
                    }
                    threshold = HOT_COUNT_THRESHOLD_OMEGA;
                    break;
                }

                if(update_count &&
                   (model->running_count >= threshold || model->running_count <= -threshold)) {
                    vibration_hot_count();
                }
            },
            true);
        consumed = true;
    }

    return consumed;
}

BlackjackCounterView* blackjack_counter_view_alloc() {
    BlackjackCounterView* counter_view = malloc(sizeof(BlackjackCounterView));
    counter_view->view = view_alloc();

    view_allocate_model(
        counter_view->view, ViewModelTypeLocking, sizeof(BlackjackCounterViewModel));
    view_set_context(counter_view->view, counter_view);
    view_set_draw_callback(counter_view->view, blackjack_counter_view_draw_callback);
    view_set_input_callback(counter_view->view, blackjack_counter_view_input_callback);

    with_view_model(
        counter_view->view,
        BlackjackCounterViewModel * model,
        {
            model->running_count = 0;
            model->side_count = 0;
            model->decks_remaining = 6;
            model->method = BlackjackCounterMethodHiLo;
        },
        true);

    return counter_view;
}

void blackjack_counter_view_free(BlackjackCounterView* counter_view) {
    furi_assert(counter_view);
    view_free(counter_view->view);
    free(counter_view);
}

View* blackjack_counter_view_get_view(BlackjackCounterView* counter_view) {
    furi_assert(counter_view);
    return counter_view->view;
}

void blackjack_counter_view_set_method(
    BlackjackCounterView* counter_view,
    BlackjackCounterMethod method) {
    furi_assert(counter_view);

    with_view_model(
        counter_view->view,
        BlackjackCounterViewModel * model,
        {
            model->method = method;
            model->running_count = 0;
            model->side_count = 0;
            model->decks_remaining = 6;
        },
        true);
}
