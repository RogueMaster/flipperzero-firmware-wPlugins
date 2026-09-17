#include "sr_test.h"

#include "sr_rx_policy.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/*
 * Independent oracle: a per-bit if-chain with literal shifts, no
 * SR_RX_BIT_* combined masks, and no call into sr_rx_decide. The style is
 * deliberately different from the implementation under test, so this check
 * cannot self-validate.
 */
static SrRxDecision oracle_decide(uint32_t bits, bool closing) {
    unsigned data = 0;
    unsigned frame = 0;
    unsigned noise = 0;
    unsigned overrun = 0;
    unsigned parity = 0;
    SrRxDecision o;

    if(bits & 1u) {
        data = 1;
    }
    if(bits & 4u) {
        frame = 1;
    }
    if(bits & 8u) {
        noise = 1;
    }
    if(bits & 16u) {
        overrun = 1;
    }
    if(bits & 32u) {
        parity = 1;
    }

    o.drain = data ? true : false;
    o.enqueue = (data != 0u && !closing) ? true : false;
    o.error = (frame | noise | overrun | parity) ? true : false;
    return o;
}

static void dump_bits(uint32_t bits, char* out, size_t cap) {
    size_t n = 0;
    unsigned first = 1;
    unsigned i;
    static const char* names[6] = {"DATA", "IDLE", "FRAME", "NOISE", "OVERRUN", "PARITY"};

    if(cap == 0) {
        return;
    }
    out[0] = '\0';
    for(i = 0; i < 6u; i++) {
        if((bits & (1u << i)) == 0u) {
            continue;
        }
        int w = snprintf(
            out + n,
            cap - n,
            "%s%s",
            first ? "" : "|",
            names[i]);
        if(w < 0) {
            return;
        }
        n += (size_t)w;
        if(n >= cap) {
            out[cap - 1] = '\0';
            return;
        }
        first = 0;
    }
    if(first && cap > 1) {
        snprintf(out, cap, "none");
    }
}

int test_rx_policy_run(void) {
    uint32_t bits;
    int closing_i;
    unsigned drain_true = 0;
    unsigned enqueue_true = 0;
    unsigned error_true = 0;
    unsigned total = 0;
    SrRxDecision combo_idle;
    SrRxDecision combo_over;
    SrRxDecision close_data;
    SrRxDecision idle_only;

    for(bits = 0; bits < 64u; bits++) {
        for(closing_i = 0; closing_i < 2; closing_i++) {
            bool closing = closing_i != 0;
            SrRxDecision got = sr_rx_decide(bits, closing);
            SrRxDecision exp = oracle_decide(bits, closing);

            if(got.drain != exp.drain || got.enqueue != exp.enqueue ||
               got.error != exp.error) {
                char label[64];
                dump_bits(bits, label, sizeof(label));
                fprintf(
                    stderr,
                    "rx_policy mismatch bits=0x%02x (%s) closing=%d "
                    "got(drain=%d enqueue=%d error=%d) "
                    "exp(drain=%d enqueue=%d error=%d)\n",
                    bits,
                    label,
                    closing ? 1 : 0,
                    got.drain ? 1 : 0,
                    got.enqueue ? 1 : 0,
                    got.error ? 1 : 0,
                    exp.drain ? 1 : 0,
                    exp.enqueue ? 1 : 0,
                    exp.error ? 1 : 0);
            }
            CHECK(got.drain == exp.drain);
            CHECK(got.enqueue == exp.enqueue);
            CHECK(got.error == exp.error);

            if(got.drain) {
                drain_true++;
            }
            if(got.enqueue) {
                enqueue_true++;
            }
            if(got.error) {
                error_true++;
            }
            total++;
        }
    }

    printf(
        "rx_policy cover: drain_true=%u enqueue_true=%u error_true=%u total=%u\n",
        drain_true,
        enqueue_true,
        error_true,
        total);

    CHECK(drain_true == 64);
    CHECK(enqueue_true == 32);
    CHECK(error_true == 120);
    CHECK(total == 128);

    /* The exact case an == comparison would miss is Data|Idle. */
    combo_idle = sr_rx_decide(SR_RX_BIT_DATA | SR_RX_BIT_IDLE, false);
    CHECK(combo_idle.drain == true);

    combo_over = sr_rx_decide(SR_RX_BIT_DATA | SR_RX_BIT_OVERRUN, false);
    CHECK(combo_over.drain && combo_over.enqueue && combo_over.error);

    close_data = sr_rx_decide(SR_RX_BIT_DATA, true);
    CHECK(close_data.drain == true && close_data.enqueue == false);

    idle_only = sr_rx_decide(SR_RX_BIT_IDLE, false);
    CHECK(idle_only.drain == false);

    return sr_test_failures;
}
