#define SR_HOST_TEST 1

#include "sr_test.h"

#include "sr_capture_health.h"
#include "sr_view_fmt.h"
#include "sr_types.h"
#include "../../views/sr_view_dash.h" /* SR_HEALTH_COLS_MAX (host-testable half) */

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

/*
 * D19 / ADR-025 status-bar oracle: builds the expected copy with snprintf,
 * deliberately a different code path than sr_view_fmt_health's sr_fmt__cpy
 * ladder. Mirrors the copy table pinned in sr_view_fmt.h's doc comment.
 */
static void oracle_health(
    const SrHealthEval* e, const SrQualInfo* q, bool fresh, char* out, size_t cap) {
    static const char* const k_topd[7] = {"?", "gps", "sd", "link", "queue", "scan", "dedup"};
    char sat[8];
    unsigned pct;

    if(!fresh || e == NULL) {
        snprintf(out, cap, "- need SigRoam Qual");
        return;
    }
    if(q != NULL && q->sats < 100u) {
        snprintf(sat, sizeof(sat), "%02u", (unsigned)q->sats);
    } else {
        snprintf(sat, sizeof(sat), "%u", q != NULL ? (unsigned)q->sats : 0u);
    }
    pct = e->fixpct;
    if(pct > 100u) {
        pct = 100u;
    }
    if(e->v == SrHealthCrit) {
        snprintf(out, cap, "SD not writing");
    } else if(e->v == SrHealthAcquiring) {
        snprintf(out, cap, "acquiring SAT %s", sat);
    } else if(e->v == SrHealthOk) {
        snprintf(out, cap, "OK fix%u%% %udrop SAT %s", pct, q != NULL ? (unsigned)q->drop : 0u, sat);
    } else if(e->reason == (uint8_t)SrHealthReasonNoFix) {
        snprintf(out, cap, "WARN nofix SAT %s", sat);
    } else if(e->reason == (uint8_t)SrHealthReasonLowFix) {
        snprintf(out, cap, "WARN fix%u%% SAT %s", pct, sat);
    } else {
        unsigned r = e->reason;
        snprintf(out, cap, "WARN drop %s SAT %s", k_topd[r <= 6u ? r : 0u], sat);
    }
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
    CHECK(streq(line, "OK fix100% 0drop SAT 08"));

    q.sd = 0u;
    e = sr_capture_health_eval(&q, 120000u);
    n = sr_view_fmt_health(&e, &q, true, line, sizeof(line));
    CHECK(streq(line, "SD not writing"));

    q = q_ok();
    q.gga = 0u;
    q.ggafix = 0u;
    e = sr_capture_health_eval(&q, 0u);
    n = sr_view_fmt_health(&e, &q, true, line, sizeof(line));
    CHECK(streq(line, "acquiring SAT 08"));

    e = sr_capture_health_eval(&q, ACQUIRE_GRACE_MS_TBD);
    n = sr_view_fmt_health(&e, &q, true, line, sizeof(line));
    CHECK(streq(line, "WARN nofix SAT 08"));

    q = q_ok();
    q.ggafix = (uint32_t)FIXPCT_WARN_TBD - 1u;
    e = sr_capture_health_eval(&q, 120000u);
    n = sr_view_fmt_health(&e, &q, true, line, sizeof(line));
    CHECK(streq(line, "WARN fix89% SAT 08"));

    q = q_ok();
    q.drop = 2u;
    q.net = 9u;
    q.topd = (uint8_t)SrHealthReasonGps;
    e = sr_capture_health_eval(&q, 120000u);
    n = sr_view_fmt_health(&e, &q, true, line, sizeof(line));
    CHECK(streq(line, "WARN drop gps SAT 08"));

    /* ---- D19 / ADR-025: status-bar copy matrix + mark gate ----
     * Every verdict x sats {0,9,10,99,100,255} x drop {0,7,UINT32_MAX}
     * (drop only shows on the OK row; elsewhere it must not leak in).
     * Compared against the snprintf oracle, not hand-mirrored strings. */
    {
        static const uint8_t k_sats[6] = {0u, 9u, 10u, 99u, 100u, 255u};
        static const uint32_t k_drop[3] = {0u, 7u, 4294967295u};
        char got[40];
        char exp[40];
        char fitted[40];
        size_t fn;
        unsigned cov_ok = 0;
        unsigned cov_lowfix = 0;
        unsigned cov_nofix = 0;
        unsigned cov_drop = 0;
        unsigned cov_crit = 0;
        unsigned cov_acq = 0;
        unsigned cov_stale = 0;
        unsigned cov_sat3 = 0;
        unsigned cov_drop_big = 0;
        unsigned cov_fit_cut = 0;
        unsigned cov_topd[7] = {0u, 0u, 0u, 0u, 0u, 0u, 0u};
        unsigned si;
        unsigned di;
        unsigned ri;

        /* Review NIT-B: every produced row must survive the status-bar
         * character budget once fitted (worst raw string is 33 cols). */
#define D19_FIT_CHECK()                                                        \
    do {                                                                       \
        fn = sr_fmt_fit(got, n, (size_t)SR_HEALTH_COLS_MAX, fitted, sizeof(fitted)); \
        CHECK(fn <= (size_t)SR_HEALTH_COLS_MAX);                               \
        if(n > (size_t)SR_HEALTH_COLS_MAX) {                                   \
            CHECK(strchr(fitted, '~') != NULL);                                \
            cov_fit_cut++;                                                     \
        }                                                                      \
    } while(0)

        for(si = 0u; si < 6u; si++) {
            for(di = 0u; di < 3u; di++) {
                SrQualInfo qq;
                SrHealthEval ev;

                memset(&qq, 0, sizeof(qq));
                qq.sd = 1u;
                qq.sats = k_sats[si];
                qq.drop = k_drop[di];
                qq.net = 142u;

                /* OK */
                memset(&ev, 0, sizeof(ev));
                ev.v = SrHealthOk;
                ev.reason = (uint8_t)SrHealthReasonNone;
                ev.fixpct = 100u;
                n = sr_view_fmt_health(&ev, &qq, true, got, sizeof(got));
                oracle_health(&ev, &qq, true, exp, sizeof(exp));
                CHECK(n == strlen(exp));
                CHECK(streq(got, exp));
                CHECK(strstr(got, "net") == NULL); /* net left the copy (ADR-025) */
                D19_FIT_CHECK();
                cov_ok++;

                /* WARN low-fix */
                ev.v = SrHealthWarn;
                ev.reason = (uint8_t)SrHealthReasonLowFix;
                ev.fixpct = 89u;
                n = sr_view_fmt_health(&ev, &qq, true, got, sizeof(got));
                oracle_health(&ev, &qq, true, exp, sizeof(exp));
                CHECK(streq(got, exp));
                D19_FIT_CHECK();
                cov_lowfix++;

                /* WARN no-fix (D19 gap row, pinned by the main session 2026-09-17) */
                ev.v = SrHealthWarn;
                ev.reason = (uint8_t)SrHealthReasonNoFix;
                ev.fixpct = 0u;
                n = sr_view_fmt_health(&ev, &qq, true, got, sizeof(got));
                oracle_health(&ev, &qq, true, exp, sizeof(exp));
                CHECK(streq(got, exp));
                D19_FIT_CHECK();
                cov_nofix++;

                /* CRIT: no SAT suffix by design */
                ev.v = SrHealthCrit;
                ev.reason = (uint8_t)SrHealthReasonSdDead;
                ev.fixpct = 0u;
                n = sr_view_fmt_health(&ev, &qq, true, got, sizeof(got));
                oracle_health(&ev, &qq, true, exp, sizeof(exp));
                CHECK(streq(got, exp));
                CHECK(strstr(got, "SAT") == NULL);
                D19_FIT_CHECK();
                cov_crit++;

                /* Acquiring */
                ev.v = SrHealthAcquiring;
                ev.reason = (uint8_t)SrHealthReasonAcquiring;
                n = sr_view_fmt_health(&ev, &qq, true, got, sizeof(got));
                oracle_health(&ev, &qq, true, exp, sizeof(exp));
                CHECK(streq(got, exp));
                D19_FIT_CHECK();
                cov_acq++;

                /* stale: unknown copy, no SAT, mark folds to the ring */
                n = sr_view_fmt_health(&ev, &qq, false, got, sizeof(got));
                CHECK(streq(got, "- need SigRoam Qual"));
                CHECK(strstr(got, "SAT") == NULL);
                D19_FIT_CHECK();
                cov_stale++;

                if(k_sats[si] >= 100u) {
                    cov_sat3++;
                }
                if(k_drop[di] > 1000u) {
                    cov_drop_big++;
                }
            }
        }

        /* WARN drop x all six topd reasons x all sats (drop pinned big to
         * prove it cannot leak into this copy). */
        for(ri = 1u; ri <= 6u; ri++) {
            for(si = 0u; si < 6u; si++) {
                SrQualInfo qq;
                SrHealthEval ev;

                memset(&qq, 0, sizeof(qq));
                qq.sd = 1u;
                qq.sats = k_sats[si];
                qq.drop = 4294967295u;
                memset(&ev, 0, sizeof(ev));
                ev.v = SrHealthWarn;
                ev.reason = (uint8_t)ri;
                n = sr_view_fmt_health(&ev, &qq, true, got, sizeof(got));
                oracle_health(&ev, &qq, true, exp, sizeof(exp));
                CHECK(streq(got, exp));
                D19_FIT_CHECK();
                cov_drop++;
                cov_topd[ri]++;
            }
        }

        /* Mark gate matrix: fresh x verdict identity, stale always the ring.
         * Table lookup, not the implementation's if. */
        {
            static const uint8_t k_verdicts[5] = {
                (uint8_t)SrHealthAcquiring,
                (uint8_t)SrHealthOk,
                (uint8_t)SrHealthWarn,
                (uint8_t)SrHealthCrit,
                255u /* out of range: fresh passes it through untouched */
            };
            static const uint8_t k_mark[2][5] = {
                {(uint8_t)SrHealthAcquiring, (uint8_t)SrHealthAcquiring,
                 (uint8_t)SrHealthAcquiring, (uint8_t)SrHealthAcquiring,
                 (uint8_t)SrHealthAcquiring},
                {(uint8_t)SrHealthAcquiring, (uint8_t)SrHealthOk,
                 (uint8_t)SrHealthWarn, (uint8_t)SrHealthCrit, 255u}
            };
            unsigned cov_mark = 0;
            unsigned fi;
            unsigned vi;

            for(fi = 0u; fi < 2u; fi++) {
                for(vi = 0u; vi < 5u; vi++) {
                    uint8_t gotm = sr_fmt_health_mark(fi != 0u, k_verdicts[vi]);
                    CHECK(gotm == k_mark[fi][vi]);
                    cov_mark++;
                }
            }
            CHECK(cov_mark == 10u);
        }

#undef D19_FIT_CHECK

        printf(
            "capture_health d19: ok=%u lowfix=%u nofix=%u drop=%u crit=%u acq=%u "
            "stale=%u sat3=%u drop_big=%u fit_cut=%u topd=%u%u%u%u%u%u\n",
            cov_ok,
            cov_lowfix,
            cov_nofix,
            cov_drop,
            cov_crit,
            cov_acq,
            cov_stale,
            cov_sat3,
            cov_drop_big,
            cov_fit_cut,
            cov_topd[1],
            cov_topd[2],
            cov_topd[3],
            cov_topd[4],
            cov_topd[5],
            cov_topd[6]);

        CHECK(cov_ok == 18u);
        CHECK(cov_lowfix == 18u);
        CHECK(cov_nofix == 18u);
        CHECK(cov_drop == 36u);
        CHECK(cov_crit == 18u);
        CHECK(cov_acq == 18u);
        CHECK(cov_stale == 18u);
        CHECK(cov_sat3 == 6u);
        CHECK(cov_drop_big == 6u);
        /* Exactly two matrix cells exceed 32 cols raw: OK x drop=UINT32_MAX
         * with three-digit sats ("OK fix100% 4294967295drop SAT 100" and
         * "...SAT 255" = 33); two-digit sats land exactly on 32, uncut. */
        CHECK(cov_fit_cut == 2u);
        CHECK(cov_topd[1] == 6u && cov_topd[6] == 6u);
    }

    fprintf(stderr, "capture_health: ok\n");
    return sr_test_failures;
}
