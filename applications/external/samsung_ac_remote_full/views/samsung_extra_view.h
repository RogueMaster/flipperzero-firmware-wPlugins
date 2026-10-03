#pragma once

#include <gui/view.h>
#include "../samsung_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SamsungExtraView SamsungExtraView;

typedef void (*SamsungExtraViewSendCallback)(SamsungExtra extra, void* context);

SamsungExtraView* samsung_extra_view_alloc(void);
void samsung_extra_view_free(SamsungExtraView* view);
View* samsung_extra_view_get_view(SamsungExtraView* view);
void samsung_extra_view_set_state(SamsungExtraView* view, SamsungState* state);
/**
 * Point the view at the app's "last transmitted payload" buffer. The view only
 * reads it, so the string stays current without any copying.
 */
void samsung_extra_view_set_last_sent(SamsungExtraView* view, const char* last_sent);

void samsung_extra_view_set_send_callback(
    SamsungExtraView* view,
    SamsungExtraViewSendCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
