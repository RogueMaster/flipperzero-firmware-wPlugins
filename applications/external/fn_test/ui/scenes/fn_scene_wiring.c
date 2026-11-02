#include "../../fn_test_app_i.h"
#include "fn_scene.h"

/**
 * Справочный экран «Распиновка» (FR-016): полноэкранная иконка
 * `icons/Wiring_UART_128x64.png`. Воркер и UART не используются — экран
 * открывается без подключённого ФН.
 */
void fn_scene_wiring_on_enter(void* context) {
    furi_assert(context);
    FNApp* app = context;

    fn_view_text_set_image(app->view_text, &I_Wiring_UART_128x64);
    view_dispatcher_switch_to_view(app->view_dispatcher, FNAppViewText);
}

bool fn_scene_wiring_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    FNApp* app = context;

    /* Back возвращает в меню (FR-018). */
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, FNAppSceneStart);
        return true;
    }
    return false;
}

void fn_scene_wiring_on_exit(void* context) {
    furi_assert(context);
    UNUSED(context);
}
