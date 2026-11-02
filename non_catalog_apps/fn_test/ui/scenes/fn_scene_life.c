#include "../../fn_test_app_i.h"
#include "fn_scene.h"

#include "../fn_format.h"

void fn_scene_life_on_enter(void* context) {
    furi_assert(context);
    FNApp* app = context;

    fn_format_life(app->fn_life_info, &app->text_model);
    fn_view_text_set_model(app->view_text, &app->text_model);
    view_dispatcher_switch_to_view(app->view_dispatcher, FNAppViewText);
}

bool fn_scene_life_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    FNApp* app = context;

    /* Back возвращает в меню независимо от позиции прокрутки (FR-018, FR-025). */
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, FNAppSceneStart);
        return true;
    }
    return false;
}

void fn_scene_life_on_exit(void* context) {
    furi_assert(context);
    UNUSED(context);
}
