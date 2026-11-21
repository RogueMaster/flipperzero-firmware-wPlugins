#pragma once

#include <gui/view.h>

/*
 * Guided capture screen (Addendum 001 Section 4.2).
 * Left opens capture options, OK stops and keeps a raw recording, Right restarts detection.
 */

typedef enum {
    RfsCaptureViewEventOptions,
    RfsCaptureViewEventStop,
    RfsCaptureViewEventRestart,
} RfsCaptureViewEvent;

typedef void (*RfsCaptureViewCallback)(RfsCaptureViewEvent event, void* context);

typedef struct RfsCaptureView RfsCaptureView;

RfsCaptureView* rfs_capture_view_alloc(void);
void rfs_capture_view_free(RfsCaptureView* view);
View* rfs_capture_view_get_view(RfsCaptureView* view);
void rfs_capture_view_set_callback(
    RfsCaptureView* view,
    RfsCaptureViewCallback callback,
    void* context);

void rfs_capture_view_set_text(
    RfsCaptureView* view,
    const char* title,
    const char* line1,
    const char* line2,
    const char* line3);

/** RSSI bar; pass show=false to hide it. */
void rfs_capture_view_set_rssi(RfsCaptureView* view, bool show, float rssi, int32_t threshold);

/** Button hints; NULL hides a button. */
void rfs_capture_view_set_buttons(
    RfsCaptureView* view,
    const char* left,
    const char* center,
    const char* right);
