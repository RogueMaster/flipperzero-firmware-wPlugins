#pragma once

#include <gui/view.h>
#include "../goodweather_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GoodweatherExtraView GoodweatherExtraView;

typedef void (*GoodweatherExtraViewSendCallback)(GoodweatherExtra extra, void* context);

GoodweatherExtraView* goodweather_extra_view_alloc(void);
void goodweather_extra_view_free(GoodweatherExtraView* view);
View* goodweather_extra_view_get_view(GoodweatherExtraView* view);
void goodweather_extra_view_set_state(GoodweatherExtraView* view, GoodweatherState* state);
/**
 * Point the view at the app's "last transmitted payload" buffer. The view only
 * reads it, so the string stays current without any copying.
 */
void goodweather_extra_view_set_last_sent(GoodweatherExtraView* view, const char* last_sent);

void goodweather_extra_view_set_send_callback(
    GoodweatherExtraView* view,
    GoodweatherExtraViewSendCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
