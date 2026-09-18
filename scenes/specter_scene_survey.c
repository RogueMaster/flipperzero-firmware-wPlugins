#include "../specter_i.h"

/* A bounded sweep of a whole space: start it, walk the room, and get one verdict
 * at the end instead of having to watch a needle the entire time. */

static uint32_t survey_start_tick;
static uint32_t survey_total_ms;
static bool survey_finished;

static void specter_survey_restart_cb(void* context) {
    SpecterApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, SpecterCustomEventSurveyRestart);
}

static void specter_survey_finish_cb(void* context) {
    SpecterApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, SpecterCustomEventSurveyFinish);
}

static void specter_survey_begin(SpecterApp* app) {
    survey_total_ms = specter_settings_survey_seconds(&app->settings) * 1000u;
    survey_finished = false;

    specter_apply_threshold(app);
    field_detector_stop(app->detector); // a survey always starts from clean counters
    field_detector_start(app->detector);
    survey_start_tick = furi_get_tick();
}

void specter_scene_survey_on_enter(void* context) {
    SpecterApp* app = context;

    survey_view_set_restart_callback(app->survey_view, specter_survey_restart_cb, app);
    survey_view_set_finish_callback(app->survey_view, specter_survey_finish_cb, app);
    survey_view_reset(app->survey_view); // never show the last run's verdict
    specter_survey_begin(app);
    specter_stealth_enter(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, SpecterViewSurvey);
}

static void specter_survey_complete(SpecterApp* app, const FieldStats* st, uint32_t elapsed_ms) {
    SurveySummary summary = {
        .elapsed_ms = elapsed_ms,
        .in_field_ms = st->in_field_ms,
        .peak = st->peak,
        .peak_ref = st->peak_ref,
        .average = st->average,
        .contacts = st->contacts,
    };

    survey_finished = true;
    field_detector_stop(app->detector);
    survey_view_show_verdict(app->survey_view, &summary);

    SurveyVerdict verdict = survey_verdict(&summary);
    if(verdict == SurveyVerdictActive) {
        specter_notify_found(app);
    } else {
        specter_notify_gone(app);
    }

    if(app->settings.logging) {
        /* No on-screen claim is made about saving, so a failed write is not a
         * lie here - but discard the result deliberately, not by accident. */
        (void)specter_log_append(
            "SURVEY",
            "%lus %s max %u%% avg %u%% infield %u%% hits %lu m:%s",
            (unsigned long)(elapsed_ms / 1000u),
            survey_verdict_name(verdict),
            (unsigned)summary.peak,
            (unsigned)summary.average,
            (unsigned)survey_in_field_pct(&summary),
            (unsigned long)summary.contacts,
            specter_settings_meter_tag(&app->settings));
    }
}

bool specter_scene_survey_on_event(void* context, SceneManagerEvent event) {
    SpecterApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SpecterCustomEventSurveyRestart) {
            specter_survey_begin(app);
            consumed = true;
        } else if(event.event == SpecterCustomEventSurveyFinish) {
            /* Grade what we have. survey_verdict works off elapsed_ms and a
             * ratio, so a short walk is graded on its own terms - and the card
             * now prints the duration it was graded over. */
            if(!survey_finished) {
                FieldStats st;
                field_detector_get(app->detector, &st);
                if(!st.error)
                    specter_survey_complete(app, &st, furi_get_tick() - survey_start_tick);
            }
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        if(!survey_finished) {
            FieldStats st;
            field_detector_get(app->detector, &st);
            uint32_t elapsed = furi_get_tick() - survey_start_tick;

            /* A radio we never got hold of cannot survey anything; sit on the
             * error rather than counting down to a meaningless "CLEAN". */
            if(st.error) {
                survey_view_update_running(app->survey_view, &st, elapsed, survey_total_ms);
            } else if(elapsed >= survey_total_ms) {
                specter_survey_complete(app, &st, elapsed);
            } else {
                survey_view_update_running(app->survey_view, &st, elapsed, survey_total_ms);
            }
        }
        consumed = true;
    }
    return consumed;
}

void specter_scene_survey_on_exit(void* context) {
    SpecterApp* app = context;
    field_detector_stop(app->detector);
    specter_stealth_exit(app);
}
