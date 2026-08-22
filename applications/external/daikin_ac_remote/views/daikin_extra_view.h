#pragma once

#include <gui/view.h>
#include "../daikin_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DaikinExtraView DaikinExtraView;

typedef void (*DaikinExtraViewSendCallback)(DaikinExtra extra, void* context);

DaikinExtraView* daikin_extra_view_alloc(void);
void daikin_extra_view_free(DaikinExtraView* view);
View* daikin_extra_view_get_view(DaikinExtraView* view);
void daikin_extra_view_set_state(DaikinExtraView* view, DaikinState* state);
/**
 * Point the view at the app's "last transmitted payload" buffer. The view only
 * reads it, so the string stays current without any copying.
 */
void daikin_extra_view_set_last_sent(DaikinExtraView* view, const char* last_sent);

void daikin_extra_view_set_send_callback(
    DaikinExtraView* view,
    DaikinExtraViewSendCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
