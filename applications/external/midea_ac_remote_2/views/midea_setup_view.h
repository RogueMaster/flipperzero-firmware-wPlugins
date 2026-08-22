#pragma once

#include <gui/view.h>
#include "../midea_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MideaSetupView MideaSetupView;

/**
 * Allocate setup view
 * @return Allocated view
 */
MideaSetupView* midea_setup_view_alloc(void);

/**
 * Free setup view
 * @param view View to free
 */
void midea_setup_view_free(MideaSetupView* view);

/**
 * Get underlying View
 * @param view Setup view
 * @return View pointer for ViewDispatcher
 */
View* midea_setup_view_get_view(MideaSetupView* view);

/**
 * Set state reference
 * @param view Setup view
 * @param state State pointer
 */
void midea_setup_view_set_state(MideaSetupView* view, MideaState* state);

#ifdef __cplusplus
}
#endif
