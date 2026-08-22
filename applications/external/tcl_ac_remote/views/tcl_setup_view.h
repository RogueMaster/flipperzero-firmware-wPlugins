#pragma once

#include <gui/view.h>
#include "../tcl_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TclSetupView TclSetupView;

/**
 * Allocate setup view
 * @return Allocated view
 */
TclSetupView* tcl_setup_view_alloc(void);

/**
 * Free setup view
 * @param view View to free
 */
void tcl_setup_view_free(TclSetupView* view);

/**
 * Get underlying View
 * @param view Setup view
 * @return View pointer for ViewDispatcher
 */
View* tcl_setup_view_get_view(TclSetupView* view);

/**
 * Set state reference
 * @param view Setup view
 * @param state State pointer
 */
void tcl_setup_view_set_state(TclSetupView* view, TclState* state);

#ifdef __cplusplus
}
#endif
