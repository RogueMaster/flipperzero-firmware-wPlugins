#include "src/core/cfs_app_i.h"

// Help is a topic index: each item opens a detail page (cfs_scene_help_topic).
// Ids double as submenu ids and the value stashed for the detail scene; order
// matches the help_pages[] table in cfs_scene_help_topic.c. Raw 0-based ids are
// fine here (no NFC worker runs in the Help flow, so the worker-event aliasing
// that the Start menu guards against can't occur) - same as cfs_scene_tag_actions.
typedef enum {
    HelpTopicRead,
    HelpTopicWrite,
    HelpTopicCloneEdit,
    HelpTopicDual,
    HelpTopicSaved,
    HelpTopicErase,
    HelpTopicSettings,
    HelpTopicSafety,
} HelpTopic;

void cfs_scene_help_on_enter(void* context) {
    CfsApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    submenu_set_header(submenu, "Help");
    submenu_add_item(submenu, "Reading tags", HelpTopicRead, cfs_submenu_send_index, app);
    submenu_add_item(submenu, "Writing tags", HelpTopicWrite, cfs_submenu_send_index, app);
    submenu_add_item(submenu, "Clone & Edit", HelpTopicCloneEdit, cfs_submenu_send_index, app);
    submenu_add_item(submenu, "Dual tag (x2)", HelpTopicDual, cfs_submenu_send_index, app);
    submenu_add_item(submenu, "Saved Tags", HelpTopicSaved, cfs_submenu_send_index, app);
    submenu_add_item(submenu, "Erasing", HelpTopicErase, cfs_submenu_send_index, app);
    submenu_add_item(submenu, "Settings", HelpTopicSettings, cfs_submenu_send_index, app);
    submenu_add_item(submenu, "Safety & limits", HelpTopicSafety, cfs_submenu_send_index, app);
    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, CfsSceneHelp));
    view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewSubmenu);
}

bool cfs_scene_help_on_event(void* context, SceneManagerEvent event) {
    CfsApp* app = context;
    if(event.type == SceneManagerEventTypeCustom) {
        // Remember the cursor for when we come back, and hand the topic to the
        // detail scene via its scene state.
        scene_manager_set_scene_state(app->scene_manager, CfsSceneHelp, event.event);
        scene_manager_set_scene_state(app->scene_manager, CfsSceneHelpTopic, event.event);
        scene_manager_next_scene(app->scene_manager, CfsSceneHelpTopic);
        return true;
    }
    return false;
}

void cfs_scene_help_on_exit(void* context) {
    CfsApp* app = context;
    submenu_reset(app->submenu);
}
