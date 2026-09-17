#include "sr_test.h"

#include "sr_capture_health.h"
#include "sr_view_fmt.h"
#include "sr_types.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int streq(const char* a, const char* b) {
    if(a == NULL || b == NULL) {
        return a == b;
    }
    return strcmp(a, b) == 0;
}

static SrQualInfo q_ok(void) {
    SrQualInfo q;
    memset(&q, 0, sizeof(q));
    q.gga = 100u;
    q.ggafix = 100u;
    q.drop = 0u;
    q.net = 142u;
    q.sd = 1u;
    q.sats = 8u;
    q.topd = 0u;
    return q;
}

int test_capture_health_run(void);

int test_capture_health_run(void) {
    SrQualInfo q;
    SrHealthEval e;
    char line[32];
    size_t n;

    sr_test_failures = 0;

    CHECK(sizeof(SrQualInfo) == 20u);
    CHECK((unsigned)SrEventQual == 10u);
    CHECK(FIXPCT_WARN_TBD > 0u);
    CHECK(FIXPCT_WARN_TBD < 100u);
    CHECK(DROP_RATE_WARN_PCT_TBD > 0u);
    CHECK(DROP_RATE_WARN_PCT_TBD < 100u);
    CHECK(ACQUIRE_GRACE_MS_TBD > SR_HEALTH_ACQUIRE_EPOCH_MS_FLOOR);

    /* Named: sd==0 is CRIT. NC half-B (2) lands here. Other fields look healthy so
     * reversing sd==0→CRIT cannot be masked by a later WARN. */
    q = q_ok();
    q.sd = 0u;
    e = sr_capture_health_eval(&q, 120000u);
    CHECK(e.v == SrHealthCrit);
    CHECK(e.reason == (uint8_t)SrHealthReasonSdDead);

    /* sd==0 short-circuits gga==0 past grace (would otherwise be WARN no-fix). */
    q = q_ok();
    q.sd = 0u;
    q.gga = 0u;
    q.ggafix = 0u;
    e = sr_capture_health_eval(&q, ACQUIRE_GRACE_MS_TBD);
    CHECK(e.v == SrHealthCrit);

    /* sd==1 healthy is OK — reversing the CRIT test would trip this too. */
    q = q_ok();
    e = sr_capture_health_eval(&q, 120000u);
    CHECK(e.v == SrHealthOk);
    CHECK(e.fixpct == 100u);

    /* ACQUIRE_GRACE both sides. gga==0, sd==1. */
    q = q_ok();
    q.gga = 0u;
    q.ggafix = 0u;
    e = sr_capture_health_eval(&q, 0u);
    CHECK(e.v == SrHealthAcquiring);
    CHECK(e.reason == (uint8_t)SrHealthReasonAcquiring);

    e = sr_capture_health_eval(&q, ACQUIRE_GRACE_MS_TBD - 1u);
    CHECK(e.v == SrHealthAcquiring);

    e = sr_capture_health_eval(&q, ACQUIRE_GRACE_MS_TBD);
    CHECK(e.v == SrHealthWarn);
    CHECK(e.reason == (uint8_t)SrHealthReasonNoFix);

    e = sr_capture_health_eval(&q, ACQUIRE_GRACE_MS_TBD + 1u);
    CHECK(e.v == SrHealthWarn);
    CHECK(e.reason == (uint8_t)SrHealthReasonNoFix);

    /* FIXPCT both sides. gga=100 so integer pct == ggafix. */
    q = q_ok();
    q.ggafix = (uint32_t)FIXPCT_WARN_TBD - 1u;
    e = sr_capture_health_eval(&q, 120000u);
    CHECK(e.v == SrHealthWarn);
    CHECK(e.reason == (uint8_t)SrHealthReasonLowFix);
    CHECK(e.fixpct == (uint8_t)(FIXPCT_WARN_TBD - 1u));

    q.ggafix = (uint32_t)FIXPCT_WARN_TBD;
    e = sr_capture_health_eval(&q, 120000u);
    CHECK(e.v == SrHealthOk);
    CHECK(e.fixpct == (uint8_t)FIXPCT_WARN_TBD);

    /* DROP_RATE both sides. 2/11=18 > 10; 1/10=10 not >; 1/9=11 > 10. */
    q = q_ok();
    q.ggafix = 100u;
    q.drop = 2u;
    q.net = 9u;
    q.topd = (uint8_t)SrHealthReasonGps;
    e = sr_capture_health_eval(&q, 120000u);
    CHECK(e.v == SrHealthWarn);
    CHECK(e.reason == (uint8_t)SrHealthReasonGps);

    q.drop = 1u;
    q.net = 9u;
    e = sr_capture_health_eval(&q, 120000u);
    CHECK(e.v == SrHealthOk);

    q.drop = 1u;
    q.net = 8u;
    q.topd = (uint8_t)SrHealthReasonScan;
    e = sr_capture_health_eval(&q, 120000u);
    CHECK(e.v == SrHealthWarn);
    CHECK(e.reason == (uint8_t)SrHealthReasonScan);

    /* den==0 skips drop-rate (would divide by zero). */
    q = q_ok();
    q.drop = 0u;
    q.net = 0u;
    e = sr_capture_health_eval(&q, 120000u);
    CHECK(e.v == SrHealthOk);

    /* NULL q does not CRIT (sd path needs a struct). */
    e = sr_capture_health_eval(NULL, 120000u);
    CHECK(e.v == SrHealthAcquiring);

    /* fmt: unseen is not zeros */
    n = sr_view_fmt_health(NULL, NULL, false, line, sizeof(line));
    CHECK(n > 0);
    CHECK(streq(line, "- need SigRoam Qual"));

    q = q_ok();
    e = sr_capture_health_eval(&q, 120000u);
    n = sr_view_fmt_health(&e, &q, true, line, sizeof(line));
    CHECK(n > 0);
    CHECK(streq(line, "OK fix100% 0drop 142net"));

    q.sd = 0u;
    e = sr_capture_health_eval(&q, 120000u);
    n = sr_view_fmt_health(&e, &q, true, line, sizeof(line));
    CHECK(streq(line, "SD not writing"));

    q = q_ok();
    q.gga = 0u;
    q.ggafix = 0u;
    e = sr_capture_health_eval(&q, 0u);
    n = sr_view_fmt_health(&e, &q, true, line, sizeof(line));
    CHECK(streq(line, "acquiring"));

    e = sr_capture_health_eval(&q, ACQUIRE_GRACE_MS_TBD);
    n = sr_view_fmt_health(&e, &q, true, line, sizeof(line));
    CHECK(streq(line, "WARN nofix sky"));

    q = q_ok();
    q.ggafix = (uint32_t)FIXPCT_WARN_TBD - 1u;
    e = sr_capture_health_eval(&q, 120000u);
    n = sr_view_fmt_health(&e, &q, true, line, sizeof(line));
    CHECK(line[0] == 'W');

    q = q_ok();
    q.drop = 2u;
    q.net = 9u;
    q.topd = (uint8_t)SrHealthReasonGps;
    e = sr_capture_health_eval(&q, 120000u);
    n = sr_view_fmt_health(&e, &q, true, line, sizeof(line));
    CHECK(streq(line, "WARN drop gps"));

    fprintf(stderr, "capture_health: ok\n");
    return sr_test_failures;
}
