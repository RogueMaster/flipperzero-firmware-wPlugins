#pragma once

#include <gui/view.h>
#include "../htw_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HtwTimerView HtwTimerView;

// Timer command type
typedef enum {
    HtwTimerCommandOn,
    HtwTimerCommandOff
} HtwTimerCommand;

// Callback for sending timer command
typedef void (*HtwTimerViewSendCallback)(HtwTimerCommand cmd, void* context);

/**
 * Allocate timer view
 * @return Allocated view
 */
HtwTimerView* htw_timer_view_alloc(void);

/**
 * Free timer view
 * @param view View to free
 */
void htw_timer_view_free(HtwTimerView* view);

/**
 * Get underlying View
 * @param view Timer view
 * @return View pointer for ViewDispatcher
 */
View* htw_timer_view_get_view(HtwTimerView* view);

/**
 * Set state reference
 * @param view Timer view
 * @param state State pointer
 */
void htw_timer_view_set_state(HtwTimerView* view, HtwState* state);

/**
 * Set callback for sending timer commands
 * @param view Timer view
 * @param callback Callback function
 * @param context Callback context
 */
void htw_timer_view_set_send_callback(
    HtwTimerView* view,
    HtwTimerViewSendCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
