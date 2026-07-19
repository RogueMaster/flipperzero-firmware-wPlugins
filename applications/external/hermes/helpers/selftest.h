#pragma once

#include <furi.h>
#include "baud_table.h"

/* Rates the loopback is proven at. Spread across the range so a marginal cable
 * that works slowly and fails fast shows up as exactly that. */
#define SELFTEST_STEPS (4u)

typedef enum {
    SelfTestIdle,
    SelfTestRunning,
    SelfTestPassed, // every byte came back at every rate
    SelfTestPartial, // slow rates fine, fast ones not - a marginal link
    SelfTestFailed, // nothing came back at all
} SelfTestVerdict;

typedef struct {
    uint32_t baud;
    uint16_t sent;
    uint16_t echoed; // bytes that returned intact
    bool ok;
} SelfTestStep;

typedef struct {
    SelfTestVerdict verdict;
    uint8_t completed; // steps finished so far
    SelfTestStep steps[SELFTEST_STEPS];
} SelfTestResult;

typedef struct SelfTest SelfTest;

typedef void (*SelfTestCallback)(void* context);

SelfTest* selftest_alloc(void);
void selftest_free(SelfTest* st);

void selftest_set_callback(SelfTest* st, SelfTestCallback cb, void* context);

/** Send a pattern out TX and check it arrives back on RX.
 *
 * Requires the two pins bridged with a jumper. Proves the Flipper's own UART,
 * the pins and the wire, so a silent target can be blamed on the target.
 */
bool selftest_start(SelfTest* st, HermesPort port);
void selftest_stop(SelfTest* st);

bool selftest_is_running(const SelfTest* st);
uint8_t selftest_progress(const SelfTest* st); // 0..100

const SelfTestResult* selftest_result(const SelfTest* st);

/** One-line plain-English reading of the verdict. */
const char* selftest_verdict_text(SelfTestVerdict verdict);
