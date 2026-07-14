#include "../dallas_tester_i.h"

typedef enum {
    SubmenuIndexTest,
    SubmenuIndexAbout,
} SubmenuIndex;

static void dallas_tester_scene_start_submenu_callback(void* context, uint32_t index) {
    DallasTester* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void dallas_tester_scene_start_on_enter(void* context) {
    DallasTester* app = context;
    Submenu* submenu = app->submenu;

    submenu_set_header(submenu, "Dallas Tester");
    submenu_add_item(
        submenu, "Test", SubmenuIndexTest, dallas_tester_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu, "About", SubmenuIndexAbout, dallas_tester_scene_start_submenu_callback, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, DallasTesterSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, DallasTesterViewSubmenu);
}

bool dallas_tester_scene_start_on_event(void* context, SceneManagerEvent event) {
    DallasTester* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, DallasTesterSceneStart, event.event);
        if(event.event == SubmenuIndexTest) {
            scene_manager_next_scene(app->scene_manager, DallasTesterSceneTest);
            consumed = true;
        } else if(event.event == SubmenuIndexAbout) {
            scene_manager_next_scene(app->scene_manager, DallasTesterSceneAbout);
            consumed = true;
        }
    }

    return consumed;
}

void dallas_tester_scene_start_on_exit(void* context) {
    DallasTester* app = context;
    submenu_reset(app->submenu);
}
