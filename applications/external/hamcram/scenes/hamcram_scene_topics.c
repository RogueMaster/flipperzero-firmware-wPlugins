#include "../hamcram.h"

/* Item 0 is the whole pool; item 1 + n is subelement n */

static unsigned topics_percent(HamCram* app, uint16_t first, uint16_t count) {
    ProgressSummary summary;
    progress_summarize(
        app->progress, app->pool, first, count, app->settings.skip_figures, &summary);
    return summary.total ? (unsigned)summary.mastered * 100 / summary.total : 0;
}

void hamcram_scene_topics_on_enter(void* context) {
    HamCram* app = context;
    Submenu* submenu = app->submenu;
    const Pool* pool = app->pool;

    submenu_set_header(submenu, "Study by Topic");
    furi_string_printf(
        app->text, "All Topics  %u%%", topics_percent(app, 0, pool->header.num_questions));
    submenu_add_item(submenu, furi_string_get_cstr(app->text), 0, hamcram_submenu_callback, app);

    for(uint8_t s = 0; s < pool->header.num_subelements; s++) {
        const PoolSubelement* sub = &pool->subelements[s];
        /* Percentage before the title, so a long title is what gets cut off */
        furi_string_printf(
            app->text,
            "%s %u%% %s",
            sub->id,
            topics_percent(app, sub->first_question, sub->num_questions),
            sub->title);
        submenu_add_item(
            submenu, furi_string_get_cstr(app->text), s + 1, hamcram_submenu_callback, app);
    }
    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, HamCramSceneTopics));

    view_dispatcher_switch_to_view(app->view_dispatcher, HamCramViewSubmenu);
}

bool hamcram_scene_topics_on_event(void* context, SceneManagerEvent event) {
    HamCram* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    scene_manager_set_scene_state(app->scene_manager, HamCramSceneTopics, event.event);
    if(event.event == 0) {
        hamcram_start_study(app, 0, app->pool->header.num_questions);
    } else if(event.event <= app->pool->header.num_subelements) {
        app->topic = event.event - 1;
        scene_manager_set_scene_state(app->scene_manager, HamCramSceneGroups, 0);
        scene_manager_next_scene(app->scene_manager, HamCramSceneGroups);
    }
    return true;
}

void hamcram_scene_topics_on_exit(void* context) {
    HamCram* app = context;
    submenu_reset(app->submenu);
}
