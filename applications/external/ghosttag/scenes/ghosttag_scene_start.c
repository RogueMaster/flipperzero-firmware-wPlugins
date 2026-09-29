#include <stdio.h>
#include "../ghosttag_i.h"

/*
 * These values ARE the submenu's row indices - submenu_set_selected_item takes
 * a row, not an identifier - so a conditionally-added item must be
 * conditionally enumerated too. Leaving StartIndexAir in the enum while
 * omitting its row put every saved cursor position one row off.
 */
typedef enum {
    StartIndexHunt,
#if GHOSTTAG_ENABLE_AIR_CHECK
    StartIndexAir,
#endif
    StartIndexDemo,
    StartIndexDetections,
    StartIndexSettings,
    StartIndexAbout,
    StartIndexClear, /* only offered when there is something to clear */
    StartIndexCount,
} StartIndex;

static void ghosttag_scene_start_submenu_cb(void* context, uint32_t index) {
    GhostTagApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void ghosttag_scene_start_on_enter(void* context) {
    GhostTagApp* app = context;
    Submenu* submenu = app->submenu;

    /* Reaching the main menu ends the session, whichever source it was. The
     * detections survive so Detections can still review them - which is also
     * why the row says how many there are. */
    ghosttag_source_stop(app);

    size_t found = tracker_db_count(app->db);
    char detections[24];
    if(found > 0) {
        snprintf(
            detections,
            sizeof(detections),
            "Detections (%u)",
            (unsigned)(found > 999 ? 999 : found));
    } else {
        snprintf(detections, sizeof(detections), "Detections");
    }

    submenu_reset(submenu);
    submenu_set_header(submenu, "GhostTag");
    submenu_add_item(submenu, "Hunt", StartIndexHunt, ghosttag_scene_start_submenu_cb, app);
#if GHOSTTAG_ENABLE_AIR_CHECK
    /* Off by default - see the note in ghosttag_i.h. The onboard radio returns
     * a failed read for every sample on this firmware, so the screen can only
     * ever say NO READING. */
    submenu_add_item(
        submenu, "Air Check (onboard)", StartIndexAir, ghosttag_scene_start_submenu_cb, app);
#endif
    submenu_add_item(
        submenu, "Demo (no board)", StartIndexDemo, ghosttag_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, detections, StartIndexDetections, ghosttag_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "Settings", StartIndexSettings, ghosttag_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "Help & About", StartIndexAbout, ghosttag_scene_start_submenu_cb, app);
    /* Wiping the session is now something the user asks for, rather than
     * something that happens to them every time they restart a hunt. */
    if(found > 0) {
        submenu_add_item(
            submenu, "Clear detections", StartIndexClear, ghosttag_scene_start_submenu_cb, app);
    }

    uint32_t saved = scene_manager_get_scene_state(app->scene_manager, GhostTagSceneStart);
    /* The Clear row comes and goes, so a saved selection pointing at it after
     * the list emptied would land on nothing. */
    if(saved >= StartIndexCount || (saved == StartIndexClear && found == 0))
        saved = StartIndexHunt;
    submenu_set_selected_item(submenu, saved);

    view_dispatcher_switch_to_view(app->view_dispatcher, GhostTagViewSubmenu);
}

bool ghosttag_scene_start_on_event(void* context, SceneManagerEvent event) {
    GhostTagApp* app = context;

    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event >= StartIndexCount) return false;

    /* Only remember a row we actually acted on. */
    scene_manager_set_scene_state(app->scene_manager, GhostTagSceneStart, event.event);

    switch(event.event) {
    case StartIndexHunt:
        ghosttag_source_start(app, GhostTagSourceEsp32);
        scene_manager_next_scene(app->scene_manager, GhostTagSceneScan);
        return true;
#if GHOSTTAG_ENABLE_AIR_CHECK
    case StartIndexAir:
        scene_manager_next_scene(app->scene_manager, GhostTagSceneAir);
        return true;
#endif
    case StartIndexDemo:
        ghosttag_source_start(app, GhostTagSourceDemo);
        scene_manager_next_scene(app->scene_manager, GhostTagSceneScan);
        return true;
    case StartIndexDetections:
        scene_manager_next_scene(app->scene_manager, GhostTagSceneList);
        return true;
    case StartIndexSettings:
        scene_manager_next_scene(app->scene_manager, GhostTagSceneSettings);
        return true;
    case StartIndexAbout:
        scene_manager_next_scene(app->scene_manager, GhostTagSceneAbout);
        return true;
    case StartIndexClear:
        ghosttag_clear_detections(app);
        notification_message(app->notifications, &sequence_semi_success);
        /* Rebuild in place: the Clear row has to disappear now that there is
         * nothing left to clear, and the cursor must not be left on a row that
         * no longer exists. */
        scene_manager_set_scene_state(
            app->scene_manager, GhostTagSceneStart, StartIndexDetections);
        ghosttag_scene_start_on_enter(app);
        return true;
    default:
        return false;
    }
}

void ghosttag_scene_start_on_exit(void* context) {
    GhostTagApp* app = context;
    submenu_reset(app->submenu);
}
