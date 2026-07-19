#include "../gauntlet_i.h"

/* next rank threshold + name above the given score; needed==0 means maxed */
static uint32_t rank_next(uint32_t score, const char** name) {
    const uint32_t th[] = {100, 350, 700, 1200, 2000, 3000};
    const char* nm[] = {"Initiate", "Operator", "Breaker", "Cipherpunk", "Ghost", "Legend"};
    for(uint8_t i = 0; i < 6; i++) {
        if(score < th[i]) {
            *name = nm[i];
            return th[i] - score;
        }
    }
    *name = "Legend";
    return 0;
}

void gauntlet_scene_progress_on_enter(void* context) {
    GauntletApp* app = context;
    Widget* widget = app->widget;
    widget_reset(widget);

    uint32_t score = app->progress->score;

    char scorebuf[12];
    snprintf(scorebuf, sizeof(scorebuf), "%lu", (unsigned long)score);

    char rankbuf[28];
    snprintf(rankbuf, sizeof(rankbuf), "Rank: %s", ctf_rank(score));

    char solvedbuf[32];
    snprintf(
        solvedbuf,
        sizeof(solvedbuf),
        "%u flag%s captured",
        app->progress->solved_count,
        app->progress->solved_count == 1 ? "" : "s");

    const char* nextname = "Legend";
    uint32_t need = rank_next(score, &nextname);
    char nextbuf[32];
    if(need == 0)
        snprintf(nextbuf, sizeof(nextbuf), "Top rank reached");
    else
        snprintf(nextbuf, sizeof(nextbuf), "%lu pts to %s", (unsigned long)need, nextname);

    widget_add_string_element(widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "Your Gauntlet");
    widget_add_string_element(
        widget, 64, 38, AlignCenter, AlignBottom, FontBigNumbers, scorebuf);
    widget_add_string_element(widget, 64, 47, AlignCenter, AlignBottom, FontSecondary, rankbuf);
    widget_add_string_element(
        widget, 64, 56, AlignCenter, AlignBottom, FontSecondary, solvedbuf);
    widget_add_string_element(widget, 64, 64, AlignCenter, AlignBottom, FontSecondary, nextbuf);

    view_dispatcher_switch_to_view(app->view_dispatcher, GauntletViewWidget);
}

bool gauntlet_scene_progress_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void gauntlet_scene_progress_on_exit(void* context) {
    GauntletApp* app = context;
    widget_reset(app->widget);
}
