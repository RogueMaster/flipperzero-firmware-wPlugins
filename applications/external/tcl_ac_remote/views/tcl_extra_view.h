#pragma once

#include <gui/view.h>
#include "../tcl_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TclExtraView TclExtraView;

typedef void (*TclExtraViewSendCallback)(TclExtra extra, void* context);

TclExtraView* tcl_extra_view_alloc(void);
void tcl_extra_view_free(TclExtraView* view);
View* tcl_extra_view_get_view(TclExtraView* view);
void tcl_extra_view_set_state(TclExtraView* view, TclState* state);
/**
 * Point the view at the app's "last transmitted payload" buffer. The view only
 * reads it, so the string stays current without any copying.
 */
void tcl_extra_view_set_last_sent(TclExtraView* view, const char* last_sent);

void tcl_extra_view_set_send_callback(
    TclExtraView* view,
    TclExtraViewSendCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
