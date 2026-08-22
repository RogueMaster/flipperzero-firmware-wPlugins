#pragma once

#include <gui/view.h>
#include "../gree_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GreeExtraView GreeExtraView;

typedef void (*GreeExtraViewSendCallback)(GreeExtra extra, void* context);

GreeExtraView* gree_extra_view_alloc(void);
void gree_extra_view_free(GreeExtraView* view);
View* gree_extra_view_get_view(GreeExtraView* view);
void gree_extra_view_set_state(GreeExtraView* view, GreeState* state);
/**
 * Point the view at the app's "last transmitted payload" buffer. The view only
 * reads it, so the string stays current without any copying.
 */
void gree_extra_view_set_last_sent(GreeExtraView* view, const char* last_sent);

void gree_extra_view_set_send_callback(
    GreeExtraView* view,
    GreeExtraViewSendCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
