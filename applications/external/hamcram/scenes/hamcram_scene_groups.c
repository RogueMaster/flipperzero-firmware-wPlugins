#include "../hamcram.h"

/* Item 0 is the whole subelement; item 1 + n is its group n */

void hamcram_scene_groups_on_enter(void* context) {
    HamCram* app = context;
    Submenu* submenu = app->submenu;
    const Pool* pool = app->pool;
    const PoolSubelement* sub = &pool->subelements[app->topic];
    ProgressSummary summary;

    snprintf(app->line[0], sizeof(app->line[0]), "%s %s", sub->id, sub->title);
    submenu_set_header(submenu, app->line[0]);

    progress_summarize(
        app->progress,
        pool,
        sub->first_question,
        sub->num_questions,
        app->settings.skip_figures,
        &summary);
    furi_string_printf(app->text, "All of %s  %u/%u", sub->id, summary.mastered, summary.total);
    submenu_add_item(submenu, furi_string_get_cstr(app->text), 0, hamcram_submenu_callback, app);

    for(uint8_t g = 0; g < sub->num_groups; g++) {
        const PoolGroup* group = &pool->groups[sub->first_group + g];
        progress_summarize(
            app->progress,
            pool,
            group->first_question,
            group->num_questions,
            app->settings.skip_figures,
            &summary);
        furi_string_printf(app->text, "%s  %u/%u", group->id, summary.mastered, summary.total);
        submenu_add_item(
            submenu, furi_string_get_cstr(app->text), g + 1, hamcram_submenu_callback, app);
    }
    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, HamCramSceneGroups));

    view_dispatcher_switch_to_view(app->view_dispatcher, HamCramViewSubmenu);
}

bool hamcram_scene_groups_on_event(void* context, SceneManagerEvent event) {
    HamCram* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    const PoolSubelement* sub = &app->pool->subelements[app->topic];
    scene_manager_set_scene_state(app->scene_manager, HamCramSceneGroups, event.event);
    if(event.event == 0) {
        hamcram_start_study(app, sub->first_question, sub->num_questions);
    } else if(event.event <= sub->num_groups) {
        const PoolGroup* group = &app->pool->groups[sub->first_group + event.event - 1];
        hamcram_start_study(app, group->first_question, group->num_questions);
    }
    return true;
}

void hamcram_scene_groups_on_exit(void* context) {
    HamCram* app = context;
    submenu_reset(app->submenu);
}
