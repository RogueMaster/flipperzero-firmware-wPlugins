#pragma once

#include <gui/view.h>
#include <furi.h>
#include "../htw_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HtwMainView HtwMainView;

// Debounce delay for carousel parameters (ms)
#define HTW_SEND_DEBOUNCE_MS 800

// Callback types
typedef void (*HtwMainViewSendCallback)(void* context);
typedef void (*HtwMainViewNavigateCallback)(void* context);

/**
 * Allocate main view
 * @return Allocated view
 */
HtwMainView* htw_main_view_alloc(void);

/**
 * Free main view
 * @param view View to free
 */
void htw_main_view_free(HtwMainView* view);

/**
 * Get underlying View
 * @param view Main view
 * @return View pointer for ViewDispatcher
 */
View* htw_main_view_get_view(HtwMainView* view);

/**
 * Set state reference
 * @param view Main view
 * @param state State pointer
 */
void htw_main_view_set_state(HtwMainView* view, HtwState* state);

/**
 * Set callback for sending IR commands
 * @param view Main view
 * @param callback Callback function
 * @param context Callback context
 */
void htw_main_view_set_send_callback(
    HtwMainView* view,
    HtwMainViewSendCallback callback,
    void* context);

/**
 * Set callback for navigating to timer menu
 * @param view Main view
 * @param callback Callback function
 * @param context Callback context
 */
void htw_main_view_set_timer_callback(
    HtwMainView* view,
    HtwMainViewNavigateCallback callback,
    void* context);

/**
 * Set callback for navigating to setup menu
 * @param view Main view
 * @param callback Callback function
 * @param context Callback context
 */
void htw_main_view_set_setup_callback(
    HtwMainView* view,
    HtwMainViewNavigateCallback callback,
    void* context);

/**
 * Start sending animation
 * @param view Main view
 */
void htw_main_view_start_sending(HtwMainView* view);

/**
 * Update sending animation frame
 * @param view Main view
 */
void htw_main_view_update_sending(HtwMainView* view);

/**
 * Stop sending animation
 * @param view Main view
 */
void htw_main_view_stop_sending(HtwMainView* view);

/**
 * Get the last command that was triggered
 * Used by app to determine what IR to send
 *
 * @param view Main view
 * @param out_toggle Output: toggle command (if applicable)
 * @return 0 for state command, 1 for toggle command
 */
int htw_main_view_get_last_command(HtwMainView* view, HtwToggle* out_toggle);

#ifdef __cplusplus
}
#endif
