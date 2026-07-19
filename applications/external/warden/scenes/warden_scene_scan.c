#include "../warden_i.h"

void warden_scene_scan_on_enter(void* context) {
    WardenApp* app = context;

    scan_view_reset(app->scan_view);
    card_reader_start(app->reader);
    view_dispatcher_switch_to_view(app->view_dispatcher, WardenViewScan);
}

bool warden_scene_scan_on_event(void* context, SceneManagerEvent event) {
    WardenApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == WardenCustomEventCardRead) {
            scene_manager_next_scene(app->scene_manager, WardenSceneResult);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        scan_view_tick(app->scan_view);

        if(card_reader_get(app->reader, &app->reading)) {
            /* card in hand -> grade it and hand off to the result scene */
            grader_evaluate(&app->reading, &app->grade);
            app->have_result = true;
            warden_notify_graded(app, app->grade.band);
            view_dispatcher_send_custom_event(
                app->view_dispatcher, WardenCustomEventCardRead);
        }
        consumed = true;
    }
    return consumed;
}

void warden_scene_scan_on_exit(void* context) {
    WardenApp* app = context;
    card_reader_stop(app->reader);
}
