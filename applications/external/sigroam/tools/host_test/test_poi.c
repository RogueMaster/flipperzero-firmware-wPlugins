#include "sr_test.h"

#include "sr_poi.h"
#include "sr_model.h"
#include "sr_scan_ctl.h"
#include "sr_parse_marauder.h"
#include "sr_source_codec.h"
#include "sr_types.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Gate oracle: pack three conditions into a bitfield and look it up in an
 * 8-entry priority table. Deliberately not the same code path as the
 * implementation's if-ladder. bit0=!link bit1=not-scanning bit2=!fix.
 * Scanning uses the numeric values pinned below (Running=1, scan Running=2),
 * not a call to sr_poi_gate.
 */
static const SrPoiGate k_gate_pri[8] = {
    SrPoiGateOk,          /* 000 */
    SrPoiGateNoLink,      /* 001 */
    SrPoiGateNotScanning, /* 010 */
    SrPoiGateNoLink,      /* 011 */
    SrPoiGateNoFix,       /* 100 */
    SrPoiGateNoLink,      /* 101 */
    SrPoiGateNotScanning, /* 110 */
    SrPoiGateNoLink,      /* 111 */
};

static SrPoiGate oracle_gate(bool link_ok, uint8_t session, uint8_t scan_ui, bool fix) {
    unsigned bits = 0;

    if(!link_ok) {
        bits |= 1u;
    }
    /* Literals 1 and 2 are SrSessionRunning / SrScanUiRunning, pinned by CHECK
     * at the top of test_poi_run. Not taken from sr_poi_gate. */
    if(session != 1u || scan_ui != 2u) {
        bits |= 2u;
    }
    if(!fix) {
        bits |= 4u;
    }
    return k_gate_pri[bits];
}

/*
 * WaitAck next-phase table. bit0 = ack changed ( != ), bit1 = unsigned elapsed
 * >= SR_POI_ACK_MS. Ack wins when both are set -- same priority as the
 * implementation, different shape (lookup vs if-ladder).
 */
static const uint8_t k_wait_next[4] = {
    (uint8_t)SrPoiPhaseWaitAck,  /* 00 */
    (uint8_t)SrPoiPhaseDone,     /* 01 ack */
    (uint8_t)SrPoiPhaseNoReply,  /* 10 timeout */
    (uint8_t)SrPoiPhaseDone,     /* 11 ack wins */
};

static SrPoiPhase oracle_step(SrPoiCtx* ctx, uint32_t ack_now, uint32_t tick_ms) {
    unsigned idx;
    uint8_t next;

    if(ctx == NULL) {
        return SrPoiPhaseIdle;
    }
    if(ctx->phase != (uint8_t)SrPoiPhaseWaitAck) {
        return (SrPoiPhase)ctx->phase;
    }
    idx = 0;
    if(ack_now != ctx->ack_at_send) {
        idx |= 1u;
    }
    if((uint32_t)(tick_ms - ctx->sent_tick_ms) >= (uint32_t)SR_POI_ACK_MS) {
        idx |= 2u;
    }
    next = k_wait_next[idx];
    ctx->phase = next;
    return (SrPoiPhase)next;
}

static SrPoiPhase both(SrPoiCtx* impl, SrPoiCtx* ora, uint32_t ack_now, uint32_t tick_ms) {
    SrPoiPhase a;
    SrPoiPhase b;

    a = sr_poi_step(impl, ack_now, tick_ms);
    b = oracle_step(ora, ack_now, tick_ms);
    CHECK(a == b);
    CHECK(impl->phase == ora->phase);
    CHECK(impl->ack_at_send == ora->ack_at_send);
    CHECK(impl->sent_tick_ms == ora->sent_tick_ms);
    return a;
}

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

int test_poi_run(void) {
    unsigned link_i;
    unsigned sess_i;
    unsigned ui_i;
    unsigned fix_i;
    unsigned gate_ok = 0;
    unsigned gate_nolink = 0;
    unsigned gate_notscan = 0;
    unsigned gate_nofix = 0;
    unsigned gate_total = 0;
    unsigned step_idle = 0;
    unsigned step_wait = 0;
    unsigned step_done = 0;
    unsigned step_noreply = 0;
    unsigned step_keep = 0;
    unsigned step_null = 0;
    unsigned wrap = 0;
    unsigned ackwrap = 0;
    SrPoiCtx impl;
    SrPoiCtx ora;
    SrPoiPhase ph;
    const char* s;
    SrParser parser;
    SrEvent ev;
    SrParseResult r;
    char* win;
    char cmdbuf[32];
    size_t n;
    static const uint8_t k_keep_phase[3] = {
        (uint8_t)SrPoiPhaseIdle,
        (uint8_t)SrPoiPhaseDone,
        (uint8_t)SrPoiPhaseNoReply,
    };
    unsigned kp;

    sr_test_failures = 0;

    CHECK((uint8_t)SrSessionIdle == 0u);
    CHECK((uint8_t)SrSessionRunning == 1u);
    CHECK((uint8_t)SrSessionStopped == 2u);
    CHECK((uint8_t)SrScanUiIdle == 0u);
    CHECK((uint8_t)SrScanUiStarting == 1u);
    CHECK((uint8_t)SrScanUiRunning == 2u);
    CHECK((uint8_t)SrScanUiStopping == 3u);
    CHECK((uint8_t)SrScanUiStartFailed == 4u);
    CHECK((uint8_t)SrScanUiStopFailed == 5u);
    CHECK((uint8_t)SrScanUiBusy == 6u);

    CHECK(SR_POI_ACK_MS == 2000);

    sr_poi_reset(&impl);
    CHECK(impl.phase == (uint8_t)SrPoiPhaseIdle);
    CHECK(impl.ack_at_send == 0u);
    CHECK(impl.sent_tick_ms == 0u);
    sr_poi_reset(NULL);

    /* ---- gate 84 = 2 x 3 x 7 x 2 ---- */
    for(link_i = 0; link_i < 2u; link_i++) {
        for(sess_i = 0; sess_i < 3u; sess_i++) {
            for(ui_i = 0; ui_i < 7u; ui_i++) {
                for(fix_i = 0; fix_i < 2u; fix_i++) {
                    bool link_ok = link_i != 0u;
                    bool fix = fix_i != 0u;
                    SrPoiGate got;
                    SrPoiGate exp;

                    got = sr_poi_gate(link_ok, (uint8_t)sess_i, (uint8_t)ui_i, fix);
                    exp = oracle_gate(link_ok, (uint8_t)sess_i, (uint8_t)ui_i, fix);
                    CHECK(got == exp);
                    if(got == SrPoiGateOk) {
                        gate_ok++;
                    } else if(got == SrPoiGateNoLink) {
                        gate_nolink++;
                    } else if(got == SrPoiGateNotScanning) {
                        gate_notscan++;
                    } else if(got == SrPoiGateNoFix) {
                        gate_nofix++;
                    }
                    gate_total++;
                }
            }
        }
    }

    /* a: ctx == NULL */
    ph = sr_poi_step(NULL, 1u, 0u);
    CHECK(ph == SrPoiPhaseIdle);
    step_null++;

    /* b: Idle / Done / NoReply stay put even when ack moved and time elapsed. */
    for(kp = 0; kp < 3u; kp++) {
        sr_poi_reset(&impl);
        impl.phase = k_keep_phase[kp];
        impl.ack_at_send = 5u;
        impl.sent_tick_ms = 0u;
        ora = impl;
        ph = both(&impl, &ora, 99u, 99999u);
        CHECK(ph == (SrPoiPhase)k_keep_phase[kp]);
        CHECK(impl.phase == k_keep_phase[kp]);
        CHECK(impl.ack_at_send == 5u);
        CHECK(impl.sent_tick_ms == 0u);
        step_keep++;
        if(k_keep_phase[kp] == (uint8_t)SrPoiPhaseIdle) {
            step_idle++;
        }
    }

    /* c: WaitAck + ack != -> Done */
    sr_poi_reset(&impl);
    impl.phase = (uint8_t)SrPoiPhaseWaitAck;
    impl.ack_at_send = 5u;
    impl.sent_tick_ms = 1000u;
    ora = impl;
    ph = both(&impl, &ora, 6u, 1000u);
    CHECK(ph == SrPoiPhaseDone);
    CHECK(impl.phase == (uint8_t)SrPoiPhaseDone);
    step_done++;

    /* d: WaitAck + elapsed == 2000 -> NoReply (boundary is >=, not >) */
    sr_poi_reset(&impl);
    impl.phase = (uint8_t)SrPoiPhaseWaitAck;
    impl.ack_at_send = 5u;
    impl.sent_tick_ms = 1000u;
    ora = impl;
    ph = both(&impl, &ora, 5u, 1000u + 2000u);
    CHECK(ph == SrPoiPhaseNoReply);
    CHECK(impl.phase == (uint8_t)SrPoiPhaseNoReply);
    step_noreply++;

    /* e: WaitAck + elapsed == 1999 stays WaitAck */
    sr_poi_reset(&impl);
    impl.phase = (uint8_t)SrPoiPhaseWaitAck;
    impl.ack_at_send = 5u;
    impl.sent_tick_ms = 1000u;
    ora = impl;
    ph = both(&impl, &ora, 5u, 1000u + 1999u);
    CHECK(ph == SrPoiPhaseWaitAck);
    CHECK(impl.phase == (uint8_t)SrPoiPhaseWaitAck);
    step_wait++;

    /* wrap: sent_tick_ms = 0xFFFFFF00, tick_ms = 0x00000100 -> unsigned diff 512
     * < 2000, must stay WaitAck. A signed or naive sent+ACK <= tick comparison
     * is what this case is for. */
    sr_poi_reset(&impl);
    impl.phase = (uint8_t)SrPoiPhaseWaitAck;
    impl.ack_at_send = 7u;
    impl.sent_tick_ms = 0xFFFFFF00u;
    ora = impl;
    ph = both(&impl, &ora, 7u, 0x00000100u);
    CHECK(ph == SrPoiPhaseWaitAck);
    CHECK(impl.phase == (uint8_t)SrPoiPhaseWaitAck);
    wrap++;
    step_wait++;

    /* ackwrap: ack_at_send = 0xFFFFFFFF, ack_now = 0 -> wrapping != is true
     * -> Done. Written as ack_now > ack_at_send, this case stays WaitAck. */
    sr_poi_reset(&impl);
    impl.phase = (uint8_t)SrPoiPhaseWaitAck;
    impl.ack_at_send = 0xFFFFFFFFu;
    impl.sent_tick_ms = 10u;
    ora = impl;
    ph = both(&impl, &ora, 0u, 10u);
    CHECK(ph == SrPoiPhaseDone);
    CHECK(impl.phase == (uint8_t)SrPoiPhaseDone);
    ackwrap++;
    step_done++;

    /* Ack and timeout together: ack wins (table idx 11). */
    sr_poi_reset(&impl);
    impl.phase = (uint8_t)SrPoiPhaseWaitAck;
    impl.ack_at_send = 1u;
    impl.sent_tick_ms = 0u;
    ora = impl;
    ph = both(&impl, &ora, 2u, 2000u);
    CHECK(ph == SrPoiPhaseDone);
    step_done++;

    fprintf(
        stderr,
        "poi cover: gate_ok=%u gate_nolink=%u gate_notscan=%u gate_nofix=%u "
        "gate_total=%u step_idle=%u step_wait=%u step_done=%u step_noreply=%u "
        "step_keep=%u step_null=%u wrap=%u ackwrap=%u\n",
        gate_ok,
        gate_nolink,
        gate_notscan,
        gate_nofix,
        gate_total,
        step_idle,
        step_wait,
        step_done,
        step_noreply,
        step_keep,
        step_null,
        wrap,
        ackwrap);

    CHECK(gate_total == 84u);
    CHECK(gate_ok > 0u);
    CHECK(gate_nolink > 0u);
    CHECK(gate_notscan > 0u);
    CHECK(gate_nofix > 0u);
    CHECK(step_idle >= 1u);
    CHECK(step_wait >= 1u);
    CHECK(step_done >= 1u);
    CHECK(step_noreply >= 1u);
    CHECK(step_keep >= 1u);
    CHECK(step_null == 1u);
    CHECK(wrap == 1u);
    CHECK(ackwrap == 1u);

    s = sr_poi_status_text((uint8_t)SrPoiPhaseWaitAck, (uint8_t)SrPoiGateOk);
    CHECK(s != NULL);
    CHECK(cstr_len(s) == 11u);
    CHECK(cstr_len(s) <= 20u);
    s = sr_poi_status_text((uint8_t)SrPoiPhaseDone, (uint8_t)SrPoiGateOk);
    CHECK(s != NULL);
    CHECK(cstr_len(s) == 10u);
    s = sr_poi_status_text((uint8_t)SrPoiPhaseNoReply, (uint8_t)SrPoiGateOk);
    CHECK(s != NULL);
    /* "POI unconfirmed" = 15. Was 12 ("POI no reply") until 2026-09-04, renamed
     * because the peer does receive the command and stays silent on purpose.
     * Keep the literal: it pins the string length, an expression would not. */
    CHECK(cstr_len(s) == 15u);
    s = sr_poi_status_text((uint8_t)SrPoiPhaseIdle, (uint8_t)SrPoiGateNoFix);
    CHECK(s != NULL);
    CHECK(cstr_len(s) == 17u);
    s = sr_poi_status_text((uint8_t)SrPoiPhaseIdle, (uint8_t)SrPoiGateNoLink);
    CHECK(s != NULL);
    CHECK(cstr_len(s) == 12u);
    s = sr_poi_status_text((uint8_t)SrPoiPhaseIdle, (uint8_t)SrPoiGateOk);
    CHECK(s == NULL);
    s = sr_poi_status_text((uint8_t)SrPoiPhaseIdle, (uint8_t)SrPoiGateNotScanning);
    CHECK(s == NULL);

    /* Parser: prefix "POI tagged: " is SrCmdAckPoi. Not added to the 320-group
     * exhaustive in test_parse_marauder.c (cmdack cover numbers must not move). */
    memset(&parser, 0, sizeof(parser));
    memset(&ev, 0, sizeof(ev));
    r = sr_codec_marauder.feed_line(
        &parser, "POI tagged: POI 1 (12.34, -56.78)", 33u, &ev);
    CHECK(r == SrParseUnknown);
    CHECK(ev.kind == SrEventUnknown);
    CHECK(parser.cmdack.count[SrCmdAckPoi] == 1u);
    CHECK(parser.cmdack.rev == 1u);
    CHECK(parser.cmdack.count[SrCmdAckStart] == 0u);

    /* Window view: only [text, text+len) is readable (ADR-010). */
    win = (char*)malloc(12u);
    CHECK(win != NULL);
    if(win != NULL) {
        memcpy(win, "POI tagged: ", 12u);
        memset(&parser, 0, sizeof(parser));
        memset(&ev, 0, sizeof(ev));
        r = sr_codec_marauder.feed_line(&parser, win, 12u, &ev);
        CHECK(r == SrParseUnknown);
        CHECK(parser.cmdack.count[SrCmdAckPoi] == 1u);
        free(win);
    }

    memset(&parser, 0, sizeof(parser));
    r = sr_codec_marauder.feed_line(&parser, "POI tagged", 10u, &ev);
    CHECK(parser.cmdack.count[SrCmdAckPoi] == 0u);
    CHECK(parser.cmdack.rev == 0u);

    CHECK(sr_codec_marauder.build_poi_cmd != NULL);
    n = sr_codec_marauder.build_poi_cmd(cmdbuf, sizeof(cmdbuf));
    CHECK(n == 12u);
    CHECK(memcmp(cmdbuf, "wardrivepoi\n", 12u) == 0);
    CHECK(cmdbuf[12] == '\0');
    n = sr_codec_marauder.build_poi_cmd(cmdbuf, 12u);
    CHECK(n == 0u);

    return sr_test_failures;
}
