#pragma once

#include <gui/view.h>
#include "../lg_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LgExtraView LgExtraView;

typedef void (*LgExtraViewSendCallback)(LgExtra extra, void* context);

LgExtraView* lg_extra_view_alloc(void);
void lg_extra_view_free(LgExtraView* view);
View* lg_extra_view_get_view(LgExtraView* view);
void lg_extra_view_set_state(LgExtraView* view, LgState* state);
/**
 * Point the view at the app's "last transmitted payload" buffer. The view only
 * reads it, so the string stays current without any copying.
 */
void lg_extra_view_set_last_sent(LgExtraView* view, const char* last_sent);

void lg_extra_view_set_send_callback(
    LgExtraView* view,
    LgExtraViewSendCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
