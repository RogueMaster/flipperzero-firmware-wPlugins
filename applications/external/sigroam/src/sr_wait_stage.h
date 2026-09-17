#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * ★ Pure-logic decision layer. Must not include any furi header (ADR-003).
 *
 * T4.10 / ADR-022 decision 1: while a command is in flight, determine which layer it is stuck
 * at so the UI can say what it is waiting for.
 * The four-layer observation is itself a **determinate** progress indicator -- advancing from
 * one layer to the next IS visible progress (NN/g, 2014-10-26, "progress indicators": beyond
 * 10 s you must show distinguishable progress; D7 measured 40 s+).
 *
 * NOTE: semantic boundary (ADR-022 decision 4 + src/sr_types.h:227-232; UI wording must comply):
 *   SrWaitStageCmd expresses only the **observation** that the command has not been processed
 *   by the CLI, and **must not** be worded as any specific cause -- there are at least two
 *   causes, and they call for opposite responses.
 *
 * NOTE: cmdack is a **wrapping counter**: this file compares with == / !=, and changing that
 *    to > is forbidden.
 *    The dedicated wraparound test in test_wait_stage.c pins this with at_send=0xFFFFFFFF / now=0.
 *
 * This layer **touches no clock**: it judges no timeout and accepts no tick argument. How long
 * counts as failure is sr_scan_ctl.h's separate responsibility; keeping the two independently
 * editable is what makes attribution unique (plan section 6, constraint 1).
 */

typedef enum {
    SrWaitStageNone = 0, /* No pending command */
    SrWaitStageLink,     /* L1 not reached: no bytes received yet -> waiting for Scout to speak */
    SrWaitStageCmd,      /* L2 not reached: command not processed by the CLI -> waiting for it to be received */
    SrWaitStageFunc,     /* L3 not reached: command received, function not yet started -> waiting for the scan to start/stop */
} SrWaitStage;

typedef struct {
    bool cmd_pending;        /* From SrScanCtlCtx.cmd_pending */
    uint32_t rx_bytes;       /* From SrIoStats.rx_bytes */
    uint32_t cmdack_now;     /* Current value of sr_worker_cmdack_count() */
    uint32_t cmdack_at_send; /* Snapshot of that same counter taken **before** queuing the command */
} SrWaitCtx;

static inline SrWaitStage sr_wait_stage_eval(const SrWaitCtx* c) {
    if(c == NULL || !c->cmd_pending) {
        return SrWaitStageNone;
    }
    /* The L1 criterion is "no bytes received yet" (verbatim from the plan's section 5.2 table
     * row 1 and section 9.1 table row L1),
     * not "the delta since the command was sent" -- the latter would need another snapshot, and
     * rx ceasing to grow does not mean the link is dead. */
    if(c->rx_bytes == 0u) {
        return SrWaitStageLink;
    }
    if(c->cmdack_now == c->cmdack_at_send) {
        return SrWaitStageCmd;
    }
    return SrWaitStageFunc;
}
