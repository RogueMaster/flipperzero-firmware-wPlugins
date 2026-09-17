#define SR_HOST_TEST 1

#include "sr_test.h"

#include "sr_gps_sample.h"
#include "sr_model.h"
#include "sr_scan_ctl.h"
#include "sr_view_fmt.h"
#include "../../views/sr_view_dash.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 * Gate oracle: pack four conditions into a bitfield and look it up in a
 * 16-entry priority table. Deliberately not the same code path as the
 * implementation's if-ladder. bit0=nolink bit1=scanning bit2=busy
 * bit3=inflight.
 */
static const uint8_t k_scan_busy[8] = {
    0, 1, 1, 1, 0, 0, 0, 0 /* Starting / Running / Stopping */
};
static const uint8_t k_phase_fly[8] = {
    0, 1, 1, 0, 0, 0, 0, 1 /* WaitBlock / WaitStop / WaitSlow */
};
static const SrGpsGate k_gate_pri[16] = {
    SrGpsGateOk,       /* 0000 */
    SrGpsGateNoLink,   /* 0001 */
    SrGpsGateScanning, /* 0010 */
    SrGpsGateNoLink,   /* 0011 */
    SrGpsGateBusy,     /* 0100 */
    SrGpsGateNoLink,   /* 0101 */
    SrGpsGateScanning, /* 0110 */
    SrGpsGateNoLink,   /* 0111 */
    SrGpsGateInFlight, /* 1000 */
    SrGpsGateNoLink,   /* 1001 */
    SrGpsGateScanning, /* 1010 */
    SrGpsGateNoLink,   /* 1011 */
    SrGpsGateBusy,     /* 1100 */
    SrGpsGateNoLink,   /* 1101 */
    SrGpsGateScanning, /* 1110 */
    SrGpsGateNoLink,   /* 1111 */
};

static SrGpsGate oracle_gate(bool link_ok, uint8_t session, uint8_t scan_ui, uint8_t phase) {
    unsigned bits = 0;

    if(!link_ok) {
        bits |= 1u;
    }
    if(session == (uint8_t)SrSessionRunning) {
        bits |= 2u;
    }
    if(scan_ui < 8u && k_scan_busy[scan_ui]) {
        bits |= 4u;
    }
    if(phase < 8u && k_phase_fly[phase]) {
        bits |= 8u;
    }
    return k_gate_pri[bits];
}

/*
 * Step oracle: WaitBlock / WaitStop use a 4-entry table (hit x2 + timeout);
 * other phases pass through unchanged. Deliberately not the same code path
 * as the implementation's if/else ladder.
 */
static SrGpsStep oracle_step(
    const SrGpsSampleCtx* c, uint32_t blocks_now, uint32_t stop_rev_now, uint32_t now_ms) {
    SrGpsStep s;
    unsigned elapsed_first;
    unsigned elapsed_slow;
    unsigned elapsed_stop;
    unsigned idx;
    static const uint8_t k_wb_phase[4] = {
        (uint8_t)SrGpsPhaseWaitBlock,
        (uint8_t)SrGpsPhaseWaitSlow,
        (uint8_t)SrGpsPhaseWaitStop,
        (uint8_t)SrGpsPhaseWaitStop,
    };
    static const uint8_t k_wb_act[4] = {
        (uint8_t)SrGpsActNone,
        (uint8_t)SrGpsActNone,
        (uint8_t)SrGpsActSendStop,
        (uint8_t)SrGpsActSendStop,
    };
    static const uint8_t k_wsl_phase[4] = {
        (uint8_t)SrGpsPhaseWaitSlow,
        (uint8_t)SrGpsPhaseWaitStop,
        (uint8_t)SrGpsPhaseWaitStop,
        (uint8_t)SrGpsPhaseWaitStop,
    };
    static const uint8_t k_wsl_act[4] = {
        (uint8_t)SrGpsActNone,
        (uint8_t)SrGpsActSendStop,
        (uint8_t)SrGpsActSendStop,
        (uint8_t)SrGpsActSendStop,
    };
    static const uint8_t k_ws_got[4] = {
        (uint8_t)SrGpsPhaseWaitStop,
        (uint8_t)SrGpsPhaseStopUnsure,
        (uint8_t)SrGpsPhaseDone,
        (uint8_t)SrGpsPhaseDone,
    };
    static const uint8_t k_ws_nogot[4] = {
        (uint8_t)SrGpsPhaseWaitStop,
        (uint8_t)SrGpsPhaseNoReply,
        (uint8_t)SrGpsPhaseNoReply,
        (uint8_t)SrGpsPhaseNoReply,
    };
    static const uint8_t k_nr_phase[2] = {
        (uint8_t)SrGpsPhaseNoReply, /* block unchanged */
        (uint8_t)SrGpsPhaseIdle,    /* block changed -> recheck */
    };

    s.phase = c->phase;
    s.act = (uint8_t)SrGpsActNone;
    s.got_block = c->got_block;

    elapsed_first = (uint32_t)(now_ms - c->sent_tick_ms);
    elapsed_slow = (uint32_t)(now_ms - c->sent_tick_ms);
    elapsed_stop = (uint32_t)(now_ms - c->stop_tick_ms);

    if(c->phase == (uint8_t)SrGpsPhaseWaitBlock) {
        idx = 0;
        if(elapsed_first >= (unsigned)SR_GPS_FIRST_BLOCK_MS) {
            idx |= 1u;
        }
        if(blocks_now != c->blocks_at_send) {
            idx |= 2u;
        }
        s.phase = k_wb_phase[idx];
        s.act = k_wb_act[idx];
        s.got_block = (idx >= 2u) ? true : ((idx == 1u) ? false : c->got_block);
        return s;
    }
    if(c->phase == (uint8_t)SrGpsPhaseWaitSlow) {
        idx = 0;
        if(elapsed_slow >= (unsigned)SR_GPS_SLOW_BLOCK_MS) {
            idx |= 1u;
        }
        if(blocks_now != c->blocks_at_send) {
            idx |= 2u;
        }
        s.phase = k_wsl_phase[idx];
        s.act = k_wsl_act[idx];
        s.got_block = (idx >= 2u) ? true : ((idx == 1u) ? false : c->got_block);
        return s;
    }
    if(c->phase == (uint8_t)SrGpsPhaseWaitStop) {
        idx = 0;
        if(elapsed_stop >= (unsigned)SR_GPS_STOP_ACK_MS) {
            idx |= 1u;
        }
        if(stop_rev_now != c->stop_rev_at_send) {
            idx |= 2u;
        }
        s.phase = c->got_block ? k_ws_got[idx] : k_ws_nogot[idx];
        s.act = (uint8_t)SrGpsActNone;
        s.got_block = c->got_block;
        return s;
    }
    if(c->phase == (uint8_t)SrGpsPhaseNoReply) {
        idx = (blocks_now != c->blocks_at_send) ? 1u : 0u;
        s.phase = k_nr_phase[idx];
        s.act = (uint8_t)SrGpsActNone;
        s.got_block = (idx == 0u) ? c->got_block : false;
        return s;
    }
    return s;
}

/* [phase 0..7][gate 0..5][idle_hint 0..1] -- a table lookup, not a switch. */
static const char* const k_text[8][6][2] = {
    /* Idle */
    {
        {NULL, "Press OK to sample"},
        {NULL, NULL},
        {NULL, "Wardrive running"},
        {NULL, "Radio busy"},
        {NULL, NULL},
        {NULL, NULL},
    },
    /* WaitBlock */
    {
        {"Sampling GPS...", "Sampling GPS..."},
        {"Sampling GPS...", "Sampling GPS..."},
        {"Sampling GPS...", "Sampling GPS..."},
        {"Sampling GPS...", "Sampling GPS..."},
        {"Sampling GPS...", "Sampling GPS..."},
        {"Sampling GPS...", "Sampling GPS..."},
    },
    /* WaitStop */
    {
        {"Sampling GPS...", "Sampling GPS..."},
        {"Sampling GPS...", "Sampling GPS..."},
        {"Sampling GPS...", "Sampling GPS..."},
        {"Sampling GPS...", "Sampling GPS..."},
        {"Sampling GPS...", "Sampling GPS..."},
        {"Sampling GPS...", "Sampling GPS..."},
    },
    /* Done */
    {
        {NULL, NULL},
        {NULL, NULL},
        {NULL, NULL},
        {NULL, NULL},
        {NULL, NULL},
        {NULL, NULL},
    },
    /* NoReply */
    {
        {"No GPS reply", "No GPS reply"},
        {"No GPS reply", "No GPS reply"},
        {"No GPS reply", "No GPS reply"},
        {"No GPS reply", "No GPS reply"},
        {"No GPS reply", "No GPS reply"},
        {"No GPS reply", "No GPS reply"},
    },
    /* StopUnsure */
    {
        {"Stop unconfirmed", "Stop unconfirmed"},
        {"Stop unconfirmed", "Stop unconfirmed"},
        {"Stop unconfirmed", "Stop unconfirmed"},
        {"Stop unconfirmed", "Stop unconfirmed"},
        {"Stop unconfirmed", "Stop unconfirmed"},
        {"Stop unconfirmed", "Stop unconfirmed"},
    },
    /* Rejected */
    {
        {"Radio busy, retry", "Radio busy, retry"},
        {"Radio busy, retry", "Radio busy, retry"},
        {"Radio busy, retry", "Radio busy, retry"},
        {"Radio busy, retry", "Radio busy, retry"},
        {"Radio busy, retry", "Radio busy, retry"},
        {"Radio busy, retry", "Radio busy, retry"},
    },
    /* WaitSlow */
    {
        {"Waiting for GPS...", "Waiting for GPS..."},
        {"Waiting for GPS...", "Waiting for GPS..."},
        {"Waiting for GPS...", "Waiting for GPS..."},
        {"Waiting for GPS...", "Waiting for GPS..."},
        {"Waiting for GPS...", "Waiting for GPS..."},
        {"Waiting for GPS...", "Waiting for GPS..."},
    },
};

static size_t cstr_len(const char* s) {
    size_t n = 0;

    if(s == NULL) {
        return 0;
    }
    while(s[n] != '\0') {
        n++;
    }
    return n;
}

static int streq(const char* a, const char* b) {
    size_t i = 0;

    if(a == NULL || b == NULL) {
        return a == b;
    }
    while(a[i] != '\0' && b[i] != '\0') {
        if(a[i] != b[i]) {
            return 0;
        }
        i++;
    }
    return a[i] == b[i];
}

int test_gps_sample_run(void) {
    unsigned link_i;
    unsigned sess_i;
    unsigned ui_i;
    unsigned ph_i;
    unsigned got_i;
    unsigned blk_i;
    unsigned rev_i;
    unsigned now_i;
    unsigned stop_i;
    unsigned gate_i;
    unsigned hint_i;
    unsigned gate_ok = 0;
    unsigned gate_nolink = 0;
    unsigned gate_scanning = 0;
    unsigned gate_busy = 0;
    unsigned gate_inflight = 0;
    unsigned gate_total = 0;
    unsigned step_idle = 0;
    unsigned step_waitblock = 0;
    unsigned step_waitslow = 0;
    unsigned step_waitstop = 0;
    unsigned step_done = 0;
    unsigned step_noreply = 0;
    unsigned step_stopunsure = 0;
    unsigned step_keep = 0;
    unsigned step_send = 0;
    unsigned step_total = 0;
    unsigned text_null = 0;
    unsigned text_nonnull = 0;
    unsigned text_max = 0;
    unsigned text_total = 0;
    unsigned a_first = 0;
    unsigned a_stop = 0;
    unsigned b_first = 0;
    unsigned b_stop = 0;
    unsigned alpha = 0;
    unsigned beta = 0;
    unsigned gamma = 0;
    unsigned delta = 0;
    unsigned epsilon = 0;
    unsigned zeta = 0;
    unsigned z_ok = 0;
    SrGpsSampleCtx wrap;
    SrGpsStep ws;

    sr_test_failures = 0;

    /* ---- gate 512 = 2 × 4 × 8 × 8 ---- */
    for(link_i = 0; link_i < 2u; link_i++) {
        for(sess_i = 0; sess_i < 4u; sess_i++) {
            for(ui_i = 0; ui_i < 8u; ui_i++) {
                for(ph_i = 0; ph_i < 8u; ph_i++) {
                    bool link_ok = link_i != 0u;
                    SrGpsGate got = sr_gps_gate(link_ok, (uint8_t)sess_i, (uint8_t)ui_i, (uint8_t)ph_i);
                    SrGpsGate exp = oracle_gate(link_ok, (uint8_t)sess_i, (uint8_t)ui_i, (uint8_t)ph_i);

                    CHECK(got == exp);
                    if(got == SrGpsGateOk) {
                        gate_ok++;
                    } else if(got == SrGpsGateNoLink) {
                        gate_nolink++;
                    } else if(got == SrGpsGateScanning) {
                        gate_scanning++;
                    } else if(got == SrGpsGateBusy) {
                        gate_busy++;
                    } else if(got == SrGpsGateInFlight) {
                        gate_inflight++;
                    }
                    gate_total++;
                }
            }
        }
    }

    /* ---- step 384 ----
     * phase 8 × got 2 × blocks 2 × stop_rev 2 × now 3 × stop_elapsed 2
     * sent_tick_ms is fixed at 1000; stop_rev_at_send is non-zero, or the
     * false-ack guard would have no teeth.
     * The third now tier covers the hard timeout (SR_GPS_SLOW_BLOCK_MS);
     * otherwise WaitSlow's hard timeout would get zero coverage.
     */
    for(ph_i = 0; ph_i < 8u; ph_i++) {
        for(got_i = 0; got_i < 2u; got_i++) {
            for(blk_i = 0; blk_i < 2u; blk_i++) {
                for(rev_i = 0; rev_i < 2u; rev_i++) {
                    for(now_i = 0; now_i < 3u; now_i++) {
                        for(stop_i = 0; stop_i < 2u; stop_i++) {
                            SrGpsSampleCtx c;
                            SrGpsStep got;
                            SrGpsStep exp;
                            uint32_t now_ms;
                            uint32_t elapsed_stop;

                            memset(&c, 0, sizeof(c));
                            c.phase = (uint8_t)ph_i;
                            c.got_block = got_i != 0u;
                            c.sent_tick_ms = 1000u;
                            c.blocks_at_send = 5u;
                            c.stop_rev_at_send = 3u;
                            now_ms = (now_i == 0u) ? (1000u + (uint32_t)SR_GPS_FIRST_BLOCK_MS - 1u) :
                                     (now_i == 1u) ? (1000u + (uint32_t)SR_GPS_FIRST_BLOCK_MS) :
                                                     (1000u + (uint32_t)SR_GPS_SLOW_BLOCK_MS);
                            elapsed_stop = (stop_i == 0u) ? ((uint32_t)SR_GPS_STOP_ACK_MS - 1u) :
                                                           (uint32_t)SR_GPS_STOP_ACK_MS;
                            c.stop_tick_ms = now_ms - elapsed_stop;

                            got = sr_gps_step(
                                &c,
                                (blk_i == 0u) ? c.blocks_at_send : (c.blocks_at_send + 1u),
                                (rev_i == 0u) ? c.stop_rev_at_send : (c.stop_rev_at_send + 1u),
                                now_ms);
                            exp = oracle_step(
                                &c,
                                (blk_i == 0u) ? c.blocks_at_send : (c.blocks_at_send + 1u),
                                (rev_i == 0u) ? c.stop_rev_at_send : (c.stop_rev_at_send + 1u),
                                now_ms);

                            CHECK(got.phase == exp.phase);
                            CHECK(got.act == exp.act);
                            CHECK(got.got_block == exp.got_block);

                            if(ph_i == 3u || ph_i == 5u || ph_i == 6u) {
                                CHECK(got.phase == c.phase);
                                CHECK(got.act == (uint8_t)SrGpsActNone);
                                CHECK(got.got_block == c.got_block);
                                step_keep++;
                            } else if(got.phase == (uint8_t)SrGpsPhaseIdle) {
                                step_idle++;
                            } else if(got.phase == (uint8_t)SrGpsPhaseWaitBlock) {
                                step_waitblock++;
                            } else if(got.phase == (uint8_t)SrGpsPhaseWaitSlow) {
                                step_waitslow++;
                            } else if(got.phase == (uint8_t)SrGpsPhaseWaitStop) {
                                step_waitstop++;
                            } else if(got.phase == (uint8_t)SrGpsPhaseDone) {
                                step_done++;
                            } else if(got.phase == (uint8_t)SrGpsPhaseNoReply) {
                                step_noreply++;
                            } else if(got.phase == (uint8_t)SrGpsPhaseStopUnsure) {
                                step_stopunsure++;
                            }
                            if(got.act == (uint8_t)SrGpsActSendStop) {
                                step_send++;
                            }
                            step_total++;
                        }
                    }
                }
            }
        }
    }

    /* ---- text 96 = 8 × 6 × 2 ---- */
    for(ph_i = 0; ph_i < 8u; ph_i++) {
        for(gate_i = 0; gate_i < 6u; gate_i++) {
            for(hint_i = 0; hint_i < 2u; hint_i++) {
                bool idle_hint = hint_i != 0u;
                const char* got = sr_gps_status_text((uint8_t)ph_i, (uint8_t)gate_i, idle_hint);
                const char* exp = k_text[ph_i][gate_i][hint_i];
                size_t n;

                CHECK(streq(got, exp));
                if(got == NULL) {
                    text_null++;
                } else {
                    n = cstr_len(got);
                    CHECK(n <= (size_t)SR_VIEW_COLS);
                    CHECK(n == cstr_len(exp));
                    CHECK(n > 0u);
                    if(n > text_max) {
                        text_max = (unsigned)n;
                    }
                    text_nonnull++;
                }
                text_total++;
            }
        }
    }

    printf(
        "gps_sample cover: gate_ok=%u gate_nolink=%u gate_scanning=%u gate_busy=%u "
        "gate_inflight=%u gate_total=%u step_idle=%u step_waitblock=%u step_waitslow=%u "
        "step_waitstop=%u step_done=%u step_noreply=%u step_stopunsure=%u step_keep=%u "
        "step_send=%u step_total=%u text_null=%u text_nonnull=%u text_max=%u text_total=%u\n",
        gate_ok,
        gate_nolink,
        gate_scanning,
        gate_busy,
        gate_inflight,
        gate_total,
        step_idle,
        step_waitblock,
        step_waitslow,
        step_waitstop,
        step_done,
        step_noreply,
        step_stopunsure,
        step_keep,
        step_send,
        step_total,
        text_null,
        text_nonnull,
        text_max,
        text_total);

    CHECK(gate_ok == 75);
    CHECK(gate_nolink == 256);
    CHECK(gate_scanning == 64);
    CHECK(gate_busy == 72);
    CHECK(gate_inflight == 45);
    CHECK(gate_total == 512);
    CHECK(step_idle == 72);
    CHECK(step_waitblock == 8);
    CHECK(step_waitslow == 32);
    CHECK(step_waitstop == 68);
    CHECK(step_done == 12);
    CHECK(step_noreply == 42);
    CHECK(step_stopunsure == 6);
    CHECK(step_keep == 144);
    CHECK(step_send == 56);
    CHECK(step_total == 384);
    CHECK(text_null == 21);
    CHECK(text_nonnull == 75);
    CHECK(text_max == 18);
    CHECK(text_max <= (unsigned)SR_VIEW_COLS);
    CHECK(text_total == 96);

    /* ---- A4 wrap ---- */
    memset(&wrap, 0, sizeof(wrap));
    wrap.phase = (uint8_t)SrGpsPhaseWaitBlock;
    wrap.got_block = false;
    wrap.sent_tick_ms = 0xFFFFFF00u;
    wrap.blocks_at_send = 1u;
    wrap.stop_rev_at_send = 3u;
    wrap.stop_tick_ms = 0xFFFFFF00u;
    ws = sr_gps_step(&wrap, wrap.blocks_at_send, wrap.stop_rev_at_send, 0x40u);
    if(ws.phase == (uint8_t)SrGpsPhaseWaitBlock && ws.act == (uint8_t)SrGpsActNone) {
        a_first = 1;
    }
    CHECK(ws.phase == (uint8_t)SrGpsPhaseWaitBlock);
    CHECK(ws.act == (uint8_t)SrGpsActNone);

    memset(&wrap, 0, sizeof(wrap));
    wrap.phase = (uint8_t)SrGpsPhaseWaitStop;
    wrap.got_block = true;
    wrap.sent_tick_ms = 0xFFFFFF00u;
    wrap.stop_tick_ms = 0xFFFFFF00u;
    wrap.blocks_at_send = 1u;
    wrap.stop_rev_at_send = 3u;
    ws = sr_gps_step(&wrap, wrap.blocks_at_send, wrap.stop_rev_at_send, 0x40u);
    if(ws.phase == (uint8_t)SrGpsPhaseWaitStop && ws.act == (uint8_t)SrGpsActNone) {
        a_stop = 1;
    }
    CHECK(ws.phase == (uint8_t)SrGpsPhaseWaitStop);
    CHECK(ws.act == (uint8_t)SrGpsActNone);

    memset(&wrap, 0, sizeof(wrap));
    wrap.phase = (uint8_t)SrGpsPhaseWaitBlock;
    wrap.got_block = false;
    wrap.sent_tick_ms = 0x80000000u;
    wrap.blocks_at_send = 1u;
    wrap.stop_rev_at_send = 3u;
    wrap.stop_tick_ms = 0x80000000u;
    ws = sr_gps_step(&wrap, wrap.blocks_at_send, wrap.stop_rev_at_send, 0x100u);
    if(ws.phase == (uint8_t)SrGpsPhaseWaitSlow && ws.act == (uint8_t)SrGpsActNone &&
       ws.got_block == false) {
        b_first = 1;
    }
    CHECK(ws.phase == (uint8_t)SrGpsPhaseWaitSlow);
    CHECK(ws.act == (uint8_t)SrGpsActNone);
    CHECK(ws.got_block == false);

    memset(&wrap, 0, sizeof(wrap));
    wrap.phase = (uint8_t)SrGpsPhaseWaitStop;
    wrap.got_block = true;
    wrap.sent_tick_ms = 0x80000000u;
    wrap.stop_tick_ms = 0x80000000u;
    wrap.blocks_at_send = 1u;
    wrap.stop_rev_at_send = 3u;
    ws = sr_gps_step(&wrap, wrap.blocks_at_send, wrap.stop_rev_at_send, 0x100u);
    if(ws.phase == (uint8_t)SrGpsPhaseStopUnsure && ws.act == (uint8_t)SrGpsActNone &&
       ws.got_block == true) {
        b_stop = 1;
    }
    CHECK(ws.phase == (uint8_t)SrGpsPhaseStopUnsure);
    CHECK(ws.act == (uint8_t)SrGpsActNone);
    CHECK(ws.got_block == true);

    printf("gps_sample wrap: a_first=%u a_stop=%u b_first=%u b_stop=%u\n", a_first, a_stop, b_first, b_stop);
    CHECK(a_first == 1);
    CHECK(a_stop == 1);
    CHECK(b_first == 1);
    CHECK(b_stop == 1);

    /* Group alpha: sent + SLOW does not wrap; the hard timeout must fire.
     * By design, A7-④'s buggy version does not go red for this group. */
    memset(&wrap, 0, sizeof(wrap));
    wrap.phase = (uint8_t)SrGpsPhaseWaitSlow;
    wrap.got_block = false;
    wrap.sent_tick_ms = 0x10000000u;
    wrap.blocks_at_send = 1u;
    wrap.stop_rev_at_send = 3u;
    wrap.stop_tick_ms = 0x10000000u;
    ws = sr_gps_step(
        &wrap, wrap.blocks_at_send, wrap.stop_rev_at_send, 0x10000000u + (uint32_t)SR_GPS_SLOW_BLOCK_MS);
    if(ws.phase == (uint8_t)SrGpsPhaseWaitStop && ws.act == (uint8_t)SrGpsActSendStop &&
       ws.got_block == false) {
        alpha = 1;
    }
    CHECK(ws.phase == (uint8_t)SrGpsPhaseWaitStop);
    CHECK(ws.act == (uint8_t)SrGpsActSendStop);
    CHECK(ws.got_block == false);

    /* Group beta: sent is close to UINT32_MAX, so sent + SLOW wraps.
     * The correct implementation (unsigned subtraction) gets elapsed=16 <
     * SLOW and stays in WaitSlow. If A7-④ is changed to now >= sent + SLOW,
     * this group must go red (alpha staying green is expected). */
    memset(&wrap, 0, sizeof(wrap));
    wrap.phase = (uint8_t)SrGpsPhaseWaitSlow;
    wrap.got_block = false;
    wrap.sent_tick_ms = 0xFFFFFF00u;
    wrap.blocks_at_send = 1u;
    wrap.stop_rev_at_send = 3u;
    wrap.stop_tick_ms = 0xFFFFFF00u;
    ws = sr_gps_step(&wrap, wrap.blocks_at_send, wrap.stop_rev_at_send, 0xFFFFFF10u);
    if(ws.phase == (uint8_t)SrGpsPhaseWaitSlow && ws.act == (uint8_t)SrGpsActNone &&
       ws.got_block == false) {
        beta = 1;
    }
    CHECK(ws.phase == (uint8_t)SrGpsPhaseWaitSlow);
    CHECK(ws.act == (uint8_t)SrGpsActNone);
    CHECK(ws.got_block == false);

    printf("gps_sample wrap_slow: alpha=%u beta=%u\n", alpha, beta);
    CHECK(alpha == 1);
    CHECK(beta == 1);

    /* Group gamma: NoReply seeing a new block -> Idle */
    memset(&wrap, 0, sizeof(wrap));
    wrap.phase = (uint8_t)SrGpsPhaseNoReply;
    wrap.got_block = false;
    wrap.blocks_at_send = 5u;
    wrap.sent_tick_ms = 1000u;
    wrap.stop_tick_ms = 1000u;
    wrap.stop_rev_at_send = 3u;
    ws = sr_gps_step(&wrap, 6u, wrap.stop_rev_at_send, 1000u);
    if(ws.phase == (uint8_t)SrGpsPhaseIdle && ws.act == (uint8_t)SrGpsActNone &&
       ws.got_block == false) {
        gamma = 1;
    }
    CHECK(ws.phase == (uint8_t)SrGpsPhaseIdle);
    CHECK(ws.act == (uint8_t)SrGpsActNone);
    CHECK(ws.got_block == false);

    /* Group delta: NoReply with the block unchanged, got_block stays true */
    memset(&wrap, 0, sizeof(wrap));
    wrap.phase = (uint8_t)SrGpsPhaseNoReply;
    wrap.got_block = true;
    wrap.blocks_at_send = 5u;
    wrap.sent_tick_ms = 1000u;
    wrap.stop_tick_ms = 1000u;
    wrap.stop_rev_at_send = 3u;
    ws = sr_gps_step(&wrap, 5u, wrap.stop_rev_at_send, 1000u);
    if(ws.phase == (uint8_t)SrGpsPhaseNoReply && ws.act == (uint8_t)SrGpsActNone &&
       ws.got_block == true) {
        delta = 1;
    }
    CHECK(ws.phase == (uint8_t)SrGpsPhaseNoReply);
    CHECK(ws.act == (uint8_t)SrGpsActNone);
    CHECK(ws.got_block == true);

    /* Group epsilon: counter wraps 0xFFFFFFFF -> 0; only != (not >) can still catch it */
    memset(&wrap, 0, sizeof(wrap));
    wrap.phase = (uint8_t)SrGpsPhaseNoReply;
    wrap.got_block = false;
    wrap.blocks_at_send = 0xFFFFFFFFu;
    wrap.sent_tick_ms = 1000u;
    wrap.stop_tick_ms = 1000u;
    wrap.stop_rev_at_send = 3u;
    ws = sr_gps_step(&wrap, 0u, wrap.stop_rev_at_send, 1000u);
    if(ws.phase == (uint8_t)SrGpsPhaseIdle) {
        epsilon = 1;
    }
    CHECK(ws.phase == (uint8_t)SrGpsPhaseIdle);

    /* Group zeta: Done / StopUnsure / Rejected must not flip state on seeing a new block */
    memset(&wrap, 0, sizeof(wrap));
    wrap.phase = (uint8_t)SrGpsPhaseDone;
    wrap.got_block = true;
    wrap.blocks_at_send = 5u;
    wrap.sent_tick_ms = 1000u;
    wrap.stop_tick_ms = 1000u;
    wrap.stop_rev_at_send = 3u;
    ws = sr_gps_step(&wrap, 6u, wrap.stop_rev_at_send, 1000u);
    if(ws.phase == (uint8_t)SrGpsPhaseDone) {
        z_ok++;
    }
    CHECK(ws.phase == (uint8_t)SrGpsPhaseDone);

    memset(&wrap, 0, sizeof(wrap));
    wrap.phase = (uint8_t)SrGpsPhaseStopUnsure;
    wrap.got_block = true;
    wrap.blocks_at_send = 5u;
    wrap.sent_tick_ms = 1000u;
    wrap.stop_tick_ms = 1000u;
    wrap.stop_rev_at_send = 3u;
    ws = sr_gps_step(&wrap, 6u, wrap.stop_rev_at_send, 1000u);
    if(ws.phase == (uint8_t)SrGpsPhaseStopUnsure) {
        z_ok++;
    }
    CHECK(ws.phase == (uint8_t)SrGpsPhaseStopUnsure);

    memset(&wrap, 0, sizeof(wrap));
    wrap.phase = (uint8_t)SrGpsPhaseRejected;
    wrap.got_block = false;
    wrap.blocks_at_send = 5u;
    wrap.sent_tick_ms = 1000u;
    wrap.stop_tick_ms = 1000u;
    wrap.stop_rev_at_send = 3u;
    ws = sr_gps_step(&wrap, 6u, wrap.stop_rev_at_send, 1000u);
    if(ws.phase == (uint8_t)SrGpsPhaseRejected) {
        z_ok++;
    }
    CHECK(ws.phase == (uint8_t)SrGpsPhaseRejected);

    if(z_ok == 3u) {
        zeta = 1;
    }

    printf(
        "gps_sample revisit: gamma=%u delta=%u epsilon=%u zeta=%u\n",
        gamma,
        delta,
        epsilon,
        zeta);
    CHECK(gamma == 1);
    CHECK(delta == 1);
    CHECK(epsilon == 1);
    CHECK(zeta == 1);

    fprintf(stderr, "sizeof(SrModel)=%zu\n", sizeof(SrModel));
    fprintf(stderr, "sizeof(SrDashModel)=%zu\n", sizeof(SrDashModel));
    fprintf(stderr, "sizeof(SrGpsSampleCtx)=%zu\n", sizeof(SrGpsSampleCtx));
    CHECK(sizeof(SrModel) == 3328);
    CHECK(sizeof(SrDashModel) == 644);
    CHECK(sizeof(SrDashModel) <= 768);
    CHECK(sizeof(SrGpsSampleCtx) == 20);

    return sr_test_failures;
}
