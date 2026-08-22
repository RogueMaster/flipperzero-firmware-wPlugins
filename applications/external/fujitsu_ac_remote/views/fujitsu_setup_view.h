#pragma once

#include <gui/view.h>
#include "../fujitsu_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FujitsuSetupView FujitsuSetupView;

/**
 * Allocate setup view
 * @return Allocated view
 */
FujitsuSetupView* fujitsu_setup_view_alloc(void);

/**
 * Free setup view
 * @param view View to free
 */
void fujitsu_setup_view_free(FujitsuSetupView* view);

/**
 * Get underlying View
 * @param view Setup view
 * @return View pointer for ViewDispatcher
 */
View* fujitsu_setup_view_get_view(FujitsuSetupView* view);

/**
 * Set state reference
 * @param view Setup view
 * @param state State pointer
 */
void fujitsu_setup_view_set_state(FujitsuSetupView* view, FujitsuState* state);

#ifdef __cplusplus
}
#endif
