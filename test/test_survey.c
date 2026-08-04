/* Host tests for the site-survey verdict layer.
 *
 *   make -C test
 *
 * The verdict is what a user actually acts on - "is this room clean?" - so the
 * boundaries between CLEAN, TRACE and ACTIVE are pinned down here rather than
 * discovered on the device. */

#include "../helpers/survey_verdict.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

static void check(int cond, const char* what) {
    checks++;
    if(!cond) {
        failures++;
        printf("  FAIL: %s\n", what);
    }
}

static void check_verdict(const SurveySummary* s, SurveyVerdict want, const char* what) {
    SurveyVerdict got = survey_verdict(s);
    checks++;
    if(got != want) {
        failures++;
        printf(
            "  FAIL: %s -> got %s, want %s\n",
            what,
            survey_verdict_name(got),
            survey_verdict_name(want));
    }
}

int main(void) {
    printf("survey_verdict\n");

    /* --- clean ---------------------------------------------------------- */
    {
        SurveySummary s = {60000, 0, 0, 0, 0};
        check_verdict(&s, SurveyVerdictClean, "a quiet minute");
    }
    {
        /* Noise that never crossed the floor leaves contacts at zero, so even a
         * non-zero peak reading is still a clean room. */
        SurveySummary s = {60000, 0, 9, 2, 0};
        check_verdict(&s, SurveyVerdictClean, "sub-threshold noise only");
    }
    {
        SurveyVerdict v = survey_verdict(NULL);
        check(v == SurveyVerdictClean, "NULL is safe");
        check(survey_in_field_pct(NULL) == 0, "NULL percentage is zero");
    }

    /* --- trace ---------------------------------------------------------- */
    {
        /* One faint blip in a minute: something happened, but not enough to
         * call it an active reader. */
        SurveySummary s = {60000, 1200, 22, 3, 1};
        check_verdict(&s, SurveyVerdictTrace, "one faint 2% blip");
    }
    {
        /* Just under both active thresholds on every axis. */
        SurveySummary s = {60000, 11400, 49, 20, 4};
        check_verdict(&s, SurveyVerdictTrace, "19% in-field, peak 49");
    }

    /* --- active --------------------------------------------------------- */
    {
        /* Crossing the duration threshold alone is enough. */
        SurveySummary s = {60000, 12000, 30, 18, 4};
        check_verdict(&s, SurveyVerdictActive, "20% in-field at modest strength");
    }
    {
        /* Crossing the strength threshold alone is enough, even for an instant:
         * nothing weak produces a reading that strong. */
        SurveySummary s = {60000, 600, 50, 4, 1};
        check_verdict(&s, SurveyVerdictActive, "brief but peak 50");
    }
    {
        SurveySummary s = {60000, 58000, 88, 71, 1};
        check_verdict(&s, SurveyVerdictActive, "reader up the whole time");
    }
    {
        /* Regression: a real polling reader you walked right up to. Its raw duty
         * saturates near 30%, which on the scaled meter is ~89. Before the meter
         * was scaled this arrived as peak 31 and the peak test could not fire -
         * a brief close pass over a skimmer could be filed as mere TRACE. */
        SurveySummary s = {60000, 4000, 89, 12, 2};
        check_verdict(&s, SurveyVerdictActive, "brief close pass on a polling reader");
    }

    /* --- in-field percentage -------------------------------------------- */
    {
        SurveySummary s = {60000, 15000, 0, 0, 0};
        check(survey_in_field_pct(&s) == 25, "15s of 60s is 25%");

        SurveySummary zero = {0, 0, 0, 0, 0};
        check(survey_in_field_pct(&zero) == 0, "no elapsed time does not divide by zero");

        /* A survey stopped mid-window can report more in-field than elapsed;
         * the percentage must still be sane rather than wrapping. */
        SurveySummary over = {1000, 4000, 0, 0, 0};
        check(survey_in_field_pct(&over) == 100, "over-long in-field clamps to 100%");

        /* Long surveys must not overflow the intermediate multiply. */
        SurveySummary lng = {3600000, 1800000, 0, 0, 0};
        check(survey_in_field_pct(&lng) == 50, "an hour-long survey still computes");
    }

    /* --- display strings ------------------------------------------------ */
    {
        for(int v = SurveyVerdictClean; v <= SurveyVerdictActive; v++) {
            const char* n = survey_verdict_name((SurveyVerdict)v);
            const char* a = survey_verdict_advice((SurveyVerdict)v);
            check(n && *n, "verdict name is non-empty");
            check(a && *a, "verdict advice is non-empty");
            /* The verdict banner budgets 13 characters, the advice line 21. */
            check(strlen(n) <= 13, "verdict name fits the banner");
            check(strlen(a) <= 21, "advice fits one screen line");
        }
    }

    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
