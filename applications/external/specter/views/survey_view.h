#pragma once

#include <gui/view.h>
#include "../helpers/field_detector.h"
#include "../helpers/survey_verdict.h"

typedef struct SurveyView SurveyView;

typedef void (*SurveyViewCallback)(void* context);

SurveyView* survey_view_alloc(void);
void survey_view_free(SurveyView* v);
View* survey_view_get_view(SurveyView* v);

/* OK restarts the survey (available in both states). */
void survey_view_set_restart_callback(SurveyView* v, SurveyViewCallback cb, void* ctx);

/* Survey in progress: live stats plus how far through we are. */
void survey_view_update_running(
    SurveyView* v,
    const FieldStats* stats,
    uint32_t elapsed_ms,
    uint32_t total_ms);

/* Survey finished: freeze on the verdict card. */
void survey_view_show_verdict(SurveyView* v, const SurveySummary* summary);

/* Short OK while the survey is running: end it now and grade what we have. */
void survey_view_set_finish_callback(SurveyView* v, SurveyViewCallback cb, void* ctx);

/* Clears the card. Call on scene entry so a re-entry cannot draw the previous
 * run's verdict before the first tick arrives. */
void survey_view_reset(SurveyView* v);
