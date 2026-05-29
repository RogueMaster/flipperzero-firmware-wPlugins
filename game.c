#include <stdint.h>

#include <gui/modules/widget.h>

#include <stratahero_icons.h>

#include "constants.h"
#include "stratagems.h"
#include "settings.h"
#include "notifications.h"
#include "glyphs.h"
#include "game.h"

#define ROUND_TIME_MS 15000
#define CODE_TIME_BONUS 2000
#define INTRO_DELAY 3000
#define STATS_DELAY 1000
#define GAMEOVER_DELAY 6000
#define GAMEPLAY_TICK_INTERVAL 200
#define INVALID_CODE_DELAY 500

#define with_widget_model(widget, model, code, update) \
    {                                                              \
        StrataHeroGameModel* model = view_get_model(widget->view); \
        {code};                                                    \
        view_commit_model(widget->view, update);                   \
    }

typedef enum {
    GameView_Intro,
    GameView_Gameplay,
    GameView_Stats,
    GameView_GameOver,
    GameView_QuitConfirmation,
} GameView;

struct StrataHeroGameWidget {
    View* view;
    NotificationApp* notification;

    StrataHeroSettings settings;

    FuriTimer* intro_timer;
    FuriTimer* stats_timer;
    FuriTimer* gameplay_timer;
    FuriTimer* gameover_timer;
    FuriTimer* invalid_code_timer;
    FuriTimer* score_popup_timer;

    StrataHeroGameWidgetNavigationCallback navigation_callback;
    void* navigation_callback_context;
};

typedef struct {
    GameView current_view;

    int current_round;
    const Stratagem* round_stratagems[MAX_STRATAGEMS_PER_ROUND + 1];
    int round_stratagems_count;

    int current_round_stratagem;

    int current_code_length;
    int current_code_progress;

    bool input_blocked;

    uint32_t remaining_time;
    uint32_t last_tick_time;
    bool perfect_round;
    int round_bonus;
    int time_bonus;
    int perfect_bonus;

    int score;
    int score_popup_points;

    int stats_view_count;
    bool round_complete;

    GameView prev_view;
} StrataHeroGameModel;


static void game_widget_navigate(StrataHeroGameWidget* widget, StrataHeroGameWidgetNavigationEvent event);
static void game_widget_navigate_back(StrataHeroGameWidget* widget);


uint32_t get_time_delta(uint32_t old, uint32_t new) {
    return (new > old) ? (new - old) : UINT32_MAX - old + new;
}

static void update_score(StrataHeroGameModel* model) {
    model->round_bonus = (model->current_round + 2) * 25;
    model->time_bonus = model->remaining_time / 100;
    model->perfect_bonus = model->perfect_round ? 100 : 0;
    model->score += model->round_bonus + model->time_bonus + model->perfect_bonus;
}

static void next_code(StrataHeroGameModel* model) {
    model->current_round_stratagem++;
    if (model->current_round_stratagem >= model->round_stratagems_count) {
        update_score(model);
        model->stats_view_count = 0;
        model->round_complete = true;
        return;
    }
    model->current_code_progress = 0;
    model->current_code_length = strlen(model->round_stratagems[model->current_round_stratagem]->code);
    model->input_blocked = false;
}

static void next_round(StrataHeroGameModel* model) {
    model->current_round++;

    int count = model->current_round - 1 + MIN_STRATAGEMS_PER_ROUND;
    if (count > MAX_STRATAGEMS_PER_ROUND) {
        count = MAX_STRATAGEMS_PER_ROUND;
    }

    for (int i=0; i < count; i++) {
        for (int retry=0; retry < MAX_STRATEGEM_SHUFFLE_RETRIES; retry++) {
            model->round_stratagems[i] = stratagems[rand() % stratagems_count];

            // Ensure that stratagems do not repeat
            bool success = true;
            for (int k=0; k < i; k++) {
                if (model->round_stratagems[k] == model->round_stratagems[i]) {
                    success = false;
                    break;
                }
            }

            if (success) {
                break;
            }
        }
    }
    model->round_stratagems_count = count;
    model->current_round_stratagem = 0;
    model->perfect_round = true;
    model->round_complete = false;
}

static void intro_timer_callback(void* context) {
    furi_assert(context);
    StrataHeroGameWidget* widget = context;

    with_widget_model(widget, model, {
        model->current_code_length = strlen(model->round_stratagems[0]->code);
        model->current_code_progress = 0;
        model->remaining_time = ROUND_TIME_MS;
        model->last_tick_time = furi_get_tick();
        model->current_view = GameView_Gameplay;
        furi_timer_start(widget->gameplay_timer, GAMEPLAY_TICK_INTERVAL);
    }, true);
}

static void stats_timer_callback(void* context) {
    furi_assert(context);
    StrataHeroGameWidget* widget = context;

    with_widget_model(widget, model, {
        model->stats_view_count++;
        if (model->stats_view_count >= 8) {
            next_round(model);
            model->current_view = GameView_Intro;
            furi_timer_stop(widget->stats_timer);
            furi_timer_start(widget->intro_timer, INTRO_DELAY);
        }
    }, true);
}

static void gameplay_timer_callback(void* context) {
    furi_assert(context);
    StrataHeroGameWidget* widget = context;

    with_widget_model(widget, model, {
        uint32_t current_time = furi_get_tick();
        uint32_t elapsed_time = get_time_delta(model->last_tick_time, current_time);
        if (model->remaining_time <= elapsed_time) {
            furi_timer_stop(widget->gameplay_timer);
            model->current_view = GameView_GameOver;
            furi_timer_start(widget->gameover_timer, GAMEOVER_DELAY);
        } else {
            model->remaining_time -= elapsed_time;
            model->last_tick_time = current_time;
        }
    }, true);
}

static void gameover_timer_callback(void* context) {
    furi_assert(context);
    StrataHeroGameWidget* widget = context;
    game_widget_navigate_back(widget);
}

static void invalid_code_timer_callback(void* context) {
    furi_assert(context);
    StrataHeroGameWidget* widget = context;

    with_widget_model(widget, model, {
        model->input_blocked = false;
        model->current_code_progress = 0;
    }, true);
}

static void score_popup_timer_callback(void* context) {
    furi_assert(context);
    StrataHeroGameWidget* widget = context;

    with_widget_model(widget, model, {
        model->score_popup_points = 0;
        if(model->round_complete) {
            model->round_complete = false;
            model->current_view = GameView_Stats;
            furi_timer_start(widget->stats_timer, STATS_DELAY);
        }
    }, true);
}


static void draw_intro(Canvas* canvas, StrataHeroGameModel* model) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer)-1, "Round %d", model->current_round);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, SCREEN_WIDTH / 2, 20, AlignCenter, AlignTop, buffer);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, SCREEN_WIDTH / 2, 50, AlignCenter, AlignTop, "Get Ready");
}

static void draw_gameplay(Canvas* canvas, StrataHeroGameModel* model) {
    canvas_clear(canvas);

    char buffer[32];

    uint32_t time_bar_width = (model->remaining_time) * 60 / 1000 / ROUND_TIME_SECONDS;
    if (time_bar_width > 80) {
        time_bar_width = 80;
    }
    canvas_draw_line(canvas, 0, SCREEN_HEIGHT-1, time_bar_width, SCREEN_HEIGHT-1);
    for (uint32_t i=0; i < time_bar_width; i += 15) {
        canvas_draw_dot(canvas, i, SCREEN_HEIGHT-2);
        canvas_draw_dot(canvas, i, SCREEN_HEIGHT-3);
    }

    canvas_set_font(canvas, FontPrimary);
    snprintf(buffer, sizeof(buffer)-1, "%d", model->score);
    canvas_draw_str_aligned(canvas, SCREEN_WIDTH, SCREEN_HEIGHT, AlignRight, AlignBottom, buffer);

    if(model->score_popup_points > 0) {
        snprintf(buffer, sizeof(buffer)-1, "+%d", model->score_popup_points);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, SCREEN_WIDTH, SCREEN_HEIGHT - 12, AlignRight, AlignBottom, buffer);
    }

    // Display stratagem queue
    int offset_x = 5;
    int offset_y = 5;
    for (int i=model->current_round_stratagem; i < model->round_stratagems_count; i++) {
        const Icon* icon = model->round_stratagems[i]->icon;
        if (icon == NULL) {
            icon = &I_no_icon_stratagem;
        }

        canvas_draw_icon(canvas, offset_x, 5, icon);
        offset_x += icon_get_width(icon) + 5;
    }

    if(model->round_complete) return;

    // Display current stratagem code
    const Stratagem* stratagem = model->round_stratagems[model->current_round_stratagem];

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, SCREEN_WIDTH / 2, 32, AlignCenter, AlignTop, stratagem->title);

    bool inverse = model->input_blocked;

    int code_len = strlen(stratagem->code);
    offset_x = (SCREEN_WIDTH - CODE_GLYPH_WIDTH * code_len) / 2;
    if (offset_x < 0) {
        offset_x = 4;
    }
    // offset_y = (code_len > 10) ? 35 : 45;
    offset_y = 42;
    for (int i=0; i < code_len; i++) {
        const StrataHeroCodeGlyph* glyph = stratahero_get_code_glyph(stratagem->code[i]);
        if (!glyph) {
            continue;
        }

        const Icon* icon = inverse ? glyph->inverse : ((i < model->current_code_progress) ? glyph->black : glyph->white);
        canvas_draw_icon(canvas, offset_x, offset_y, icon);
        offset_x += CODE_GLYPH_WIDTH;
        if (offset_x > SCREEN_WIDTH - CODE_GLYPH_WIDTH) {
            offset_y += CODE_GLYPH_HEIGHT;

            offset_x = (SCREEN_WIDTH - CODE_GLYPH_WIDTH * (code_len - i - 1)) / 2;
            if (offset_x < 0) {
                offset_x = 4;
            }
        }
    }
}

static void draw_stats(Canvas* canvas, StrataHeroGameModel* model) {
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);

    char buffer[32];
    snprintf(buffer, sizeof(buffer)-1, "Round %d Complete", model->current_round);
    canvas_draw_str_aligned(canvas, SCREEN_WIDTH/2, 2, AlignCenter, AlignTop, buffer);

    if (model->stats_view_count > 0) {
        canvas_draw_str_aligned(canvas, 5, 20, AlignLeft, AlignTop, "Round Bonus");
        snprintf(buffer, sizeof(buffer)-1, "%d", model->round_bonus);
        canvas_draw_str_aligned(canvas, SCREEN_WIDTH - 5, 20, AlignRight, AlignTop, buffer);
    }

    if (model->stats_view_count > 1) {
        canvas_draw_str_aligned(canvas, 5, 30, AlignLeft, AlignTop, "Time Bonus");
        snprintf(buffer, sizeof(buffer)-1, "%d", model->time_bonus);
        canvas_draw_str_aligned(canvas, SCREEN_WIDTH - 5, 30, AlignRight, AlignTop, buffer);
    }

    if (model->stats_view_count > 2) {
        canvas_draw_str_aligned(canvas, 5, 40, AlignLeft, AlignTop, "Perfect Bonus");
        snprintf(buffer, sizeof(buffer)-1, "%d", model->perfect_bonus);
        canvas_draw_str_aligned(canvas, SCREEN_WIDTH - 5, 40, AlignRight, AlignTop, buffer);
    }

    if (model->stats_view_count > 3) {
        canvas_draw_str_aligned(canvas, 5, 50, AlignLeft, AlignTop, "Total Score");
        snprintf(buffer, sizeof(buffer)-1, "%d", model->score);
        canvas_draw_str_aligned(canvas, SCREEN_WIDTH - 5, 50, AlignRight, AlignTop, buffer);
    }
}

static void draw_quit_confirmation(Canvas* canvas, StrataHeroGameModel* model) {
    UNUSED(model);
    canvas_clear(canvas);

    canvas_draw_rframe(canvas, 14, 10, 100, 44, 4);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, SCREEN_WIDTH / 2, 17, AlignCenter, AlignTop, "Quit game?");

    canvas_draw_line(canvas, 14, 32, 113, 32);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 32, 39, AlignCenter, AlignTop, "< No");
    canvas_draw_str_aligned(canvas, 96, 39, AlignCenter, AlignTop, "Yes >");
}

static void draw_game_over(Canvas* canvas, StrataHeroGameModel* model) {
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, SCREEN_WIDTH / 2, 12, AlignCenter, AlignTop, "Game Over");

    canvas_draw_str_aligned(canvas, SCREEN_WIDTH / 2, 30, AlignCenter, AlignTop, "Score");
    char buffer[16];
    snprintf(buffer, sizeof(buffer)-1, "%d", model->score);
    canvas_draw_str_aligned(canvas, SCREEN_WIDTH / 2, 40, AlignCenter, AlignTop, buffer);
}

static void draw_callback(Canvas* canvas, void* _model) {
    furi_assert(_model);
    StrataHeroGameModel* model = _model;

    switch (model->current_view) {
        case GameView_Intro:
            draw_intro(canvas, model);
            break;

        case GameView_Gameplay:
            draw_gameplay(canvas, model);
            break;

        case GameView_Stats:
            draw_stats(canvas, model);
            break;

        case GameView_GameOver:
            draw_game_over(canvas, model);
            break;

        case GameView_QuitConfirmation:
            draw_quit_confirmation(canvas, model);
            break;
    }
}

static bool input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    StrataHeroGameWidget* widget = context;

    StrataHeroGameModel* model = view_get_model(widget->view);

    bool handled = false;
    switch (model->current_view) {
        case GameView_Intro: {
            if (event->key == InputKeyBack && event->type == InputTypeShort) {
                furi_timer_stop(widget->intro_timer);
                with_widget_model(widget, m, {
                    m->prev_view = GameView_Intro;
                    m->current_view = GameView_QuitConfirmation;
                }, true);
                handled = true;
            }
            break;
        }

        case GameView_Gameplay: {
            if (event->type == InputTypeShort) {
                if (model->input_blocked) {
                    break;
                }

                char code_input = 0;
                switch (event->key) {
                    case InputKeyLeft:  code_input = 'L'; break;
                    case InputKeyRight: code_input = 'R'; break;
                    case InputKeyUp:    code_input = 'U'; break;
                    case InputKeyDown:  code_input = 'D'; break;
                    case InputKeyBack: {
                        furi_timer_stop(widget->gameplay_timer);
                        furi_timer_stop(widget->score_popup_timer);
                        with_widget_model(widget, m, {
                            m->prev_view = GameView_Gameplay;
                            m->current_view = GameView_QuitConfirmation;
                        }, true);
                        handled = true;
                        break;
                    }
                    default: break;
                }

                if (code_input != 0) {
                    const Stratagem* stratagem = model->round_stratagems[model->current_round_stratagem];
                    if (stratagem->code[model->current_code_progress] == code_input) {
                        bool last = (model->current_code_progress + 1 >= model->current_code_length);
                        if (last) {
                            stratahero_code_complete_notification(widget->notification, &widget->settings);
                        } else {
                            stratahero_code_glyph_entry_success_notification(widget->notification, &widget->settings);
                        }
                        with_widget_model(widget, model, {
                            model->current_code_progress++;
                            if (model->current_code_progress >= model->current_code_length) {
                                int points = model->current_code_length * 5;
                                model->score += points;
                                model->remaining_time += CODE_TIME_BONUS;
                                model->score_popup_points = points;
                                furi_timer_start(widget->score_popup_timer, 1000);

                                next_code(model);
                                if(model->round_complete) {
                                    furi_timer_stop(widget->gameplay_timer);
                                }
                            }
                        }, true);
                    } else {
                        stratahero_code_glyph_entry_failure_notification(widget->notification, &widget->settings);
                        furi_timer_start(widget->invalid_code_timer, INVALID_CODE_DELAY);
                        with_widget_model(widget, model, {
                            model->input_blocked = true;
                            model->perfect_round = false;
                        }, true);
                    }

                    handled = true;
                }
            }
            break;
        }

        case GameView_Stats: {
            if (event->key == InputKeyBack && event->type == InputTypeShort) {
                furi_timer_stop(widget->stats_timer);
                with_widget_model(widget, m, {
                    m->prev_view = GameView_Stats;
                    m->current_view = GameView_QuitConfirmation;
                }, true);
                handled = true;
            }
            break;
        }

        case GameView_QuitConfirmation: {
            if (event->type == InputTypeShort) {
                if(event->key == InputKeyLeft || event->key == InputKeyBack) {
                    GameView prev = model->prev_view;
                    bool round_done = model->round_complete;
                    with_widget_model(widget, m, {
                        m->current_view = prev;
                        if(prev == GameView_Gameplay && !round_done) {
                            m->last_tick_time = furi_get_tick();
                        }
                    }, true);
                    if(prev == GameView_Gameplay && !round_done) {
                        furi_timer_start(widget->gameplay_timer, GAMEPLAY_TICK_INTERVAL);
                    } else if(prev == GameView_Gameplay && round_done) {
                        furi_timer_start(widget->score_popup_timer, 1000);
                    } else if(prev == GameView_Intro) {
                        furi_timer_start(widget->intro_timer, INTRO_DELAY);
                    } else if(prev == GameView_Stats) {
                        furi_timer_start(widget->stats_timer, STATS_DELAY);
                    }
                    handled = true;
                } else if(event->key == InputKeyRight || event->key == InputKeyOk) {
                    with_widget_model(widget, m, {
                        m->current_view = GameView_GameOver;
                    }, true);
                    handled = true;
                }
            }
            break;
        }

        case GameView_GameOver: {
            if (event->key == InputKeyBack && event->type == InputTypeShort) {
                game_widget_navigate_back(widget);
                handled = true;
            }
            break;
        }
    }

    return handled;
}

static void enter_callback(void* context) {
    furi_assert(context);

    StrataHeroGameWidget* widget = context;
    with_widget_model(widget, model, {
        model->current_round = 0;
        model->current_view = GameView_Intro;
        next_round(model);
    }, true);
    furi_timer_start(widget->intro_timer, INTRO_DELAY);
}

static void exit_callback(void* context) {
    furi_assert(context);

    StrataHeroGameWidget* widget = context;
    furi_timer_stop(widget->intro_timer);
    furi_timer_stop(widget->stats_timer);
    furi_timer_stop(widget->gameplay_timer);
    furi_timer_stop(widget->gameover_timer);
    furi_timer_stop(widget->invalid_code_timer);
    furi_timer_stop(widget->score_popup_timer);
}

StrataHeroGameWidget* stratahero_game_widget_alloc() {
    StrataHeroGameWidget* widget = malloc(sizeof(StrataHeroGameWidget));
    widget->view = view_alloc();
    view_set_context(widget->view, widget);
    view_set_draw_callback(widget->view, draw_callback);
    view_set_input_callback(widget->view, input_callback);
    view_allocate_model(widget->view, ViewModelTypeLockFree, sizeof(StrataHeroGameModel));
    view_set_enter_callback(widget->view, enter_callback);
    view_set_exit_callback(widget->view, exit_callback);

    widget->intro_timer = furi_timer_alloc(intro_timer_callback, FuriTimerTypeOnce, widget);
    widget->stats_timer = furi_timer_alloc(stats_timer_callback, FuriTimerTypePeriodic, widget);
    widget->gameplay_timer = furi_timer_alloc(gameplay_timer_callback, FuriTimerTypePeriodic, widget);
    widget->gameover_timer = furi_timer_alloc(gameover_timer_callback, FuriTimerTypeOnce, widget);
    widget->invalid_code_timer = furi_timer_alloc(invalid_code_timer_callback, FuriTimerTypeOnce, widget);
    widget->score_popup_timer = furi_timer_alloc(score_popup_timer_callback, FuriTimerTypeOnce, widget);

    widget->navigation_callback = NULL;
    widget->navigation_callback_context = NULL;

    widget->notification = furi_record_open(RECORD_NOTIFICATION);

    return widget;
}

void stratahero_game_widget_free(StrataHeroGameWidget* widget) {
    furi_timer_stop(widget->intro_timer);
    furi_timer_free(widget->intro_timer);

    furi_timer_stop(widget->stats_timer);
    furi_timer_free(widget->stats_timer);

    furi_timer_stop(widget->gameplay_timer);
    furi_timer_free(widget->gameplay_timer);

    furi_timer_stop(widget->gameover_timer);
    furi_timer_free(widget->gameover_timer);

    furi_timer_stop(widget->invalid_code_timer);
    furi_timer_free(widget->invalid_code_timer);

    furi_timer_stop(widget->score_popup_timer);
    furi_timer_free(widget->score_popup_timer);

    view_free(widget->view);
    free(widget);

    furi_record_close(RECORD_NOTIFICATION);
}

View* stratahero_game_widget_get_view(StrataHeroGameWidget* widget) {
    return widget->view;
}

void stratahero_game_widget_set_settings(StrataHeroGameWidget* widget, StrataHeroSettings* settings) {
    furi_check(widget);
    widget->settings = *settings;
}

void stratahero_game_widget_set_navigation_callback(
    StrataHeroGameWidget* widget,
    StrataHeroGameWidgetNavigationCallback callback,
    void* context
) {
    furi_check(widget);
    widget->navigation_callback = callback;
    widget->navigation_callback_context = context;
}

static void game_widget_navigate(StrataHeroGameWidget* widget, StrataHeroGameWidgetNavigationEvent event) {
    furi_check(widget);
    if (widget->navigation_callback) {
        widget->navigation_callback(event, widget->navigation_callback_context);
    }
}

static void game_widget_navigate_back(StrataHeroGameWidget* widget) {
    game_widget_navigate(widget, StrataHeroGameWidgetNavigationEvent_Back);
}


