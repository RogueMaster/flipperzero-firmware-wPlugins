#pragma once

#include <gui/view.h>
#include "../mitsubishi_heavy_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MitsubishiHeavyExtraView MitsubishiHeavyExtraView;

typedef void (*MitsubishiHeavyExtraViewSendCallback)(MitsubishiHeavyExtra extra, void* context);

MitsubishiHeavyExtraView* mitsubishi_heavy_extra_view_alloc(void);
void mitsubishi_heavy_extra_view_free(MitsubishiHeavyExtraView* view);
View* mitsubishi_heavy_extra_view_get_view(MitsubishiHeavyExtraView* view);
void mitsubishi_heavy_extra_view_set_state(
    MitsubishiHeavyExtraView* view,
    MitsubishiHeavyState* state);
/**
 * Point the view at the app's "last transmitted payload" buffer. The view only
 * reads it, so the string stays current without any copying.
 */
void mitsubishi_heavy_extra_view_set_last_sent(
    MitsubishiHeavyExtraView* view,
    const char* last_sent);

void mitsubishi_heavy_extra_view_set_send_callback(
    MitsubishiHeavyExtraView* view,
    MitsubishiHeavyExtraViewSendCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
