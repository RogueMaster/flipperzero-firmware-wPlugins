#include "include/views/gurpil_game_view.h"

#include "include/ui/render_map.h"
#include "include/views/game_view.h"

#include <furi.h>
#include <gui/canvas.h>
#include <input/input.h>
#include <stdlib.h>

// How long the "+Ns" checkpoint flash (game_view.h's show_checkpoint_flash) stays on screen
// after a checkpoint is crossed. A real-time duration, not a tick count, so it stays correct
// regardless of the Game scene's own FuriTimer period.
enum {
    CHECKPOINT_FLASH_DURATION_MS = 500,
};

// Everything the draw callback, the input callback and gurpil_game_view_tick touch lives inside
// this model rather than a hand-rolled FuriMutex+struct pair: view_get_model/view_commit_model
// (via view_allocate_model(..., ViewModelTypeLocking, ...) below) already serialize access
// across the GUI thread (draw/input) and the Game scene's FuriTimer thread (tick) — the same
// get/mutate/commit idiom the firmware's own PowerOff view uses for its countdown
// (applications/services/power/power_service/views/power_off.c).
typedef struct {
    GameState game;
    uint32_t frame; // per-tick counter driving the wheel-spin animation; reset per run.
    int32_t best;
    bool is_new_best;       // whether the game-over panel should show "New best!"; reset per run.
    bool game_over_handled; // true once this run's game-over has been scored, so
                            // gurpil_game_view_tick reports it exactly once.
    uint32_t checkpoint_flash_remaining_ms; // >0 while show_checkpoint_flash should render;
                                            // ticks down by dt_ms, reset on a fresh crossing.
} GurpilGameViewModel;

struct GurpilGameView {
    View *view;
    ViewDispatcher *view_dispatcher; // only used to send GurpilGameEventReturnToMenu.
};

static void gurpil_game_view_draw_callback(Canvas *canvas, void *model) {
    const GurpilGameViewModel *m = model;
    gurpil_render(canvas, &m->game, m->best, m->frame, m->checkpoint_flash_remaining_ms > 0,
                  m->is_new_best);
}

static GurpilKey gurpil_key_from_input(InputKey key) {
    switch (key) {
        case InputKeyUp:
            return GurpilKeyUp;
        case InputKeyDown:
            return GurpilKeyDown;
        case InputKeyLeft:
            return GurpilKeyLeft;
        case InputKeyRight:
            return GurpilKeyRight;
        case InputKeyOk:
            return GurpilKeyOk;
        case InputKeyBack:
            return GurpilKeyBack;
        default:
            return GurpilKeyOther;
    }
}

static bool gurpil_game_view_input_callback(InputEvent *event, void *context) {
    GurpilGameView *instance = context;

    if (event->type == InputTypePress) {
        // Immediate feedback for shape selection: react on press, not on release+debounce
        // (InputTypeShort) — a reflex game can't afford that latency. shape_for_input_key
        // returns ShapeCount (ignored by game_set_shape's own range guard) for Ok/Back/Other,
        // so this is safe to call unconditionally for every key while the run is live.
        GurpilKey key = gurpil_key_from_input(event->key);
        GurpilGameViewModel *model = view_get_model(instance->view);
        bool consumed = !game_is_over(&model->game);
        if (consumed) {
            game_set_shape(&model->game, shape_for_input_key(key));
        }
        view_commit_model(instance->view, false);
        return consumed;
    }

    if (event->type == InputTypeShort && event->key == InputKeyOk) {
        GurpilGameViewModel *model = view_get_model(instance->view);
        bool over = game_is_over(&model->game);
        view_commit_model(instance->view, false);
        if (over) {
            // From game-over, Ok returns to the menu; Back does the same at any time via
            // SceneManager's default "unconsumed Back pops the scene" fallback (see
            // gurpil_scene.h), so it needs no handling here.
            view_dispatcher_send_custom_event(instance->view_dispatcher,
                                              GurpilGameEventReturnToMenu);
            return true;
        }
    }

    return false;
}

GurpilGameView *gurpil_game_view_alloc(ViewDispatcher *view_dispatcher) {
    GurpilGameView *instance = malloc(sizeof(GurpilGameView));
    furi_check(instance != NULL);

    instance->view_dispatcher = view_dispatcher;
    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(GurpilGameViewModel));
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, gurpil_game_view_draw_callback);
    view_set_input_callback(instance->view, gurpil_game_view_input_callback);

    return instance;
}

void gurpil_game_view_free(GurpilGameView *instance) {
    view_free(instance->view);
    free(instance);
}

View *gurpil_game_view_get_view(const GurpilGameView *instance) {
    return instance->view;
}

void gurpil_game_view_start_run(GurpilGameView *instance, uint32_t seed, int32_t best) {
    GurpilGameViewModel *model = view_get_model(instance->view);
    game_start(&model->game, seed);
    model->frame = 0;
    model->best = best;
    model->is_new_best = false;
    model->game_over_handled = false;
    model->checkpoint_flash_remaining_ms = 0;
    view_commit_model(instance->view, true);
}

bool gurpil_game_view_tick(GurpilGameView *instance, uint32_t dt_ms) {
    GurpilGameViewModel *model = view_get_model(instance->view);

    bool became_over = false;
    if (!game_is_over(&model->game)) {
        game_tick(&model->game, dt_ms);
        model->frame++;

        if (game_checkpoint_just_hit(&model->game)) {
            model->checkpoint_flash_remaining_ms = CHECKPOINT_FLASH_DURATION_MS;
        } else if (model->checkpoint_flash_remaining_ms > dt_ms) {
            model->checkpoint_flash_remaining_ms -= dt_ms;
        } else {
            model->checkpoint_flash_remaining_ms = 0;
        }

        if (game_is_over(&model->game) && !model->game_over_handled) {
            model->game_over_handled = true;
            became_over = true;
        }
    }

    view_commit_model(instance->view, true);
    return became_over;
}

int32_t gurpil_game_view_distance(GurpilGameView *instance) {
    GurpilGameViewModel *model = view_get_model(instance->view);
    int32_t distance = game_distance(&model->game);
    view_commit_model(instance->view, false);
    return distance;
}

bool gurpil_game_view_is_new_best(GurpilGameView *instance, int32_t pre_run_best) {
    GurpilGameViewModel *model = view_get_model(instance->view);
    bool is_new_best = game_is_new_best(&model->game, pre_run_best);
    view_commit_model(instance->view, false);
    return is_new_best;
}

void gurpil_game_view_set_best(GurpilGameView *instance, int32_t best, bool is_new_best) {
    GurpilGameViewModel *model = view_get_model(instance->view);
    model->best = best;
    model->is_new_best = is_new_best;
    view_commit_model(instance->view, true);
}
