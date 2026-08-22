#pragma once

#include <gui/view.h>
#include "../haier_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HaierExtraView HaierExtraView;

typedef void (*HaierExtraViewSendCallback)(HaierExtra extra, void* context);

HaierExtraView* haier_extra_view_alloc(void);
void haier_extra_view_free(HaierExtraView* view);
View* haier_extra_view_get_view(HaierExtraView* view);
void haier_extra_view_set_state(HaierExtraView* view, HaierState* state);
/**
 * Point the view at the app's "last transmitted payload" buffer. The view only
 * reads it, so the string stays current without any copying.
 */
void haier_extra_view_set_last_sent(HaierExtraView* view, const char* last_sent);

void haier_extra_view_set_send_callback(
    HaierExtraView* view,
    HaierExtraViewSendCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
