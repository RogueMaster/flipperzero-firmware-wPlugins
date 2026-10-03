#pragma once

#include <gui/view.h>
#include "../kelvinator_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct KelvinatorExtraView KelvinatorExtraView;

typedef void (*KelvinatorExtraViewSendCallback)(KelvinatorExtra extra, void* context);

KelvinatorExtraView* kelvinator_extra_view_alloc(void);
void kelvinator_extra_view_free(KelvinatorExtraView* view);
View* kelvinator_extra_view_get_view(KelvinatorExtraView* view);
void kelvinator_extra_view_set_state(KelvinatorExtraView* view, KelvinatorState* state);
/**
 * Point the view at the app's "last transmitted payload" buffer. The view only
 * reads it, so the string stays current without any copying.
 */
void kelvinator_extra_view_set_last_sent(KelvinatorExtraView* view, const char* last_sent);

void kelvinator_extra_view_set_send_callback(
    KelvinatorExtraView* view,
    KelvinatorExtraViewSendCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
