/**
 * RollCall - verdict screen.
 *
 * The hero card: a letter-grade badge, a plain-English headline, a "replay
 * resistance" meter and the press/unique-code tally. OK opens the full
 * breakdown, Right re-runs the check.
 */
#pragma once

#include <gui/view.h>
#include "../helpers/analyzer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    VerdictEventDetails,
    VerdictEventRescan,
} VerdictEvent;

typedef void (*VerdictViewCallback)(void* context, VerdictEvent event);

typedef struct VerdictView VerdictView;

VerdictView* verdict_view_alloc(void);
void verdict_view_free(VerdictView* v);
View* verdict_view_get_view(VerdictView* v);

void verdict_view_set_callback(VerdictView* v, VerdictViewCallback cb, void* context);
void verdict_view_set_verdict(VerdictView* v, const RcVerdict* verdict);

#ifdef __cplusplus
}
#endif
