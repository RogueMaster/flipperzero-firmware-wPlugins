#pragma once

#include <gui/view.h>
#include <furi.h>
#include "../coolix_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CoolixMainView CoolixMainView;

// Debounce delay for temperature changes (ms)
#define COOLIX_SEND_DEBOUNCE_MS 800

// Callback types
typedef void (*CoolixMainViewSendCallback)(void* context);
typedef void (*CoolixMainViewNavigateCallback)(void* context);

/**
 * Allocate main view
 * @return Allocated view
 */
CoolixMainView* coolix_main_view_alloc(void);

/**
 * Free main view
 * @param view View to free
 */
void coolix_main_view_free(CoolixMainView* view);

/**
 * Get underlying View
 * @param view Main view
 * @return View pointer for ViewDispatcher
 */
View* coolix_main_view_get_view(CoolixMainView* view);

/**
 * Set state reference
 * @param view Main view
 * @param state State pointer
 */
void coolix_main_view_set_state(CoolixMainView* view, CoolixState* state);

/**
 * Set callback for sending IR commands
 * @param view Main view
 * @param callback Callback function
 * @param context Callback context
 */
void coolix_main_view_set_send_callback(
    CoolixMainView* view,
    CoolixMainViewSendCallback callback,
    void* context);

/**
 * Set callback for navigating to the Extra menu
 * @param view Main view
 * @param callback Callback function
 * @param context Callback context
 */
void coolix_main_view_set_extra_callback(
    CoolixMainView* view,
    CoolixMainViewNavigateCallback callback,
    void* context);

/**
 * Set callback for navigating to setup menu
 * @param view Main view
 * @param callback Callback function
 * @param context Callback context
 */
void coolix_main_view_set_setup_callback(
    CoolixMainView* view,
    CoolixMainViewNavigateCallback callback,
    void* context);

/**
 * Start sending animation
 * @param view Main view
 */
void coolix_main_view_start_sending(CoolixMainView* view);

/**
 * Update sending animation frame
 * @param view Main view
 */
void coolix_main_view_update_sending(CoolixMainView* view);

/**
 * Stop sending animation
 * @param view Main view
 */
void coolix_main_view_stop_sending(CoolixMainView* view);

/**
 * Get the last command that was triggered
 * Used by app to determine what IR to send
 *
 * @param view Main view
 * @param out_toggle Output: toggle command (if applicable)
 * @return 0 for state command, 1 for toggle command
 */
int coolix_main_view_get_last_command(CoolixMainView* view, CoolixToggle* out_toggle);

#ifdef __cplusplus
}
#endif
