#include "pocketlab_i.h"

#include <datetime/datetime.h>
#include <furi_hal_rtc.h>

// Monotonic day number from the RTC, used to track the daily streak.
static uint32_t pocketlab_today(void) {
    DateTime datetime;
    furi_hal_rtc_get_datetime(&datetime);
    return datetime_datetime_to_timestamp(&datetime) / 86400UL;
}

uint32_t pocketlab_level_for_xp(uint32_t xp) {
    uint32_t level = 1;
    while(xp >= level * level * 50) {
        level++;
    }
    return level;
}

const char* pocketlab_level_title(uint32_t level) {
    static const char* const titles[] = {
        "Novice",
        "Apprentice",
        "Explorer",
        "Tinkerer",
        "Adept",
        "Expert",
        "Master",
    };
    const uint32_t count = COUNT_OF(titles);
    const uint32_t index = level >= 1 ? level - 1 : 0;
    return titles[index < count ? index : count - 1];
}

bool pocketlab_is_lab_completed(const PocketLab* app, const PocketLabLab* lab) {
    const size_t index = lab - pocketlab_labs;
    return (app->state.completed_mask & (1ULL << index)) != 0;
}

uint8_t pocketlab_completed_count(const PocketLab* app) {
    uint8_t count = 0;
    for(size_t i = 0; i < pocketlab_labs_count; i++) {
        if(app->state.completed_mask & (1ULL << i)) {
            count++;
        }
    }
    return count;
}

bool pocketlab_award_lab(PocketLab* app, const PocketLabLab* lab) {
    // Bump the daily streak on any completion (new or review).
    const uint32_t today = pocketlab_today();
    if(today != app->state.last_day) {
        if(app->state.last_day != 0 && today == app->state.last_day + 1) {
            app->state.streak_days++;
        } else {
            app->state.streak_days = 1;
        }
        app->state.last_day = today;
    }

    bool leveled = false;
    const size_t index = lab - pocketlab_labs;
    const uint64_t bit = 1ULL << index;
    if(!(app->state.completed_mask & bit)) {
        app->state.completed_mask |= bit;
        app->state.xp += lab->xp;
        const uint32_t new_level = pocketlab_level_for_xp(app->state.xp);
        leveled = new_level > app->state.level;
        app->state.level = new_level;
    }

    pocketlab_storage_save(&app->state);
    return leveled;
}

void pocketlab_reset_progress(PocketLab* app) {
    app->state.xp = 0;
    app->state.level = 1;
    app->state.completed_mask = 0;
    app->state.streak_days = 0;
    app->state.last_day = 0;
    pocketlab_storage_save(&app->state);
}

static void pocketlab_lesson_done_callback(void* context) {
    PocketLab* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, PocketLabCustomEventLessonDone);
}

static void pocketlab_index_event_callback(void* context, uint32_t index) {
    PocketLab* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void pocketlab_open_badges_callback(void* context) {
    PocketLab* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, PocketLabCustomEventOpenBadges);
}

static void pocketlab_levelup_done_callback(void* context) {
    PocketLab* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, PocketLabCustomEventLevelUpDone);
}

static void pocketlab_exam_done_callback(void* context) {
    PocketLab* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, PocketLabCustomEventExamDone);
}

static bool pocketlab_custom_event_callback(void* context, uint32_t event) {
    PocketLab* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool pocketlab_back_event_callback(void* context) {
    PocketLab* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static PocketLab* pocketlab_alloc(void) {
    PocketLab* app = malloc(sizeof(PocketLab));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&pocketlab_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, pocketlab_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, pocketlab_back_event_callback);

    app->home_view = home_view_alloc();
    home_view_set_callback(app->home_view, pocketlab_index_event_callback, app);
    view_dispatcher_add_view(
        app->view_dispatcher, PocketLabViewHome, home_view_get_view(app->home_view));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, PocketLabViewWidget, widget_get_view(app->widget));

    app->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        PocketLabViewSettings,
        variable_item_list_get_view(app->variable_item_list));

    app->labs_list_view = labs_list_view_alloc();
    labs_list_view_set_callback(app->labs_list_view, pocketlab_index_event_callback, app);
    view_dispatcher_add_view(
        app->view_dispatcher, PocketLabViewLabs, labs_list_view_get_view(app->labs_list_view));

    app->progress_view = progress_view_alloc();
    progress_view_set_callback(app->progress_view, pocketlab_open_badges_callback, app);
    view_dispatcher_add_view(
        app->view_dispatcher, PocketLabViewProgress, progress_view_get_view(app->progress_view));

    app->badges_view = badges_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, PocketLabViewBadges, badges_view_get_view(app->badges_view));

    app->lesson_view = lesson_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, PocketLabViewLesson, lesson_view_get_view(app->lesson_view));
    lesson_view_set_done_callback(app->lesson_view, pocketlab_lesson_done_callback, app);

    app->about_view = about_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, PocketLabViewAbout, about_view_get_view(app->about_view));

    app->levelup_view = levelup_view_alloc();
    levelup_view_set_done_callback(app->levelup_view, pocketlab_levelup_done_callback, app);
    view_dispatcher_add_view(
        app->view_dispatcher, PocketLabViewLevelUp, levelup_view_get_view(app->levelup_view));

    app->exam_view = exam_view_alloc();
    exam_view_set_done_callback(app->exam_view, pocketlab_exam_done_callback, app);
    view_dispatcher_add_view(
        app->view_dispatcher, PocketLabViewExam, exam_view_get_view(app->exam_view));

    pocketlab_storage_load(&app->state);
    pocketlab_sound_configure_fx(app->state.led != 0, app->state.vibro != 0);
    app->current_lab = NULL;

    return app;
}

static void pocketlab_free(PocketLab* app) {
    view_dispatcher_remove_view(app->view_dispatcher, PocketLabViewHome);
    view_dispatcher_remove_view(app->view_dispatcher, PocketLabViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, PocketLabViewSettings);
    view_dispatcher_remove_view(app->view_dispatcher, PocketLabViewLabs);
    view_dispatcher_remove_view(app->view_dispatcher, PocketLabViewProgress);
    view_dispatcher_remove_view(app->view_dispatcher, PocketLabViewBadges);
    view_dispatcher_remove_view(app->view_dispatcher, PocketLabViewLesson);
    view_dispatcher_remove_view(app->view_dispatcher, PocketLabViewAbout);
    view_dispatcher_remove_view(app->view_dispatcher, PocketLabViewLevelUp);
    view_dispatcher_remove_view(app->view_dispatcher, PocketLabViewExam);

    home_view_free(app->home_view);
    widget_free(app->widget);
    variable_item_list_free(app->variable_item_list);
    labs_list_view_free(app->labs_list_view);
    progress_view_free(app->progress_view);
    badges_view_free(app->badges_view);
    lesson_view_free(app->lesson_view);
    about_view_free(app->about_view);
    levelup_view_free(app->levelup_view);
    exam_view_free(app->exam_view);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t pocketlab_app(void* p) {
    UNUSED(p);

    PocketLab* app = pocketlab_alloc();

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, PocketLabSceneMenu);
    view_dispatcher_run(app->view_dispatcher);

    pocketlab_free(app);
    return 0;
}
