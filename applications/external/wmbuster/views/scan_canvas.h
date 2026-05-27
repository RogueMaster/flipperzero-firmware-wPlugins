/* Custom canvas widget for the meter list. Stock Submenu can't render
 * signal bars or right-aligned values, so this widget owns its own
 * cursor / scroll / row data and is mutated only on the GUI thread. */

#pragma once

#include <gui/view.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct ScanCanvas ScanCanvas;

/* `meter_idx` is the index into WmbusApp::meters[] passed to the on-tap
 * callback. */
typedef struct {
    uint16_t meter_idx;
    int8_t   rssi;
    bool     encrypted;
    char     head[28];   /* friendly model / manufacturer name      */
    char     value[20];  /* short reading, e.g. "Now 3 u"           */
} ScanRow;

ScanCanvas* scan_canvas_alloc(void);
void        scan_canvas_free(ScanCanvas* sc);
View*       scan_canvas_get_view(ScanCanvas* sc);

/* OK-tap callback. ctx is the user-supplied pointer set previously. */
typedef void (*ScanCanvasTapCb)(void* ctx, uint16_t meter_idx);
void scan_canvas_set_tap_callback(ScanCanvas* sc, ScanCanvasTapCb cb, void* ctx);

/* Replace the row set entirely. `rows` is copied. */
void scan_canvas_set_rows(ScanCanvas* sc, const ScanRow* rows, size_t n);

/* Set header (band/mode) and footer (filter/sort) labels. `total_telegrams`
 * is shown next to the meter count so the user sees the radio is still
 * receiving even after the meter table fills. `stats_tail` is an optional
 * right-aligned diagnostics string — pass "" to hide it. */
void scan_canvas_set_meta(ScanCanvas* sc, const char* band,
                          const char* filter, const char* sort,
                          uint32_t total_telegrams,
                          const char* stats_tail);

/* Restore the cursor to the row whose meter_idx matches; fall back to
 * row 0 if not found. */
void scan_canvas_focus_meter(ScanCanvas* sc, uint16_t meter_idx);
