#pragma once

#include <gui/view.h>
#include "../ballu_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BalluSetupView BalluSetupView;

/**
 * Allocate setup view
 * @return Allocated view
 */
BalluSetupView* ballu_setup_view_alloc(void);

/**
 * Free setup view
 * @param view View to free
 */
void ballu_setup_view_free(BalluSetupView* view);

/**
 * Get underlying View
 * @param view Setup view
 * @return View pointer for ViewDispatcher
 */
View* ballu_setup_view_get_view(BalluSetupView* view);

/**
 * Set state reference
 * @param view Setup view
 * @param state State pointer
 */
void ballu_setup_view_set_state(BalluSetupView* view, BalluState* state);

#ifdef __cplusplus
}
#endif
