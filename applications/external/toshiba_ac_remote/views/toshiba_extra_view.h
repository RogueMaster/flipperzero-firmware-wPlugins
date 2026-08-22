#pragma once

#include <gui/view.h>
#include "../toshiba_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ToshibaExtraView ToshibaExtraView;

typedef void (*ToshibaExtraViewSendCallback)(ToshibaExtra extra, void* context);

ToshibaExtraView* toshiba_extra_view_alloc(void);
void toshiba_extra_view_free(ToshibaExtraView* view);
View* toshiba_extra_view_get_view(ToshibaExtraView* view);
void toshiba_extra_view_set_state(ToshibaExtraView* view, ToshibaState* state);
/**
 * Point the view at the app's "last transmitted payload" buffer. The view only
 * reads it, so the string stays current without any copying.
 */
void toshiba_extra_view_set_last_sent(ToshibaExtraView* view, const char* last_sent);

void toshiba_extra_view_set_send_callback(
    ToshibaExtraView* view,
    ToshibaExtraViewSendCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
