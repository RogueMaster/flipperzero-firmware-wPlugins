#pragma once

#include <gui/view.h>

/* ------------------------------------------------------------------ */
/*  Hex Viewer – custom View that shows a hex+ASCII dump              */
/*                                                                    */
/*  Displays 4 bytes per row:                                         */
/*    0000: 48 65 6C 6C  Hell                                         */
/*                                                                    */
/*  Scrollable with Up/Down keys.  Loads the first 4 KB of a file.   */
/* ------------------------------------------------------------------ */

typedef struct HexViewer HexViewer;

/** Allocate a HexViewer. */
HexViewer* hex_viewer_alloc(void);

/** Free a HexViewer. */
void hex_viewer_free(HexViewer* hv);

/** Get the underlying View* for use with ViewDispatcher. */
View* hex_viewer_get_view(HexViewer* hv);

/**
 * Load the first HEX_PREVIEW_SIZE bytes from `path` into the viewer.
 * Returns the number of bytes actually loaded (may be less for small files).
 */
uint32_t hex_viewer_load_file(HexViewer* hv, const char* path);
