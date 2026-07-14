#include "../guess_the_number.h"
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_random.h>
#include <input/input.h>
#include <gui/elements.h>
#include <dolphin/dolphin.h>
#include "../helpers/game_haptic.h"
#include "../helpers/game_speaker.h"
#include "../helpers/game_led.h"
#include "../helpers/game_storage.h"

struct GamePlay {
    View* view;
    GamePlayCallback callback;
    void* context;
};

typedef struct {
    int target_number, player_guess;
    char game_message[50];
    int attempts;
    int best_score;
    bool game_won;
    int range_min;
    int range_max;
} GamePlayModel;

void game_play_set_callback(GamePlay* instance, GamePlayCallback callback, void* context) {
    furi_assert(instance);
    furi_assert(callback);
    instance->callback = callback;
    instance->context = context;
}

void game_play_draw(Canvas* canvas, GamePlayModel* model) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "GUESS THE NUMBER");

    canvas_set_font(canvas, FontPrimary);
    char guessBuffer[10];
    snprintf(guessBuffer, sizeof(guessBuffer), "[ %02d ]", model->player_guess);
    canvas_draw_str_aligned(canvas, 64, 15, AlignCenter, AlignTop, guessBuffer);

    if(strlen(model->game_message) > 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, model->game_message);
    }

    canvas_set_font(canvas, FontSecondary);
    if(model->game_won) {
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, "Press BACK for new game");
    } else {
        char rangeBuffer[20];
        snprintf(
            rangeBuffer, sizeof(rangeBuffer), "Range: %d-%d", model->range_min, model->range_max);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, rangeBuffer);
    }

    canvas_set_font(canvas, FontSecondary);
    char statusBuffer[32];
    if(model->best_score > 0) {
        snprintf(
            statusBuffer,
            sizeof(statusBuffer),
            "Tries: %d | Best: %d",
            model->attempts,
            model->best_score);
    } else {
        snprintf(statusBuffer, sizeof(statusBuffer), "Tries: %d", model->attempts);
    }
    canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignTop, statusBuffer);
}

static void game_play_model_init(GamePlayModel* const model) {
    model->target_number = furi_hal_random_get() % 100;
    model->player_guess = 50;
    model->attempts = 0;
    model->game_won = false;
    model->range_min = 0;
    model->range_max = 99;
    strcpy(model->game_message, "Make your first guess!");
    if(model->best_score == 0) {
        model->best_score = game_read_best_score();
    }
    dolphin_deed(DolphinDeedPluginGameStart);
}

bool game_play_input(InputEvent* event, void* context) {
    furi_assert(context);
    GamePlay* instance = context;

    if(event->type == InputTypeLong && event->key == InputKeyBack) {
        game_play_long_bump(instance->context);
        game_led_set_rgb(instance->context, 255, 0, 0);
        furi_thread_flags_wait(0, FuriFlagWaitAny, 200);
        game_led_reset(instance->context);
        instance->callback(GameCustomEventPlayBack, instance->context);
        return true;
    }

    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        with_view_model(
            instance->view,
            GamePlayModel * model,
            {
                int saved_best_score = model->best_score;
                strcpy(model->game_message, "New game started!");
                game_play_model_init(model);
                model->best_score = saved_best_score;
            },
            true);
        game_play_long_bump(instance->context);
        game_led_set_rgb(instance->context, 0, 0, 255);
        furi_thread_flags_wait(0, FuriFlagWaitAny, 200);
        game_led_reset(instance->context);
        return true;
    }

    if(event->type == InputTypePress) {
        switch(event->key) {
        case InputKeyUp:
            with_view_model(
                instance->view,
                GamePlayModel * model,
                {
                    model->player_guess = model->player_guess < 99 ? model->player_guess + 1 : 99;
                    game_play_button_press(instance->context);
                    game_play_input_sound(instance->context);
                },
                true);
            break;
        case InputKeyDown:
            with_view_model(
                instance->view,
                GamePlayModel * model,
                {
                    model->player_guess = model->player_guess > 0 ? model->player_guess - 1 : 0;
                    game_play_button_press(instance->context);
                    game_play_input_sound(instance->context);
                },
                true);
            break;
        case InputKeyLeft:
            with_view_model(
                instance->view,
                GamePlayModel * model,
                {
                    model->player_guess = model->player_guess >= 10 ? model->player_guess - 10 : 0;
                    game_play_short_bump(instance->context);
                    game_play_input_sound(instance->context);
                },
                true);
            break;
        case InputKeyRight:
            with_view_model(
                instance->view,
                GamePlayModel * model,
                {
                    model->player_guess = model->player_guess <= 89 ? model->player_guess + 10 :
                                                                      99;
                    game_play_short_bump(instance->context);
                    game_play_input_sound(instance->context);
                },
                true);
            break;
        case InputKeyOk:
            with_view_model(
                instance->view,
                GamePlayModel * model,
                {
                    if(!model->game_won) {
                        model->attempts++;

                        if(model->target_number == model->player_guess) {
                            model->game_won = true;
                            snprintf(
                                model->game_message,
                                sizeof(model->game_message),
                                "🎉 Winner! Got it in %d tries!",
                                model->attempts);

                            if(model->best_score == 0 || model->attempts < model->best_score) {
                                model->best_score = model->attempts;
                                game_save_best_score(model->best_score);
                            }

                            game_play_win_sound(instance->context);
                            game_play_happy_bump(instance->context);
                            game_led_set_rgb(instance->context, 0, 255, 0);
                            dolphin_deed(DolphinDeedPluginGameWin);
                        } else {
                            int difference = abs(model->target_number - model->player_guess);

                            // Update dynamic range
                            if(model->target_number > model->player_guess) {
                                model->range_min = model->player_guess + 1;
                            } else {
                                model->range_max = model->player_guess - 1;
                            }

                            if(difference <= 2) {
                                strcpy(
                                    model->game_message,
                                    model->target_number > model->player_guess ?
                                        "Very close! Go higher!" :
                                        "Very close! Go lower!");
                                game_play_close_sound(instance->context);
                                game_play_close_bump(instance->context);
                                game_led_set_rgb(instance->context, 255, 255, 0);
                            } else if(difference <= 5) {
                                strcpy(
                                    model->game_message,
                                    model->target_number > model->player_guess ?
                                        "Close! Try higher!" :
                                        "Close! Try lower!");
                                game_play_close_sound(instance->context);
                                game_play_short_bump(instance->context);
                                game_led_set_rgb(instance->context, 255, 165, 0);
                            } else if(difference <= 15) {
                                strcpy(
                                    model->game_message,
                                    model->target_number > model->player_guess ? "Too low!" :
                                                                                 "Too high!");
                                game_play_error_sound(instance->context);
                                game_play_bad_bump(instance->context);
                                game_led_set_rgb(instance->context, 255, 0, 0);
                            } else {
                                strcpy(
                                    model->game_message,
                                    model->target_number > model->player_guess ? "Way too low!" :
                                                                                 "Way too high!");
                                game_play_error_sound(instance->context);
                                game_play_bad_bump(instance->context);
                                game_led_set_rgb(instance->context, 255, 0, 0);
                            }
                        }
                        game_play_button_press(instance->context);
                    }
                },
                true);
            break;
        case InputKeyBack:
            break;
        case InputKeyMAX:
            break;
        }
    } else if(event->type == InputTypeRelease) {
        switch(event->key) {
        case InputKeyUp:
        case InputKeyDown:
        case InputKeyLeft:
        case InputKeyRight:
        case InputKeyOk:
            game_stop_all_sound(instance->context);
            game_led_reset(instance->context);
            break;
        case InputKeyBack:
        case InputKeyMAX:
            break;
        }
    }

    return true;
}

void game_play_exit(void* context) {
    furi_assert(context);
    GamePlay* instance = context;
    if(instance->context) {
        game_stop_all_sound(instance->context);
        game_led_reset(instance->context);
    }
}

void game_play_enter(void* context) {
    furi_assert(context);
    GamePlay* instance = context;
    if(instance->context) {
        game_led_reset(instance->context);
    }
    dolphin_deed(DolphinDeedPluginStart);
}

GamePlay* game_play_alloc() {
    GamePlay* instance = malloc(sizeof(GamePlay));
    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(GamePlayModel));
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, (ViewDrawCallback)game_play_draw);
    view_set_input_callback(instance->view, game_play_input);
    view_set_enter_callback(instance->view, game_play_enter);
    view_set_exit_callback(instance->view, game_play_exit);

    with_view_model(instance->view, GamePlayModel * model, { game_play_model_init(model); }, true);

    return instance;
}

void game_play_free(GamePlay* instance) {
    furi_assert(instance);
    with_view_model(instance->view, GamePlayModel * model, { UNUSED(model); }, true);
    view_free(instance->view);
    free(instance);
}

View* game_play_get_view(GamePlay* instance) {
    furi_assert(instance);
    return instance->view;
}
