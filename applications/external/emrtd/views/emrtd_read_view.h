/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * The screen shown while a document is being read.
 *
 * A passport read is slow - PACE alone is several seconds of elliptic curve
 * arithmetic, and DG2 is tens of kilobytes through a 106 kbit link - so the
 * screen has to keep saying something true the whole time. It shows the stage,
 * the file in flight, a progress bar, and the access method as soon as it is
 * known, so that a failure halfway through still tells the user how far it got.
 */
#pragma once

#include <gui/view.h>

#include "../worker/emrtd_worker.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EmrtdReadView EmrtdReadView;

EmrtdReadView* emrtd_read_view_alloc(void);
void emrtd_read_view_free(EmrtdReadView* instance);
View* emrtd_read_view_get_view(EmrtdReadView* instance);

/** Update the display. Call from the GUI thread only. */
void emrtd_read_view_set_progress(EmrtdReadView* instance, const EmrtdWorkerProgress* progress);

/** Show the access method once a driver has succeeded, e.g. "PACE AES-128". */
void emrtd_read_view_set_access(EmrtdReadView* instance, const char* text);

/** Invoked when the user presses back. */
void emrtd_read_view_set_callback(EmrtdReadView* instance, void (*callback)(void*), void* context);

#ifdef __cplusplus
}
#endif
