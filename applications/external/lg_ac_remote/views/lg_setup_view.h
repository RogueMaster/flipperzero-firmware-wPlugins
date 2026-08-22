#pragma once

#include <gui/view.h>
#include "../lg_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LgSetupView LgSetupView;

/**
 * Allocate setup view
 * @return Allocated view
 */
LgSetupView* lg_setup_view_alloc(void);

/**
 * Free setup view
 * @param view View to free
 */
void lg_setup_view_free(LgSetupView* view);

/**
 * Get underlying View
 * @param view Setup view
 * @return View pointer for ViewDispatcher
 */
View* lg_setup_view_get_view(LgSetupView* view);

/**
 * Set state reference
 * @param view Setup view
 * @param state State pointer
 */
void lg_setup_view_set_state(LgSetupView* view, LgState* state);

#ifdef __cplusplus
}
#endif
