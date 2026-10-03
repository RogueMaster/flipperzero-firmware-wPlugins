#pragma once

#include <gui/view.h>
#include "../kelon_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct KelonExtraView KelonExtraView;

typedef void (*KelonExtraViewSendCallback)(KelonExtra extra, void* context);

KelonExtraView* kelon_extra_view_alloc(void);
void kelon_extra_view_free(KelonExtraView* view);
View* kelon_extra_view_get_view(KelonExtraView* view);
void kelon_extra_view_set_state(KelonExtraView* view, KelonState* state);
/**
 * Point the view at the app's "last transmitted payload" buffer. The view only
 * reads it, so the string stays current without any copying.
 */
void kelon_extra_view_set_last_sent(KelonExtraView* view, const char* last_sent);

void kelon_extra_view_set_send_callback(
    KelonExtraView* view,
    KelonExtraViewSendCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
