#include "../glitch_trigger_i.h"

/* Shows the fault map accumulated by the last sweep. OK exports it to CSV. */

static void glitch_scene_faultmap_cb(FaultmapViewEvent event, void* ctx) {
    GlitchApp* app = ctx;
    if(event == FaultmapViewEventExport) {
        view_dispatcher_send_custom_event(app->view_dispatcher, 0);
    }
}

void glitch_scene_faultmap_on_enter(void* context) {
    GlitchApp* app = context;
    faultmap_view_set_callback(app->faultmap_view, glitch_scene_faultmap_cb, app);
    faultmap_view_set_map(app->faultmap_view, &app->map);
    view_dispatcher_switch_to_view(app->view_dispatcher, GlitchViewFaultmap);
}

bool glitch_scene_faultmap_on_event(void* context, SceneManagerEvent event) {
    GlitchApp* app = context;
    if(event.type == SceneManagerEventTypeCustom) {
        bool ok = glitch_map_export(&app->map);
        if(ok) {
            glitch_notify_hit(app);
        } else {
            glitch_notify_disarm(app);
        }
        return true;
    }
    return false;
}

void glitch_scene_faultmap_on_exit(void* context) {
    UNUSED(context);
}
