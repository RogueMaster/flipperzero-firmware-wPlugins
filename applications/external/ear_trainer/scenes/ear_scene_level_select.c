#include "../ear_trainer_i.h"
#include "ear_scene.h"

static void level_callback(void* context, uint32_t index) {
    EarTrainerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void ear_scene_level_select_on_enter(void* context) {
    EarTrainerApp* app = context;
    Submenu* submenu = app->submenu;
    uint8_t unlocked = app->progress.unlocked[app->mode];
    uint8_t levels = curriculum_level_count(app->mode);

    submenu_reset(submenu);
    submenu_set_header(submenu, mode_name(app->mode));

    for(uint8_t i = 0; i < levels; i++) {
        const EarLevel* level = curriculum_get(app->mode, i);
        char label[32];
        if(i < unlocked) {
            uint8_t stars = app->progress.stars[app->mode][i];
            /* stars render as filled/empty asterisks so progress reads at a glance */
            char star_buf[4] = {0};
            for(uint8_t s = 0; s < 3; s++)
                star_buf[s] = (s < stars) ? '*' : '.';
            snprintf(label, sizeof(label), "%u. %s  %s", i + 1, level->label, star_buf);
        } else {
            snprintf(label, sizeof(label), "%u. locked", i + 1);
        }
        submenu_add_item(submenu, label, i, level_callback, app);
    }

    /* start on the first level you have not cleared yet */
    uint8_t focus = unlocked - 1;
    for(uint8_t i = 0; i < unlocked; i++) {
        if(app->progress.stars[app->mode][i] == 0) {
            focus = i;
            break;
        }
    }
    submenu_set_selected_item(submenu, focus);

    view_dispatcher_switch_to_view(app->view_dispatcher, EarViewSubmenu);
}

bool ear_scene_level_select_on_event(void* context, SceneManagerEvent event) {
    EarTrainerApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    uint8_t level = event.event;
    if(level >= app->progress.unlocked[app->mode]) return true; /* locked: ignore */

    app->level = level;
    /* levels that introduce something new explain it first */
    if(curriculum_get(app->mode, level)->new_count > 0) {
        app->teach_index = 0;
        scene_manager_next_scene(app->scene_manager, EarSceneTeach);
    } else {
        scene_manager_next_scene(app->scene_manager, EarSceneQuiz);
    }
    return true;
}

void ear_scene_level_select_on_exit(void* context) {
    EarTrainerApp* app = context;
    submenu_reset(app->submenu);
}
