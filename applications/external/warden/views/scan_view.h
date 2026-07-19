#pragma once

#include <gui/view.h>

/* The "present your badge" screen: an animated reader reticle with a sweeping
 * scan bar and a pulsing NFC field, shown while the worker waits for a card. */

typedef struct ScanView ScanView;

ScanView* scan_view_alloc(void);
void scan_view_free(ScanView* v);
View* scan_view_get_view(ScanView* v);

void scan_view_reset(ScanView* v); // restart the animation phase
void scan_view_tick(ScanView* v); // advance one animation frame (UI tick)
