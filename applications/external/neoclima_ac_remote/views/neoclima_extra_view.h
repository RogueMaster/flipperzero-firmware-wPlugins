#pragma once

#include <gui/view.h>
#include "../neoclima_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NeoclimaExtraView NeoclimaExtraView;

typedef void (*NeoclimaExtraViewSendCallback)(NeoclimaExtra extra, void* context);

NeoclimaExtraView* neoclima_extra_view_alloc(void);
void neoclima_extra_view_free(NeoclimaExtraView* view);
View* neoclima_extra_view_get_view(NeoclimaExtraView* view);
void neoclima_extra_view_set_state(NeoclimaExtraView* view, NeoclimaState* state);
/**
 * Point the view at the app's "last transmitted payload" buffer. The view only
 * reads it, so the string stays current without any copying.
 */
void neoclima_extra_view_set_last_sent(NeoclimaExtraView* view, const char* last_sent);

void neoclima_extra_view_set_send_callback(
    NeoclimaExtraView* view,
    NeoclimaExtraViewSendCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
