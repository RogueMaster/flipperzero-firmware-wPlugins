#include "sr_test.h"

#include "sr_handshake.h"
#include "sr_types.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

/*
 * Independent oracle: the 16 combinations for sent=true (rev increased /
 * marauder / expired / rx_grew) are baked into a lookup table instead of
 * duplicating sr_handshake.h's five if-statements.
 *
 * Index bits:
 *   bit0 = fw_rev_now > fw_rev_at_send
 *   bit1 = (fw_kind == SrSourceMarauder)
 *   bit2 = elapsed >= timeout
 *   bit3 = rx_bytes_now > rx_bytes_at_send
 *
 * The table is filled per the text contract in ADR-016 decision 4: once
 * Marauder is recognized it's immediately Ok; otherwise wait until timeout;
 * after timeout, a byte increment means UnknownFw, otherwise NoReply.
 */
static const SrHandshakeState k_sent_table[16] = {
    /* 0  0000 !rose !mara !exp !rx */ SrHandshakeWaiting,
    /* 1  0001  rose !mara !exp !rx */ SrHandshakeWaiting,
    /* 2  0010 !rose  mara !exp !rx */ SrHandshakeWaiting,
    /* 3  0011  rose  mara !exp !rx */ SrHandshakeOk,
    /* 4  0100 !rose !mara  exp !rx */ SrHandshakeNoReply,
    /* 5  0101  rose !mara  exp !rx */ SrHandshakeNoReply,
    /* 6  0110 !rose  mara  exp !rx */ SrHandshakeNoReply,
    /* 7  0111  rose  mara  exp !rx */ SrHandshakeOk,
    /* 8  1000 !rose !mara !exp  rx */ SrHandshakeWaiting,
    /* 9  1001  rose !mara !exp  rx */ SrHandshakeWaiting,
    /* 10 1010 !rose  mara !exp  rx */ SrHandshakeWaiting,
    /* 11 1011  rose  mara !exp  rx */ SrHandshakeOk,
    /* 12 1100 !rose !mara  exp  rx */ SrHandshakeUnknownFw,
    /* 13 1101  rose !mara  exp  rx */ SrHandshakeUnknownFw,
    /* 14 1110 !rose  mara  exp  rx */ SrHandshakeUnknownFw,
    /* 15 1111  rose  mara  exp  rx */ SrHandshakeOk,
};

static SrHandshakeState oracle_eval(const SrHandshakeCtx* c, uint32_t now_ms) {
    unsigned key;
    uint32_t elapsed;

    if(c == NULL) {
        return SrHandshakeIdle;
    }
    if(!c->sent) {
        return SrHandshakeIdle;
    }

    elapsed = (uint32_t)(now_ms - c->sent_tick_ms);
    key = 0;
    if(c->fw_rev_now > c->fw_rev_at_send) {
        key |= 1u;
    }
    if(c->fw_kind == SrSourceMarauder) {
        key |= 2u;
    }
    if(elapsed >= c->timeout_ms) {
        key |= 4u;
    }
    if(c->rx_bytes_now > c->rx_bytes_at_send) {
        key |= 8u;
    }
    return k_sent_table[key];
}

int test_handshake_run(void) {
    static const SrSourceKind k_kinds[4] = {
        SrSourceUnknown,
        SrSourceMarauder,
        SrSourceGhostesp,
        SrSourceNative,
    };
    unsigned sent_i;
    unsigned rose_i;
    unsigned kind_i;
    unsigned elapsed_i;
    unsigned rx_i;
    unsigned idle = 0;
    unsigned waiting = 0;
    unsigned ok = 0;
    unsigned unknown_fw = 0;
    unsigned no_reply = 0;
    unsigned total = 0;
    SrHandshakeCtx wrap;

    sr_test_failures = 0;

    for(sent_i = 0; sent_i < 2u; sent_i++) {
        for(rose_i = 0; rose_i < 2u; rose_i++) {
            for(kind_i = 0; kind_i < 4u; kind_i++) {
                for(elapsed_i = 0; elapsed_i < 2u; elapsed_i++) {
                    for(rx_i = 0; rx_i < 2u; rx_i++) {
                        SrHandshakeCtx c;
                        uint32_t now;
                        SrHandshakeState got;
                        SrHandshakeState exp;

                        c.sent = sent_i != 0u;
                        c.sent_tick_ms = 10000u;
                        c.timeout_ms = (uint32_t)SR_HANDSHAKE_TIMEOUT_MS;
                        c.rx_bytes_at_send = 10u;
                        c.rx_bytes_now = (rx_i != 0u) ? 20u : 10u;
                        c.fw_rev_at_send = 7u;
                        c.fw_rev_now = (rose_i != 0u) ? 8u : 7u;
                        c.fw_kind = k_kinds[kind_i];
                        now = (elapsed_i == 0u) ? 10100u : 11600u;

                        got = sr_handshake_eval(&c, now);
                        exp = oracle_eval(&c, now);
                        if(got != exp) {
                            fprintf(
                                stderr,
                                "handshake mismatch sent=%u rose=%u kind=%u elapsed=%u rx=%u "
                                "got=%d exp=%d\n",
                                sent_i,
                                rose_i,
                                kind_i,
                                elapsed_i,
                                rx_i,
                                (int)got,
                                (int)exp);
                        }
                        CHECK(got == exp);

                        if(got == SrHandshakeIdle) {
                            idle++;
                        } else if(got == SrHandshakeWaiting) {
                            waiting++;
                        } else if(got == SrHandshakeOk) {
                            ok++;
                        } else if(got == SrHandshakeUnknownFw) {
                            unknown_fw++;
                        } else if(got == SrHandshakeNoReply) {
                            no_reply++;
                        }
                        total++;
                    }
                }
            }
        }
    }

    printf(
        "handshake cover: idle=%u waiting=%u ok=%u unknown_fw=%u no_reply=%u total=%u\n",
        idle,
        waiting,
        ok,
        unknown_fw,
        no_reply,
        total);

    CHECK(idle == 32);
    CHECK(waiting == 14);
    CHECK(ok == 4);
    CHECK(unknown_fw == 7);
    CHECK(no_reply == 7);
    CHECK(total == 64);

    CHECK(sr_handshake_eval(NULL, 0) == SrHandshakeIdle);

    /* Tick wraparound: 0xFFFFFF00 -> 0x40 elapses 0x140 = 320 ms < 1500 -> Waiting */
    wrap.sent = true;
    wrap.sent_tick_ms = 0xFFFFFF00u;
    wrap.timeout_ms = (uint32_t)SR_HANDSHAKE_TIMEOUT_MS;
    wrap.rx_bytes_at_send = 0;
    wrap.rx_bytes_now = 0;
    wrap.fw_rev_at_send = 7u;
    wrap.fw_rev_now = 7u;
    wrap.fw_kind = SrSourceUnknown;
    CHECK(sr_handshake_eval(&wrap, 0x40u) == SrHandshakeWaiting);
    CHECK(oracle_eval(&wrap, 0x40u) == SrHandshakeWaiting);

    /* 0x00000700 − 0xFFFFFF00 = 0x800 = 2048 ms > 1500 → NoReply */
    CHECK(sr_handshake_eval(&wrap, 0x00000700u) == SrHandshakeNoReply);
    CHECK(oracle_eval(&wrap, 0x00000700u) == SrHandshakeNoReply);

    wrap.rx_bytes_now = 1;
    CHECK(sr_handshake_eval(&wrap, 0x00000700u) == SrHandshakeUnknownFw);
    CHECK(oracle_eval(&wrap, 0x00000700u) == SrHandshakeUnknownFw);

    /* Regression T3.3-FIX: cumulative rev is non-zero but didn't increase
     * this time + Marauder -> must not resolve to Ok. */
    wrap.sent = true;
    wrap.sent_tick_ms = 10000u;
    wrap.timeout_ms = (uint32_t)SR_HANDSHAKE_TIMEOUT_MS;
    wrap.rx_bytes_at_send = 10u;
    wrap.rx_bytes_now = 10u;
    wrap.fw_rev_at_send = 4u;
    wrap.fw_rev_now = 4u;
    wrap.fw_kind = SrSourceMarauder;
    CHECK(sr_handshake_eval(&wrap, 11600u) == SrHandshakeNoReply);
    CHECK(oracle_eval(&wrap, 11600u) == SrHandshakeNoReply);

    wrap.rx_bytes_now = 20u;
    CHECK(sr_handshake_eval(&wrap, 11600u) == SrHandshakeUnknownFw);
    CHECK(oracle_eval(&wrap, 11600u) == SrHandshakeUnknownFw);

    /* A8 group beta: now has wrapped, sent+timeout has not. T3.3's
     * exhaustive sweep only covered group alpha and had no teeth here. */
    wrap.sent = true;
    wrap.sent_tick_ms = 0x80000000u;
    wrap.timeout_ms = (uint32_t)SR_HANDSHAKE_TIMEOUT_MS;
    wrap.rx_bytes_at_send = 0;
    wrap.rx_bytes_now = 0;
    wrap.fw_rev_at_send = 7u;
    wrap.fw_rev_now = 7u;
    wrap.fw_kind = SrSourceUnknown;
    CHECK(sr_handshake_eval(&wrap, 0x100u) == SrHandshakeNoReply);

    return sr_test_failures;
}
