#include "minesweeper.h"
#include "scenes/minesweeper_scene.h"
#include "minesweeper_redux_icons.h"

typedef enum {
    MineSweeperSceneStartScreenContinueEvent,
} MineSweeperSceneStartScreenEvent;

bool minesweeper_scene_start_screen_input_callback(InputEvent* event, void* context) {
    furi_assert(event);
    furi_assert(context);

    MineSweeperApp* app = context;

    if(event->key == InputKeyOk && event->type == InputTypeShort) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, MineSweeperSceneStartScreenContinueEvent);
        return true;
    }

    return false;
}

void minesweeper_scene_start_screen_secondary_draw_callback(Canvas* canvas, void* _model) {
    furi_assert(canvas);
    furi_assert(_model);
    UNUSED(_model);
    UNUSED(canvas);
}

void minesweeper_scene_start_screen_on_enter(void* context) {
    furi_assert(context);
    MineSweeperApp* app = context;

    furi_assert(app->start_screen);

    start_screen_set_context(app->start_screen, app);

    start_screen_set_input_callback(
        app->start_screen, minesweeper_scene_start_screen_input_callback);

    start_screen_set_secondary_draw_callback(
        app->start_screen, minesweeper_scene_start_screen_secondary_draw_callback);

    start_screen_set_icon_animation(app->start_screen, 0, 0, &A_StartScreen_128x64);

    view_dispatcher_switch_to_view(app->view_dispatcher, MineSweeperStartScreenView);
}

bool minesweeper_scene_start_screen_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);

    MineSweeperApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MineSweeperSceneStartScreenContinueEvent) {
            app->generation_origin = MineSweeperGenerationOriginStart;
            scene_manager_next_scene(app->scene_manager, MineSweeperSceneGenerating);
            consumed = true;
        }
    }

    return consumed;
}

void minesweeper_scene_start_screen_on_exit(void* context) {
    furi_assert(context);

    MineSweeperApp* app = context;
    start_screen_reset(app->start_screen);
}
