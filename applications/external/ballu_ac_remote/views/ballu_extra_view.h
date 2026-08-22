#pragma once

#include <gui/view.h>
#include "../ballu_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BalluExtraView BalluExtraView;

typedef void (*BalluExtraViewSendCallback)(BalluExtra extra, void* context);

BalluExtraView* ballu_extra_view_alloc(void);
void ballu_extra_view_free(BalluExtraView* view);
View* ballu_extra_view_get_view(BalluExtraView* view);
void ballu_extra_view_set_state(BalluExtraView* view, BalluState* state);
/**
 * Point the view at the app's "last transmitted payload" buffer. The view only
 * reads it, so the string stays current without any copying.
 */
void ballu_extra_view_set_last_sent(BalluExtraView* view, const char* last_sent);

void ballu_extra_view_set_send_callback(
    BalluExtraView* view,
    BalluExtraViewSendCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
