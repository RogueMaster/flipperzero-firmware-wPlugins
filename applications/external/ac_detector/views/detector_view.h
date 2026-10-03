#pragma once

#include <gui/view.h>

#include "../ac_decode.h"

typedef struct DetectorView DetectorView;

typedef void (*DetectorViewCallback)(void* context);

DetectorView* detector_view_alloc(void);
void detector_view_free(DetectorView* v);
View* detector_view_get_view(DetectorView* v);

/// Called when the user presses Back, so the app can exit.
void detector_view_set_back_callback(DetectorView* v, DetectorViewCallback cb, void* context);

/// Show a decoded signal. Only ever called with a detection the decoder was
/// willing to act on, so what is on screen survives noise untouched.
void detector_view_set_result(DetectorView* v, const AcDetection* d);

/// A capture arrived and was thrown away. Blinks the receive marker in the
/// hollow style and bumps the counter shown while idle.
void detector_view_note_noise(DetectorView* v);

/// Drive the marquee and the receive marker. Call a few times a second.
void detector_view_tick(DetectorView* v);
