#pragma once

#include <gui/view.h>
#include "../panasonic_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PanasonicExtraView PanasonicExtraView;

typedef void (*PanasonicExtraViewSendCallback)(PanasonicExtra extra, void* context);

PanasonicExtraView* panasonic_extra_view_alloc(void);
void panasonic_extra_view_free(PanasonicExtraView* view);
View* panasonic_extra_view_get_view(PanasonicExtraView* view);
void panasonic_extra_view_set_state(PanasonicExtraView* view, PanasonicState* state);
/**
 * Point the view at the app's "last transmitted payload" buffer. The view only
 * reads it, so the string stays current without any copying.
 */
void panasonic_extra_view_set_last_sent(PanasonicExtraView* view, const char* last_sent);

void panasonic_extra_view_set_send_callback(
    PanasonicExtraView* view,
    PanasonicExtraViewSendCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
