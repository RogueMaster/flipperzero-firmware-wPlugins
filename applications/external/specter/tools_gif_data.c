/* Emits the numbers the demo GIF should show, computed by the app's own logic.
 *
 * The animation used to be drawn from numbers I picked by hand, and they were
 * not physically possible: it showed a field of 81% while still claiming to be
 * SCANNING with zero contacts, when in reality anything above the noise floor
 * latches presence and flips the screen to the alarm strip immediately.
 *
 * So the trajectory below is a plausible physical story - walking up to a
 * contactless terminal, resting on it, walking away - and every displayed value
 * is derived by linking the real helpers: the same EMA the detector applies, the
 * same presence latch, the same meter scaling, the same proximity vocabulary,
 * the same classifier and the same survey verdict. If the app changes, this
 * changes with it.
 *
 *   cc -I helpers -o /tmp/gifdata tools_gif_data.c helpers/field_scale.c \
 *      helpers/emitter_classify.c helpers/survey_verdict.c
 */
#include "ema.h"
#include "emitter_classify.h"
#include "field_scale.h"
#include "present_hold.h"
#include "survey_verdict.h"

#include <stdio.h>

/* Mirrors helpers/field_detector.c: one entry per ~96 ms strength window. */
#define WINDOW_MS 96u
#define THRESHOLD 7u /* a realistic post-calibration Custom noise floor */

/* Raw carrier duty per window: quiet room, approach, resting on the reader,
 * then withdrawing. A real terminal saturates around 30-32%. */
static const unsigned traj[] = {
    3,  2,  4,  3,  5,  2,  3,  4,  2,  3, /* quiet */
    6,  9,  13, 17, 21, 25, 28, 30, /* closing in */
    31, 32, 31, 30, 32, 31, 30, 31, 32, 31, /* resting on it */
    26, 19, 12, 6,  3,  2,  3,  2, /* walking away */
};

int main(void) {
    const uint8_t FS = SPECTER_FULL_SCALE_DUTY;
    PresentHold hold;
    present_hold_reset(&hold);

    Ema smoother;
    ema_reset(&smoother);
    unsigned ema = 0, peak = 0, contacts = 0, in_field_ms = 0, shown_sum = 0;
    bool was_present = false;
    unsigned n = sizeof(traj) / sizeof(*traj);

    /* A reader whose measured duty is ~31% at a 204 ms poll period must be
     * emitting for ~63 ms of each cycle - the burst and the duty have to agree,
     * or the fingerprint screen contradicts the meter next to it. */
    CadenceStats cad = {16, 63, 141, 204, 2, 0};

    uint8_t hist[64] = {0};
    uint8_t head = 0;

    printf("# raw ema shown peak present saturated contacts trend word\n");
    for(unsigned i = 0; i < n; i++) {
        unsigned duty = traj[i];
        ema = ema_update(&smoother, (uint8_t)duty); /* the detector's own filter */

        uint32_t now = i * WINDOW_MS;
        bool present =
            present_hold_update(&hold, duty > THRESHOLD, now, present_hold_ms_for(cad.period_ms));

        uint8_t shown = field_scale_apply((uint8_t)ema, FS);
        bool sat = field_scale_is_saturated((uint8_t)ema, FS);
        if(shown > peak) peak = shown;
        if(present && !was_present) contacts++;
        was_present = present;

        in_field_ms += (WINDOW_MS * duty) / 100u;
        shown_sum += shown;

        head = (uint8_t)((head + 1u) % 64u);
        hist[head] = shown;

        printf(
            "%u %u %u %u %d %d %u %d %s\n",
            duty,
            ema,
            shown,
            peak,
            present,
            sat,
            contacts,
            field_trend(hist, head, 64),
            field_proximity_word(shown, sat));
    }

    /* The fingerprint screen, judged by the real classifier. */
    cad.duty = (uint8_t)ema;
    cad.duty = 31; /* the reading while it was actually held on the reader */
    EmitterVerdict v = emitter_classify(&cad);
    printf(
        "FP %s|%s|%u|%u|%u|%u|%u|%d\n",
        emitter_class_name(v.klass),
        emitter_class_blurb(v.klass),
        v.confidence,
        cad.period_ms,
        cad.burst_ms,
        cad.jitter_ms,
        cad.duty,
        v.timing_reliable);

    /* The survey card, judged by the real verdict layer. */
    SurveySummary sum = {
        .elapsed_ms = n * WINDOW_MS,
        .in_field_ms = in_field_ms,
        .peak = (uint8_t)peak,
        .average = (uint8_t)(shown_sum / n),
        .contacts = contacts,
    };
    SurveyVerdict sv = survey_verdict(&sum);
    printf(
        "SV %s|%s|%u|%u|%u|%lu\n",
        survey_verdict_name(sv),
        survey_verdict_advice(sv),
        sum.peak,
        sum.average,
        survey_in_field_pct(&sum),
        (unsigned long)sum.contacts);
    return 0;
}
