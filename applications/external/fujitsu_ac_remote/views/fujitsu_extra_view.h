#pragma once

#include <gui/view.h>
#include "../fujitsu_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FujitsuExtraView FujitsuExtraView;

typedef void (*FujitsuExtraViewSendCallback)(FujitsuExtra extra, void* context);

FujitsuExtraView* fujitsu_extra_view_alloc(void);
void fujitsu_extra_view_free(FujitsuExtraView* view);
View* fujitsu_extra_view_get_view(FujitsuExtraView* view);
void fujitsu_extra_view_set_state(FujitsuExtraView* view, FujitsuState* state);
/**
 * Point the view at the app's "last transmitted payload" buffer. The view only
 * reads it, so the string stays current without any copying.
 */
void fujitsu_extra_view_set_last_sent(FujitsuExtraView* view, const char* last_sent);

void fujitsu_extra_view_set_send_callback(
    FujitsuExtraView* view,
    FujitsuExtraViewSendCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
