/**
 * RollCall - capture screen.
 *
 * The "listening" view: a live antenna emitting expanding rings, a big
 * captured/target press counter, a row of press slots that fill as you press,
 * and the protocol of the last decoded press.
 *
 * Along the bottom is the signal row - a carrier-strength bar and an RF
 * activity pip. That row is the difference between a screen that just sits
 * there and one that tells you WHY nothing is landing: a dead bar means you
 * are on the wrong frequency, while a live bar with no presses counted means
 * you are on the right one but the protocol is not being decoded.
 *
 * OK finishes early and analyses.
 */
#pragma once

#include <gui/view.h>
#include "../helpers/rc_radio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CaptureEventFinish, // user pressed OK to analyse now
} CaptureEvent;

typedef void (*CaptureViewCallback)(void* context, CaptureEvent event);

typedef struct CaptureView CaptureView;

CaptureView* capture_view_alloc(void);
void capture_view_free(CaptureView* v);
View* capture_view_get_view(CaptureView* v);

void capture_view_set_callback(CaptureView* v, CaptureViewCallback cb, void* context);

/** Band/modulation labels + the target press count for this run. */
void capture_view_set_config(CaptureView* v, const char* band, const char* mod, uint8_t target);

/** Update the live progress after each registered press. */
void capture_view_set_progress(
    CaptureView* v,
    uint8_t count,
    const char* protocol,
    RcCodeClass cls,
    uint16_t bits);

/** Carrier strength in dBm plus whether raw pulses are currently flowing. */
void capture_view_set_signal(CaptureView* v, int8_t rssi_dbm, bool active);

void capture_view_reset(CaptureView* v);
void capture_view_tick(CaptureView* v);

#ifdef __cplusplus
}
#endif
