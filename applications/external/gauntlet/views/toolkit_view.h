/**
 * Gauntlet - the decoder Toolkit view.
 *
 * A carousel over the codecs in helpers/ctf_codec. Left/Right flip the decoder,
 * Up/Down scroll the output (or, for ROT, dial the shift and watch the
 * plaintext fall into place). OK hands the current output back so it can be
 * used as a flag. Fed either from a challenge's payload or free text.
 */
#pragma once

#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ToolkitEventUse, // OK -> use the current output as a flag
} ToolkitEvent;

typedef void (*ToolkitViewCallback)(void* context, ToolkitEvent event);

typedef struct ToolkitView ToolkitView;

ToolkitView* toolkit_view_alloc(void);
void toolkit_view_free(ToolkitView* v);
View* toolkit_view_get_view(ToolkitView* v);
void toolkit_view_set_callback(ToolkitView* v, ToolkitViewCallback cb, void* context);

/** Load the source text to decode; resets to the first codec. */
void toolkit_view_set_source(ToolkitView* v, const char* src);
/** Copy the current decoded output into buf. */
void toolkit_view_get_output(ToolkitView* v, char* buf, size_t sz);

#ifdef __cplusplus
}
#endif
