/*
 * Custom canvas detail view — replaces TextBox for the per-meter detail
 * screen. Shows a compact, label-aligned readout of the decoded values
 * for one meter, with scrolling when there are more rows than fit on
 * the 64-px-tall screen.
 *
 * Public API mirrors the philosophy of scan_canvas: caller pushes a
 * snapshot of the data via _set_*, the widget owns its own model and
 * input handling, and Back is routed through the standard navigation
 * callback that the scene manager provides.
 */

#pragma once

#include <gui/view.h>
#include <stdint.h>
#include <stddef.h>

typedef struct DetailCanvas DetailCanvas;

DetailCanvas* detail_canvas_alloc(void);
void          detail_canvas_free(DetailCanvas* dc);
View*         detail_canvas_get_view(DetailCanvas* dc);

/* Identity strip drawn at the top of the screen. The `badge` string is
 * rendered next to the packet counter — typical values are
 * "ENC" / "DEC" / "BAD" / "" — any 3-character tag fits. */
void detail_canvas_set_header(DetailCanvas* dc,
                              const char* line1,    /* e.g. "TCH 03776014"     */
                              const char* line2,    /* e.g. "Heat-cost alloc." */
                              int8_t rssi,
                              uint32_t telegram_count,
                              bool encrypted,
                              const char* badge);

/* Label/value rows. The widget renders only those that are non-empty
 * and skips trivially uninformative entries (zero-valued temperatures
 * etc.) — see the implementation for the filtering rules. */
typedef struct {
    char label[12];
    char value[20];
} DetailRow;

void detail_canvas_set_rows(DetailCanvas* dc, const DetailRow* rows, size_t n);

/* Push the raw APDU bytes so the user can toggle into a hex-dump view via
 * the OK button. When `apdu` is NULL or `apdu_len` is zero the canvas
 * disables the toggle and stays in label/value mode. The buffer is copied;
 * the caller may free or reuse it after the call returns. */
void detail_canvas_set_raw(DetailCanvas* dc,
                           const uint8_t* apdu, size_t apdu_len);
