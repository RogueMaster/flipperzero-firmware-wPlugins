#pragma once

#include <gui/view.h>
#include "../carrier_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CarrierExtraView CarrierExtraView;

typedef void (*CarrierExtraViewSendCallback)(CarrierExtra extra, void* context);

CarrierExtraView* carrier_extra_view_alloc(void);
void carrier_extra_view_free(CarrierExtraView* view);
View* carrier_extra_view_get_view(CarrierExtraView* view);
void carrier_extra_view_set_state(CarrierExtraView* view, CarrierState* state);
/**
 * Point the view at the app's "last transmitted payload" buffer. The view only
 * reads it, so the string stays current without any copying.
 */
void carrier_extra_view_set_last_sent(CarrierExtraView* view, const char* last_sent);

void carrier_extra_view_set_send_callback(
    CarrierExtraView* view,
    CarrierExtraViewSendCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
