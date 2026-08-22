#pragma once

#include <gui/view.h>
#include "../delonghi_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DelonghiExtraView DelonghiExtraView;

typedef void (*DelonghiExtraViewSendCallback)(DelonghiExtra extra, void* context);

DelonghiExtraView* delonghi_extra_view_alloc(void);
void delonghi_extra_view_free(DelonghiExtraView* view);
View* delonghi_extra_view_get_view(DelonghiExtraView* view);
void delonghi_extra_view_set_state(DelonghiExtraView* view, DelonghiState* state);
/**
 * Point the view at the app's "last transmitted payload" buffer. The view only
 * reads it, so the string stays current without any copying.
 */
void delonghi_extra_view_set_last_sent(DelonghiExtraView* view, const char* last_sent);

void delonghi_extra_view_set_send_callback(
    DelonghiExtraView* view,
    DelonghiExtraViewSendCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
