#include "sr_test.h"

#include "sr_scan_ctl.h"
#include "sr_types.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/*
 * A standalone oracle: compute all three candidate outcomes first, then pick one based on
 * pending / is_start. This differs from sr_scan_ctl.h's approach, which splits start/stop
 * first within pending and only checks rejected afterward.
 * Timeout uses elapsed >= timeout (timed_out); the implementation under test uses
 * elapsed < timeout (waiting).
 *
 * Per the literal contract of ADR-017 decision 1:
 *   pending ∧ start ∧ (rev rose ∧ Running) → Running, else Starting / StartFailed by timeout
 *   pending ∧ stop  ∧ (rev rose ∧ Stopped) → Idle, else Stopping / StopFailed by timeout
 *   !pending ∧ rejected → Busy
 *   !pending ∧ !rejected ∧ Running → Running, else Idle
 */
static SrScanUiState oracle_eval(const SrScanCtlCtx* c, uint32_t now_ms) {
    bool rose;
    bool timed_out;
    bool start_ack;
    bool stop_ack;
    SrScanUiState pending_start;
    SrScanUiState pending_stop;
    SrScanUiState no_cmd;

    if(c == NULL) {
        return SrScanUiIdle;
    }

    rose = c->session_rev_now > c->session_rev_at_send;
    timed_out = (uint32_t)(now_ms - c->cmd_tick_ms) >= c->timeout_ms;
    start_ack = rose && (c->session_now == SrSessionRunning);
    stop_ack = rose && (c->session_now == SrSessionStopped);

    pending_start = start_ack ? SrScanUiRunning :
                                (timed_out ? SrScanUiStartFailed : SrScanUiStarting);
    pending_stop = stop_ack ? SrScanUiIdle : (timed_out ? SrScanUiStopFailed : SrScanUiStopping);
    no_cmd = c->cmd_rejected ? SrScanUiBusy :
                               ((c->session_now == SrSessionRunning) ? SrScanUiRunning :
                                                                      SrScanUiIdle);

    if(!c->cmd_pending) {
        return no_cmd;
    }
    return c->cmd_is_start ? pending_start : pending_stop;
}

/*
 * Independent oracle for D12 A1 OK-key dispatch. Membership in two disjoint
 * sets, then default None -- not a switch, unlike sr_scan_ctl_on_ok.
 */
static SrScanAct oracle_on_ok(SrScanUiState st) {
    unsigned i;
    static const SrScanUiState stop_set[] = {
        SrScanUiRunning,
        SrScanUiStarting,
        SrScanUiStopFailed,
    };
    static const SrScanUiState start_set[] = {
        SrScanUiIdle,
        SrScanUiStartFailed,
    };

    for(i = 0; i < (unsigned)(sizeof(stop_set) / sizeof(stop_set[0])); i++) {
        if(st == stop_set[i]) {
            return SrScanActSendStop;
        }
    }
    for(i = 0; i < (unsigned)(sizeof(start_set) / sizeof(start_set[0])); i++) {
        if(st == start_set[i]) {
            return SrScanActSendStart;
        }
    }
    return SrScanActNone;
}

static void wrap_base(SrScanCtlCtx* c, bool is_start, uint32_t sent) {
    memset(c, 0, sizeof(*c));
    c->cmd_pending = true;
    c->cmd_is_start = is_start;
    c->cmd_rejected = false;
    c->cmd_tick_ms = sent;
    c->timeout_ms = (uint32_t)SR_SCAN_CTL_TIMEOUT_MS;
    c->session_rev_at_send = 1;
    c->session_rev_now = 1; /* has not risen; goes down the waiting / timeout branch */
    c->session_now = SrSessionIdle;
}

int test_scan_ctl_run(void) {
    static const SrSessionState k_sess[3] = {
        SrSessionIdle,
        SrSessionRunning,
        SrSessionStopped,
    };
    unsigned rejected_i;
    unsigned pending_i;
    unsigned start_i;
    unsigned rose_i;
    unsigned sess_i;
    unsigned elapsed_i;
    unsigned idle = 0;
    unsigned running = 0;
    unsigned starting = 0;
    unsigned start_failed = 0;
    unsigned stopping = 0;
    unsigned stop_failed = 0;
    unsigned busy = 0;
    unsigned total = 0;
    SrScanCtlCtx wrap;

    sr_test_failures = 0;

    for(rejected_i = 0; rejected_i < 2u; rejected_i++) {
        for(pending_i = 0; pending_i < 2u; pending_i++) {
            for(start_i = 0; start_i < 2u; start_i++) {
                for(rose_i = 0; rose_i < 2u; rose_i++) {
                    for(sess_i = 0; sess_i < 3u; sess_i++) {
                        for(elapsed_i = 0; elapsed_i < 2u; elapsed_i++) {
                            SrScanCtlCtx c;
                            uint32_t now;
                            SrScanUiState got;
                            SrScanUiState exp;

                            memset(&c, 0, sizeof(c));
                            c.cmd_rejected = rejected_i != 0u;
                            c.cmd_pending = pending_i != 0u;
                            c.cmd_is_start = start_i != 0u;
                            c.session_rev_at_send = 5u;
                            c.session_rev_now = (rose_i != 0u) ? 6u : 5u;
                            c.session_now = k_sess[sess_i];
                            c.timeout_ms = (uint32_t)SR_SCAN_CTL_TIMEOUT_MS;
                            c.cmd_tick_ms = 10000u;
                            now = (elapsed_i == 0u) ? 11000u : 13000u;

                            got = sr_scan_ctl_eval(&c, now);
                            exp = oracle_eval(&c, now);
                            if(got != exp) {
                                fprintf(
                                    stderr,
                                    "scan_ctl mismatch rejected=%u pending=%u start=%u rose=%u "
                                    "sess=%u elapsed=%u got=%d exp=%d\n",
                                    rejected_i,
                                    pending_i,
                                    start_i,
                                    rose_i,
                                    sess_i,
                                    elapsed_i,
                                    (int)got,
                                    (int)exp);
                            }
                            CHECK(got == exp);

                            if(got == SrScanUiIdle) {
                                idle++;
                            } else if(got == SrScanUiRunning) {
                                running++;
                            } else if(got == SrScanUiStarting) {
                                starting++;
                            } else if(got == SrScanUiStartFailed) {
                                start_failed++;
                            } else if(got == SrScanUiStopping) {
                                stopping++;
                            } else if(got == SrScanUiStopFailed) {
                                stop_failed++;
                            } else if(got == SrScanUiBusy) {
                                busy++;
                            }
                            total++;
                        }
                    }
                }
            }
        }
    }

    printf(
        "scan_ctl cover: idle=%u running=%u starting=%u start_failed=%u stopping=%u "
        "stop_failed=%u busy=%u total=%u\n",
        idle,
        running,
        starting,
        start_failed,
        stopping,
        stop_failed,
        busy,
        total);

    /* Independent derivation (cross-checked against the lead session card-side derivation):
     * pending=1: 48 total, independent of rejected.
     *   start 24: confirmed (rose ∧ Running) = 2×2 = 4 → running; the remaining 20 split
     *     evenly by elapsed: starting=10 / start_failed=10
     *   stop 24: confirmed (rose ∧ Stopped) = 4 → idle; the remaining 20 → stopping=10 /
     *     stop_failed=10
     * pending=0: 48 total:
     *   rejected=1 → busy 24
     *   rejected=0 → Running 8 → running; the other 16 → idle
     * Total: idle=4+16=20 / running=4+8=12 / starting=10 / start_failed=10
     *        stopping=10 / stop_failed=10 / busy=24 / total=96
     */
    CHECK(idle == 20);
    CHECK(running == 12);
    CHECK(starting == 10);
    CHECK(start_failed == 10);
    CHECK(stopping == 10);
    CHECK(stop_failed == 10);
    CHECK(busy == 24);
    CHECK(total == 96);

    CHECK(sr_scan_ctl_eval(NULL, 0) == SrScanUiIdle);

    /*
     * A4 group alpha: sent+timeout itself also wraps around. Verifies correctness;
     * by design, negative control #3 cannot catch this one.
     */
    wrap_base(&wrap, true, 0xFFFFFF00u);
    CHECK(sr_scan_ctl_eval(&wrap, 0x40u) == SrScanUiStarting);
    CHECK(oracle_eval(&wrap, 0x40u) == SrScanUiStarting);
    CHECK(sr_scan_ctl_eval(&wrap, 0xF00u) == SrScanUiStartFailed);
    CHECK(oracle_eval(&wrap, 0xF00u) == SrScanUiStartFailed);

    wrap_base(&wrap, false, 0xFFFFFF00u);
    CHECK(sr_scan_ctl_eval(&wrap, 0x40u) == SrScanUiStopping);
    CHECK(oracle_eval(&wrap, 0x40u) == SrScanUiStopping);
    CHECK(sr_scan_ctl_eval(&wrap, 0xF00u) == SrScanUiStopFailed);
    CHECK(oracle_eval(&wrap, 0xF00u) == SrScanUiStopFailed);

    /*
     * A4 group beta: now has wrapped around but sent+timeout has not. This is the
     * actual hook for negative control #3.
     */
    wrap_base(&wrap, true, 0x80000000u);
    CHECK(sr_scan_ctl_eval(&wrap, 0x100u) == SrScanUiStartFailed);
    CHECK(oracle_eval(&wrap, 0x100u) == SrScanUiStartFailed);

    wrap_base(&wrap, false, 0x80000000u);
    CHECK(sr_scan_ctl_eval(&wrap, 0x100u) == SrScanUiStopFailed);
    CHECK(oracle_eval(&wrap, 0x100u) == SrScanUiStopFailed);

    /*
     * D12 Part A: table-driven OK-key mapping. k_spec is the A1 table
     * (7 enum values; 6 rows, Running/Starting grouped). Coverage counts
     * are printed first; numeric CHECKs are welded after the first run.
     */
    {
        static const struct {
            SrScanUiState st;
            SrScanAct act;
        } k_spec[] = {
            {SrScanUiIdle, SrScanActSendStart},
            {SrScanUiStarting, SrScanActSendStop},
            {SrScanUiRunning, SrScanActSendStop},
            {SrScanUiStopping, SrScanActNone},
            {SrScanUiStartFailed, SrScanActSendStart},
            {SrScanUiStopFailed, SrScanActSendStop},
            {SrScanUiBusy, SrScanActNone},
        };
        unsigned i;
        unsigned act_none = 0;
        unsigned act_start = 0;
        unsigned act_stop = 0;
        unsigned on_ok_total = 0;

        for(i = 0; i < (unsigned)(sizeof(k_spec) / sizeof(k_spec[0])); i++) {
            SrScanAct got = sr_scan_ctl_on_ok(k_spec[i].st);
            SrScanAct exp_oracle = oracle_on_ok(k_spec[i].st);

            if(got != k_spec[i].act || got != exp_oracle) {
                fprintf(
                    stderr,
                    "on_ok mismatch st=%d got=%d spec=%d oracle=%d\n",
                    (int)k_spec[i].st,
                    (int)got,
                    (int)k_spec[i].act,
                    (int)exp_oracle);
            }
            CHECK(got == k_spec[i].act);
            CHECK(got == exp_oracle);

            if(got == SrScanActNone) {
                act_none++;
            } else if(got == SrScanActSendStart) {
                act_start++;
            } else if(got == SrScanActSendStop) {
                act_stop++;
            }
            on_ok_total++;
        }

        printf(
            "scan_ctl on_ok cover: none=%u start=%u stop=%u total=%u\n",
            act_none,
            act_start,
            act_stop,
            on_ok_total);
        /* Welded after first print (D12 A1 / common precondition 7):
         * stop = Running + Starting + StopFailed = 3
         * start = Idle + StartFailed = 2
         * none = Stopping + Busy = 2
         * total = 7 (every SrScanUiState value) */
        CHECK(act_none == 2);
        CHECK(act_start == 2);
        CHECK(act_stop == 3);
        CHECK(on_ok_total == 7);
    }

    fprintf(stderr, "sizeof(SrModel)=%zu\n", sizeof(SrModel));
    fprintf(stderr, "sizeof(SrParser)=%zu\n", sizeof(SrParser));
    fprintf(stderr, "sizeof(SrScanCtlCtx)=%zu\n", sizeof(SrScanCtlCtx));
    CHECK(sizeof(SrScanCtlCtx) == 24);
    /* SrIoStats lives in sr_io.h and pulls in furi, so host_test cannot include it.
     * 8 uint32_t fields (including the newly added rx_max_fill), no pointers, no padding
     * → 32. */
    fprintf(stderr, "sizeof(SrIoStats)=%zu\n", (size_t)(8u * sizeof(uint32_t)));
    /* T3.4 was 3168. After T4.6 added SrRawLog* (8 B, right next to SrBloom*), the 64-bit
     * host value became 3176. D12 added SrGpsCsvView + gps_csv_rev → 3328. */
    CHECK(sizeof(SrModel) == 3328);
    CHECK(sizeof(SrModel) <= 4096);
    CHECK(sizeof(SrParser) == 424);
    CHECK((size_t)(8u * sizeof(uint32_t)) == 32);

    return sr_test_failures;
}
