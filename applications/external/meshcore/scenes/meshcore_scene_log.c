/*
 * scene_log — the passthrough debug view: every framed payload in and out,
 * as hex, newest at the bottom. Refreshed on the scene tick.
 */
#include "../meshcore_cfg.h"

static void meshcore_scene_log_refresh(MeshCoreApp* app) {
    /* TextBox keeps the pointer it is handed and the GUI thread may be drawing
     * from it right now, so build into the spare buffer and swap — never
     * mutate the string that is currently on screen. */
    app->text_buf_slot ^= 1u;
    FuriString* next = app->text_buf[app->text_buf_slot];

    meshcore_log_snapshot(app->log, next);
    if(furi_string_empty(next)) {
        furi_string_set_str(next, "No traffic yet.\n\nOpen Connect first.");
    }

    text_box_set_text(app->text_box, furi_string_get_cstr(next));
}

void meshcore_scene_log_on_enter(void* context) {
    MeshCoreApp* app = context;

    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontHex);
    text_box_set_focus(app->text_box, TextBoxFocusEnd);
    meshcore_scene_log_refresh(app);
    scene_manager_set_scene_state(
        app->scene_manager, MeshCoreSceneLog, meshcore_log_revision(app->log));

    view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewTextBox);
}

bool meshcore_scene_log_on_event(void* context, SceneManagerEvent event) {
    MeshCoreApp* app = context;

    if(event.type == SceneManagerEventTypeTick) {
        /* Refresh only when a line was actually added. Handing the TextBox new
         * text snaps it back to the focus position, so refreshing on every
         * tick makes scrolling back through the log impossible. */
        uint32_t shown = scene_manager_get_scene_state(app->scene_manager, MeshCoreSceneLog);
        uint32_t now = meshcore_log_revision(app->log);
        if(shown != now) {
            scene_manager_set_scene_state(app->scene_manager, MeshCoreSceneLog, now);
            meshcore_scene_log_refresh(app);
        }
        return true;
    }

    return false;
}

void meshcore_scene_log_on_exit(void* context) {
    MeshCoreApp* app = context;
    text_box_reset(app->text_box);
}
