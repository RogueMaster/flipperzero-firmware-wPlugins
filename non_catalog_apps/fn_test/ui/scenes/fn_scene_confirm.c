#include "../../fn_test_app_i.h"
#include "fn_scene.h"

#include <string.h>

/** Выбор в экране подтверждения: только custom event, без scene_manager. */
static void fn_scene_confirm_callback(void* context, bool confirmed) {
    furi_assert(context);
    FNApp* app = context;

    view_dispatcher_send_custom_event(
        app->view_dispatcher,
        confirmed ? FNAppCustomEventConfirmAccepted : FNAppCustomEventConfirmDeclined);
}

/** Отказ от подтверждения: возврат в меню без отправки команды (FR-013). */
static void fn_scene_confirm_decline(FNApp* app) {
    app->flash_preflight = false;
    scene_manager_search_and_switch_to_previous_scene(app->scene_manager, FNAppSceneStart);
}

void fn_scene_confirm_on_enter(void* context) {
    furi_assert(context);
    FNApp* app = context;

    fn_view_confirm_set(app->view_confirm, FN_STR_CONFIRM_TITLE, FN_STR_CONFIRM_BODY);
    fn_view_confirm_set_callback(app->view_confirm, fn_scene_confirm_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, FNAppViewConfirm);
}

bool fn_scene_confirm_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    FNApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == FNAppCustomEventConfirmAccepted) {
            /* Подтверждено: одна отправка 60h+16h в сцене ожидания (FR-012). */
            app->flash_preflight = false;
            app->result_reason = FnResultReasonOperation;
            memset(&app->op_result, 0, sizeof(app->op_result));
            app->pending_op = FnOpFlashMgm;
            scene_manager_next_scene(app->scene_manager, FNAppSceneWait);
            return true;
        }
        if(event.event == FNAppCustomEventConfirmDeclined) {
            fn_scene_confirm_decline(app);
            return true;
        }
    }

    if(event.type == SceneManagerEventTypeBack) {
        fn_scene_confirm_decline(app);
        return true;
    }

    return false;
}

void fn_scene_confirm_on_exit(void* context) {
    furi_assert(context);
    FNApp* app = context;
    /* Callback остаётся привязанным к view: контекст — само приложение,
     * повторная установка в on_enter идемпотентна. */
    UNUSED(app);
}
