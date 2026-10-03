#pragma once

#include <gui/view.h>
#include "../mitsubishi_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MitsubishiExtraView MitsubishiExtraView;

typedef void (*MitsubishiExtraViewSendCallback)(MitsubishiExtra extra, void* context);

MitsubishiExtraView* mitsubishi_extra_view_alloc(void);
void mitsubishi_extra_view_free(MitsubishiExtraView* view);
View* mitsubishi_extra_view_get_view(MitsubishiExtraView* view);
void mitsubishi_extra_view_set_state(MitsubishiExtraView* view, MitsubishiState* state);
/**
 * Point the view at the app's "last transmitted payload" buffer. The view only
 * reads it, so the string stays current without any copying.
 */
void mitsubishi_extra_view_set_last_sent(MitsubishiExtraView* view, const char* last_sent);

void mitsubishi_extra_view_set_send_callback(
    MitsubishiExtraView* view,
    MitsubishiExtraViewSendCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
