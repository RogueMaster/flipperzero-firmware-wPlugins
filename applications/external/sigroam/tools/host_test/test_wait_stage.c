#include "sr_test.h"

#include "sr_wait_stage.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/*
 * Oracle: pack (pending, rx_zero, ack_eq) into a 3-bit index and look it up
 * in an 8-entry table. Deliberately not the same code path as the
 * if-ladder in sr_wait_stage_eval.
 *
 * bit0 = cmd_pending
 * bit1 = rx_bytes == 0
 * bit2 = cmdack_now == cmdack_at_send
 *
 * pending=0 -> always None
 * pending=1, rx_zero=1 -> Link (ack irrelevant)
 * pending=1, rx_zero=0, ack_eq=1 -> Cmd
 * pending=1, rx_zero=0, ack_eq=0 -> Func
 */
static const SrWaitStage k_oracle[8] = {
    SrWaitStageNone, /* 000 !p !z !eq */
    SrWaitStageFunc, /* 001  p !z !eq */
    SrWaitStageNone, /* 010 !p  z !eq */
    SrWaitStageLink, /* 011  p  z !eq */
    SrWaitStageNone, /* 100 !p !z  eq */
    SrWaitStageCmd,  /* 101  p !z  eq */
    SrWaitStageNone, /* 110 !p  z  eq */
    SrWaitStageLink, /* 111  p  z  eq */
};

static SrWaitStage oracle_wait(const SrWaitCtx* c) {
    unsigned bits = 0;

    if(c == NULL) {
        return SrWaitStageNone;
    }
    if(c->cmd_pending) {
        bits |= 1u;
    }
    if(c->rx_bytes == 0u) {
        bits |= 2u;
    }
    if(c->cmdack_now == c->cmdack_at_send) {
        bits |= 4u;
    }
    return k_oracle[bits];
}

int test_wait_stage_run(void) {
    static const bool k_pending[2] = {false, true};
    static const uint32_t k_rx[3] = {0u, 1u, 0x80000000u};
    static const uint32_t k_ack_now[7] = {
        0u, 5u, 6u, 5u, 0u, 0xFFFFFFFFu, 0xFFFFFFFFu};
    static const uint32_t k_ack_at[7] = {
        0u, 5u, 5u, 6u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0u};
    unsigned n_none = 0;
    unsigned n_link = 0;
    unsigned n_cmd = 0;
    unsigned n_func = 0;
    unsigned i;
    unsigned j;
    unsigned k;
    SrWaitCtx c;
    SrWaitCtx wrap;
    SrWaitStage got;
    SrWaitStage want;

    sr_test_failures = 0;

    CHECK(sr_wait_stage_eval(NULL) == SrWaitStageNone);

    for(i = 0; i < 2u; i++) {
        for(j = 0; j < 3u; j++) {
            for(k = 0; k < 7u; k++) {
                c.cmd_pending = k_pending[i];
                c.rx_bytes = k_rx[j];
                c.cmdack_now = k_ack_now[k];
                c.cmdack_at_send = k_ack_at[k];
                got = sr_wait_stage_eval(&c);
                want = oracle_wait(&c);
                CHECK(got == want);
                if(got == SrWaitStageNone) {
                    n_none++;
                } else if(got == SrWaitStageLink) {
                    n_link++;
                } else if(got == SrWaitStageCmd) {
                    n_cmd++;
                } else if(got == SrWaitStageFunc) {
                    n_func++;
                }
            }
        }
    }

    fprintf(
        stderr,
        "wait_stage cover: none=%u link=%u cmd=%u func=%u\n",
        n_none,
        n_link,
        n_cmd,
        n_func);
    CHECK(n_none > 0u);
    CHECK(n_link > 0u);
    CHECK(n_cmd > 0u);
    CHECK(n_func > 0u);

    /* Wraparound case: at_send=0xFFFFFFFF / now=0 must resolve to Func (==);
     * writing it as > would wrongly yield Cmd. */
    wrap.cmd_pending = true;
    wrap.rx_bytes = 1u;
    wrap.cmdack_at_send = 0xFFFFFFFFu;
    wrap.cmdack_now = 0u;
    CHECK(sr_wait_stage_eval(&wrap) == SrWaitStageFunc);

    wrap.cmdack_at_send = 5u;
    wrap.cmdack_now = 5u;
    CHECK(sr_wait_stage_eval(&wrap) == SrWaitStageCmd);

    wrap.cmdack_now = 6u;
    CHECK(sr_wait_stage_eval(&wrap) == SrWaitStageFunc);

    return sr_test_failures;
}
