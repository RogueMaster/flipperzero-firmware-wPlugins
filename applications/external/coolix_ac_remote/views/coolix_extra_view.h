#pragma once

#include <gui/view.h>
#include "../coolix_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CoolixExtraView CoolixExtraView;

// Callback for sending an extra command
typedef void (*CoolixExtraViewSendCallback)(CoolixExtra extra, void* context);

/**
 * Allocate extra-commands view
 * @return Allocated view
 */
CoolixExtraView* coolix_extra_view_alloc(void);

/**
 * Free extra-commands view
 * @param view View to free
 */
void coolix_extra_view_free(CoolixExtraView* view);

/**
 * Get underlying View
 * @param view Extra view
 * @return View pointer for ViewDispatcher
 */
View* coolix_extra_view_get_view(CoolixExtraView* view);

/**
 * Set state reference
 * @param view Extra view
 * @param state State pointer
 */
void coolix_extra_view_set_state(CoolixExtraView* view, CoolixState* state);

/**
 * Point the view at the app's "last transmitted payload" buffer. The view only
 * reads it, so the string stays current without any copying.
 */
void coolix_extra_view_set_last_sent(CoolixExtraView* view, const char* last_sent);

/**
 * Set callback for sending extra commands
 * @param view Extra view
 * @param callback Callback function
 * @param context Callback context
 */
void coolix_extra_view_set_send_callback(
    CoolixExtraView* view,
    CoolixExtraViewSendCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
