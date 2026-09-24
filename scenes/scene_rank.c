#include "../sigroam.h"
#include "../src/sr_upload_prompt.h"

#include <stdio.h>

_Static_assert(sizeof("WiGLE rank") - 1u <= 20u, "rank title fits 20 columns");
_Static_assert(sizeof("No rank yet") - 1u <= 20u, "rank empty fits 20 columns");
_Static_assert(sizeof("upload once") - 1u <= 20u, "rank hint fits 20 columns");
_Static_assert(sizeof("not this trip") - 1u <= 20u, "rank footer fits 20 columns");
_Static_assert(sizeof("Rank ") - 1u + 10u <= 20u, "rank digits fit 20 columns");
_Static_assert(sizeof("Month ") - 1u + 10u <= 20u, "month digits fit 20 columns");
_Static_assert(sizeof("WiFi GPS ") - 1u + 10u <= 20u, "wifi gps digits fit 20 columns");
_Static_assert(
    (unsigned)SR_UPLOAD_STATUS_PERIOD_TICKS * (unsigned)SR_TICK_PERIOD_MS == 2000u,
    "rank uploadstatus stays on the 2 s cadence");

/* GUI thread only. ViewDispatcher does not enter this scene from the worker. */
static char s_rank_text[96];
static uint32_t s_rank_rev_shown;

static void sigroam_scene_rank_fill(const SigRoamApp* app) {
    const SrRankInfo* r = &app->model.rank;
    int n;

    if(app->model.rank_rev == 0u) {
        n = snprintf(
            s_rank_text,
            sizeof(s_rank_text),
            "WiGLE rank\n"
            "No rank yet\n"
            "upload once");
    } else {
        n = snprintf(
            s_rank_text,
            sizeof(s_rank_text),
            "WiGLE rank\n"
            "Rank %lu\n"
            "Month %lu\n"
            "WiFi GPS %lu\n"
            "not this trip",
            (unsigned long)r->rank,
            (unsigned long)r->month,
            (unsigned long)r->wifi_gps);
    }
    if(n < 0 || (size_t)n >= sizeof(s_rank_text)) {
        s_rank_text[0] = '\0';
    }
}

static void sigroam_scene_rank_draw(SigRoamApp* app) {
    sigroam_scene_rank_fill(app);
    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, SR_CANVAS_W, SR_CANVAS_H, s_rank_text);
    s_rank_rev_shown = app->model.rank_rev;
}

void sigroam_scene_rank_on_enter(void* context) {
    SigRoamApp* app = context;

    if(app->worker != NULL) {
        (void)sr_worker_send_cmd(app->worker, "uploadstatus\n");
    }
    sigroam_scene_rank_draw(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, SigRoamViewWidget);
}

bool sigroam_scene_rank_on_event(void* context, SceneManagerEvent event) {
    SigRoamApp* app = context;

    if(event.type != SceneManagerEventTypeTick) {
        return false;
    }
    if((app->tick_n % (uint32_t)SR_UPLOAD_STATUS_PERIOD_TICKS) == 0u && app->worker != NULL) {
        (void)sr_worker_send_cmd(app->worker, "uploadstatus\n");
    }
    if(s_rank_rev_shown != app->model.rank_rev) {
        sigroam_scene_rank_draw(app);
    }
    return true;
}

void sigroam_scene_rank_on_exit(void* context) {
    SigRoamApp* app = context;
    widget_reset(app->widget);
}
