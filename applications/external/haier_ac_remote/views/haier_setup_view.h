#pragma once

#include <gui/view.h>
#include "../haier_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HaierSetupView HaierSetupView;

/**
 * Allocate setup view
 * @return Allocated view
 */
HaierSetupView* haier_setup_view_alloc(void);

/**
 * Free setup view
 * @param view View to free
 */
void haier_setup_view_free(HaierSetupView* view);

/**
 * Get underlying View
 * @param view Setup view
 * @return View pointer for ViewDispatcher
 */
View* haier_setup_view_get_view(HaierSetupView* view);

/**
 * Set state reference
 * @param view Setup view
 * @param state State pointer
 */
void haier_setup_view_set_state(HaierSetupView* view, HaierState* state);

#ifdef __cplusplus
}
#endif
