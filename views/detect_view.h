#pragma once

#include <gui/view.h>
#include "../helpers/autobaud.h"

typedef enum {
    DetectPhaseListening, // timing edges on the raw pin
    DetectPhaseVerifying, // re-opened as a UART, scoring candidates
} DetectPhase;

typedef struct DetectView DetectView;

DetectView* detect_view_alloc(void);
void detect_view_free(DetectView* dv);
View* detect_view_get_view(DetectView* dv);

void detect_view_reset(DetectView* dv);
void detect_view_set_phase(DetectView* dv, DetectPhase phase);
void detect_view_set_port(DetectView* dv, const char* port_name, const char* pins);

/** Newest segments, for the live waveform. `bit_time` scales the trace so one
 *  bit is a fixed width; pass 0 before a fit exists and the view will guess. */
void detect_view_set_trace(
    DetectView* dv,
    const AutobaudSegment* segs,
    size_t count,
    uint32_t bit_time);

void detect_view_set_listen(DetectView* dv, uint32_t edges, uint32_t target, uint32_t live_baud);
void detect_view_set_verify(DetectView* dv, uint8_t percent, uint32_t baud);

/** Drives the "is this thing even plugged in?" hint. */
void detect_view_set_idle(DetectView* dv, bool line_idle_high);
