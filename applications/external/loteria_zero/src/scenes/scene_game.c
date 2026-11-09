#include "scenes.h"
#include "../renderer.h"

#include <gui/elements.h>

typedef struct {
    LoteriaApp* app;
} GameViewModel;

static void loteria_game_view_redraw(LoteriaApp* app) {
    with_view_model(app->game_view, GameViewModel * model, { UNUSED(model); }, true);
}

static void loteria_game_view_draw_callback(Canvas* canvas, void* model) {
    LoteriaApp* app = ((GameViewModel*)model)->app;
    LoteriaRenderModel render = {0};
    uint8_t sung = loteria_game_sung_count(&app->game);
    uint8_t lookback = app->history_offset;

    render.total = LOTERIA_CARD_COUNT;
    render.sung = sung;
    render.paused = app->game.paused;
    render.auto_mode = app->persist.auto_mode;
    render.finished = app->game.finished;
    render.showing_previous = (lookback > 0);
    render.rhyme_scroll = app->rhyme_scroll;
    render.card = loteria_game_card_at_lookback(&app->game, lookback);
    if(sung > lookback) {
        render.card_number = (uint8_t)(sung - lookback);
    } else {
        render.card_number = sung;
    }

    loteria_renderer_draw_card(canvas, &render);
}

static void loteria_game_exit_history(LoteriaApp* app) {
    app->history_offset = 0;
    app->viewing_previous = false;
    app->rhyme_scroll = 0;
}

static void loteria_game_request_next(LoteriaApp* app) {
    /* Browsing history: first OK/Right returns to the live card (no advance).
     * A second press advances. This avoids "re-calling" a past card. */
    if(app->history_offset > 0) {
        loteria_game_exit_history(app);
        loteria_game_view_redraw(app);
        return;
    }
    if(app->game.paused) {
        app->game.paused = false;
        loteria_auto_timer_apply(app);
        loteria_game_view_redraw(app);
        return;
    }
    view_dispatcher_send_custom_event(app->view_dispatcher, LoteriaCustomEventGameNext);
}

static bool loteria_game_view_input_callback(InputEvent* event, void* context) {
    LoteriaApp* app = context;
    /* Only Short for next/history nav — Repeat was easy to double-advance. */
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    switch(event->key) {
    case InputKeyOk:
        if(event->type != InputTypeShort) {
            return true;
        }
        loteria_game_request_next(app);
        return true;
    case InputKeyRight:
        if(event->type != InputTypeShort) {
            return true;
        }
        if(app->history_offset > 0) {
            app->history_offset--;
            app->viewing_previous = (app->history_offset > 0);
            app->rhyme_scroll = 0;
            loteria_game_view_redraw(app);
            return true;
        }
        loteria_game_request_next(app);
        return true;
    case InputKeyLeft: {
        /* Walk back through every called card, not only the last one. */
        if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
            return true;
        }
        uint8_t max_lb = loteria_game_max_lookback(&app->game);
        if(app->history_offset < max_lb) {
            app->history_offset++;
            app->viewing_previous = true;
            app->rhyme_scroll = 0;
            loteria_game_view_redraw(app);
        }
        return true;
    }
    case InputKeyUp:
        if(app->rhyme_scroll > 0) {
            app->rhyme_scroll--;
        }
        loteria_game_view_redraw(app);
        return true;
    case InputKeyDown:
        app->rhyme_scroll++;
        if(app->persist.auto_mode != LoteriaAutoManual) {
            app->game.paused = true;
            loteria_auto_timer_apply(app);
        }
        loteria_game_view_redraw(app);
        return true;
    default:
        break;
    }
    return false;
}

void loteria_scene_game_on_enter(void* context) {
    LoteriaApp* app = context;
    loteria_game_exit_history(app);

    with_view_model(app->game_view, GameViewModel * model, { model->app = app; }, false);

    view_set_orientation(app->game_view, ViewOrientationVertical);
    view_dispatcher_switch_to_view(app->view_dispatcher, LoteriaViewGame);
    loteria_notify_card(app);
    loteria_auto_timer_apply(app);
}

bool loteria_scene_game_on_event(void* context, SceneManagerEvent event) {
    LoteriaApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(event.event == LoteriaCustomEventGameNext ||
           event.event == LoteriaCustomEventGameAutoTick) {
            if(app->history_offset > 0) {
                /* Auto-tick while browsing: snap back to live card, do not advance. */
                loteria_game_exit_history(app);
                loteria_game_view_redraw(app);
                return true;
            }
            if(app->game.paused && event.event == LoteriaCustomEventGameAutoTick) {
                return true;
            }

            if(loteria_game_sung_count(&app->game) >= LOTERIA_CARD_COUNT) {
                app->game.finished = true;
                app->game.active = false;
                app->persist.stats.games_completed++;
                app->persist.has_saved_game = false;
                loteria_app_save(app);
                scene_manager_next_scene(app->scene_manager, LoteriaSceneGameEnd);
                return true;
            }

            uint8_t before_count = loteria_game_sung_count(&app->game);
            bool ok = loteria_game_next(&app->game);
            uint8_t after_count = loteria_game_sung_count(&app->game);
            if(after_count > before_count) {
                app->persist.stats.cards_sung++;
                loteria_notify_card(app);
            }
            app->rhyme_scroll = 0;
            app->history_offset = 0;
            app->viewing_previous = false;
            /* Keep save in RAM during play; flush on Back / exit / game end. */
            app->persist.saved_game = app->game;
            app->persist.has_saved_game = loteria_game_has_save(&app->game);
            loteria_game_view_redraw(app);
            loteria_auto_timer_apply(app);

            if(!ok || after_count >= LOTERIA_CARD_COUNT) {
                app->game.finished = true;
                app->game.active = false;
                app->persist.stats.games_completed++;
                app->persist.has_saved_game = false;
                loteria_app_save(app);
                scene_manager_next_scene(app->scene_manager, LoteriaSceneGameEnd);
            }
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = true;
        if(app->history_offset > 0) {
            /* Back leaves history browse first, then exits to menu. */
            loteria_game_exit_history(app);
            loteria_game_view_redraw(app);
            return true;
        }
        app->persist.saved_game = app->game;
        app->persist.has_saved_game = loteria_game_has_save(&app->game);
        loteria_app_save(app);
        if(!scene_manager_previous_scene(app->scene_manager)) {
            scene_manager_next_scene(app->scene_manager, LoteriaSceneMainMenu);
        }
    }

    return consumed;
}

void loteria_scene_game_on_exit(void* context) {
    LoteriaApp* app = context;
    if(app->auto_timer) {
        furi_timer_stop(app->auto_timer);
    }
    view_set_orientation(app->game_view, ViewOrientationHorizontal);
}

void loteria_game_view_setup(View* view, LoteriaApp* app) {
    view_allocate_model(view, ViewModelTypeLocking, sizeof(GameViewModel));
    view_set_context(view, app);
    view_set_draw_callback(view, loteria_game_view_draw_callback);
    view_set_input_callback(view, loteria_game_view_input_callback);
    with_view_model(view, GameViewModel * model, { model->app = app; }, true);
}
