#pragma once
// Recommendation table for alerts. Pure C (no firmware deps).
#include "alerts.h"

#define ADVICE_MAX_LINES 3
#define ADVICE_LINE_LEN 24 // incl. NUL; every line fits 128 px with the secondary font

/**
 * Fills up to ADVICE_MAX_LINES short tips for `rule`, using process names from `t`
 * (may be NULL). Returns the number of lines written.
 */
size_t advice_get(
    AlertRuleId rule,
    const PhrTelemetry* t,
    char lines[ADVICE_MAX_LINES][ADVICE_LINE_LEN]);
