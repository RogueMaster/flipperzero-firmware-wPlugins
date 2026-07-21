#include "../ear_trainer_i.h"
#include "ear_scene.h"

void ear_scene_progress_on_enter(void* context) {
    EarTrainerApp* app = context;
    const EarProgress* p = &app->progress;
    Widget* widget = app->widget;
    char line[48];

    widget_reset(widget);
    widget_add_string_element(widget, 64, 11, AlignCenter, AlignBottom, FontPrimary, "Progress");

    static const char* const mode_names[MODE_COUNT] = {"Asc", "Desc", "Mix"};
    for(uint8_t m = 0; m < MODE_COUNT; m++) {
        uint8_t total_stars = 0;
        for(uint8_t l = 0; l < LEVEL_COUNT; l++)
            total_stars += p->stars[m][l];
        snprintf(
            line,
            sizeof(line),
            "%-5s level %u/%u   %u/%u stars",
            mode_names[m],
            p->unlocked[m],
            LEVEL_COUNT,
            total_stars,
            LEVEL_COUNT * 3);
        widget_add_string_element(
            widget, 2, 23 + m * 10, AlignLeft, AlignCenter, FontSecondary, line);
    }

    uint32_t pct = p->answered ? (p->correct * 100) / p->answered : 0;
    snprintf(line, sizeof(line), "%lu/%lu right (%lu%%)", p->correct, p->answered, pct);
    widget_add_string_element(widget, 2, 55, AlignLeft, AlignCenter, FontSecondary, line);

    snprintf(line, sizeof(line), "best streak %u", p->best_streak);
    widget_add_string_element(widget, 126, 55, AlignRight, AlignCenter, FontSecondary, line);

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
