#include "../ear_trainer_i.h"
#include "ear_scene.h"

void ear_scene_progress_on_enter(void* context) {
    EarTrainerApp* app = context;
    const EarProgress* p = &app->progress;
    Widget* widget = app->widget;
    char line[48];

    widget_reset(widget);
    widget_add_string_element(widget, 2, 9, AlignLeft, AlignBottom, FontPrimary, "Progress");

    uint32_t pct = p->answered ? (p->correct * 100) / p->answered : 0;
    snprintf(line, sizeof(line), "%lu%% of %lu", pct, p->answered);
    widget_add_string_element(widget, 126, 9, AlignRight, AlignBottom, FontSecondary, line);

    /* one compact row per mode: level reached and stars collected */
    static const char* const short_names[MODE_COUNT] = {"Asc", "Desc", "Mix", "Chord", "Scale"};
    for(uint8_t m = 0; m < MODE_COUNT; m++) {
        uint8_t levels = curriculum_level_count(m);
        uint8_t total_stars = 0;
        for(uint8_t l = 0; l < levels; l++)
            total_stars += p->stars[m][l];
        snprintf(
            line,
            sizeof(line),
            "%-5s  lvl %u/%u   %u/%u *",
            short_names[m],
            p->unlocked[m],
            levels,
            total_stars,
            (uint8_t)(levels * 3));
        widget_add_string_element(
            widget, 2, (uint8_t)(20 + m * 9), AlignLeft, AlignBottom, FontSecondary, line);
    }

    snprintf(line, sizeof(line), "best streak %u", p->best_streak);
    widget_add_string_element(widget, 126, 63, AlignRight, AlignBottom, FontSecondary, line);

    view_dispatcher_switch_to_view(app->view_dispatcher, EarViewWidget);
}

bool ear_scene_progress_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void ear_scene_progress_on_exit(void* context) {
    EarTrainerApp* app = context;
    widget_reset(app->widget);
}
