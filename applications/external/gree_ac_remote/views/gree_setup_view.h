#pragma once

#include <gui/view.h>
#include "../gree_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GreeSetupView GreeSetupView;

/**
 * Allocate setup view
 * @return Allocated view
 */
GreeSetupView* gree_setup_view_alloc(void);

/**
 * Free setup view
 * @param view View to free
 */
void gree_setup_view_free(GreeSetupView* view);

/**
 * Get underlying View
 * @param view Setup view
 * @return View pointer for ViewDispatcher
 */
View* gree_setup_view_get_view(GreeSetupView* view);

/**
 * Set state reference
 * @param view Setup view
 * @param state State pointer
 */
void gree_setup_view_set_state(GreeSetupView* view, GreeState* state);

#ifdef __cplusplus
}
#endif
