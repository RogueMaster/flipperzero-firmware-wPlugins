#include "../specter_i.h"

/* Which kind of finding do you want to look at?
 *
 * A logbook that has been running for a few sweeps is mostly noise when you are
 * after one thing - writing up a survey means skipping every SWEEP line, and
 * checking what a Watch caught overnight means skipping everything else. This
 * sits between the menu and the viewer so the answer is one keypress away. */

static const char* const filter_labels[] = {
    "Everything",
    "Sweep readings",
    "Readers found",
    "Site surveys",
    "Watch contacts",
};

/* Must line up with filter_labels; NULL means "no filtering". */
static const char* const filter_types[] = {NULL, "SWEEP", "READER", "SURVEY", "WATCH"};

const char* specter_log_filter_type(uint32_t index) {
    if(index >= COUNT_OF(filter_types)) return NULL;
    return filter_types[index];
}

const char* specter_log_filter_label(uint32_t index) {
    if(index >= COUNT_OF(filter_labels)) return filter_labels[0];
    return filter_labels[index];
}

static void specter_logfilter_cb(void* context, uint32_t index) {
    SpecterApp* app = context;
    /* The viewer reads this to know what to show. */
    scene_manager_set_scene_state(app->scene_manager, SpecterSceneLogbook, index);
    scene_manager_set_scene_state(app->scene_manager, SpecterSceneLogFilter, index);
    scene_manager_next_scene(app->scene_manager, SpecterSceneLogbook);
}

void specter_scene_logfilter_on_enter(void* context) {
    SpecterApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Logbook");
    for(uint32_t i = 0; i < COUNT_OF(filter_labels); i++) {
        submenu_add_item(submenu, filter_labels[i], i, specter_logfilter_cb, app);
    }
    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, SpecterSceneLogFilter));
    view_dispatcher_switch_to_view(app->view_dispatcher, SpecterViewSubmenu);
}

bool specter_scene_logfilter_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void specter_scene_logfilter_on_exit(void* context) {
    SpecterApp* app = context;
    submenu_reset(app->submenu);
}
