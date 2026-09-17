#include "../sigroam.h"

/*
 * Ordering rationale (T4.10: Wardrive was folded into the Dash tab's OK key, so the main
 * menu no longer has a separate Start/Stop page):
 *   D-1 Code layering: the pages that handle Tick are the continuous-observation pages
 *       (dash/raw); the zero-Tick ones are static pages (settings/about). Observation pages
 *       must come before static ones, which immediately rules out About first.
 *   D-2 Pain-point table in plan section 1: of 5 pain points, 3 land on Dashboard and 1
 *       used to land on Wardrive (now folded into the Dash tab).
 *   D-3 Task flow: Probe (once per outing) -> Dashboard (Start/Stop plus repeated checking)
 *       -> Raw (only on anomalies) -> Settings -> About.
 * What settles the ranking: while a scan runs, Back returns to the main menu without stopping
 * the scan (Stop is triggered only by OK on the Dash tab),
 * so the live dashboard must be reachable exactly when it needs looking at -- hence Dashboard first.
 *
 * The enum values deliberately match the display order (not required -- set_selected_item
 * matches on the index field, see the start_selected comment in sigroam.h); it just makes the
 * code line up with the screen at a glance.
 */
typedef enum {
    SigRoamStartItemDash = 0,
    SigRoamStartItemProbe,
    SigRoamStartItemRaw,
    SigRoamStartItemSettings,
    SigRoamStartItemAbout,
} SigRoamStartItem;

/* Submenu callback only enqueues. No lock, no scene switch here. */
static void sigroam_start_submenu_callback(void* context, uint32_t index) {
    SigRoamApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void sigroam_scene_start_on_enter(void* context) {
    SigRoamApp* app = context;

    submenu_reset(app->submenu);
    /*
     * The header is always the product name (user decision, 2026-08-31). Two reasons:
     *  (1) The main menu is a navigation page, not a status page -- serial status appears both
     *      on the Dash tab (hint + the rx= counter) and on the Probe page (full explanation,
     *      including "unplug USB, then exit and reopen"), so nothing is lost;
     *  (2) The Catalog hero image must be taken with qFlipper's built-in screenshot -> USB must
     *      be plugged in -> with USB plugged in OTG necessarily fails closed (V-053) -> the old
     *      code would put `SR [5V OFF]` in the hero image's header bar, which reads like an
     *      error; and the `rx=N d=N` the old code showed while io was open is a debug string,
     *      equally unsuitable.
     * sigroam_start_header() was removed along with it: its first branch `return "SigRoam"` was
     * **dead code** -- its only call site sat in the else branch where io is not open, where
     * sr_io_is_open() is necessarily false, so the main menu header had never once displayed
     * the product name.
     */
    submenu_set_header(app->submenu, "SigRoam");
    submenu_add_item(
        app->submenu, "Dashboard", SigRoamStartItemDash, sigroam_start_submenu_callback, app);
    submenu_add_item(
        app->submenu,
        "Probe firmware",
        SigRoamStartItemProbe,
        sigroam_start_submenu_callback,
        app);
    submenu_add_item(
        app->submenu, "Raw log", SigRoamStartItemRaw, sigroam_start_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Settings", SigRoamStartItemSettings, sigroam_start_submenu_callback, app);
    submenu_add_item(
        app->submenu, "About", SigRoamStartItemAbout, sigroam_start_submenu_callback, app);

    /* Must be called after every add_item: it walks items looking for a matching index, and
     * finds nothing on an empty list. */
    submenu_set_selected_item(app->submenu, (uint32_t)app->start_selected);

    view_dispatcher_switch_to_view(app->view_dispatcher, SigRoamViewSubmenu);
}

bool sigroam_scene_start_on_event(void* context, SceneManagerEvent event) {
    SigRoamApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        /* Remember this selection so the cursor stays put on returning to Start (saving
         * repeated down presses).
         * Range guard: this scene's custom events only come from the submenu callback, but do
         * not take that on trust -- out of range is simply not recorded.
         * app->start_selected is GUI-thread exclusive (the exclusive-field rule at
         * sigroam.h:90-96), so no lock is needed. */
        if(event.event <= (uint32_t)SigRoamStartItemAbout) {
            app->start_selected = (uint8_t)event.event;
        }
        if(event.event == SigRoamStartItemAbout) {
            scene_manager_next_scene(app->scene_manager, SigRoamSceneAbout);
            return true;
        }
        if(event.event == SigRoamStartItemProbe) {
            scene_manager_next_scene(app->scene_manager, SigRoamSceneProbe);
            return true;
        }
        if(event.event == SigRoamStartItemDash) {
            scene_manager_next_scene(app->scene_manager, SigRoamSceneDash);
            return true;
        }
        if(event.event == SigRoamStartItemRaw) {
            scene_manager_next_scene(app->scene_manager, SigRoamSceneRaw);
            return true;
        }
        if(event.event == SigRoamStartItemSettings) {
            scene_manager_next_scene(app->scene_manager, SigRoamSceneSettings);
            return true;
        }
    }

    /* Back not consumed: no previous scene → dispatcher exits. */
    return false;
}

void sigroam_scene_start_on_exit(void* context) {
    SigRoamApp* app = context;
    submenu_reset(app->submenu);
}
