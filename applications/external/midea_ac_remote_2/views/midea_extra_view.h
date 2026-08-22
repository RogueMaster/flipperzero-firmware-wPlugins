#pragma once

#include <gui/view.h>
#include "../midea_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MideaExtraView MideaExtraView;

typedef void (*MideaExtraViewSendCallback)(MideaExtra extra, void* context);

MideaExtraView* midea_extra_view_alloc(void);
void midea_extra_view_free(MideaExtraView* view);
View* midea_extra_view_get_view(MideaExtraView* view);
void midea_extra_view_set_state(MideaExtraView* view, MideaState* state);
/**
 * Point the view at the app's "last transmitted payload" buffer. The view only
 * reads it, so the string stays current without any copying.
 */
void midea_extra_view_set_last_sent(MideaExtraView* view, const char* last_sent);

void midea_extra_view_set_send_callback(
    MideaExtraView* view,
    MideaExtraViewSendCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
