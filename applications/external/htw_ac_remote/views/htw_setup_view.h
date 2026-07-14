#pragma once

#include <gui/view.h>
#include "../htw_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HtwSetupView HtwSetupView;

/**
 * Allocate setup view
 * @return Allocated view
 */
HtwSetupView* htw_setup_view_alloc(void);

/**
 * Free setup view
 * @param view View to free
 */
void htw_setup_view_free(HtwSetupView* view);

/**
 * Get underlying View
 * @param view Setup view
 * @return View pointer for ViewDispatcher
 */
View* htw_setup_view_get_view(HtwSetupView* view);

/**
 * Set state reference
 * @param view Setup view
 * @param state State pointer
 */
void htw_setup_view_set_state(HtwSetupView* view, HtwState* state);

#ifdef __cplusplus
}
#endif
