#include "scene_call_detail.h"

#include "../api_caller.h"

#define SCENE_CALL_DETAIL_TEXT            \
    "Dettaglio chiamata\n\n"              \
    "TODO\n"                              \
    "Invio della richiesta e log della\n" \
    "risposta a schermo.\n"               \
    "\n"                                  \
    "Implementazione prevista nello Sprint 4."

void api_caller_scene_call_detail_on_enter(void* context) {
    furi_assert(context);
    AppContext* app = context;

    text_box_reset(app->text_box);
    text_box_set_text(app->text_box, SCENE_CALL_DETAIL_TEXT);

    view_dispatcher_switch_to_view(app->view_dispatcher, ApiCallerViewTextBox);
}

bool api_caller_scene_call_detail_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void api_caller_scene_call_detail_on_exit(void* context) {
    furi_assert(context);
    AppContext* app = context;
    text_box_reset(app->text_box);
}
